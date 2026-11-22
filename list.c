// list.c
// Implementation for linked list of memory blocks.
//
// Ownership rule: every block_t lives in exactly one list at a time, and a
// list "owns" its blocks. list_free() therefore frees the blocks too.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

/* Allocates an empty list.
 * Returns: pointer to the new list (exits on allocation failure). */
list_t *list_alloc() {
  list_t* list = (list_t*)malloc(sizeof(list_t));
  if (list == NULL) {
    fprintf(stderr, "Error: out of memory\n");
    exit(1);
  }
  list->head = NULL;
  return list;
}

/* Allocates a node that points at blk.
 * Params:  blk - block the node will hold (not copied)
 * Returns: pointer to the new node (exits on allocation failure). */
node_t *node_alloc(block_t *blk) {
  node_t* node = malloc(sizeof(node_t));
  if (node == NULL) {
    fprintf(stderr, "Error: out of memory\n");
    exit(1);
  }
  node->next = NULL;
  node->blk = blk;
  return node;
}

/* Frees a single node. Does NOT free the block it points to.
 * Params: node - node to free */
void node_free(node_t *node){
  free(node);
  return;
}

/* Frees the list struct, every node in it, and every block those nodes
 * point to. Safe to call on an already-emptied list (frees only the struct).
 * Params: l - list to free */
void list_free(list_t *l){
  node_t *current = l->head;
  while (current != NULL) {
    node_t *next = current->next;
    free(current->blk);
    node_free(current);
    current = next;
  }
  free(l);
}

/* Prints every block in the list, or a message if it is empty.
 * Params: l - list to print */
void list_print(list_t *l) {
  node_t *current = l->head;
  block_t *b;

  if (current == NULL){
    printf("list is empty\n");
  }
  while (current != NULL){
    b = current->blk;
    printf("PID=%d START:%d END:%d\n", b->pid, b->start, b->end);
    current = current->next;
  }
}

/* Counts the nodes in the list.
 * Params:  l - list to measure
 * Returns: number of nodes */
int list_length(list_t *l) {
  node_t *current = l->head;
  int i = 0;
  while (current != NULL){
    i++;
    current = current->next;
  }
  return i;
}

/* Appends blk at the end of the list (FIFO order, used by First Fit).
 * Params: l - list to modify, blk - block to add */
void list_add_to_back(list_t *l, block_t *blk){
  node_t* newNode = node_alloc(blk);
  if(l->head == NULL){
    l->head = newNode;
  }
  else{
    node_t *current = l->head;
    while(current->next != NULL){
      current = current->next;
    }
    current->next = newNode;
  }
}

/* Inserts blk at the front of the list.
 * Params: l - list to modify, blk - block to add */
void list_add_to_front(list_t *l, block_t *blk){
  node_t* newNode = node_alloc(blk);
  newNode->next = l->head;
  l->head = newNode;
}

/* Inserts blk so that it ends up at position index (0 = front).
 * An index <= 0, or any index on an empty list, inserts at the front.
 * An index past the end appends to the back.
 * Params: l - list to modify, blk - block to add, index - target position */
void list_add_at_index(list_t *l, block_t *blk, int index){
  if (index <= 0 || l->head == NULL) {
    list_add_to_front(l, blk);
    return;
  }

  node_t *newNode = node_alloc(blk);
  node_t *current = l->head;
  int i = 0;

  // walk to the node just before the target position (or the last node)
  while (i < index - 1 && current->next != NULL) {
    current = current->next;
    i++;
  }
  newNode->next = current->next;
  current->next = newNode;
}

/* Inserts newblk in ascending order by START address (keeps the allocated
 * list in physical memory order, and is used when coalescing).
 * Params: l - list to modify, newblk - block to insert */
void list_add_ascending_by_address(list_t *l, block_t *newblk){
  node_t *newNode = node_alloc(newblk);
  node_t *prev = NULL;
  node_t *current = l->head;

  // advance while the current node should stay ahead of newblk
  while (current != NULL && newblk->start > current->blk->start) {
    prev = current;
    current = current->next;
  }

  newNode->next = current;
  if (prev == NULL)
    l->head = newNode;
  else
    prev->next = newNode;
}

/* Inserts newblk in ascending order by block size (end - start + 1).
 * Ties go behind existing equal-sized blocks. Used by Best Fit.
 * Params: l - list to modify, newblk - block to insert */
void list_add_ascending_by_blocksize(list_t *l, block_t *newblk){
  node_t *newNode = node_alloc(newblk);
  node_t *prev = NULL;
  node_t *current = l->head;
  int newsize = newblk->end - newblk->start + 1;

  // insert after any node whose size is <= newsize
  while (current != NULL &&
         newsize >= (current->blk->end - current->blk->start + 1)) {
    prev = current;
    current = current->next;
  }

  newNode->next = current;
  if (prev == NULL)
    l->head = newNode;
  else
    prev->next = newNode;
}

/* Inserts blk in descending order by block size (end - start + 1).
 * Used by Worst Fit. A block equal in size to the head goes in front of it;
 * ties with later nodes go behind them (original provided behavior, kept so
 * output still matches the reference).
 * Params: l - list to modify, blk - block to insert */
void list_add_descending_by_blocksize(list_t *l, block_t *blk){
  node_t *newNode = node_alloc(blk);
  node_t *prev;
  node_t *current;
  int newblk_size = blk->end - blk->start + 1;
  int curblk_size;

  if (l->head == NULL) {
    l->head = newNode;
    return;
  }

  current = l->head;
  curblk_size = current->blk->end - current->blk->start + 1;

  if (newblk_size >= curblk_size) {      // place in front of head
    newNode->next = l->head;
    l->head = newNode;
    return;
  }

  // walk past every node that is at least as large as the new block
  prev = current;
  while (current != NULL && newblk_size <= curblk_size) {
    prev = current;
    current = current->next;
    if (current != NULL)
      curblk_size = current->blk->end - current->blk->start + 1;
  }
  prev->next = newNode;
  newNode->next = current;
}

/* Merges physically adjacent nodes (prev.end + 1 == current.start).
 * Requires: l is already sorted by ascending address.
 * The absorbed node and its block are freed; merged blocks get pid 0.
 * Params: l - sorted list of free blocks */
void list_coalese_nodes(list_t *l){
  if (l->head == NULL)
    return;

  node_t *prev = l->head;
  node_t *current = prev->next;
  prev->blk->pid = 0;

  while (current != NULL) {
    if (prev->blk->end + 1 == current->blk->start) {
      // physically adjacent: absorb current into prev
      prev->blk->end = current->blk->end;
      prev->next = current->next;
      free(current->blk);
      node_free(current);
      current = prev->next;      // prev stays put; it may merge again
    }
    else {
      prev = current;
      prev->blk->pid = 0;
      current = current->next;
    }
  }
}

/* Removes the last node and returns its block.
 * Params:  l - list to modify
 * Returns: the removed block, or NULL if the list is empty */
block_t* list_remove_from_back(list_t *l){
  block_t *value = NULL;
  node_t *current = l->head;

  if (current == NULL)
    return NULL;

  if (current->next == NULL) {        // exactly one node
    value = current->blk;
    node_free(current);
    l->head = NULL;                   // FIXED: head no longer dangles
  }
  else {
    while (current->next->next != NULL)
      current = current->next;
    value = current->next->blk;
    node_free(current->next);
    current->next = NULL;
  }
  return value;
}

/* Returns the first block without removing it.
 * Params:  l - list to inspect
 * Returns: the first block, or NULL if the list is empty */
block_t* list_get_from_front(list_t *l) {
  if (l->head == NULL)
    return NULL;
  return l->head->blk;
}

/* Removes the first node and returns its block.
 * Params:  l - list to modify
 * Returns: the removed block, or NULL if the list is empty */
block_t* list_remove_from_front(list_t *l) {
  block_t *value = NULL;
  if (l->head != NULL) {
    node_t *current = l->head;
    value = current->blk;
    l->head = current->next;
    node_free(current);
  }
  return value;
}

/* Removes the node at position index and returns its block.
 * Params:  l - list to modify, index - position (0 = front)
 * Returns: the removed block, or NULL if the list is empty or index is
 *          out of range */
block_t* list_remove_at_index(list_t *l, int index) {
  block_t* value = NULL;

  if (l->head == NULL || index < 0)
    return NULL;
  if (index == 0)
    return list_remove_from_front(l);

  node_t *prev = l->head;
  node_t *current = prev->next;
  int i = 1;

  while (current != NULL && i < index) {
    prev = current;
    current = current->next;
    i++;
  }

  if (current != NULL) {
    value = current->blk;
    prev->next = current->next;
    node_free(current);
  }
  return value;
}

/* Returns true if two blocks have identical pid, start, and end. */
bool compareBlks(block_t* a, block_t *b) {
  if(a->pid == b->pid && a->start == b->start && a->end == b->end)
     return true;
  return false;
}

/* Returns true if a request of size a fits in block b (a <= size of b). */
bool compareSize(int a, block_t *b) {
  if(a <= (b->end - b->start) + 1)
     return true;
  return false;
}

/* Returns true if pid a equals the pid of block b. */
bool comparePid(int a, block_t *b) {
  if(a == b->pid)
     return true;
  return false;
}

/* Checks whether a block identical to value is in the list.
 * Params:  l - list to search, value - block to look for
 * Returns: true if found */
bool list_is_in(list_t *l, block_t* value) {
  node_t *current = l->head;
  while(current != NULL){
    if(compareBlks(value, current->blk)){
      return true;
    }
    current = current->next;
  }
  return false;
}

/* Returns the block at position index.
 * Params:  l - list to search, index - position (0 = front)
 * Returns: the block, or NULL if index is out of range */
block_t* list_get_elem_at(list_t *l, int index) {
  int i = 0;
  node_t *current = l->head;

  if (index < 0)
    return NULL;

  while(current != NULL){
    if(i == index)
      return current->blk;
    current = current->next;
    i++;
  }
  return NULL;
}

/* Returns the index of the first block identical to value, or -1. */
int list_get_index_of(list_t *l, block_t* value){
  int i = 0;
  node_t *current = l->head;

  while (current != NULL){
    if (compareBlks(value, current->blk))
      return i;
    current = current->next;
    i++;
  }
  return -1;
}

/* Checks whether a block of Size or greater exists in the list.
 * Params:  l - list to search, Size - requested size
 * Returns: true if one exists */
bool list_is_in_by_size(list_t *l, int Size){
  node_t *current = l->head;
  while(current != NULL){
    if(compareSize(Size, current->blk)){
      return true;
    }
    current = current->next;
  }
  return false;
}

/* Checks whether a block owned by pid exists in the list.
 * Params:  l - list to search, pid - process id to look for
 * Returns: true if found */
bool list_is_in_by_pid(list_t *l, int pid){
  node_t *current = l->head;
  while (current != NULL) {
    if (comparePid(pid, current->blk))
      return true;
    current = current->next;
  }
  return false;
}

/* Returns the index of the first block of Size or greater, or -1. */
int list_get_index_of_by_Size(list_t *l, int Size){
  int i = 0;
  node_t *current = l->head;

  while (current != NULL){
    if (compareSize(Size, current->blk))
      return i;
    current = current->next;
    i++;
  }
  return -1;
}

/* Returns the index of the first block owned by pid, or -1. */
int list_get_index_of_by_Pid(list_t *l, int pid){
  int i = 0;
  node_t *current = l->head;

  while (current != NULL){
    if (comparePid(pid, current->blk))
      return i;
    current = current->next;
    i++;
  }
  return -1;
}
/*
    call_tree.h
 */

#ifndef call_tree_h
#define call_tree_h

typedef struct call_tree call_tree;

struct call_tree {
    void* pc;               /* program counter */
    call_tree* parent;      /* parent node     */
    call_tree* siblings;    /* next sibling    */
    call_tree* children;    /* children nodes  */
};

#endif


/**
 * This file defines the binary tree class and its
 * nsNode child class.
 *
 * This simple version stores nodes, and the
 * nodes store void* ptrs.
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */ 

/** 
 * MODULE NOTES 
 * @update gess 4/11/98
 *
 * This file declares the nsRBTree (red/black tree).
 * Red/black trees are auto-balancing binary trees.
 *
 * To use this class, define a subclass of nsNode
 * which stores your data type. It's important that
 * you overload the following methods:
 *
 *      virtual PRBool operator<()
 *      virtual PRBool operator==()
 *
 */ 

#ifndef _nsRBTree
#define _nsRBTree


#include "nsBTree.h"


/**
 * Here comes the main event: our nsRBTree (red/black tree).
 * Red/Black trees are autobalancing binary trees.
 *
 * @update	gess4/20/98
 */

class NS_BASE nsRBTree : public nsBTree {
public:
friend class NS_BASE nsRBTreeIterator;
  
  /**
   * nsRBTree constructor
   * 
   * @update gess 4/11/98
   */
  nsRBTree();

  /**
   * nsRBTree constructor
   * 
   * @update gess 4/11/98
   */
  nsRBTree(const nsRBTree& aCopy);

  /**
   * nsRBTree destructor
   * 
   * @update gess 4/11/98
   */
  ~nsRBTree();

  /**
   * Given a node, we're supposed to add it into 
   * our tree.
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */
  nsNode*   Add(nsNode& aNode);

  /**
   * Retrive the first node in the tree
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */
  nsNode*   First(void);
  
  /**
   * Retrieve the first node given a starting node
   * 
   * @update gess 4/11/98
   * @param  aNode --
   * @return node ptr or null
   */
  nsNode*   First(nsNode& aNode);
  
  /**
   * Find the last node in the tree
   * 
   * @update gess 4/11/98
   * @param  
   * @return node ptr or null
   */
  nsNode*   Last(void);
  
  /**
   * Find the last node from a given node
   * 
   * @update gess 4/11/98
   * @param  aNode -- node ptr to start from
   * @return node ptr or null
   */
  nsNode*   Last(nsNode& aNode);
  
  /**
   * Retrieve the node that preceeds the given node
   * 
   * @update gess 4/11/98
   * @param  aNode -- node to find precedent of
   * @return preceeding node ptr, or null
   */
  nsNode*   Before(nsNode& aNode);
  
  /**
   * Retrieve a ptr to the node following the given node
   * 
   * @update gess 4/11/98
   * @param  aNode -- node to find successor node from
   * @return node ptr or null
   */
  nsNode*   After(nsNode& aNode);

  /**
   * Find a (given) node in the tree
   * 
   * @update gess 4/11/98
   * @param  node to find in the tree
   * @return node ptr (if found) or null
   */
  nsNode*   Find(nsNode& aNode);

private:

  /**
   * Causes a shift to the left, to keep the
   * underlying RB data in balance
   * 
   * @update gess 4/11/98
   * @param  
   * @return this
   */
  nsRBTree&   ShiftLeft(nsNode& aNode);

  /**
   * Causes a shift right to occur, to keep the
   * underlying RB data in balance
   * 
   * @update gess 4/11/98
   * @param  aNode -- node at which to perform shift
   * @return this
   */
  nsRBTree&   ShiftRight(nsNode& aNode);

  /**
   * Rebalances tree around the given node. This only
   * needs to be called after a node is deleted.
   * 
   * @update gess 4/11/98
   * @param  aNode -- node to balance around
   * @return this
   */
  virtual nsBTree& ReBalance(nsNode& aNode);

};

class NS_BASE nsRBTreeIterator {
public:

  /**
   * TreeIterator constructor
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */
  nsRBTreeIterator(const nsRBTree& aTree);

  /**
   * TreeIterator constructor
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */
  nsRBTreeIterator(const nsRBTreeIterator& aCopy);

  /**
   * tree iterator destructor
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */
  ~nsRBTreeIterator();

  /**
   * This method iterates over the tree, calling
   * aFunctor for each node.
   * 
   * @update gess 4/11/98
   * @param  aFunctor -- object to call for each node
   * @param  aNode -- node at which to start iteration
   * @return this
   */
  const nsRBTreeIterator& ForEach(nsNodeFunctor& aFunctor) const;

protected:
  const nsRBTree& mTree;

};


#endif

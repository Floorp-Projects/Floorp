/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/** 
 * This file defines the binary tree class and it
 * nsNode child class. Note that like all nsBTree
 * containers, this one does not automatically balance.
 * (Find for random data, bad for pre-ordered data).
 *
 * This simple version stores nodes, and the
 * nodes store void* ptrs.
 * 
 * @update gess 4/11/98
 */


/**
 * nsNode
 * 
 * @update gess 4/11/98
 * @param  
 * @return 
 */

#ifndef _BTREE_H
#define _BTREE_H

#include "nscore.h"

struct NS_BASE nsNode {

  /**
   * 
   * @update	gess4/20/98
   * @param 
   * @return
   */
  nsNode();

  /**
   * Copy constructor
   * @update gess 4/11/98
   */  
  nsNode(const nsNode& aNode);

  /**
   * destructor
   * @update gess 4/11/98
   */  
  virtual ~nsNode();

  /**
   * Retrieve parent node
   *
   * @update gess 4/11/98
   * @return 
   */  
  nsNode*  GetParentNode(void) const;

  /**
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */  
  nsNode*  GetLeftNode() const;

  /**
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */  
  nsNode*  GetRightNode() const;

  /**
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */  
  virtual nsNode& operator=(const nsNode& aNode);


  /**
   * This method gets called to determine which of 
   * two nodes is less. When you create your own
   * subclass of nsNode, this is the most important
   * method for you to overload.
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */ 
  virtual PRBool operator<(const nsNode& aNode) const=0;

  /** 
   * This method gets called to determine which of 
   * two nodes is less. When you create your own
   * subclass of nsNode, this is the most important
   * method for you to overload.
   * 
   * @update gess 4/11/98
   * @param  
   * @return 
   */ 
  virtual PRBool operator==(const nsNode& aNode) const=0;

  enum eRBColor {eRed,eBlack};

  nsNode*   mParent;
  nsNode*   mLeft;
  nsNode*   mRight;
  eRBColor  mColor;
};

/**
 * The Nodefunctor class is used when you want to create
 * callbacks between the nsRBTree and your generic code.
 *
 * @update	gess4/20/98
 */
class NS_BASE nsNodeFunctor {
public:
  virtual nsNodeFunctor& operator()(nsNode& aNode)=0;
};


/****************************************************
 * Here comes the nsBTree class...
 ****************************************************/

class NS_BASE nsBTree {
public:
friend class nsBTreeIterator;
  
  nsBTree();
  ~nsBTree();

  /** 
   * Add given node reference into our tree.
   * 
   * @update gess 4/11/98
   * @param  aNode is a ref to a node to be added
   * @return newly added node
   */ 
  nsNode*  Add(nsNode& aNode);

  /** 
   * Remove given node reference into our tree.
   * 
   * @update gess 4/11/98
   * @param  aNode is a ref to a node to be removed
   * @return Ptr to node if found (and removed) or NULL
   */ 
  nsNode*  Remove(nsNode& aNode);

  /**
   * Clears the tree of any data. 
   * Be careful here if your objects are heap based!
   * This method doesn't free the objects, so if you
   * don't have your own pointers, they will become
   * orphaned.
   * 
   * @update gess 4/11/98
   * @param  
   * @return this
   */
   nsBTree& Empty(nsNode* aNode=0);

  /**
   * This method destroys all the objects in the tree.
   * WARNING: Never call this method on stored objects
   *          that are stack-based!
   * 
   * @update gess 4/11/98
   * @param  
   * @return this
   */
   nsBTree& Erase(nsNode* aNode=0);

  /** 
   * Retrieve ptr to 1st node in tree (starting at root)
   * 
   * @update gess 4/11/98
   * @return ptr to 1st node, possible to be NULL
   */ 
  nsNode*  First(void) const;

  /** 
   * Find first node in tree starting at given node
   * 
   * @update gess 4/11/98
   * @param  aNode node to begin 1st search from
   * @return ptr to 1st node below given node
   */ 
  nsNode*  First(const nsNode& aNode) const;

  /** 
   * Retrieve ptr to last node in tree relative to root.
   * 
   * @update gess 4/11/98
   * @return ptr to last node or NULL
   */ 
  nsNode* Last(void) const;

  /** 
   * Retrieve ptr to last node in tree relative to given node.
   * 
   * @update gess 4/11/98
   * @param  node to find last node from 
   * @return ptr to last node or NULL
   */ 
  nsNode* Last(const nsNode& aNode) const;

  /** 
   * Retrieve a ptr to the node that preceeds given node
   * 
   * @update gess 4/11/98
   * @param  aNode used as reference to find prev. 
   * @return ptr to prev node or NULL
   */ 
  nsNode*  Before(const nsNode& aNode) const;

  /** 
   * Retrieve a ptr to the node after given node
   * 
   * @update gess 4/11/98
   * @param  aNode used as reference to find next. 
   * @return ptr to next node or NULL
   */ 
  nsNode*  After(const nsNode& aNode) const;

  /** 
   * Find given node in tree.
   * (Why would you want to find a node you already have?)
   * 
   * @update gess 4/11/98
   * @param  aNode is the node you're searching for
   * @return ptr to node if found, or NULL
   */ 
  nsNode*  Find(const nsNode& aNode) const;

  /** 
   * Walks the tree, starting with root.
   * 
   * @update gess 4/11/98
   */ 
  virtual const nsBTree& ForEach(nsNodeFunctor& aFunctor,nsNode* aNode=0) const;
            
protected:

  /**
   * Rebalances tree around the given node. This only
   * needs to be called after a node is deleted.
   * 
   * @update gess 4/11/98
   * @param  aNode -- node to balance around
   * @return this
   */
  virtual nsBTree& ReBalance(nsNode& aNode);


  nsNode*  mRoot;
};

#endif


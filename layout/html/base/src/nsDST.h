/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsDST_h___
#define nsDST_h___

#include <memory.h>
#include "plarena.h"
#ifdef NS_DEBUG
#include <stdio.h>
#endif

/**
 * Digital search tree for doing a radix-search of pointer-based keys
 */
class nsDST {
public:
  typedef unsigned long PtrBits;

  // By ignoring low-order pointer bits that are always 0, the tree height can
  // be reduced. Because pointer memory should be at least 32-bit aligned, the
  // default is for level 0 of the tree to start with bit 0x04 (i.e., we ignore
  // the two low-order bits)
  nsDST(PtrBits aLevelZeroBit = 0x04);
  ~nsDST();

  void* Search(void* aKey) const;
  void* Insert(void* aKey, void* aValue);  // returns the previous value (or 0)
  void* Remove(void* aKey);
  void  Clear();

#ifdef NS_DEBUG
  void  Dump(FILE*) const;
#endif

private:
  struct Node;
  struct NodeArena;
  friend struct Node;       // needs access to struct NodeArena
  friend struct NodeArena;  // needs access to struct Node

  struct Node {
    void* mKey;
    void* mValue;
    Node* mLeft;   // left subtree
    Node* mRight;  // right subtree

    Node(void* aKey, void* aValue);
    int IsLeaf() const;

    // Overloaded placement operator for allocating from an arena
    void* operator new(size_t aSize, NodeArena& aArena);
  };

  struct NodeArena {
    PLArenaPool mPool;
    Node*       mFreeList;

    NodeArena();
    ~NodeArena();

    void* AllocNode(size_t);
    void  FreeNode(void*);
    void  FreeArenaPool();
  };

  Node*       mRoot;  // root node of the tree
  NodeArena   mArena;
  PtrBits     mLevelZeroBit;

private:
  // Helper functions
  Node** SearchTree(void* aKey) const;
  Node** FindLeafNode(Node** aNode) const;
  void   DestroyNode(Node* aNode);

#ifdef NS_DEBUG
  // Diagnostic functions
  Node* DepthFirstSearch(Node* aNode, void* aKey) const;
  void  VerifyTree(Node* aNode, int aLevel = 0, PtrBits aLevelKeyBits = 0) const;
  void  GatherStatistics(Node* aNode,
                         int   aLevel,
                         int&  aNumNodes,
                         int   aNodesPerLevel[]) const;
#endif

	nsDST(const nsDST&);           // no implementation
	void operator=(const nsDST&);  // no implementation
};

#endif /* nsDST_h___ */


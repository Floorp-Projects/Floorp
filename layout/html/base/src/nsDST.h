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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
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
  class LeafNode;
  class TwoNode;
  struct NodeArena;
  friend class LeafNode;
  friend class TwoNode;
  friend struct NodeArena;  // needs access to structs LeafNode and TwoNode

  struct NodeArena {
    PLArenaPool mPool;
    LeafNode*   mLeafNodeFreeList;
    TwoNode*    mTwoNodeFreeList;

    NodeArena();
    ~NodeArena();

    void*   AllocLeafNode();
    void*   AllocTwoNode();
    void    FreeNode(LeafNode*);
    void    FreeNode(TwoNode*);
    void    FreeArenaPool();
  };

  LeafNode*   mRoot;  // root node of the tree
  NodeArena   mArena;
  PtrBits     mLevelZeroBit;

private:
  // Helper functions
  LeafNode*   RemoveLeftMostLeafNode(TwoNode** aTwoNode);
  void        DestroyNode(LeafNode* aLeafNode);
  void        DestroyNode(TwoNode* aTwoNode);
  LeafNode*   ConvertToLeafNode(TwoNode** aTwoNode);
  TwoNode*    ConvertToTwoNode(LeafNode** aLeafNode);

#ifdef NS_DEBUG
  // Diagnostic functions
  void        VerifyTree(LeafNode* aNode, int aLevel = 0, PtrBits aLevelKeyBits = 0) const;
  LeafNode*   DepthFirstSearch(LeafNode* aNode, void* aKey) const;
  void        GatherStatistics(LeafNode* aNode,
                               int       aLevel,
                               int&      aNumLeafNodes,
                               int&      aNumTwoNodes,
                               int       aNodesPerLevel[]) const;
#endif

	nsDST(const nsDST&);           // no implementation
	void operator=(const nsDST&);  // no implementation
};

#endif /* nsDST_h___ */


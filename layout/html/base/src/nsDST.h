/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsDST_h___
#define nsDST_h___

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include "nsError.h"
#ifdef NS_DEBUG
#include <stdio.h>
#endif

#include "plarena.h"

/**
 * Function-like object used when enumerating the nodes of the DST
 */  
class nsDSTNodeFunctor {
public:
  virtual void operator() (void* aKey, void* aValue) = 0;  // call operator
};

// Option flags for Search() member function
#define NS_DST_REMOVE_KEY_VALUE   0x0001

// nsresult error codes
#define NS_DST_KEY_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 1)

#define NS_DST_VALUE_OVERWRITTEN \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 2)

/**
 * Digital search tree for doing a radix-search of pointer-based keys
 */
class nsDST {
public:
  class NodeArena;
  friend class NodeArena;

private:
  class LeafNode;
  class TwoNode;
  friend class LeafNode;
  friend class TwoNode;

public:
  typedef unsigned long PtrBits;

  // Memory arena pool used for fast allocation and deallocation of DST nodes.
  // Maintains a free-list of freed objects.
  // Node arenas can be shared across DST objects (they don't lock when allocating
  // and freeing memory, so don't share them across threads). The DST object(s)
  // own the node arena, and you just hold a weak reference.
  class NodeArena {
  public:
    NodeArena(PRUint32 aArenaSize);
    ~NodeArena();

    // Memory management functions
    void*     AllocLeafNode();
    void*     AllocTwoNode();
    void      FreeNode(LeafNode*);
    void      FreeNode(TwoNode*);
    void      FreeArenaPool();

    // Lifetime management functions
    void      AddRef() {mRefCnt++;}
    void      Release();
    PRBool    IsShared() const {return mRefCnt > 1;}

  #ifdef NS_DEBUG
    int       NumArenas() const;
    PRUint32  ArenaSize() const {return mPool.arenasize;}
  #endif

  private:
    PLArenaPool mPool;
    LeafNode*   mLeafNodeFreeList;
    TwoNode*    mTwoNodeFreeList;
    PRUint32    mRefCnt;
  };

  // Create a DST. You specify the node arena to use; this allows the arena to
  // be shared.
  // By ignoring low-order pointer bits that are always 0, the tree height can
  // be reduced. Because pointer memory should be at least 32-bit aligned, the
  // default is for level 0 of the tree to start with bit 0x04 (i.e., we ignore
  // the two low-order bits)
  nsDST(NodeArena* aArena, PtrBits aLevelZeroBit = 0x04);
  ~nsDST();

  nsresult  Search(void* aKey, unsigned aOptions, void** aValue);
  nsresult  Insert(void* aKey, void* aValue, void** aOldValue);
  nsresult  Remove(void* aKey);
  nsresult  Clear();
  nsresult  Enumerate(nsDSTNodeFunctor& aFunctor) const;

#ifdef NS_DEBUG
  void  Dump(FILE*) const;
#endif

  // Create a memory arena pool. You can specify the size of the underlying arenas
  static NodeArena*  NewMemoryArena(PRUint32 aArenaSize = 512);

private:

  /////////////////////////////////////////////////////////////////////////////
  // Classes that represents nodes in the DST

  // To reduce the amount of memory we use there are two types of nodes:
  // - leaf nodes
  // - two nodes (left and right child)
  //
  // We distinguish the two types of nodes by looking at the low-order
  // bit of the key. If it is 0, then the node is a leaf node. If it is
  // 1, then the node is a two node. Use function Key() when retrieving
  // the key, and function IsLeaf() to tell what type of node it is
  //
  // It's an invariant of the tree that a two node can not have both child links
  // NULL. In that case it must be converted back to a leaf node
  //
  // NOTE: This code was originally put into nsDST.cpp to reduce size
  //       and to hide the implementation - See Troy's 1.10 revision.
  //       However the AIX xlC 3.6.4.0 compiler does not allow this.

  class LeafNode {
  public:
    void* mKey;
    void* mValue;

    // Constructors
    LeafNode(void* aKey, void* aValue);
    LeafNode(const LeafNode& aLeafNode);

    // Accessor for getting the key. Clears the bit used to tell if the
    // node is a leaf. This function can be safely used on any node without
    // knowing whether it's a leaf or a two node
    void*   Key() const {return (void*)(PtrBits(mKey) & ~0x1);}

    // Helper function that returns TRUE if the node is a leaf
    int     IsLeaf() const {return 0 == (PtrBits(mKey) & 0x1);}

    // Overloaded placement operator for allocating from an arena
    void* operator new(size_t aSize, NodeArena* aArena) {return aArena->AllocLeafNode();}
  };

  // Definition of TwoNode class
  class TwoNode : public LeafNode {
  public:
    LeafNode* mLeft;   // left subtree
    LeafNode* mRight;  // right subtree

    TwoNode(const LeafNode& aLeafNode);

    void    SetKeyAndValue(void* aKey, void* aValue) {
      mKey = (void*)(PtrBits(aKey) | 0x01);
      mValue = aValue;
    }

    // Overloaded placement operator for allocating from an arena
    void* operator new(size_t aSize, NodeArena* aArena) {return aArena->AllocTwoNode();}

  private:
    TwoNode(const TwoNode&);        // no implementation
    void operator=(const TwoNode&); // no implementation
  };

  LeafNode*   mRoot;  // root node of the tree
  NodeArena*  mArena;
  PtrBits     mLevelZeroBit;

private:
  // Helper functions
  LeafNode*   RemoveLeftMostLeafNode(TwoNode** aTwoNode);
  void        DestroyNode(LeafNode* aLeafNode);
  void        DestroyNode(TwoNode* aTwoNode);
  LeafNode*   ConvertToLeafNode(TwoNode** aTwoNode);
  TwoNode*    ConvertToTwoNode(LeafNode** aLeafNode);
  void        EnumTree(LeafNode* aNode, nsDSTNodeFunctor& aFunctor) const;
  void        FreeTree(LeafNode* aNode);
  nsresult    SearchTree(void* aKey, unsigned aOptions, void** aValue);

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


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
#define PL_ARENA_CONST_ALIGN_MASK 3
#include "nslayout.h"
#include "nsDST.h"
#include "nsISupportsUtils.h"
#ifdef NS_DEBUG
#include <string.h>
#endif

// Constructors
inline nsDST::LeafNode::LeafNode(void* aKey, void* aValue)
  : mKey(aKey), mValue(aValue)
{
}
  
inline nsDST::LeafNode::LeafNode(const LeafNode& aNode)
  : mKey(aNode.Key()),  // don't assume it's a leaf node
    mValue(aNode.mValue)
{
}

// Constructor
inline nsDST::TwoNode::TwoNode(const LeafNode& aLeafNode)
  : nsDST::LeafNode((void*)(PtrBits(aLeafNode.mKey) | 0x1), aLeafNode.mValue),
    mLeft(0), mRight(0)
{
}

// If you know that the node is a leaf node, then you can quickly access
// the key without clearing the bit that indicates whether it's a leaf or
// a two node
#define DST_GET_LEAF_KEY(leaf)  ((leaf)->mKey)

// Macros to check whether we branch left or branch right for a given
// key and bit mask
#define DST_BRANCHES_LEFT(key, mask) \
  (0 == (PtrBits(key) & (mask)))
  
#define DST_BRANCHES_RIGHT(key, mask) \
  ((mask) == (PtrBits(key) & (mask)))

/////////////////////////////////////////////////////////////////////////////
// Arena used for fast allocation and deallocation of Node structures.
// Maintains free-list of freed objects

nsDST::NodeArena*
nsDST::NewMemoryArena(PRUint32 aArenaSize)
{
  return new NodeArena(aArenaSize);
}

MOZ_DECL_CTOR_COUNTER(NodeArena)

// Constructor
nsDST::NodeArena::NodeArena(PRUint32 aArenaSize)
  : mLeafNodeFreeList(0), mTwoNodeFreeList(0), mRefCnt(0)
{
  MOZ_COUNT_CTOR(NodeArena);
  PL_INIT_ARENA_POOL(&mPool, "DSTNodeArena", aArenaSize);
}

// Destructor
nsDST::NodeArena::~NodeArena()
{
  MOZ_COUNT_DTOR(NodeArena);

  // Free the arena in the pool and finish using it
  PL_FinishArenaPool(&mPool);
}

void
nsDST::NodeArena::Release()
{
  NS_PRECONDITION(mRefCnt > 0, "unexpected ref count");
  if (--mRefCnt == 0) {
    delete this;
  }
}

// Called by the nsDST::LeafNode's overloaded placement operator when
// allocating a new node. First checks the free list. If the free list is
// empty, then it allocates memory from the arena
void*
nsDST::NodeArena::AllocLeafNode()
{
  void* p;
  
  if (mLeafNodeFreeList) {
    // Remove the node at the head of the free-list
    p = mLeafNodeFreeList;
    mLeafNodeFreeList = (LeafNode*)mLeafNodeFreeList->mKey;

  } else {
    PL_ARENA_ALLOCATE(p, &mPool, sizeof(nsDST::LeafNode));
  }

  return p;
}

// Called by the nsDST::TwoNode's overloaded placement operator when
// allocating a new node. First checks the free list. If the free list is
// empty, then it allocates memory from the arena
void*
nsDST::NodeArena::AllocTwoNode()
{
  void* p;
  
  if (mTwoNodeFreeList) {
    // Remove the node at the head of the free-list
    p = mTwoNodeFreeList;
    mTwoNodeFreeList = (TwoNode*)mTwoNodeFreeList->mKey;

  } else {
    PL_ARENA_ALLOCATE(p, &mPool, sizeof(nsDST::TwoNode));
  }

  return p;
}

// Called by the DST's DestroyNode() function. Adds the node to the head
// of the free list where it can be reused by AllocateNode()
void
nsDST::NodeArena::FreeNode(LeafNode* aLeafNode)
{
#ifdef NS_DEBUG
  memset(aLeafNode, 0xde, sizeof(*aLeafNode));
#endif
  // Add this node to the head of the free-list
  aLeafNode->mKey = mLeafNodeFreeList;
  mLeafNodeFreeList = aLeafNode;
}

// Called by the DST's DestroyNode() function. Adds the node to the head
// of the free list where it can be reused by AllocateNode()
void
nsDST::NodeArena::FreeNode(TwoNode* aTwoNode)
{
#ifdef NS_DEBUG
  memset(aTwoNode, 0xde, sizeof(*aTwoNode));
#endif
  // Add this node to the head of the free-list
  aTwoNode->mKey = mTwoNodeFreeList;
  mTwoNodeFreeList = aTwoNode;
}

// Called by the DST's Clear() function when we want to free the memory
// in the arena pool, but continue using the arena
void
nsDST::NodeArena::FreeArenaPool()
{
  // Free the arena in the pool, but continue using it
  PL_FreeArenaPool(&mPool);

  // Clear the free lists
  mLeafNodeFreeList = 0;
  mTwoNodeFreeList = 0;
}

#ifdef NS_DEBUG
int
nsDST::NodeArena::NumArenas() const
{
  // Calculate the number of arenas in use
  int numArenas = 0;
  for (PLArena* arena = mPool.first.next; arena; arena = arena->next) {
    numArenas++;
  }

  return numArenas;
}
#endif

/////////////////////////////////////////////////////////////////////////////
// Digital search tree for doing a radix-search of pointer-based keys

MOZ_DECL_CTOR_COUNTER(nsDST)

// Constructor
nsDST::nsDST(NodeArena* aArena, PtrBits aLevelZeroBit)
  : mRoot(0), mArena(aArena), mLevelZeroBit(aLevelZeroBit)
{
  NS_PRECONDITION(aArena, "no node arena");
  MOZ_COUNT_CTOR(nsDST);

  // Add a reference to the node arena
  mArena->AddRef();
}

// Destructor
nsDST::~nsDST()
{
  MOZ_COUNT_DTOR(nsDST);

  // If the node arena is shared, then we'll need to explicitly
  // free each node in the tree
  if (mArena->IsShared()) {
    FreeTree(mRoot);
  }

  // Release our reference to the node arena
  mArena->Release();
}

// Called by Remove() to destroy a node. Explicitly calls the destructor
// and then asks the memory arena to free the memory
inline void
nsDST::DestroyNode(LeafNode* aLeafNode)
{
  aLeafNode->~LeafNode();      // call destructor
  mArena->FreeNode(aLeafNode); // free memory
}

// Called by Remove() to destroy a node. Explicitly calls the destructor
// and then asks the memory arena to free the memory
inline void
nsDST::DestroyNode(TwoNode* aTwoNode)
{
  aTwoNode->~TwoNode();       // call destructor
  mArena->FreeNode(aTwoNode); // free memory
}

void
nsDST::FreeTree(LeafNode* aNode)
{
keepLooping:
  if (!aNode) {
    return;
  }

  if (aNode->IsLeaf()) {
    DestroyNode(aNode);

  } else {
    LeafNode* left = ((TwoNode*)aNode)->mLeft;
    LeafNode* right = ((TwoNode*)aNode)->mRight;
  
    DestroyNode((TwoNode*)aNode);
    FreeTree(left);
    aNode = right;
    goto keepLooping;    
  }
}

// Removes all nodes from the tree
nsresult
nsDST::Clear()
{
  if (mArena->IsShared()) {
    FreeTree(mRoot);
  } else {
    mArena->FreeArenaPool();
  }
  mRoot = 0;
  return NS_OK;
}

// Enumerate all the nodes in the tree
nsresult
nsDST::Enumerate(nsDSTNodeFunctor& aFunctor) const
{
  EnumTree(mRoot, aFunctor);
  return NS_OK;
}

void
nsDST::EnumTree(LeafNode* aNode, nsDSTNodeFunctor& aFunctor) const
{
keepLooping:
  if (!aNode) {
    return;
  }

  // Call the function-like object
  aFunctor(aNode->Key(), aNode->mValue);

  if (!aNode->IsLeaf()) {
    EnumTree(((TwoNode*)aNode)->mLeft, aFunctor);
    aNode = ((TwoNode*)aNode)->mRight;
    goto keepLooping;    
  }
}

nsDST::LeafNode*
nsDST::ConvertToLeafNode(TwoNode** aTwoNode)
{
  LeafNode* leaf = new (mArena)LeafNode((*aTwoNode)->Key(), (*aTwoNode)->mValue);

  DestroyNode(*aTwoNode);
  *aTwoNode = (TwoNode*)leaf;
  return leaf;
}

nsDST::TwoNode*
nsDST::ConvertToTwoNode(LeafNode** aLeafNode)
{
  TwoNode* twoNode = new (mArena)TwoNode(**aLeafNode);

  DestroyNode(*aLeafNode);
  *aLeafNode = (LeafNode*)twoNode;
  return twoNode;
}

// Searches the tree for a node with the specified key. Return the value
// or NULL if the key is not in the tree
nsresult
nsDST::Search(void* aKey, unsigned aOptions, void** aValue)
{
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = 0;  // initialize OUT parameter
  
  nsresult  result = SearchTree(aKey, aOptions, aValue);

#ifdef DEBUG_troy
  if (NS_DST_KEY_NOT_THERE == result) {
    // Use an alternative algorithm to verify that there's really
    // no node with a matching key
    DepthFirstSearch(mRoot, aKey);
  }
#endif
  return result;
}

// Adds a new key to the tree. If the specified key is already in the
// tree, then the existing value is replaced by the new value. Returns
// NS_DST_VALUE_OVERWRITTEN if there is an existing value that is
// overwritten
nsresult
nsDST::Insert(void* aKey, void* aValue, void** aOldValue)
{
  NS_PRECONDITION(0 == (PtrBits(aKey) & (mLevelZeroBit - 1)),
                  "ignored low-order bits are not zero");

  // Initialize OUT parameter
  if (aOldValue) {
    *aOldValue = 0;
  }
  
  // See if there's an existing node with a matching key
  LeafNode**  node = (LeafNode**)&mRoot;
  TwoNode*    branchReduction = 0;
  nsresult    result = NS_OK;

  if (*node) {
    PtrBits   bitMask = mLevelZeroBit;

    while (1) {
      // See if the key matches
      if ((*node)->Key() == aKey) {
        // We found an existing node with a matching key. Replace the value and
        // don't do a branch reduction
        branchReduction = 0;
        break;
      }
      
      // Is this a leaf node?
      if ((*node)->IsLeaf()) {
        if (!branchReduction) {
          // Replace the leaf node with a two node and destroy the
          // leaf node
          TwoNode*  twoNode = ConvertToTwoNode(node);
          
          // Exit the loop and allocate a new leaf node and set its
          // key and value
          node = DST_BRANCHES_LEFT(aKey, bitMask) ? &twoNode->mLeft :
                                                    &twoNode->mRight;
        }
        break;

      } else {
        TwoNode*& twoNode = *(TwoNode**)node;

        // Check whether we search the left branch or the right branch
        if (DST_BRANCHES_LEFT(aKey, bitMask)) {
          // If there's a left node and no right node, then see if we
          // can reduce the one way braching in the tree
          if (twoNode->mLeft && !twoNode->mRight) {
            if (DST_BRANCHES_RIGHT(twoNode->mKey, bitMask)) {
              // Yes, this node can become the right node of the tree. Remember
              // this for later in case we don't find a matching node
              branchReduction = twoNode;
            }
          }
          
          node = &twoNode->mLeft;

        } else {
          // If there's a right node and no left node, then see if we
          // can reduce the one way braching in the tree
          if (twoNode->mRight && !twoNode->mLeft) {
            if (DST_BRANCHES_LEFT(twoNode->mKey, bitMask)) {
              // Yes, this node can become the left node of the tree. Remember
              // this for later in case we don't find a matching node
              branchReduction = twoNode;
            }
          }

          node = &twoNode->mRight;
        }

        // Did we reach a null link?
        if (!*node) {
          break;
        }
      }

      // Move to the next bit in the key
      bitMask <<= 1;
    }
  }

  if (branchReduction) {
    // Reduce the one way branching by moving the existing key and
    // value to either the right or left node
    if (!branchReduction->mLeft) {
      NS_ASSERTION(branchReduction->mRight, "bad state");
      branchReduction->mLeft = new (mArena)LeafNode(*branchReduction);

    } else {
      NS_ASSERTION(!branchReduction->mRight, "bad state");
      branchReduction->mRight = new (mArena)LeafNode(*branchReduction);
    }

    // Replace the existing key and value with the new key and value
    branchReduction->SetKeyAndValue(aKey, aValue);

  } else if (*node) {
    // We found an existing node with a matching key. Replace the current
    // value with the new value
    if (aOldValue) {
      *aOldValue = (*node)->mValue;
    }
    (*node)->mValue = aValue;
    result = NS_DST_VALUE_OVERWRITTEN;

  } else {
    // Allocate a new leaf node and insert it into the tree
    *node = new (mArena)LeafNode(aKey, aValue);
  }

#ifdef DEBUG_troy
  VerifyTree(mRoot);

  // Verify that one and only one node in the tree have the specified key
  DepthFirstSearch(mRoot, aKey);
#endif
  return result;
}

// Helper function that removes and returns the left most leaf node
// of the specified subtree.
// Note: if the parent of the node that is removed is now a leaf,
// it will be converted to a leaf node...
nsDST::LeafNode*
nsDST::RemoveLeftMostLeafNode(TwoNode** aTwoNode)
{
  NS_PRECONDITION(!(*aTwoNode)->IsLeaf(), "bad parameter");

keepLooking:
  LeafNode**  child;

  if ((*aTwoNode)->mLeft) {
    // Walk down the left branch
    child = &(*aTwoNode)->mLeft;
    
    // See if it's a leaf
    if ((*child)->IsLeaf()) {
      // Remove the child from its parent
      LeafNode* result = *child;
      *child = 0;

      // If there's no right node then the parent is now a leaf so
      // convert it to a leaf node
      if (!(*aTwoNode)->mRight) {
        ConvertToLeafNode(aTwoNode);
      }

      // Return the leaf node
      return result;
    }


  } else if ((*aTwoNode)->mRight) {
    // No left branch, so walk down the right branch
    child = &(*aTwoNode)->mRight;

    if ((*child)->IsLeaf()) {
      // Remove the child from its parent
      LeafNode* result = *child;
      *child = 0;

      // That was the parent's only child node so convert the parent
      // node to a leaf node
      ConvertToLeafNode(aTwoNode);

      // Return the leaf node
      return result;
    }

  } else {
    // We should never encounter a two node with both links NULL. It should
    // have been coverted to a leaf instead...
    NS_ASSERTION(0, "bad node type");
    return 0;
  }

  aTwoNode = (TwoNode**)child;
  goto keepLooking;
}

// Returns NS_OK if there is a matching key and NS_DST_KEY_NOT_THERE
// otherwise
nsresult
nsDST::SearchTree(void* aKey, unsigned aOptions, void** aValue)
{
  NS_PRECONDITION(0 == (PtrBits(aKey) & (mLevelZeroBit - 1)),
                  "ignored low-order bits are not zero");

  if (mRoot) {
    LeafNode**  node;
    TwoNode**   parentNode = 0;
    PtrBits     bitMask = mLevelZeroBit;
    
    if (mRoot->Key() == aKey) {
      node = (LeafNode**)&mRoot;

    } else if (mRoot->IsLeaf()) {
      return NS_DST_KEY_NOT_THERE;  // no node with a matching key

    } else {
      // Look for a node with a matching key
      node = (LeafNode**)&mRoot;
      while (1) {
        NS_ASSERTION(!(*node)->IsLeaf(), "unexpected leaf mode");
        parentNode = (TwoNode**)node;
        
        // Check whether we search the left branch or the right branch
        if (DST_BRANCHES_LEFT(aKey, bitMask)) {
          node = &(*(TwoNode**)node)->mLeft;
        } else {
          node = &(*(TwoNode**)node)->mRight;
        }
  
        if (!*node) {
          // We found a NULL link which means no node with a matching key
          return NS_DST_KEY_NOT_THERE;
        }
        
        // Check if the key matches
        if ((*node)->Key() == aKey) {
          break;
        }

        // The key doesn't match. If this is a leaf node that means no
        // node with a matching key
        if ((*node)->IsLeaf()) {
          return NS_DST_KEY_NOT_THERE;
        }

        // Move to the next bit in the key
        bitMask <<= 1;
      }
    }

    // We found a matching node
    *aValue = (*node)->mValue;

    // Should we remove the key/value pair?
    if (aOptions & NS_DST_REMOVE_KEY_VALUE) {
      if ((*node)->IsLeaf()) {
        // Delete the leaf node
        DestroyNode(*node);
  
        // Disconnect the node from its parent node
        *node = 0;
  
        // If the parent now has no child nodes, then convert it to a
        // leaf frame
        if (parentNode && !(*parentNode)->mLeft && !(*parentNode)->mRight) {
          ConvertToLeafNode(parentNode);
        }
  
      } else {
        // We can't just move the left or right subtree up one level, because
        // then we would have to re-sort the tree. Instead replace the node's
        // key and value with that of its left most leaf node (any leaf frame
        // would do)
        LeafNode* leaf = RemoveLeftMostLeafNode((TwoNode**)node);
  
        // Copy over the leaf's key and value
        // Note: RemoveLeftMostLeafNode() may have converted "node" to a 
        // leaf node so don't make any assumptions here
        if ((*node)->IsLeaf()) {
          (*node)->mKey = DST_GET_LEAF_KEY(leaf);
        } else {
          (*node)->mKey = (void*)(PtrBits(DST_GET_LEAF_KEY(leaf)) | 0x01);
        }
        (*node)->mValue = leaf->mValue;
  
        // Delete the leaf node
        DestroyNode(leaf);
      }
#ifdef DEBUG_troy
      VerifyTree(mRoot);
#endif
    }
  
    return NS_OK;
  }

  return NS_DST_KEY_NOT_THERE;
}

// Removes a key from the tree. Returns NS_OK if successful and
// NS_DST_KEY_NOT_THERE if there is no node with a matching key
nsresult
nsDST::Remove(void* aKey)
{
  void*     value;
  nsresult  result = SearchTree(aKey, NS_DST_REMOVE_KEY_VALUE, &value);

#ifdef DEBUG_troy
  if (NS_OK == result) {
    // We found a node with a matching key and we removed it. Verify that
    // we successfully removed the node
    void* ignoreValue;

    NS_POSTCONDITION(Search(aKey, 0, &ignoreValue) == NS_DST_KEY_NOT_THERE,
                     "remove operation failed");
  }
#endif

  return result;
}

#ifdef NS_DEBUG
// Helper function used to verify the integrity of the tree. Does a
// depth-first search of the tree looking for a node with the specified
// key. Also verifies that the key is not in the tree more than once
nsDST::LeafNode*
nsDST::DepthFirstSearch(LeafNode* aNode, void* aKey) const
{
  if (!aNode) {
    return 0;
  } else if (aNode->IsLeaf()) {
    return (aNode->Key() == aKey) ? aNode : 0;
  } else {
    // Search the left branch of the tree
    LeafNode* result = DepthFirstSearch(((TwoNode*)aNode)->mLeft, aKey);

    if (result) {
      // Verify there's no matching node in the right branch
      NS_ASSERTION(!DepthFirstSearch(((TwoNode*)aNode)->mRight, aKey),
                   "key in tree more than once");

    } else {
      // Search the right branch of the tree
      result = DepthFirstSearch(((TwoNode*)aNode)->mRight, aKey);
    }

    // See if the node's key matches
    if (aNode->Key() == aKey) {
      NS_ASSERTION(!result, "key in tree more than once");
      result = aNode;
    }

    return result;
  }
}

// Helper function that verifies the integrity of the tree. Called
// by Insert() and Remove()
void
nsDST::VerifyTree(LeafNode* aNode, int aLevel, PtrBits aLevelKeyBits) const
{
  if (aNode) {
    // Verify that the first "aLevel" bits of this node's key agree with the
    // accumulated key bits that apply to this branch of the tree, i.e., the
    // path to this node as specified by the bits of the key
    if (aLevel > 0) {
      // When calculating the bit mask, take into consideration the low-order
      // bits we ignore
      PtrBits bitMask = (mLevelZeroBit << aLevel) - 1;
      NS_ASSERTION(aLevelKeyBits == (PtrBits(aNode->Key()) & bitMask),
                   "key's bits don't match");
    }

    if (!aNode->IsLeaf()) {
      const TwoNode*  twoNode = (TwoNode*)aNode;
      NS_ASSERTION(twoNode->mLeft || twoNode->mRight,
                   "two node with no child nodes");

      // All node keys in the left subtree should have the next bit set to 0
      VerifyTree(twoNode->mLeft, aLevel + 1, aLevelKeyBits);
  
      // All node keys in the left subtree should have the next bit set to 1
      VerifyTree(twoNode->mRight, aLevel + 1, aLevelKeyBits | (mLevelZeroBit << aLevel));
    }
  }
}

void
nsDST::GatherStatistics(LeafNode* aNode,
                        int       aLevel,
                        int&      aNumLeafNodes,
                        int&      aNumTwoNodes,
                        int       aNodesPerLevel[]) const
{
  if (aNode) {
    aNodesPerLevel[aLevel]++;
    if (aNode->IsLeaf()) {
      aNumLeafNodes++;
    } else {
      aNumTwoNodes++;
      GatherStatistics(((TwoNode*)aNode)->mLeft, aLevel + 1, aNumLeafNodes,
                       aNumTwoNodes, aNodesPerLevel);
      GatherStatistics(((TwoNode*)aNode)->mRight, aLevel + 1, aNumLeafNodes,
                       aNumTwoNodes, aNodesPerLevel);
    }
  }
}

void
nsDST::Dump(FILE* out) const
{
  // Walk the tree gathering statistics about the number of nodes, the height
  // of the tree (maximum node level), the average node level, and the median
  // node level
  static const int maxLevels = sizeof(void*) * 8;

  int numLeafNodes = 0;
  int numTwoNodes = 0;
  int nodesPerLevel[maxLevels];  // count of the number of nodes at a given level
  memset(&nodesPerLevel, 0, sizeof(int) * maxLevels);

  // Walk each node in the tree recording its node level
  GatherStatistics(mRoot, 0, numLeafNodes, numTwoNodes, nodesPerLevel);

  // Calculate the height, average node level, and median node level
  int height, medianLevel = 0, pathLength = 0;
  for (height = 0; height < maxLevels; height++) {
    int count = nodesPerLevel[height];
    if (0 == count) {
      break;
    }

    // Update the median node level
    if (count > nodesPerLevel[medianLevel]) {
      medianLevel = height;
    }

    // Update the path length
    pathLength += height * count;
  }

  // Output the statistics
  int numTotalNodes = numLeafNodes + numTwoNodes;

  fputs("DST statistics\n", out);
  fprintf(out, "  Number of leaf nodes: %d\n", numLeafNodes);
  fprintf(out, "  Number of tree nodes: %d\n", numTwoNodes);
  if (numTotalNodes > 0) {
    fprintf(out, "  Average node size: %.1f\n",
            float(numLeafNodes * sizeof(LeafNode) + numTwoNodes * sizeof(TwoNode)) /
            float(numTotalNodes));
  }
  fprintf(out, "  Number of arenas: %d(%d)\n", mArena->NumArenas(), mArena->ArenaSize());
  fprintf(out, "  Height (maximum node level) of the tree: %d\n", height - 1);
  if (numTotalNodes > 0) {
    fprintf(out, "  Average node level: %.1f\n", float(pathLength) / float(numTotalNodes));
  }
  fprintf(out, "  Median node level: %d\n", medianLevel);
  fprintf(out, "  Path length: %d\n", pathLength);

  // Output the number of nodes at each level of the tree
  fputs("  Nodes per level: ", out);
  fprintf(out, "%d", nodesPerLevel[0]);
  for (int i = 1; i < height; i++) {
    fprintf(out, ", %d", nodesPerLevel[i]);
  }
  fputs("\n", out);
}
#endif


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
#define PL_ARENA_CONST_ALIGN_MASK 7
#include "nslayout.h"
#include "nsDST.h"
#ifdef NS_DEBUG
#include <string.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Structure that represents a node in the DST

// Constructor
inline nsDST::Node::Node(void* aKey, void* aValue)
  : mKey(aKey), mValue(aValue), mLeft(0), mRight(0)
{
}

// Helper function that returns TRUE if the node is a leaf (i.e., no left or
// right child), and FALSE otherwise
inline int nsDST::Node::IsLeaf() const
{
  return !mLeft && !mRight;
}

// Overloaded placement operator for allocating from an arena
inline void* nsDST::Node::operator new(size_t aSize, NodeArena& aArena)
{
  return aArena.AllocNode(aSize);
}

/////////////////////////////////////////////////////////////////////////////
// Arena used for fast allocation and deallocation of Node structures.
// Maintains a free-list of freed objects

#define NS_DST_ARENA_BLOCK_SIZE   1024

// Constructor
nsDST::NodeArena::NodeArena()
  : mFreeList(0)
{
  PL_INIT_ARENA_POOL(&mPool, "DSTNodeArena", NS_DST_ARENA_BLOCK_SIZE);
}

// Destructor
nsDST::NodeArena::~NodeArena()
{
  // Free the arena in the pool and finish using it
  PL_FinishArenaPool(&mPool);
}

// Called by the nsDST::Node's overloaded placement operator when allocating
// a new node. First checks the free list. If the free list is empty, then
// it allocates memory from the arena
void*
nsDST::NodeArena::AllocNode(size_t aSize)
{
  void* p;
  
  if (mFreeList) {
    // Remove the node at the head of the free-list
    p = mFreeList;
    mFreeList = mFreeList->mLeft;
  } else {
    PL_ARENA_ALLOCATE(p, &mPool, aSize);
  }
  return p;
}

// Called by the DST's DestroyNode() function. Adds the node to the head
// of the free list where it can be reused by AllocateNode()
void
nsDST::NodeArena::FreeNode(void* p)
{
#ifdef NS_DEBUG
  memset(p, 0xde, sizeof(Node));
#endif
  // Add this node to the head of the free-list
  ((Node*)p)->mLeft = mFreeList;
  mFreeList = (Node*)p;
}

// Called by the DST's Clear() function when we want to free the memory
// in the arena pool, but continue using the arena
void
nsDST::NodeArena::FreeArenaPool()
{
  // Free the arena in the pool, but continue using it
  PL_FreeArenaPool(&mPool);

  // Clear the free list
  mFreeList = 0;
}

/////////////////////////////////////////////////////////////////////////////
// Digital search tree for doing a radix-search of pointer-based keys

// Constructor
nsDST::nsDST(PtrBits aLevelZeroBit)
  : mRoot(0), mLevelZeroBit(aLevelZeroBit)
{
}

// Destructor
nsDST::~nsDST()
{
}

// Removes all nodes from the tree
void
nsDST::Clear()
{
  mArena.FreeArenaPool();
  mRoot = 0;
}

// Adds a new key to the tree. If the specified key is already in the
// tree, then the existing value is replaced by the new value. Returns
// the old value, or NULL if this is a new key
void*
nsDST::Insert(void* aKey, void* aValue)
{
  // See if there's a matching key
  Node** node = SearchTree(aKey);
  void*  previousValue;

  if (*node) {
    previousValue = (*node)->mValue;
    
    // Replace the current value with the new value
    (*node)->mValue = aValue;

  } else {
    // Allocate a new node and insert it into the tree
    *node = new (mArena)Node(aKey, aValue);
    previousValue = 0;
  }

#ifdef DEBUG_troy
  VerifyTree(mRoot);
#endif
  return previousValue;
}

// Helper function that returns the left most leaf node of the specified
// subtree
nsDST::Node**
nsDST::GetLeftMostLeafNode(Node** aNode) const
{
keepLooking:
  // See if the node has none, one, or two child nodes
  if ((*aNode)->mLeft) {
    // Walk down the left branch
    aNode = &(*aNode)->mLeft;
    goto keepLooking;

  } else if ((*aNode)->mRight) {
    // No left branch, so walk down the right branch
    aNode = &(*aNode)->mRight;
    goto keepLooking;

  } else {
    // We found a leaf node
    return aNode;
  }
}

// Called by Remove() to destroy a node. Explicitly calls the destructor
// and then asks the memory arena to free the memory
inline void
nsDST::DestroyNode(Node* aNode)
{
  aNode->~Node();          // call destructor
  mArena.FreeNode(aNode);  // free memory
}

// Removes a key from the tree. Returns the current value, or NULL if
// the key is not in the tree
void*
nsDST::Remove(void* aKey)
{
  Node** node = SearchTree(aKey);

  if (*node) {
    Node* tmp = *node;
    void* value = (*node)->mValue;

    if ((*node)->IsLeaf()) {
      // Just disconnect the node from its parent node
      *node = 0;
    } else {
      // We can't just move the left or right subtree up one level, because
      // then we would have to re-sort the tree. Instead replace the node
      // with the left most leaf node
      Node** leaf = GetLeftMostLeafNode(node);

      // Copy over both the left and right subtree pointers
      if ((*node)->mLeft != (*leaf)) {
        (*leaf)->mLeft = (*node)->mLeft;
      }
      if ((*node)->mRight != (*leaf)) {
        (*leaf)->mRight = (*node)->mRight;
      }

      // Insert the leaf node in its new level, and disconnect it from its old
      // parent node
      *node = *leaf;
      *leaf = 0;
    }

    DestroyNode(tmp);
#ifdef DEBUG_troy
    VerifyTree(mRoot);
#endif
    return value;
  }

  return 0;
}

// Searches the tree for a node with the specified key. Return the value
// or NULL if the key is not in the tree
void*
nsDST::Search(void* aKey) const
{
  Node** result = SearchTree(aKey);

#ifdef DEBUG_troy
  if (!*result) {
    // Use an alternative algorithm to verify that the key is not in
    // the tree
    NS_POSTCONDITION(!DepthFirstSearch(mRoot, aKey), "DST search failed");
  }
#endif

  return (*result) ? (*result)->mValue : 0;
}

// Non-recursive search function. Returns a pointer to the pointer to the
// node. Called by Search(), Insert(), and Remove()
nsDST::Node**
nsDST::SearchTree(void* aKey) const
{
  NS_PRECONDITION(0 == (PtrBits(aKey) & (mLevelZeroBit - 1)),
                  "ignored low-order bits are not zero");

  Node**  result = (Node**)&mRoot;
  PtrBits bitMask = mLevelZeroBit;

  while (*result) {
    // Check if the node matches
    if ((*result)->mKey == aKey) {
      return result;
    }

    // Check whether we search the left branch or the right branch
    if (0 == (PtrBits(aKey) & bitMask)) {
      result = &(*result)->mLeft;
    } else {
      result = &(*result)->mRight;
    }
    // Move to the next bit in the key
    bitMask <<= 1;
  }

  // We failed to find the key: return where the node would be inserted
  return result;
}

#ifdef NS_DEBUG
// Helper function used to verify the integrity of the tree. Does a
// depth-first search of the tree looking for a node with the specified
// key. Called by Search() if we don't find the key using the radix-search
nsDST::Node*
nsDST::DepthFirstSearch(Node* aNode, void* aKey) const
{
  if (!aNode) {
    return 0;
  } else if (aNode->mKey == aKey) {
    return aNode;
  } else {
    Node* result = DepthFirstSearch(aNode->mLeft, aKey);

    if (!result) {
      result = DepthFirstSearch(aNode->mRight, aKey);
    }

    return result;
  }
}

// Helper function that verifies the integrity of the tree. Called
// by Insert() and Remove()
void
nsDST::VerifyTree(Node* aNode, int aLevel, PtrBits aLevelKeyBits) const
{
  if (aNode) {
    // Verify that the first "aLevel" bits of this node's key agree with the
    // accumulated key bits that apply to this branch of the tree, i.e., the
    // path to this node as specified by the bits of the key
    if (aLevel > 0) {
      // When calculating the bit mask, take into consideration the low-order
      // bits we ignore
      PtrBits bitMask = (mLevelZeroBit << aLevel) - 1;
      NS_ASSERTION(aLevelKeyBits == (PtrBits(aNode->mKey) & bitMask),
                   "key's bits don't match");
    }

    // All node keys in the left subtree should have the next bit set to 0
    VerifyTree(aNode->mLeft, aLevel + 1, aLevelKeyBits);

    // All node keys in the left subtree should have the next bit set to 1
    VerifyTree(aNode->mRight, aLevel + 1, aLevelKeyBits | (mLevelZeroBit << aLevel));
  }
}

void
nsDST::GatherStatistics(Node* aNode,
                        int   aLevel,
                        int&  aNumNodes,
                        int   aNodesPerLevel[]) const
{
  aNumNodes++;
  aNodesPerLevel[aLevel]++;
  if (aNode->mLeft) {
    GatherStatistics(aNode->mLeft, aLevel + 1, aNumNodes, aNodesPerLevel);
  }
  if (aNode->mRight) {
    GatherStatistics(aNode->mRight, aLevel + 1, aNumNodes, aNodesPerLevel);
  }
}

void
nsDST::Dump(FILE* out) const
{
  // Walk the tree gathering statistics about the number of nodes, the height
  // of the tree (maximum node level), the average node level, and the median
  // node level
  static const int maxLevels = sizeof(void*) * 8;

  int numNodes = 0;
  int nodesPerLevel[maxLevels];  // count of the number of nodes at a given level
  memset(&nodesPerLevel, 0, sizeof(int) * maxLevels);

  // Walk each node in the tree recording its node level
  GatherStatistics(mRoot, 0, numNodes, nodesPerLevel);

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
  fputs("DST statistics\n", out);
  fprintf(out, "  Number of nodes: %d\n", numNodes);
  fprintf(out, "  Height (maximum node level) of the tree: %d\n", height - 1);
  fprintf(out, "  Average node level: %.1f\n", float(pathLength) / float(numNodes));
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


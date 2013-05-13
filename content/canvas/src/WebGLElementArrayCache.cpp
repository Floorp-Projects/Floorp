/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLElementArrayCache.h"

#include "nsTArray.h"
#include "mozilla/Assertions.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <algorithm>

namespace mozilla {

/*
 * WebGLElementArrayCacheTree contains most of the implementation of WebGLElementArrayCache,
 * which performs WebGL element array buffer validation for drawElements.
 *
 * Attention: Here lie nontrivial data structures, bug-prone algorithms, and non-canonical tweaks!
 * Whence the explanatory comments, and compiled unit test.
 *
 * *** What problem are we solving here? ***
 *
 * WebGL::DrawElements has to validate that the elements are in range wrt the current vertex attribs.
 * This boils down to the problem, given an array of integers, of computing the maximum in an arbitrary
 * sub-array. The naive algorithm has linear complexity; this has been a major performance problem,
 * see bug 569431. In that bug, we took the approach of caching the max for the whole array, which
 * does cover most cases (DrawElements typically consumes the whole element array buffer) but doesn't
 * help in other use cases:
 *  - when doing "partial DrawElements" i.e. consuming only part of the element array buffer
 *  - when doing frequent "partial buffer updates" i.e. bufferSubData calls updating parts of the
 *    element array buffer
 *
 * *** The solution: a binary tree ***
 *
 * The solution implemented here is to use a binary tree as the cache data structure. Each tree node
 * contains the max of its two children nodes. In this way, finding the maximum in any contiguous sub-array
 * has log complexity instead of linear complexity.
 *
 * Simplistically, if the element array is
 *
 *     1   4   3   2
 *
 * then the corresponding tree is
 *
 *           4
 *         _/ \_
 *       4       3
 *      / \     / \
 *     1   4   3   2
 *
 * In practice, the bottom-most levels of the tree are both the largest to store (because they
 * have more nodes), and the least useful performance-wise (because each node in the bottom
 * levels concerns only few entries in the elements array buffer, it is cheap to compute).
 *
 * For this reason, we stop the tree a few levels above, so that each tree leaf actually corresponds
 * to more than one element array entry.
 *
 * The number of levels that we "drop" is |sSkippedBottomTreeLevels| and the number of element array entries
 * that each leaf corresponds to, is |sElementsPerLeaf|. This being a binary tree, we have
 *
 *   sElementsPerLeaf = 2 ^ sSkippedBottomTreeLevels.
 *
 * *** Storage layout of the binary tree ***
 *
 * We take advantage of the specifics of the situation to avoid generalist tree storage and instead
 * store the tree entries in a vector, mTreeData.
 *
 * The number of leaves is given by mNumLeaves, and mTreeData is always a vector of length
 *
 *    2 * mNumLeaves.
 *
 * Its data layout is as follows: mTreeData[0] is unused, mTreeData[1] is the root node,
 * then at offsets 2..3 is the tree level immediately below the root node, then at offsets 4..7
 * is the tree level below that, etc.
 *
 * The figure below illustrates this by writing at each tree node the offset into mTreeData at
 * which it is stored:
 *
 *           1
 *         _/ \_
 *       2       3
 *      / \     / \
 *     4   5   6   7
 *    ...
 *
 * Thus, under the convention that the root level is level 0, we see that level N is stored at offsets
 *
 *    [ 2^n .. 2^(n+1) - 1 ]
 *
 * in mTreeData. Likewise, all the usual tree operations have simple mathematical expressions in
 * terms of mTreeData offsets, see all the methods such as ParentNode, LeftChildNode, etc.
 *
 * *** Design constraint: element types aren't known at buffer-update time ***
 *
 * Note that a key constraint that we're operating under, is that we don't know the types of the elements
 * by the time WebGL bufferData/bufferSubData methods are called. The type of elements is only
 * specified in the drawElements call. This means that we may potentially have to store caches for
 * multiple element types, for the same element array buffer. Since we don't know yet how many
 * element types we'll eventually support (extensions add more), the concern about memory usage is serious.
 * This is addressed by sSkippedBottomTreeLevels as explained above. Of course, in the typical
 * case where each element array buffer is only ever used with one type, this is also addressed
 * by having WebGLElementArrayCache lazily create trees for each type only upon first use.
 *
 * Another consequence of this constraint is that when invalidating the trees, we have to invalidate
 * all existing trees. So if trees for types uint8_t, uint16_t and uint32_t have ever been constructed for this buffer,
 * every subsequent invalidation will have to invalidate all trees even if one of the types is never
 * used again. This implies that it is important to minimize the cost of invalidation i.e.
 * do lazy updates upon use as opposed to immediately updating invalidated trees. This poses a problem:
 * it is nontrivial to keep track of the part of the tree that's invalidated. The current solution
 * can only keep track of an invalidated interval, from |mFirstInvalidatedLeaf| to |mLastInvalidatedLeaf|.
 * The problem is that if one does two small, far-apart partial buffer updates, the resulting invalidated
 * area is very large even though only a small part of the array really needed to be invalidated.
 * The real solution to this problem would be to use a smarter data structure to keep track of the
 * invalidated area, probably an interval tree. Meanwhile, we can probably live with the current situation
 * as the unfavorable case seems to be a small corner case: in order to run into performance issues,
 * the number of bufferSubData in between two consecutive draws must be small but greater than 1, and
 * the partial buffer updates must be small and far apart. Anything else than this corner case
 * should run fast in the current setting.
 */
template<typename T>
struct WebGLElementArrayCacheTree
{
  // A too-high sSkippedBottomTreeLevels would harm the performance of small drawElements calls
  // A too-low sSkippedBottomTreeLevels would cause undue memory usage.
  // The current value has been validated by some benchmarking. See bug 732660.
  static const size_t sSkippedBottomTreeLevels = 3;
  static const size_t sElementsPerLeaf = 1 << sSkippedBottomTreeLevels;
  static const size_t sElementsPerLeafMask = sElementsPerLeaf - 1; // sElementsPerLeaf is POT

private:
  WebGLElementArrayCache& mParent;
  nsTArray<T> mTreeData;
  size_t mNumLeaves;
  bool mInvalidated;
  size_t mFirstInvalidatedLeaf;
  size_t mLastInvalidatedLeaf;

public:
  WebGLElementArrayCacheTree(WebGLElementArrayCache& p)
    : mParent(p)
    , mNumLeaves(0)
    , mInvalidated(false)
    , mFirstInvalidatedLeaf(0)
    , mLastInvalidatedLeaf(0)
  {
    ResizeToParentSize();
  }

  T GlobalMaximum() const {
    MOZ_ASSERT(!mInvalidated);
    return mTreeData[1];
  }

  // returns the index of the parent node; if treeIndex=1 (the root node),
  // the return value is 0.
  static size_t ParentNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex > 1);
    return treeIndex >> 1;
  }

  static bool IsRightNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex > 1);
    return treeIndex & 1;
  }

  static bool IsLeftNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex > 1);
    return !IsRightNode(treeIndex);
  }

  static size_t SiblingNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex > 1);
    return treeIndex ^ 1;
  }

  static size_t LeftChildNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex);
    return treeIndex << 1;
  }

  static size_t RightChildNode(size_t treeIndex) {
    MOZ_ASSERT(treeIndex);
    return SiblingNode(LeftChildNode(treeIndex));
  }

  static size_t LeftNeighborNode(size_t treeIndex, size_t distance = 1) {
    MOZ_ASSERT(treeIndex > 1);
    return treeIndex - distance;
  }

  static size_t RightNeighborNode(size_t treeIndex, size_t distance = 1) {
    MOZ_ASSERT(treeIndex > 1);
    return treeIndex + distance;
  }

  size_t LeafForElement(size_t element) {
    size_t leaf = element / sElementsPerLeaf;
    MOZ_ASSERT(leaf < mNumLeaves);
    return leaf;
  }

  size_t LeafForByte(size_t byte) {
    return LeafForElement(byte / sizeof(T));
  }

  // Returns the index, into the tree storage, where a given leaf is stored
  size_t TreeIndexForLeaf(size_t leaf) {
    // See above class comment. The tree storage is an array of length 2*mNumLeaves.
    // The leaves are stored in its second half.
    return leaf + mNumLeaves;
  }

  static size_t LastElementUnderSameLeaf(size_t element) {
    return element | sElementsPerLeafMask;
  }

  static size_t FirstElementUnderSameLeaf(size_t element) {
    return element & ~sElementsPerLeafMask;
  }

  static size_t NextMultipleOfElementsPerLeaf(size_t numElements) {
    return ((numElements - 1) | sElementsPerLeafMask) + 1;
  }

  bool Validate(T maxAllowed, size_t firstLeaf, size_t lastLeaf) {
    MOZ_ASSERT(!mInvalidated);

    size_t firstTreeIndex = TreeIndexForLeaf(firstLeaf);
    size_t lastTreeIndex  = TreeIndexForLeaf(lastLeaf);

    while (true) {
      // given that we tweak these values in nontrivial ways, it doesn't hurt to do
      // this sanity check
      MOZ_ASSERT(firstTreeIndex <= lastTreeIndex);

      // final case where there is only 1 node to validate at the current tree level
      if (lastTreeIndex == firstTreeIndex) {
        return mTreeData[firstTreeIndex] <= maxAllowed;
      }

      // if the first node at current tree level is a right node, handle it individually
      // and replace it with its right neighbor, which is a left node
      if (IsRightNode(firstTreeIndex)) {
        if (mTreeData[firstTreeIndex] > maxAllowed)
          return false;
        firstTreeIndex = RightNeighborNode(firstTreeIndex);
      }

      // if the last node at current tree level is a left node, handle it individually
      // and replace it with its left neighbor, which is a right node
      if (IsLeftNode(lastTreeIndex)) {
        if (mTreeData[lastTreeIndex] > maxAllowed)
          return false;
        lastTreeIndex = LeftNeighborNode(lastTreeIndex);
      }

      // at this point it can happen that firstTreeIndex and lastTreeIndex "crossed" each
      // other. That happens if firstTreeIndex was a right node and lastTreeIndex was its
      // right neighor: in that case, both above tweaks happened and as a result, they ended
      // up being swapped: lastTreeIndex is now the _left_ neighbor of firstTreeIndex.
      // When that happens, there is nothing left to validate.
      if (lastTreeIndex == LeftNeighborNode(firstTreeIndex)) {
        return true;
      }

      // walk up 1 level
      firstTreeIndex = ParentNode(firstTreeIndex);
      lastTreeIndex = ParentNode(lastTreeIndex);
    }
  }

  template<typename U>
  static U NextPowerOfTwo(U x) {
    U result = 1;
    while (result < x)
      result <<= 1;
    MOZ_ASSERT(result >= x);
    MOZ_ASSERT((result & (result - 1)) == 0);
    return result;
  }

  bool ResizeToParentSize()
  {
    size_t numberOfElements = mParent.ByteSize() / sizeof(T);
    size_t requiredNumLeaves = (numberOfElements + sElementsPerLeaf - 1) / sElementsPerLeaf;

    size_t oldNumLeaves = mNumLeaves;
    mNumLeaves = NextPowerOfTwo(requiredNumLeaves);
    Invalidate(0, mParent.ByteSize() - 1);

    // see class comment for why we the tree storage size is 2 * mNumLeaves
    if (!mTreeData.SetLength(2 * mNumLeaves)) {
      return false;
    }
    if (mNumLeaves != oldNumLeaves) {
      memset(mTreeData.Elements(), 0, mTreeData.Length() * sizeof(mTreeData[0]));
    }
    return true;
  }

  void Invalidate(size_t firstByte, size_t lastByte);

  void Update();

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + mTreeData.SizeOfExcludingThis(aMallocSizeOf);
  }
};

// TreeForType: just a template helper to select the right tree object for a given
// element type.
template<typename T>
struct TreeForType {};

template<>
struct TreeForType<uint8_t>
{
  static WebGLElementArrayCacheTree<uint8_t>*& Run(WebGLElementArrayCache *b) { return b->mUint8Tree; }
};

template<>
struct TreeForType<uint16_t>
{
  static WebGLElementArrayCacheTree<uint16_t>*& Run(WebGLElementArrayCache *b) { return b->mUint16Tree; }
};

template<>
struct TreeForType<uint32_t>
{
  static WebGLElementArrayCacheTree<uint32_t>*& Run(WebGLElementArrayCache *b) { return b->mUint32Tree; }
};

// When the buffer gets updated from firstByte to lastByte,
// calling this method will notify the tree accordingly
template<typename T>
void WebGLElementArrayCacheTree<T>::Invalidate(size_t firstByte, size_t lastByte)
{
  lastByte = std::min(lastByte, mNumLeaves * sElementsPerLeaf * sizeof(T) - 1);
  if (firstByte > lastByte) {
    return;
  }

  size_t firstLeaf = LeafForByte(firstByte);
  size_t lastLeaf = LeafForByte(lastByte);

  if (mInvalidated) {
    mFirstInvalidatedLeaf = std::min(firstLeaf, mFirstInvalidatedLeaf);
    mLastInvalidatedLeaf = std::max(lastLeaf, mLastInvalidatedLeaf);
  } else {
    mInvalidated = true;
    mFirstInvalidatedLeaf = firstLeaf;
    mLastInvalidatedLeaf = lastLeaf;
  }
}


// When tree has been partially invalidated, from mFirstInvalidatedLeaf to
// mLastInvalidatedLeaf, calling this method will 1) update the leaves in this interval
// from the raw buffer data, and 2) propagate this update up the tree
template<typename T>
void WebGLElementArrayCacheTree<T>::Update()
{
  if (!mInvalidated) {
    return;
  }

  MOZ_ASSERT(mLastInvalidatedLeaf < mNumLeaves);

  size_t firstTreeIndex = TreeIndexForLeaf(mFirstInvalidatedLeaf);
  size_t lastTreeIndex = TreeIndexForLeaf(mLastInvalidatedLeaf);

  // Step #1: initialize the tree leaves from plain buffer data.
  // That is, each tree leaf must be set to the max of the |sElementsPerLeaf| corresponding
  // buffer entries.
  // condition-less scope to prevent leaking this scope's variables into the code below
  {
    // treeIndex is the index of the tree leaf we're writing, i.e. the destination index
    size_t treeIndex = firstTreeIndex;
    // srcIndex is the index in the source buffer
    size_t srcIndex = mFirstInvalidatedLeaf * sElementsPerLeaf;
    size_t numberOfElements = mParent.ByteSize() / sizeof(T);
    while (treeIndex <= lastTreeIndex) {
      T m = 0;
      size_t a = srcIndex;
      size_t srcIndexNextLeaf = std::min(a + sElementsPerLeaf, numberOfElements);
      for (; srcIndex < srcIndexNextLeaf; srcIndex++) {
        m = std::max(m, mParent.Element<T>(srcIndex));
      }
      mTreeData[treeIndex] = m;
      treeIndex++;
    }
  }

  // Step #2: propagate the values up the tree. This is simply a matter of walking up
  // the tree and setting each node to the max of its two children.
  while (firstTreeIndex > 1) {

    // move up 1 level
    firstTreeIndex = ParentNode(firstTreeIndex);
    lastTreeIndex = ParentNode(lastTreeIndex);

    // fast-exit case where only one node is invalidated at the current level
    if (firstTreeIndex == lastTreeIndex) {
      mTreeData[firstTreeIndex] = std::max(mTreeData[LeftChildNode(firstTreeIndex)], mTreeData[RightChildNode(firstTreeIndex)]);
      continue;
    }

    // initialize local iteration variables: child and parent.
    size_t child = LeftChildNode(firstTreeIndex);
    size_t parent = firstTreeIndex;

    // the unrolling makes this look more complicated than it is; the plain non-unrolled
    // version is in the second while loop below
    const int unrollSize = 8;
    while (RightNeighborNode(parent, unrollSize - 1) <= lastTreeIndex)
    {
      for (int unroll = 0; unroll < unrollSize; unroll++)
      {
        T a = mTreeData[child];
        child = RightNeighborNode(child);
        T b = mTreeData[child];
        child = RightNeighborNode(child);
        mTreeData[parent] = std::max(a, b);
        parent = RightNeighborNode(parent);
      }
    }
    // plain non-unrolled version, used to terminate the job after the last unrolled iteration
    while (parent <= lastTreeIndex)
    {
      T a = mTreeData[child];
      child = RightNeighborNode(child);
      T b = mTreeData[child];
      child = RightNeighborNode(child);
      mTreeData[parent] = std::max(a, b);
      parent = RightNeighborNode(parent);
    }
  }

  mInvalidated = false;
}

WebGLElementArrayCache::~WebGLElementArrayCache() {
  delete mUint8Tree;
  delete mUint16Tree;
  delete mUint32Tree;
  free(mUntypedData);
}

bool WebGLElementArrayCache::BufferData(const void* ptr, size_t byteSize) {
  mByteSize = byteSize;
  if (mUint8Tree)
    if (!mUint8Tree->ResizeToParentSize())
      return false;
  if (mUint16Tree)
    if (!mUint16Tree->ResizeToParentSize())
      return false;
  if (mUint32Tree)
    if (!mUint32Tree->ResizeToParentSize())
      return false;
  mUntypedData = realloc(mUntypedData, byteSize);
  if (!mUntypedData)
    return false;
  BufferSubData(0, ptr, byteSize);
  return true;
}

void WebGLElementArrayCache::BufferSubData(size_t pos, const void* ptr, size_t updateByteSize) {
  if (!updateByteSize) return;
  if (ptr)
      memcpy(static_cast<uint8_t*>(mUntypedData) + pos, ptr, updateByteSize);
  else
      memset(static_cast<uint8_t*>(mUntypedData) + pos, 0, updateByteSize);
  InvalidateTrees(pos, pos + updateByteSize - 1);
}

void WebGLElementArrayCache::InvalidateTrees(size_t firstByte, size_t lastByte)
{
  if (mUint8Tree)
    mUint8Tree->Invalidate(firstByte, lastByte);
  if (mUint16Tree)
    mUint16Tree->Invalidate(firstByte, lastByte);
  if (mUint32Tree)
    mUint32Tree->Invalidate(firstByte, lastByte);
}

template<typename T>
bool WebGLElementArrayCache::Validate(uint32_t maxAllowed, size_t firstElement, size_t countElements) {
  // if maxAllowed is >= the max T value, then there is no way that a T index could be invalid
  if (maxAllowed >= std::numeric_limits<T>::max())
    return true;

  T maxAllowedT(maxAllowed);

  // integer overflow must have been handled earlier, so we assert that maxAllowedT
  // is exactly the max allowed value.
  MOZ_ASSERT(uint32_t(maxAllowedT) == maxAllowed);

  if (!mByteSize || !countElements)
    return true;

  WebGLElementArrayCacheTree<T>*& tree = TreeForType<T>::Run(this);
  if (!tree) {
    tree = new WebGLElementArrayCacheTree<T>(*this);
  }

  size_t lastElement = firstElement + countElements - 1;

  tree->Update();

  // fast exit path when the global maximum for the whole element array buffer
  // falls in the allowed range
  if (tree->GlobalMaximum() <= maxAllowedT)
  {
    return true;
  }

  const T* elements = Elements<T>();

  // before calling tree->Validate, we have to validate ourselves the boundaries of the elements span,
  // to round them to the nearest multiple of sElementsPerLeaf.
  size_t firstElementAdjustmentEnd = std::min(lastElement,
                                            tree->LastElementUnderSameLeaf(firstElement));
  while (firstElement <= firstElementAdjustmentEnd) {
    if (elements[firstElement] > maxAllowedT)
      return false;
    firstElement++;
  }
  size_t lastElementAdjustmentEnd = std::max(firstElement,
                                           tree->FirstElementUnderSameLeaf(lastElement));
  while (lastElement >= lastElementAdjustmentEnd) {
    if (elements[lastElement] > maxAllowedT)
      return false;
    lastElement--;
  }

  // at this point, for many tiny validations, we're already done.
  if (firstElement > lastElement)
    return true;

  // general case
  return tree->Validate(maxAllowedT,
                        tree->LeafForElement(firstElement),
                        tree->LeafForElement(lastElement));
}

bool WebGLElementArrayCache::Validate(GLenum type, uint32_t maxAllowed, size_t firstElement, size_t countElements) {
  if (type == LOCAL_GL_UNSIGNED_BYTE)
    return Validate<uint8_t>(maxAllowed, firstElement, countElements);
  if (type == LOCAL_GL_UNSIGNED_SHORT)
    return Validate<uint16_t>(maxAllowed, firstElement, countElements);
  if (type == LOCAL_GL_UNSIGNED_INT)
    return Validate<uint32_t>(maxAllowed, firstElement, countElements);
  return false;
}

size_t WebGLElementArrayCache::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const {
  size_t uint8TreeSize  = mUint8Tree  ? mUint8Tree->SizeOfIncludingThis(aMallocSizeOf) : 0;
  size_t uint16TreeSize = mUint16Tree ? mUint16Tree->SizeOfIncludingThis(aMallocSizeOf) : 0;
  size_t uint32TreeSize = mUint32Tree ? mUint32Tree->SizeOfIncludingThis(aMallocSizeOf) : 0;
  return aMallocSizeOf(this) +
          mByteSize +
          uint8TreeSize +
          uint16TreeSize +
          uint32TreeSize;
}

} // end namespace mozilla

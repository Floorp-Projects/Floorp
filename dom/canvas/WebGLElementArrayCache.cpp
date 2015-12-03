/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLElementArrayCache.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

static void
UpdateUpperBound(uint32_t* const out_upperBound, uint32_t newBound)
{
    MOZ_ASSERT(out_upperBound);
    // Move *out_upperBound to a local variable to work around a false positive
    // -Wuninitialized gcc warning about std::max() in PGO builds.
    uint32_t upperBound = *out_upperBound;
    *out_upperBound = std::max(upperBound, newBound);
}

/* WebGLElementArrayCacheTree contains most of the implementation of
 * WebGLElementArrayCache, which performs WebGL element array buffer validation
 * for drawElements.
 *
 * Attention: Here lie nontrivial data structures, bug-prone algorithms, and
 * non-canonical tweaks! Whence the explanatory comments, and compiled unit
 * test.
 *
 * *** What problem are we solving here? ***
 *
 * WebGL::DrawElements has to validate that the elements are in range wrt the
 * current vertex attribs. This boils down to the problem, given an array of
 * integers, of computing the maximum in an arbitrary sub-array. The naive
 * algorithm has linear complexity; this has been a major performance problem,
 * see bug 569431. In that bug, we took the approach of caching the max for the
 * whole array, which does cover most cases (DrawElements typically consumes the
 * whole element array buffer) but doesn't help in other use cases:
 *  - when doing "partial DrawElements" i.e. consuming only part of the element
 *    array buffer
 *  - when doing frequent "partial buffer updates" i.e. bufferSubData calls
 *    updating parts of the element array buffer
 *
 * *** The solution: A binary tree ***
 *
 * The solution implemented here is to use a binary tree as the cache data
 * structure. Each tree node contains the max of its two children nodes. In this
 * way, finding the maximum in any contiguous sub-array has log complexity
 * instead of linear complexity.
 *
 * Simplistically, if the element array is:
 *
 *    [1   4   3   2]
 *
 * then the corresponding tree is:
 *
 *           4
 *         _/ \_
 *       4       3
 *      / \     / \
 *     1   4   3   2
 *
 * In practice, the bottom-most levels of the tree are both the largest to store
 * (because they have more nodes), and the least useful performance-wise
 * (because each node in the bottom levels concerns only few entries in the
 * elements array buffer, it is cheap to compute).
 *
 * For this reason, we stop the tree a few levels above, so that each tree leaf
 * actually corresponds to more than one element array entry.
 *
 * The number of levels that we "drop" is |kSkippedBottomTreeLevels| and the
 * number of element array entries that each leaf corresponds to, is
 * |kElementsPerLeaf|. This being a binary tree, we have:
 *
 *   kElementsPerLeaf = 2 ^ kSkippedBottomTreeLevels.
 *
 * *** Storage layout of the binary tree ***
 *
 * We take advantage of the specifics of the situation to avoid generalist tree
 * storage and instead store the tree entries in a vector, mTreeData.
 *
 * TreeData is always a vector of length:
 *
 *    2 * (number of leaves).
 *
 * Its data layout is as follows: mTreeData[0] is unused, mTreeData[1] is the
 * root node, then at offsets 2..3 is the tree level immediately below the root
 * node, then at offsets 4..7 is the tree level below that, etc.
 *
 * The figure below illustrates this by writing at each tree node the offset
 * into mTreeData at which it is stored:
 *
 *           1
 *         _/ \_
 *       2       3
 *      / \     / \
 *     4   5   6   7
 *    ...
 *
 * Thus, under the convention that the root level is level 0, we see that level
 * N is stored at offsets:
 *
 *    [ 2^n .. 2^(n+1) - 1 ]
 *
 * in mTreeData. Likewise, all the usual tree operations have simple
 * mathematical expressions in terms of mTreeData offsets, see all the methods
 * such as ParentNode, LeftChildNode, etc.
 *
 * *** Design constraint: Element types aren't known at buffer-update time ***
 *
 * Note that a key constraint that we're operating under, is that we don't know
 * the types of the elements by the time WebGL bufferData/bufferSubData methods
 * are called. The type of elements is only specified in the drawElements call.
 * This means that we may potentially have to store caches for multiple element
 * types, for the same element array buffer. Since we don't know yet how many
 * element types we'll eventually support (extensions add more), the concern
 * about memory usage is serious. This is addressed by kSkippedBottomTreeLevels
 * as explained above. Of course, in the typical case where each element array
 * buffer is only ever used with one type, this is also addressed by having
 * WebGLElementArrayCache lazily create trees for each type only upon first use.
 *
 * Another consequence of this constraint is that when updating the trees, we
 * have to update all existing trees. So if trees for types uint8_t, uint16_t
 * and uint32_t have ever been constructed for this buffer, every subsequent
 * update will have to update all trees even if one of the types is never used
 * again. That's inefficient, but content should not put indices of different
 * types in the same element array buffer anyways. Different index types can
 * only be consumed in separate drawElements calls, so nothing particular is
 * to be achieved by lumping them in the same buffer object.
 */
template<typename T>
struct WebGLElementArrayCacheTree
{
    /* A too-high kSkippedBottomTreeLevels would harm the performance of small
     * drawElements calls. A too-low kSkippedBottomTreeLevels would cause undue
     * memory usage. The current value has been validated by some benchmarking.
     * See bug 732660.
     */
    static const size_t kSkippedBottomTreeLevels = 3;
    static const size_t kElementsPerLeaf = 1 << kSkippedBottomTreeLevels;
    // Since kElementsPerLeaf is POT:
    static const size_t kElementsPerLeafMask = kElementsPerLeaf - 1;

private:
    // The WebGLElementArrayCache that owns this tree:
    WebGLElementArrayCache& mParent;

    // The tree's internal data storage. Its length is 2 * (number of leaves)
    // because of its data layout explained in the above class comment.
    FallibleTArray<T> mTreeData;

public:
    // Constructor. Takes a reference to the WebGLElementArrayCache that is to be
    // the parent. Does not initialize the tree. Should be followed by a call
    // to Update() to attempt initializing the tree.
    explicit WebGLElementArrayCacheTree(WebGLElementArrayCache& value)
        : mParent(value)
    {
    }

    T GlobalMaximum() const {
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

    size_t NumLeaves() const {
        // See class comment for why we the tree storage size is 2 * numLeaves.
        return mTreeData.Length() >> 1;
    }

    size_t LeafForElement(size_t element) const {
        size_t leaf = element / kElementsPerLeaf;
        MOZ_ASSERT(leaf < NumLeaves());
        return leaf;
    }

    size_t LeafForByte(size_t byte) const {
        return LeafForElement(byte / sizeof(T));
    }

    // Returns the index, into the tree storage, where a given leaf is stored.
    size_t TreeIndexForLeaf(size_t leaf) const {
        // See above class comment. The tree storage is an array of length
        // 2 * numLeaves. The leaves are stored in its second half.
        return leaf + NumLeaves();
    }

    static size_t LastElementUnderSameLeaf(size_t element) {
        return element | kElementsPerLeafMask;
    }

    static size_t FirstElementUnderSameLeaf(size_t element) {
        return element & ~kElementsPerLeafMask;
    }

    static size_t NextMultipleOfElementsPerLeaf(size_t numElements) {
        MOZ_ASSERT(numElements >= 1);
        return ((numElements - 1) | kElementsPerLeafMask) + 1;
    }

    bool Validate(T maxAllowed, size_t firstLeaf, size_t lastLeaf,
                  uint32_t* const out_upperBound)
    {
        size_t firstTreeIndex = TreeIndexForLeaf(firstLeaf);
        size_t lastTreeIndex  = TreeIndexForLeaf(lastLeaf);

        while (true) {
            // Given that we tweak these values in nontrivial ways, it doesn't
            // hurt to do this sanity check.
            MOZ_ASSERT(firstTreeIndex <= lastTreeIndex);

            // Final case where there is only one node to validate at the
            // current tree level:
            if (lastTreeIndex == firstTreeIndex) {
                const T& curData = mTreeData[firstTreeIndex];
                UpdateUpperBound(out_upperBound, curData);
                return curData <= maxAllowed;
            }

            // If the first node at current tree level is a right node, handle
            // it individually and replace it with its right neighbor, which is
            // a left node.
            if (IsRightNode(firstTreeIndex)) {
                const T& curData = mTreeData[firstTreeIndex];
                UpdateUpperBound(out_upperBound, curData);
                if (curData > maxAllowed)
                  return false;

                firstTreeIndex = RightNeighborNode(firstTreeIndex);
            }

            // If the last node at current tree level is a left node, handle it
            // individually and replace it with its left neighbor, which is a
            // right node.
            if (IsLeftNode(lastTreeIndex)) {
                const T& curData = mTreeData[lastTreeIndex];
                UpdateUpperBound(out_upperBound, curData);
                if (curData > maxAllowed)
                    return false;

                lastTreeIndex = LeftNeighborNode(lastTreeIndex);
            }

            /* At this point it can happen that firstTreeIndex and lastTreeIndex
             * "crossed" eachother. That happens if firstTreeIndex was a right
             * node and lastTreeIndex was its right neighor: In that case, both
             * above tweaks happened and as a result, they ended up being
             * swapped: LastTreeIndex is now the _left_ neighbor of
             * firstTreeIndex. When that happens, there is nothing left to
             * validate.
             */
            if (lastTreeIndex == LeftNeighborNode(firstTreeIndex))
                return true;

            // Walk up one level.
            firstTreeIndex = ParentNode(firstTreeIndex);
            lastTreeIndex = ParentNode(lastTreeIndex);
        }
    }

    // Updates the tree from the parent's buffer contents. Fallible, as it
    // may have to resize the tree storage.
    bool Update(size_t firstByte, size_t lastByte);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
    {
        return mallocSizeOf(this) +
               mTreeData.ShallowSizeOfExcludingThis(mallocSizeOf);
    }
};

// TreeForType: just a template helper to select the right tree object for a given
// element type.
template<typename T>
struct TreeForType {};

template<>
struct TreeForType<uint8_t>
{
    static ScopedDeletePtr<WebGLElementArrayCacheTree<uint8_t>>&
    Value(WebGLElementArrayCache* b) {
        return b->mUint8Tree;
    }
};

template<>
struct TreeForType<uint16_t>
{
    static ScopedDeletePtr<WebGLElementArrayCacheTree<uint16_t>>&
    Value(WebGLElementArrayCache* b) {
        return b->mUint16Tree;
    }
};

template<>
struct TreeForType<uint32_t>
{
    static ScopedDeletePtr<WebGLElementArrayCacheTree<uint32_t>>&
    Value(WebGLElementArrayCache* b) {
        return b->mUint32Tree;
    }
};

// Calling this method will 1) update the leaves in this interval
// from the raw buffer data, and 2) propagate this update up the tree.
template<typename T>
bool
WebGLElementArrayCacheTree<T>::Update(size_t firstByte, size_t lastByte)
{
    MOZ_ASSERT(firstByte <= lastByte);
    MOZ_ASSERT(lastByte < mParent.mBytes.Length());

    size_t numberOfElements = mParent.mBytes.Length() / sizeof(T);
    size_t requiredNumLeaves = 0;
    if (numberOfElements > 0) {
        /* If we didn't require the number of leaves to be a power of two, then
         * it would just be equal to
         *
         *    ceil(numberOfElements / kElementsPerLeaf)
         *
         * The way we implement this (division+ceil) operation in integer
         * arithmetic
         * is as follows:
         */
        size_t numLeavesNonPOT = (numberOfElements + kElementsPerLeaf - 1) / kElementsPerLeaf;
        // It only remains to round that up to the next power of two:
        requiredNumLeaves = RoundUpPow2(numLeavesNonPOT);
    }

    // Step #0: If needed, resize our tree data storage.
    if (requiredNumLeaves != NumLeaves()) {
        // See class comment for why we the tree storage size is 2 * numLeaves.
        if (!mTreeData.SetLength(2 * requiredNumLeaves, fallible)) {
            mTreeData.Clear();
            return false;
        }
        MOZ_ASSERT(NumLeaves() == requiredNumLeaves);

        if (NumLeaves()) {
            // When resizing, update the whole tree, not just the subset
            // corresponding to the part of the buffer being updated.
            memset(mTreeData.Elements(), 0, mTreeData.Length() * sizeof(T));
            firstByte = 0;
            lastByte = mParent.mBytes.Length() - 1;
        }
    }

    if (NumLeaves() == 0)
        return true;

    lastByte = std::min(lastByte, NumLeaves() * kElementsPerLeaf * sizeof(T) - 1);
    if (firstByte > lastByte)
        return true;

    size_t firstLeaf = LeafForByte(firstByte);
    size_t lastLeaf = LeafForByte(lastByte);

    MOZ_ASSERT(firstLeaf <= lastLeaf && lastLeaf < NumLeaves());

    size_t firstTreeIndex = TreeIndexForLeaf(firstLeaf);
    size_t lastTreeIndex = TreeIndexForLeaf(lastLeaf);

    // Step #1: Initialize the tree leaves from plain buffer data.
    // That is, each tree leaf must be set to the max of the |kElementsPerLeaf|
    // corresponding buffer entries.

    // Condition-less scope to prevent leaking this scope's variables into the
    // code below:
    {
        // TreeIndex is the index of the tree leaf we're writing, i.e. the
        // destination index.
        size_t treeIndex = firstTreeIndex;
        // srcIndex is the index in the source buffer.
        size_t srcIndex = firstLeaf * kElementsPerLeaf;
        while (treeIndex <= lastTreeIndex) {
            T m = 0;
            size_t a = srcIndex;
            size_t srcIndexNextLeaf = std::min(a + kElementsPerLeaf, numberOfElements);
            for (; srcIndex < srcIndexNextLeaf; srcIndex++) {
                m = std::max(m, mParent.Element<T>(srcIndex));
            }
            mTreeData[treeIndex] = m;
            treeIndex++;
        }
    }

    // Step #2: Propagate the values up the tree. This is simply a matter of
    // walking up the tree and setting each node to the max of its two children.
    while (firstTreeIndex > 1) {
        // Move up one level.
        firstTreeIndex = ParentNode(firstTreeIndex);
        lastTreeIndex = ParentNode(lastTreeIndex);

        // Fast-exit case where only one node is updated at the current level.
        if (firstTreeIndex == lastTreeIndex) {
            mTreeData[firstTreeIndex] = std::max(mTreeData[LeftChildNode(firstTreeIndex)], mTreeData[RightChildNode(firstTreeIndex)]);
            continue;
        }

        size_t child = LeftChildNode(firstTreeIndex);
        size_t parent = firstTreeIndex;
        while (parent <= lastTreeIndex) {
            T a = mTreeData[child];
            child = RightNeighborNode(child);
            T b = mTreeData[child];
            child = RightNeighborNode(child);
            mTreeData[parent] = std::max(a, b);
            parent = RightNeighborNode(parent);
        }
    }

    return true;
}

WebGLElementArrayCache::WebGLElementArrayCache()
{
}

WebGLElementArrayCache::~WebGLElementArrayCache()
{
}

bool
WebGLElementArrayCache::BufferData(const void* ptr, size_t byteLength)
{
    if (mBytes.Length() != byteLength) {
        if (!mBytes.SetLength(byteLength, fallible)) {
            mBytes.Clear();
            return false;
        }
    }
    MOZ_ASSERT(mBytes.Length() == byteLength);
    return BufferSubData(0, ptr, byteLength);
}

bool
WebGLElementArrayCache::BufferSubData(size_t pos, const void* ptr,
                                      size_t updateByteLength)
{
    MOZ_ASSERT(pos + updateByteLength <= mBytes.Length());
    if (!updateByteLength)
        return true;

    // Note, using memcpy on shared racy data is not well-defined, this
    // will need to use safe-for-races operations when those become available.
    // See bug 1225033.
    if (ptr)
        memcpy(mBytes.Elements() + pos, ptr, updateByteLength);
    else
        memset(mBytes.Elements() + pos, 0, updateByteLength);
    return UpdateTrees(pos, pos + updateByteLength - 1);
}

bool
WebGLElementArrayCache::UpdateTrees(size_t firstByte, size_t lastByte)
{
    bool result = true;
    if (mUint8Tree)
        result &= mUint8Tree->Update(firstByte, lastByte);
    if (mUint16Tree)
        result &= mUint16Tree->Update(firstByte, lastByte);
    if (mUint32Tree)
        result &= mUint32Tree->Update(firstByte, lastByte);
    return result;
}

template<typename T>
bool
WebGLElementArrayCache::Validate(uint32_t maxAllowed, size_t firstElement,
                                 size_t countElements,
                                 uint32_t* const out_upperBound)
{
    *out_upperBound = 0;

    // If maxAllowed is >= the max T value, then there is no way that a T index
    // could be invalid.
    uint32_t maxTSize = std::numeric_limits<T>::max();
    if (maxAllowed >= maxTSize) {
        UpdateUpperBound(out_upperBound, maxTSize);
        return true;
    }

    T maxAllowedT(maxAllowed);

    // Integer overflow must have been handled earlier, so we assert that
    // maxAllowedT is exactly the max allowed value.
    MOZ_ASSERT(uint32_t(maxAllowedT) == maxAllowed);

    if (!mBytes.Length() || !countElements)
      return true;

    ScopedDeletePtr<WebGLElementArrayCacheTree<T>>& tree = TreeForType<T>::Value(this);
    if (!tree) {
        tree = new WebGLElementArrayCacheTree<T>(*this);
        if (mBytes.Length()) {
            bool valid = tree->Update(0, mBytes.Length() - 1);
            if (!valid) {
                // Do not assert here. This case would happen if an allocation
                // failed. We've already settled on fallible allocations around
                // here.
                tree = nullptr;
                return false;
            }
        }
    }

    size_t lastElement = firstElement + countElements - 1;

    // Fast-exit path when the global maximum for the whole element array buffer
    // falls in the allowed range:
    T globalMax = tree->GlobalMaximum();
    if (globalMax <= maxAllowedT) {
        UpdateUpperBound(out_upperBound, globalMax);
        return true;
    }

    const T* elements = Elements<T>();

    // Before calling tree->Validate, we have to validate ourselves the
    // boundaries of the elements span, to round them to the nearest multiple of
    // kElementsPerLeaf.
    size_t firstElementAdjustmentEnd = std::min(lastElement,
                                                tree->LastElementUnderSameLeaf(firstElement));
    while (firstElement <= firstElementAdjustmentEnd) {
        const T& curData = elements[firstElement];
        UpdateUpperBound(out_upperBound, curData);
        if (curData > maxAllowedT)
            return false;

        firstElement++;
    }
    size_t lastElementAdjustmentEnd = std::max(firstElement,
                                               tree->FirstElementUnderSameLeaf(lastElement));
    while (lastElement >= lastElementAdjustmentEnd) {
        const T& curData = elements[lastElement];
        UpdateUpperBound(out_upperBound, curData);
        if (curData > maxAllowedT)
            return false;

        lastElement--;
    }

    // at this point, for many tiny validations, we're already done.
    if (firstElement > lastElement)
        return true;

    // general case
    return tree->Validate(maxAllowedT, tree->LeafForElement(firstElement),
                          tree->LeafForElement(lastElement), out_upperBound);
}

bool
WebGLElementArrayCache::Validate(GLenum type, uint32_t maxAllowed,
                                 size_t firstElement, size_t countElements,
                                 uint32_t* const out_upperBound)
{
    if (type == LOCAL_GL_UNSIGNED_BYTE)
        return Validate<uint8_t>(maxAllowed, firstElement, countElements,
                                 out_upperBound);
    if (type == LOCAL_GL_UNSIGNED_SHORT)
        return Validate<uint16_t>(maxAllowed, firstElement, countElements,
                                  out_upperBound);
    if (type == LOCAL_GL_UNSIGNED_INT)
        return Validate<uint32_t>(maxAllowed, firstElement, countElements,
                                  out_upperBound);

    MOZ_ASSERT(false, "Invalid type.");
    return false;
}

template<typename T>
static size_t
SizeOfNullable(mozilla::MallocSizeOf mallocSizeOf, const T& obj)
{
    if (!obj)
        return 0;
    return obj->SizeOfIncludingThis(mallocSizeOf);
}

size_t
WebGLElementArrayCache::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(this) +
           mBytes.ShallowSizeOfExcludingThis(mallocSizeOf) +
           SizeOfNullable(mallocSizeOf, mUint8Tree) +
           SizeOfNullable(mallocSizeOf, mUint16Tree) +
           SizeOfNullable(mallocSizeOf, mUint32Tree);
}

bool
WebGLElementArrayCache::BeenUsedWithMultipleTypes() const
{
  // C++ Standard ($4.7)
  // "If the source type is bool, the value false is converted to zero and
  //  the value true is converted to one."
  const int num_types_used = (mUint8Tree  != nullptr) +
                             (mUint16Tree != nullptr) +
                             (mUint32Tree != nullptr);
  return num_types_used > 1;
}

} // end namespace mozilla

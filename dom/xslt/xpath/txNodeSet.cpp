/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txNodeSet.h"
#include "txLog.h"
#include "nsMemory.h"
#include "txXPathTreeWalker.h"
#include <algorithm>

/**
 * Implementation of an XPath nodeset
 */

#ifdef NS_BUILD_REFCNT_LOGGING
#define LOG_CHUNK_MOVE(_start, _new_start, _count)            \
{                                                             \
    txXPathNode *start = const_cast<txXPathNode*>(_start);    \
    while (start < _start + _count) {                         \
        NS_LogDtor(start, "txXPathNode", sizeof(*start));     \
        ++start;                                              \
    }                                                         \
    start = const_cast<txXPathNode*>(_new_start);             \
    while (start < _new_start + _count) {                     \
        NS_LogCtor(start, "txXPathNode", sizeof(*start));     \
        ++start;                                              \
    }                                                         \
}
#else
#define LOG_CHUNK_MOVE(_start, _new_start, _count)
#endif

static const int32_t kTxNodeSetMinSize = 4;
static const int32_t kTxNodeSetGrowFactor = 2;

#define kForward   1
#define kReversed -1

txNodeSet::txNodeSet(txResultRecycler* aRecycler)
    : txAExprResult(aRecycler),
      mStart(nullptr),
      mEnd(nullptr),
      mStartBuffer(nullptr),
      mEndBuffer(nullptr),
      mDirection(kForward),
      mMarks(nullptr)
{
}

txNodeSet::txNodeSet(const txXPathNode& aNode, txResultRecycler* aRecycler)
    : txAExprResult(aRecycler),
      mStart(nullptr),
      mEnd(nullptr),
      mStartBuffer(nullptr),
      mEndBuffer(nullptr),
      mDirection(kForward),
      mMarks(nullptr)
{
    if (!ensureGrowSize(1)) {
        return;
    }

    new(mStart) txXPathNode(aNode);
    ++mEnd;
}

txNodeSet::txNodeSet(const txNodeSet& aSource, txResultRecycler* aRecycler)
    : txAExprResult(aRecycler),
      mStart(nullptr),
      mEnd(nullptr),
      mStartBuffer(nullptr),
      mEndBuffer(nullptr),
      mDirection(kForward),
      mMarks(nullptr)
{
    append(aSource);
}

txNodeSet::~txNodeSet()
{
    delete [] mMarks;

    if (mStartBuffer) {
        destroyElements(mStart, mEnd);

        free(mStartBuffer);
    }
}

nsresult txNodeSet::add(const txXPathNode& aNode)
{
    NS_ASSERTION(mDirection == kForward,
                 "only append(aNode) is supported on reversed nodesets");

    if (isEmpty()) {
        return append(aNode);
    }

    bool dupe;
    txXPathNode* pos = findPosition(aNode, mStart, mEnd, dupe);

    if (dupe) {
        return NS_OK;
    }

    // save pos, ensureGrowSize messes with the pointers
    int32_t moveSize = mEnd - pos;
    int32_t offset = pos - mStart;
    if (!ensureGrowSize(1)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    // set pos to where it was
    pos = mStart + offset;

    if (moveSize > 0) {
        LOG_CHUNK_MOVE(pos, pos + 1, moveSize);
        memmove(pos + 1, pos, moveSize * sizeof(txXPathNode));
    }

    new(pos) txXPathNode(aNode);
    ++mEnd;

    return NS_OK;
}

nsresult txNodeSet::add(const txNodeSet& aNodes)
{
    return add(aNodes, copyElements, nullptr);
}

nsresult txNodeSet::addAndTransfer(txNodeSet* aNodes)
{
    // failure is out-of-memory, transfer didn't happen
    nsresult rv = add(*aNodes, transferElements, destroyElements);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef TX_DONT_RECYCLE_BUFFER
    if (aNodes->mStartBuffer) {
        free(aNodes->mStartBuffer);
        aNodes->mStartBuffer = aNodes->mEndBuffer = nullptr;
    }
#endif
    aNodes->mStart = aNodes->mEnd = aNodes->mStartBuffer;

    return NS_OK;
}

/**
 * add(aNodeSet, aTransferOp)
 *
 * The code is optimized to make a minimum number of calls to
 * Node::compareDocumentPosition. The idea is this:
 * We have the two nodesets (number indicate "document position")
 *
 * 1 3 7             <- source 1
 * 2 3 6 8 9         <- source 2
 * _ _ _ _ _ _ _ _   <- result
 *
 *
 * When merging these nodesets into the result, the nodes are transfered
 * in chunks to the end of the buffer so that each chunk does not contain
 * a node from the other nodeset, in document order.
 *
 * We select the last non-transfered node in the first nodeset and find
 * where in the other nodeset it would be inserted. In this case we would
 * take the 7 from the first nodeset and find the position between the
 * 6 and 8 in the second. We then take the nodes after the insert-position
 * and transfer them to the end of the resulting nodeset. Which in this case
 * means that we first transfered the 8 and 9 nodes, giving us the following:
 *
 * 1 3 7             <- source 1
 * 2 3 6             <- source 2
 * _ _ _ _ _ _ 8 9   <- result
 *
 * The corresponding procedure is done for the second nodeset, that is
 * the insertion position of the 6 in the first nodeset is found, which
 * is between the 3 and the 7. The 7 is memmoved (as it stays within
 * the same nodeset) to the result buffer.
 *
 * As the result buffer is filled from the end, it is safe to share the
 * buffer between this nodeset and the result.
 *
 * This is repeated until both of the nodesets are empty.
 *
 * If we find a duplicate node when searching for where insertposition we
 * check for sequences of duplicate nodes, which can be optimized.
 *
 */
nsresult txNodeSet::add(const txNodeSet& aNodes, transferOp aTransfer,
                        destroyOp aDestroy)
{
    NS_ASSERTION(mDirection == kForward,
                 "only append(aNode) is supported on reversed nodesets");

    if (aNodes.isEmpty()) {
        return NS_OK;
    }

    if (!ensureGrowSize(aNodes.size())) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // This is probably a rather common case, so lets try to shortcut.
    if (mStart == mEnd ||
        txXPathNodeUtils::comparePosition(mEnd[-1], *aNodes.mStart) < 0) {
        aTransfer(mEnd, aNodes.mStart, aNodes.mEnd);
        mEnd += aNodes.size();

        return NS_OK;
    }

    // Last element in this nodeset
    txXPathNode* thisPos = mEnd;

    // Last element of the other nodeset
    txXPathNode* otherPos = aNodes.mEnd;

    // Pointer to the insertion point in this nodeset
    txXPathNode* insertPos = mEndBuffer;

    bool dupe;
    txXPathNode* pos;
    int32_t count;
    while (thisPos > mStart || otherPos > aNodes.mStart) {
        // Find where the last remaining node of this nodeset would
        // be inserted in the other nodeset.
        if (thisPos > mStart) {
            pos = findPosition(thisPos[-1], aNodes.mStart, otherPos, dupe);

            if (dupe) {
                const txXPathNode *deletePos = thisPos;
                --thisPos; // this is already added
                // check dupe sequence
                while (thisPos > mStart && pos > aNodes.mStart &&
                       thisPos[-1] == pos[-1]) {
                    --thisPos;
                    --pos;
                }

                if (aDestroy) {
                    aDestroy(thisPos, deletePos);
                }
            }
        }
        else {
            pos = aNodes.mStart;
        }

        // Transfer the otherNodes after the insertion point to the result
        count = otherPos - pos;
        if (count > 0) {
            insertPos -= count;
            aTransfer(insertPos, pos, otherPos);
            otherPos -= count;
        }

        // Find where the last remaining node of the otherNodeset would
        // be inserted in this nodeset.
        if (otherPos > aNodes.mStart) {
            pos = findPosition(otherPos[-1], mStart, thisPos, dupe);

            if (dupe) {
                const txXPathNode *deletePos = otherPos;
                --otherPos; // this is already added
                // check dupe sequence
                while (otherPos > aNodes.mStart && pos > mStart &&
                       otherPos[-1] == pos[-1]) {
                    --otherPos;
                    --pos;
                }

                if (aDestroy) {
                    aDestroy(otherPos, deletePos);
                }
            }
        }
        else {
            pos = mStart;
        }

        // Move the nodes from this nodeset after the insertion point
        // to the result
        count = thisPos - pos;
        if (count > 0) {
            insertPos -= count;
            LOG_CHUNK_MOVE(pos, insertPos, count);
            memmove(insertPos, pos, count * sizeof(txXPathNode));
            thisPos -= count;
        }
    }
    mStart = insertPos;
    mEnd = mEndBuffer;

    return NS_OK;
}

/**
 * Append API
 * These functions should be used with care.
 * They are intended to be used when the caller assures that the resulting
 * nodeset remains in document order.
 * Abuse will break document order, and cause errors in the result.
 * These functions are significantly faster than the add API, as no
 * order info operations will be performed.
 */

nsresult
txNodeSet::append(const txXPathNode& aNode)
{
    if (!ensureGrowSize(1)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mDirection == kForward) {
        new(mEnd) txXPathNode(aNode);
        ++mEnd;

        return NS_OK;
    }

    new(--mStart) txXPathNode(aNode);

    return NS_OK;
}

nsresult
txNodeSet::append(const txNodeSet& aNodes)
{
    NS_ASSERTION(mDirection == kForward,
                 "only append(aNode) is supported on reversed nodesets");

    if (aNodes.isEmpty()) {
        return NS_OK;
    }

    int32_t appended = aNodes.size();
    if (!ensureGrowSize(appended)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    copyElements(mEnd, aNodes.mStart, aNodes.mEnd);
    mEnd += appended;

    return NS_OK;
}

nsresult
txNodeSet::mark(int32_t aIndex)
{
    NS_ASSERTION(aIndex >= 0 && mStart && mEnd - mStart > aIndex,
                 "index out of bounds");
    if (!mMarks) {
        int32_t length = size();
        mMarks = new bool[length];
        memset(mMarks, 0, length * sizeof(bool));
    }
    if (mDirection == kForward) {
        mMarks[aIndex] = true;
    }
    else {
        mMarks[size() - aIndex - 1] = true;
    }

    return NS_OK;
}

nsresult
txNodeSet::sweep()
{
    if (!mMarks) {
        // sweep everything
        clear();
    }

    int32_t chunk, pos = 0;
    int32_t length = size();
    txXPathNode* insertion = mStartBuffer;

    while (pos < length) {
        while (pos < length && !mMarks[pos]) {
            // delete unmarked
            mStart[pos].~txXPathNode();
            ++pos;
        }
        // find chunk to move
        chunk = 0;
        while (pos < length && mMarks[pos]) {
            ++pos;
            ++chunk;
        }
        // move chunk
        if (chunk > 0) {
            LOG_CHUNK_MOVE(mStart + pos - chunk, insertion, chunk);
            memmove(insertion, mStart + pos - chunk,
                    chunk * sizeof(txXPathNode));
            insertion += chunk;
        }
    }
    mStart = mStartBuffer;
    mEnd = insertion;
    delete [] mMarks;
    mMarks = nullptr;

    return NS_OK;
}

void
txNodeSet::clear()
{
    destroyElements(mStart, mEnd);
#ifdef TX_DONT_RECYCLE_BUFFER
    if (mStartBuffer) {
        free(mStartBuffer);
        mStartBuffer = mEndBuffer = nullptr;
    }
#endif
    mStart = mEnd = mStartBuffer;
    delete [] mMarks;
    mMarks = nullptr;
    mDirection = kForward;
}

int32_t
txNodeSet::indexOf(const txXPathNode& aNode, uint32_t aStart) const
{
    NS_ASSERTION(mDirection == kForward,
                 "only append(aNode) is supported on reversed nodesets");

    if (!mStart || mStart == mEnd) {
        return -1;
    }

    txXPathNode* pos = mStart + aStart;
    for (; pos < mEnd; ++pos) {
        if (*pos == aNode) {
            return pos - mStart;
        }
    }

    return -1;
}

const txXPathNode&
txNodeSet::get(int32_t aIndex) const
{
    if (mDirection == kForward) {
        return mStart[aIndex];
    }

    return mEnd[-aIndex - 1];
}

short
txNodeSet::getResultType()
{
    return txAExprResult::NODESET;
}

bool
txNodeSet::booleanValue()
{
    return !isEmpty();
}
double
txNodeSet::numberValue()
{
    nsAutoString str;
    stringValue(str);

    return txDouble::toDouble(str);
}

void
txNodeSet::stringValue(nsString& aStr)
{
    NS_ASSERTION(mDirection == kForward,
                 "only append(aNode) is supported on reversed nodesets");
    if (isEmpty()) {
        return;
    }
    txXPathNodeUtils::appendNodeValue(get(0), aStr);
}

const nsString*
txNodeSet::stringValuePointer()
{
    return nullptr;
}

bool txNodeSet::ensureGrowSize(int32_t aSize)
{
    // check if there is enough place in the buffer as is
    if (mDirection == kForward && aSize <= mEndBuffer - mEnd) {
        return true;
    }

    if (mDirection == kReversed && aSize <= mStart - mStartBuffer) {
        return true;
    }

    // check if we just have to align mStart to have enough space
    int32_t oldSize = mEnd - mStart;
    int32_t oldLength = mEndBuffer - mStartBuffer;
    int32_t ensureSize = oldSize + aSize;
    if (ensureSize <= oldLength) {
        // just move the buffer
        txXPathNode* dest = mStartBuffer;
        if (mDirection == kReversed) {
            dest = mEndBuffer - oldSize;
        }
        LOG_CHUNK_MOVE(mStart, dest, oldSize);
        memmove(dest, mStart, oldSize * sizeof(txXPathNode));
        mStart = dest;
        mEnd = dest + oldSize;

        return true;
    }

    // This isn't 100% safe. But until someone manages to make a 1gig nodeset
    // it should be ok.
    int32_t newLength = std::max(oldLength, kTxNodeSetMinSize);

    while (newLength < ensureSize) {
        newLength *= kTxNodeSetGrowFactor;
    }

    txXPathNode* newArr = static_cast<txXPathNode*>
                                     (moz_xmalloc(newLength *
                                                         sizeof(txXPathNode)));
    if (!newArr) {
        return false;
    }

    txXPathNode* dest = newArr;
    if (mDirection == kReversed) {
        dest += newLength - oldSize;
    }

    if (oldSize > 0) {
        LOG_CHUNK_MOVE(mStart, dest, oldSize);
        memcpy(dest, mStart, oldSize * sizeof(txXPathNode));
    }

    if (mStartBuffer) {
#ifdef DEBUG
        memset(mStartBuffer, 0,
               (mEndBuffer - mStartBuffer) * sizeof(txXPathNode));
#endif
        free(mStartBuffer);
    }

    mStartBuffer = newArr;
    mEndBuffer = mStartBuffer + newLength;
    mStart = dest;
    mEnd = dest + oldSize;

    return true;
}

txXPathNode*
txNodeSet::findPosition(const txXPathNode& aNode, txXPathNode* aFirst,
                        txXPathNode* aLast, bool& aDupe) const
{
    aDupe = false;
    if (aLast - aFirst <= 2) {
        // If we search 2 nodes or less there is no point in further divides
        txXPathNode* pos = aFirst;
        for (; pos < aLast; ++pos) {
            int cmp = txXPathNodeUtils::comparePosition(aNode, *pos);
            if (cmp < 0) {
                return pos;
            }

            if (cmp == 0) {
                aDupe = true;

                return pos;
            }
        }
        return pos;
    }

    // (cannot add two pointers)
    txXPathNode* midpos = aFirst + (aLast - aFirst) / 2;
    int cmp = txXPathNodeUtils::comparePosition(aNode, *midpos);
    if (cmp == 0) {
        aDupe = true;

        return midpos;
    }

    if (cmp > 0) {
        return findPosition(aNode, midpos + 1, aLast, aDupe);
    }

    // midpos excluded as end of range

    return findPosition(aNode, aFirst, midpos, aDupe);
}

/* static */
void
txNodeSet::copyElements(txXPathNode* aDest,
                        const txXPathNode* aStart, const txXPathNode* aEnd)
{
    const txXPathNode* pos = aStart;
    while (pos < aEnd) {
        new(aDest) txXPathNode(*pos);
        ++aDest;
        ++pos;
    }
}

/* static */
void
txNodeSet::transferElements(txXPathNode* aDest,
                            const txXPathNode* aStart, const txXPathNode* aEnd)
{
    LOG_CHUNK_MOVE(aStart, aDest, (aEnd - aStart));
    memcpy(aDest, aStart, (aEnd - aStart) * sizeof(txXPathNode));
}

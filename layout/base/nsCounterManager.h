/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ai:sw=4:ts=4:et:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of CSS counters (for numbering things) */

#ifndef nsCounterManager_h_
#define nsCounterManager_h_

#include "mozilla/Attributes.h"
#include "nsGenConList.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "mozilla/Likely.h"
#include "CounterStyleManager.h"

class nsCounterList;
struct nsCounterUseNode;
struct nsCounterChangeNode;

struct nsCounterNode : public nsGenConNode {
    enum Type {
        RESET,     // a "counter number" pair in 'counter-reset'
        INCREMENT, // a "counter number" pair in 'counter-increment'
        USE        // counter() or counters() in 'content'
    };

    Type mType;

    // Counter value after this node
    int32_t mValueAfter;

    // mScopeStart points to the node (usually a RESET, but not in the
    // case of an implied 'counter-reset') that created the scope for
    // this element (for a RESET, its outer scope, i.e., the one it is
    // inside rather than the one it creates).

    // May be null for all types, but only when mScopePrev is also null.
    // Being null for a non-RESET means that it is an implied
    // 'counter-reset'.  Being null for a RESET means it has no outer
    // scope.
    nsCounterNode *mScopeStart;

    // mScopePrev points to the previous node that is in the same scope,
    // or for a RESET, the previous node in the scope outside of the
    // reset.

    // May be null for all types, but only when mScopeStart is also
    // null.  Following the mScopePrev links will eventually lead to
    // mScopeStart.  Being null for a non-RESET means that it is an
    // implied 'counter-reset'.  Being null for a RESET means it has no
    // outer scope.
    nsCounterNode *mScopePrev;

    inline nsCounterUseNode* UseNode();
    inline nsCounterChangeNode* ChangeNode();

    // For RESET and INCREMENT nodes, aPseudoFrame need not be a
    // pseudo-element, and aContentIndex represents the index within the
    // 'counter-reset' or 'counter-increment' property instead of within
    // the 'content' property but offset to ensure that (reset,
    // increment, use) sort in that order.  (This slight weirdness
    // allows sharing a lot of code with 'quotes'.)
    nsCounterNode(int32_t aContentIndex, Type aType)
        : nsGenConNode(aContentIndex)
        , mType(aType)
        , mValueAfter(0)
        , mScopeStart(nullptr)
        , mScopePrev(nullptr)
    {
    }

    // to avoid virtual function calls in the common case
    inline void Calc(nsCounterList* aList);
};

struct nsCounterUseNode : public nsCounterNode {
    // The same structure passed through the style system:  an array
    // containing the values in the counter() or counters() in the order
    // given in the CSS spec.
    nsRefPtr<nsCSSValue::Array> mCounterFunction;

    nsPresContext* mPresContext;
    nsRefPtr<mozilla::CounterStyle> mCounterStyle;

    // false for counter(), true for counters()
    bool mAllCounters;

    // args go directly to member variables here and of nsGenConNode
    nsCounterUseNode(nsPresContext* aPresContext,
                     nsCSSValue::Array* aCounterFunction,
                     uint32_t aContentIndex, bool aAllCounters)
        : nsCounterNode(aContentIndex, USE)
        , mCounterFunction(aCounterFunction)
        , mPresContext(aPresContext)
        , mCounterStyle(nullptr)
        , mAllCounters(aAllCounters)
    {
        NS_ASSERTION(aContentIndex <= INT32_MAX, "out of range");
    }

    virtual bool InitTextFrame(nsGenConList* aList,
            nsIFrame* aPseudoFrame, nsIFrame* aTextFrame) MOZ_OVERRIDE;

    mozilla::CounterStyle* GetCounterStyle();
    void SetCounterStyleDirty()
    {
        mCounterStyle = nullptr;
    }

    // assign the correct |mValueAfter| value to a node that has been inserted
    // Should be called immediately after calling |Insert|.
    void Calc(nsCounterList* aList);

    // The text that should be displayed for this counter.
    void GetText(nsString& aResult);
};

struct nsCounterChangeNode : public nsCounterNode {
    int32_t mChangeValue; // the numeric value of the increment or reset

    // |aPseudoFrame| is not necessarily a pseudo-element's frame, but
    // since it is for every other subclass of nsGenConNode, we follow
    // the naming convention here.
    // |aPropIndex| is the index of the value within the list in the
    // 'counter-increment' or 'counter-reset' property.
    nsCounterChangeNode(nsIFrame* aPseudoFrame,
                        nsCounterNode::Type aChangeType,
                        int32_t aChangeValue,
                        int32_t aPropIndex)
        : nsCounterNode(// Fake a content index for resets and increments
                        // that comes before all the real content, with
                        // the resets first, in order, and then the increments.
                        aPropIndex + (aChangeType == RESET
                                        ? (INT32_MIN) 
                                        : (INT32_MIN / 2)),
                        aChangeType)
        , mChangeValue(aChangeValue)
    {
        NS_ASSERTION(aPropIndex >= 0, "out of range");
        NS_ASSERTION(aChangeType == INCREMENT || aChangeType == RESET,
                     "bad type");
        mPseudoFrame = aPseudoFrame;
        CheckFrameAssertions();
    }

    // assign the correct |mValueAfter| value to a node that has been inserted
    // Should be called immediately after calling |Insert|.
    void Calc(nsCounterList* aList);
};

inline nsCounterUseNode* nsCounterNode::UseNode()
{
    NS_ASSERTION(mType == USE, "wrong type");
    return static_cast<nsCounterUseNode*>(this);
}

inline nsCounterChangeNode* nsCounterNode::ChangeNode()
{
    NS_ASSERTION(mType == INCREMENT || mType == RESET, "wrong type");
    return static_cast<nsCounterChangeNode*>(this);
}

inline void nsCounterNode::Calc(nsCounterList* aList)
{
    if (mType == USE)
        UseNode()->Calc(aList);
    else
        ChangeNode()->Calc(aList);
}

class nsCounterList : public nsGenConList {
public:
    nsCounterList() : nsGenConList(),
                      mDirty(false)
    {}

    void Insert(nsCounterNode* aNode) {
        nsGenConList::Insert(aNode);
        // Don't SetScope if we're dirty -- we'll reset all the scopes anyway,
        // and we can't usefully compute scopes right now.
        if (MOZ_LIKELY(!IsDirty())) {
            SetScope(aNode);
        }
    }

    nsCounterNode* First() {
        return static_cast<nsCounterNode*>(mFirstNode);
    }

    static nsCounterNode* Next(nsCounterNode* aNode) {
        return static_cast<nsCounterNode*>(nsGenConList::Next(aNode));
    }
    static nsCounterNode* Prev(nsCounterNode* aNode) {
        return static_cast<nsCounterNode*>(nsGenConList::Prev(aNode));
    }

    static int32_t ValueBefore(nsCounterNode* aNode) {
        return aNode->mScopePrev ? aNode->mScopePrev->mValueAfter : 0;
    }

    // Correctly set |aNode->mScopeStart| and |aNode->mScopePrev|
    void SetScope(nsCounterNode *aNode);

    // Recalculate |mScopeStart|, |mScopePrev|, and |mValueAfter| for
    // all nodes and update text in text content nodes.
    void RecalcAll();

    bool IsDirty() { return mDirty; }
    void SetDirty() { mDirty = true; }

private:
    bool mDirty;
};

/**
 * The counter manager maintains an |nsCounterList| for each named
 * counter to keep track of all scopes with that name.
 */
class nsCounterManager {
public:
    nsCounterManager();
    // Returns true if dirty
    bool AddCounterResetsAndIncrements(nsIFrame *aFrame);

    // Gets the appropriate counter list, creating it if necessary.
    // Returns null only on out-of-memory.
    nsCounterList* CounterListFor(const nsSubstring& aCounterName);

    // Clean up data in any dirty counter lists.
    void RecalcAll();

    // Set all counter styles dirty
    void SetAllCounterStylesDirty();

    // Destroy nodes for the frame in any lists, and return whether any
    // nodes were destroyed.
    bool DestroyNodesFor(nsIFrame *aFrame);

    // Clear all data.
    void Clear() { mNames.Clear(); }

#ifdef DEBUG
    void Dump();
#endif

    static int32_t IncrementCounter(int32_t aOldValue, int32_t aIncrement)
    {
        // Addition of unsigned values is defined to be arithmetic
        // modulo 2^bits (C++ 2011, 3.9.1 [basic.fundamental], clause 4);
        // addition of signed values is undefined (and clang does
        // something very strange if we use it here).  Likewise integral
        // conversion from signed to unsigned is also defined as modulo
        // 2^bits (C++ 2011, 4.7 [conv.integral], clause 2); conversion
        // from unsigned to signed is however undefined (ibid., clause 3),
        // but to do what we want we must nonetheless depend on that
        // small piece of undefined behavior.
        int32_t newValue = int32_t(uint32_t(aOldValue) + uint32_t(aIncrement));
        // The CSS Working Group resolved that a counter-increment that
        // exceeds internal limits should not increment at all.
        // http://lists.w3.org/Archives/Public/www-style/2013Feb/0392.html
        // (This means, for example, that if aIncrement is 5, the
        // counter will get stuck at the largest multiple of 5 less than
        // the maximum 32-bit integer.)
        if ((aIncrement > 0) != (newValue > aOldValue)) {
          newValue = aOldValue;
        }
        return newValue;
    }

private:
    // for |AddCounterResetsAndIncrements| only
    bool AddResetOrIncrement(nsIFrame *aFrame, int32_t aIndex,
                               const nsStyleCounterData *aCounterData,
                               nsCounterNode::Type aType);

    nsClassHashtable<nsStringHashKey, nsCounterList> mNames;
};

#endif /* nsCounterManager_h_ */

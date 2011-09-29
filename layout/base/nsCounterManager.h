/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ai:sw=4:ts=4:et:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsCounterManager.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* implementation of CSS counters (for numbering things) */

#ifndef nsCounterManager_h_
#define nsCounterManager_h_

#include "nsGenConList.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"

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
    PRInt32 mValueAfter;

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
    nsCounterNode(PRInt32 aContentIndex, Type aType)
        : nsGenConNode(aContentIndex)
        , mType(aType)
        , mValueAfter(0)
        , mScopeStart(nsnull)
        , mScopePrev(nsnull)
    {
    }

    // to avoid virtual function calls in the common case
    inline void Calc(nsCounterList* aList);
};

struct nsCounterUseNode : public nsCounterNode {
    // The same structure passed through the style system:  an array
    // containing the values in the counter() or counters() in the order
    // given in the CSS spec.
    nsRefPtr<nsCSSValue::Array> mCounterStyle;

    // false for counter(), true for counters()
    bool mAllCounters;

    // args go directly to member variables here and of nsGenConNode
    nsCounterUseNode(nsCSSValue::Array* aCounterStyle,
                     PRUint32 aContentIndex, bool aAllCounters)
        : nsCounterNode(aContentIndex, USE)
        , mCounterStyle(aCounterStyle)
        , mAllCounters(aAllCounters)
    {
        NS_ASSERTION(aContentIndex <= PR_INT32_MAX, "out of range");
    }
    
    virtual bool InitTextFrame(nsGenConList* aList,
            nsIFrame* aPseudoFrame, nsIFrame* aTextFrame);

    // assign the correct |mValueAfter| value to a node that has been inserted
    // Should be called immediately after calling |Insert|.
    void Calc(nsCounterList* aList);

    // The text that should be displayed for this counter.
    void GetText(nsString& aResult);
};

struct nsCounterChangeNode : public nsCounterNode {
    PRInt32 mChangeValue; // the numeric value of the increment or reset

    // |aPseudoFrame| is not necessarily a pseudo-element's frame, but
    // since it is for every other subclass of nsGenConNode, we follow
    // the naming convention here.
    // |aPropIndex| is the index of the value within the list in the
    // 'counter-increment' or 'counter-reset' property.
    nsCounterChangeNode(nsIFrame* aPseudoFrame,
                        nsCounterNode::Type aChangeType,
                        PRInt32 aChangeValue,
                        PRInt32 aPropIndex)
        : nsCounterNode(// Fake a content index for resets and increments
                        // that comes before all the real content, with
                        // the resets first, in order, and then the increments.
                        aPropIndex + (aChangeType == RESET
                                        ? (PR_INT32_MIN) 
                                        : (PR_INT32_MIN / 2)),
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
                      mDirty(PR_FALSE)
    {}

    void Insert(nsCounterNode* aNode) {
        nsGenConList::Insert(aNode);
        // Don't SetScope if we're dirty -- we'll reset all the scopes anyway,
        // and we can't usefully compute scopes right now.
        if (NS_LIKELY(!IsDirty())) {
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

    static PRInt32 ValueBefore(nsCounterNode* aNode) {
        return aNode->mScopePrev ? aNode->mScopePrev->mValueAfter : 0;
    }

    // Correctly set |aNode->mScopeStart| and |aNode->mScopePrev|
    void SetScope(nsCounterNode *aNode);
  
    // Recalculate |mScopeStart|, |mScopePrev|, and |mValueAfter| for
    // all nodes and update text in text content nodes.
    void RecalcAll();

    bool IsDirty() { return mDirty; }
    void SetDirty() { mDirty = PR_TRUE; }

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

    // Destroy nodes for the frame in any lists, and return whether any
    // nodes were destroyed.
    bool DestroyNodesFor(nsIFrame *aFrame);

    // Clear all data.
    void Clear() { mNames.Clear(); }

#ifdef DEBUG
    void Dump();
#endif

private:
    // for |AddCounterResetsAndIncrements| only
    bool AddResetOrIncrement(nsIFrame *aFrame, PRInt32 aIndex,
                               const nsStyleCounterData *aCounterData,
                               nsCounterNode::Type aType);

    nsClassHashtable<nsStringHashKey, nsCounterList> mNames;
};

#endif /* nsCounterManager_h_ */

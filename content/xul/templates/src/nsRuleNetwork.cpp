/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Neil Deakin <enndeakin@sympatico.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*

  Implementations for the rule network classes.

  To Do.

  - Constrain() & Propagate() still feel like they are poorly named.
  - As do Instantiation and InstantiationSet.
  - Make InstantiationSet share and do copy-on-write.
  - Make things iterative, instead of recursive.

 */

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "plhash.h"
#include "nsReadableUtils.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;

#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsXULContentUtils.h"

#endif

#include "nsRuleNetwork.h"
#include "nsXULTemplateResultSetRDF.h"
#include "nsRDFConMemberTestNode.h"
#include "nsRDFPropertyTestNode.h"

bool MemoryElement::gPoolInited;
nsFixedSizeAllocator MemoryElement::gPool;

// static
bool
MemoryElement::Init()
{
    if (!gPoolInited) {
        const size_t bucketsizes[] = {
            sizeof (nsRDFConMemberTestNode::Element),
            sizeof (nsRDFPropertyTestNode::Element)
        };

        if (NS_FAILED(gPool.Init("MemoryElement", bucketsizes,
                                 NS_ARRAY_LENGTH(bucketsizes), 256)))
            return PR_FALSE;

        gPoolInited = PR_TRUE;
    }

    return PR_TRUE;
}

//----------------------------------------------------------------------
//
// nsRuleNetwork
//

nsresult
MemoryElementSet::Add(MemoryElement* aElement)
{
    for (ConstIterator element = First(); element != Last(); ++element) {
        if (*element == *aElement) {
            // We've already got this element covered. Since Add()
            // assumes ownership, and we aren't going to need this,
            // just nuke it.
            aElement->Destroy();
            return NS_OK;
        }
    }

    List* list = new List;
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mElement = aElement;
    list->mRefCnt  = 1;
    list->mNext    = mElements;

    mElements = list;

    return NS_OK;
}


//----------------------------------------------------------------------

nsresult
nsAssignmentSet::Add(const nsAssignment& aAssignment)
{
    NS_PRECONDITION(! HasAssignmentFor(aAssignment.mVariable), "variable already bound");

    // XXXndeakin should this just silently fail?
    if (HasAssignmentFor(aAssignment.mVariable))
        return NS_ERROR_UNEXPECTED;

    List* list = new List(aAssignment);
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mRefCnt     = 1;
    list->mNext       = mAssignments;

    mAssignments = list;

    return NS_OK;
}

PRInt32
nsAssignmentSet::Count() const
{
    PRInt32 count = 0;
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment)
        ++count;

    return count;
}

bool
nsAssignmentSet::HasAssignment(nsIAtom* aVariable, nsIRDFNode* aValue) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable && assignment->mValue == aValue)
            return PR_TRUE;
    }

    return PR_FALSE;
}

bool
nsAssignmentSet::HasAssignmentFor(nsIAtom* aVariable) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable)
            return PR_TRUE;
    }

    return PR_FALSE;
}

bool
nsAssignmentSet::GetAssignmentFor(nsIAtom* aVariable, nsIRDFNode** aValue) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable) {
            *aValue = assignment->mValue;
            NS_IF_ADDREF(*aValue);
            return PR_TRUE;
        }
    }

    *aValue = nsnull;
    return PR_FALSE;
}

bool
nsAssignmentSet::Equals(const nsAssignmentSet& aSet) const
{
    if (aSet.mAssignments == mAssignments)
        return PR_TRUE;

    // If they have a different number of assignments, then they're different.
    if (Count() != aSet.Count())
        return PR_FALSE;

    // XXX O(n^2)! Ugh!
    nsCOMPtr<nsIRDFNode> value;
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (! aSet.GetAssignmentFor(assignment->mVariable, getter_AddRefs(value)))
            return PR_FALSE;

        if (assignment->mValue != value)
            return PR_FALSE;
    }

    return PR_TRUE;
}

//----------------------------------------------------------------------

PLHashNumber
Instantiation::Hash(const void* aKey)
{
    const Instantiation* inst = static_cast<const Instantiation*>(aKey);

    PLHashNumber result = 0;

    nsAssignmentSet::ConstIterator last = inst->mAssignments.Last();
    for (nsAssignmentSet::ConstIterator assignment = inst->mAssignments.First();
         assignment != last; ++assignment)
        result ^= assignment->Hash();

    return result;
}


PRIntn
Instantiation::Compare(const void* aLeft, const void* aRight)
{
    const Instantiation* left  = static_cast<const Instantiation*>(aLeft);
    const Instantiation* right = static_cast<const Instantiation*>(aRight);

    return *left == *right;
}


//----------------------------------------------------------------------
//
// InstantiationSet
//

InstantiationSet::InstantiationSet()
{
    mHead.mPrev = mHead.mNext = &mHead;
    MOZ_COUNT_CTOR(InstantiationSet);
}


InstantiationSet::InstantiationSet(const InstantiationSet& aInstantiationSet)
{
    mHead.mPrev = mHead.mNext = &mHead;

    // XXX replace with copy-on-write foo
    ConstIterator last = aInstantiationSet.Last();
    for (ConstIterator inst = aInstantiationSet.First(); inst != last; ++inst)
        Append(*inst);

    MOZ_COUNT_CTOR(InstantiationSet);
}

InstantiationSet&
InstantiationSet::operator=(const InstantiationSet& aInstantiationSet)
{
    // XXX replace with copy-on-write foo
    Clear();

    ConstIterator last = aInstantiationSet.Last();
    for (ConstIterator inst = aInstantiationSet.First(); inst != last; ++inst)
        Append(*inst);

    return *this;
}


void
InstantiationSet::Clear()
{
    Iterator inst = First();
    while (inst != Last())
        Erase(inst++);
}


InstantiationSet::Iterator
InstantiationSet::Insert(Iterator aIterator, const Instantiation& aInstantiation)
{
    List* newelement = new List();
    if (newelement) {
        newelement->mInstantiation = aInstantiation;

        aIterator.mCurrent->mPrev->mNext = newelement;

        newelement->mNext = aIterator.mCurrent;
        newelement->mPrev = aIterator.mCurrent->mPrev;

        aIterator.mCurrent->mPrev = newelement;
    }
    return aIterator;
}

InstantiationSet::Iterator
InstantiationSet::Erase(Iterator aIterator)
{
    Iterator result = aIterator;
    ++result;
    aIterator.mCurrent->mNext->mPrev = aIterator.mCurrent->mPrev;
    aIterator.mCurrent->mPrev->mNext = aIterator.mCurrent->mNext;
    delete aIterator.mCurrent;
    return result;
}


bool
InstantiationSet::HasAssignmentFor(nsIAtom* aVariable) const
{
    return !Empty() ? First()->mAssignments.HasAssignmentFor(aVariable) : PR_FALSE;
}

//----------------------------------------------------------------------
//
// ReteNode
//
//   The basic node in the network.
//

//----------------------------------------------------------------------
//
// TestNode
//
//   to do:
//     - FilterInstantiations() is poorly named
//


TestNode::TestNode(TestNode* aParent)
    : mParent(aParent)
{
}

nsresult
TestNode::Propagate(InstantiationSet& aInstantiations,
                    bool aIsUpdate, bool& aTakenInstantiations)
{
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Propagate() begin", this));

    aTakenInstantiations = PR_FALSE;

    nsresult rv = FilterInstantiations(aInstantiations, nsnull);
    if (NS_FAILED(rv))
        return rv;

    // if there is more than one child, each will need to be supplied with the
    // original set of instantiations from this node, so create a copy in this
    // case. If there is only one child, optimize and just pass the
    // instantiations along to the child without copying
    bool shouldCopy = (mKids.Count() > 1);

    // See the header file for details about how instantiation ownership works.
    if (! aInstantiations.Empty()) {
        ReteNodeSet::Iterator last = mKids.Last();
        for (ReteNodeSet::Iterator kid = mKids.First(); kid != last; ++kid) {
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("TestNode[%p]: Propagate() passing to child %p", this, kid.operator->()));

            // create a copy of the instantiations
            if (shouldCopy) {
                bool owned = false;
                InstantiationSet* instantiations =
                    new InstantiationSet(aInstantiations);
                if (!instantiations)
                    return NS_ERROR_OUT_OF_MEMORY;
                rv = kid->Propagate(*instantiations, aIsUpdate, owned);
                if (!owned)
                    delete instantiations;
                if (NS_FAILED(rv))
                    return rv;
            }
            else {
                rv = kid->Propagate(aInstantiations, aIsUpdate, aTakenInstantiations);
                if (NS_FAILED(rv))
                    return rv;
            }
        }
    }

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Propagate() end", this));

    return NS_OK;
}


nsresult
TestNode::Constrain(InstantiationSet& aInstantiations)
{
    nsresult rv;

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Constrain() begin", this));

    // if the cantHandleYet flag is set by FilterInstantiations,
    // there isn't enough information yet available to fill in.
    // For this, continue the constrain all the way to the top
    // and then call FilterInstantiations again afterwards. This
    // should fill in any missing information.
    bool cantHandleYet = false;
    rv = FilterInstantiations(aInstantiations, &cantHandleYet);
    if (NS_FAILED(rv)) return rv;

    if (mParent && (!aInstantiations.Empty() || cantHandleYet)) {
        // if we still have instantiations, or if the instantiations
        // could not be filled in yet, then ride 'em on up to the
        // parent to narrow them.

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("TestNode[%p]: Constrain() passing to parent %p", this, mParent));

        rv = mParent->Constrain(aInstantiations);

        if (NS_SUCCEEDED(rv) && cantHandleYet)
            rv = FilterInstantiations(aInstantiations, nsnull);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("TestNode[%p]: Constrain() failed", this));

        rv = NS_OK;
    }

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Constrain() end", this));

    return rv;
}


bool
TestNode::HasAncestor(const ReteNode* aNode) const
{
    return aNode == this || (mParent && mParent->HasAncestor(aNode));
}


//----------------------------------------------------------------------

ReteNodeSet::ReteNodeSet()
    : mNodes(nsnull), mCount(0), mCapacity(0)
{
}

ReteNodeSet::~ReteNodeSet()
{
    Clear();
}

nsresult
ReteNodeSet::Add(ReteNode* aNode)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        ReteNode** nodes = new ReteNode*[capacity];
        if (! nodes)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            nodes[i] = mNodes[i];

        delete[] mNodes;

        mNodes = nodes;
        mCapacity = capacity;
    }

    mNodes[mCount++] = aNode;
    return NS_OK;
}

nsresult
ReteNodeSet::Clear()
{
    delete[] mNodes;
    mNodes = nsnull;
    mCount = mCapacity = 0;
    return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
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
#include "nsRuleNetwork.h"
#include "plhash.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gXULTemplateLog;
#endif

//----------------------------------------------------------------------
//
// nsRuleNetwork
//

static PLDHashNumber PR_CALLBACK
HashEntry(PLDHashTable* aTable, const void* aKey)
{
    return nsCRT::HashCode(NS_STATIC_CAST(const PRUnichar*, aKey));
}

static PRBool PR_CALLBACK
MatchEntry(PLDHashTable* aTable, const PLDHashEntryHdr* aEntry, const void* aKey)
{
    const nsRuleNetwork::SymtabEntry* entry =
        NS_REINTERPRET_CAST(const nsRuleNetwork::SymtabEntry*, aEntry);

    return 0 == nsCRT::strcmp(entry->mSymbol, NS_STATIC_CAST(const PRUnichar*, aKey));
}

static void PR_CALLBACK
ClearEntry(PLDHashTable* aTable, PLDHashEntryHdr* aEntry)
{
    nsRuleNetwork::SymtabEntry* entry =
        NS_REINTERPRET_CAST(nsRuleNetwork::SymtabEntry*, aEntry);

    nsCRT::free(entry->mSymbol);
    PL_DHashClearEntryStub(aTable, aEntry);
}

PLDHashTableOps nsRuleNetwork::gOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    PL_DHashGetKeyStub,
    HashEntry,
    MatchEntry,
    PL_DHashMoveEntryStub,
    ClearEntry,
    PL_DHashFinalizeStub
};

void
nsRuleNetwork::Init()
{
    mNextVariable = 0;
    PL_DHashTableInit(&mSymtab, &gOps, nsnull, sizeof(SymtabEntry), PL_DHASH_MIN_SIZE);
}

void
nsRuleNetwork::Finish()
{
    PL_DHashTableFinish(&mSymtab);

    // We "own" the nodes. So it's up to us to delete 'em
    for (ReteNodeSet::Iterator node = mNodes.First(); node != mNodes.Last(); ++node)
        delete *node;

    mNodes.Clear();
    mRoot.RemoveAllChildren();
}


//----------------------------------------------------------------------
//
// Value
//

#ifdef DEBUG
/**
 * A debug-only implementation that verifies that 1) aValue really
 * is an nsISupports, and 2) that it really does support the IID
 * that is being asked for.
 */
nsISupports*
value_to_isupports(const nsIID& aIID, const Value& aValue)
{
    nsresult rv;

    // Need to const_cast aValue because QI() & Release() are not const
    nsISupports* isupports = NS_STATIC_CAST(nsISupports*, NS_CONST_CAST(Value&, aValue));
    if (isupports) {
        nsISupports* dummy;
        rv = isupports->QueryInterface(aIID, (void**) &dummy);
        if (NS_SUCCEEDED(rv)) {
            NS_RELEASE(dummy);
        }
        else {
            NS_ERROR("value does not support expected interface");
        }
    }
    return isupports;
}
#endif

Value::Value(const Value& aValue)
    : mType(aValue.mType)
{
    MOZ_COUNT_CTOR(Value);

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        mISupports = aValue.mISupports;
        NS_IF_ADDREF(mISupports);
        break;

    case eString:
        mString = nsCRT::strdup(aValue.mString);
        break;

    case eInteger:
        mInteger = aValue.mInteger;
        break;
    }
}

Value::Value(nsISupports* aISupports)
    : mType(eISupports)
{
    MOZ_COUNT_CTOR(Value);
    mISupports = aISupports;
    NS_IF_ADDREF(mISupports);
}

Value::Value(const PRUnichar* aString)
    : mType(eString)
{
    MOZ_COUNT_CTOR(Value);
    mString = nsCRT::strdup(aString);
}

Value::Value(PRInt32 aInteger)
    : mType(eInteger)
{
    MOZ_COUNT_CTOR(Value);
    mInteger = aInteger;
}

Value&
Value::operator=(const Value& aValue)
{
    Clear();

    mType = aValue.mType;

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        mISupports = aValue.mISupports;
        NS_IF_ADDREF(mISupports);
        break;

    case eString:
        mString = nsCRT::strdup(aValue.mString);
        break;

    case eInteger:
        mInteger = aValue.mInteger;
        break;
    }

    return *this;
}

Value&
Value::operator=(nsISupports* aISupports)
{
    Clear();

    mType = eISupports;
    mISupports = aISupports;
    NS_IF_ADDREF(mISupports);

    return *this;
}

Value&
Value::operator=(const PRUnichar* aString)
{
    Clear();

    mType = eString;
    mString = nsCRT::strdup(aString);

    return *this;
}


Value::~Value()
{
    MOZ_COUNT_DTOR(Value);
    Clear();
}


void
Value::Clear()
{
    switch (mType) {
    case eInteger:
    case eUndefined:
        break;

    case eISupports:
        NS_IF_RELEASE(mISupports);
        break;

    case eString:
        nsCRT::free(mString);
        break;

    }
}


PRBool
Value::Equals(const Value& aValue) const
{
    if (mType == aValue.mType) {
        switch (mType) {
        case eUndefined:
            return PR_FALSE;

        case eISupports:
            return mISupports == aValue.mISupports;

        case eString:
            return nsCRT::strcmp(mString, aValue.mString) == 0;

        case eInteger:
            return mInteger == aValue.mInteger;
        }
    }
    return PR_FALSE;
}

PRBool
Value::Equals(nsISupports* aISupports) const
{
    return (mType == eISupports) && (mISupports == aISupports);
}

PRBool
Value::Equals(const PRUnichar* aString) const
{
    return (mType == eString) && (nsCRT::strcmp(aString, mString) == 0);
}

PRBool
Value::Equals(PRInt32 aInteger) const
{
    return (mType == eInteger) && (mInteger == aInteger);
}


PLHashNumber
Value::Hash() const
{
    PLHashNumber temp = 0;

    switch (mType) {
    case eUndefined:
        break;

    case eISupports:
        temp = PLHashNumber(NS_PTR_TO_INT32(mISupports)) >> 2; // strip alignment bits
        break;

    case eString:
        {
            PRUnichar* p = mString;
            PRUnichar c;
            while ((c = *p) != 0) {
                temp = (temp >> 28) ^ (temp << 4) ^ c;
                ++p;
            }
        }
        break;

    case eInteger:
        temp = mInteger;
        break;
    }

    return temp;
}


Value::operator nsISupports*() const
{
    NS_ASSERTION(mType == eISupports, "not an nsISupports");
    return (mType == eISupports) ? mISupports : 0;
}

Value::operator const PRUnichar*() const
{
    NS_ASSERTION(mType == eString, "not a string");
    return (mType == eString) ? mString : 0;
}

Value::operator PRInt32() const
{
    NS_ASSERTION(mType == eInteger, "not an integer");
    return (mType == eInteger) ? mInteger : 0;
}

#ifdef DEBUG
#include "nsIRDFResource.h"
#include "nsIRDFLiteral.h"
#include "nsString.h"
#include "nsPrintfCString.h"

void
Value::ToCString(nsACString& aResult)
{
    switch (mType) {
    case eUndefined:
        aResult = "[(undefined)]";
        break;

    case eISupports:
        do {
            nsCOMPtr<nsIRDFResource> res = do_QueryInterface(mISupports);
            if (res) {
                aResult = "[nsIRDFResource ";
                const char* s;
                res->GetValueConst(&s);
                aResult += s;
                aResult += "]";
                break;
            }

            nsCOMPtr<nsIRDFLiteral> lit = do_QueryInterface(mISupports);
            if (lit) {
                aResult = "[nsIRDFLiteral \"";
                const PRUnichar* s;
                lit->GetValueConst(&s);
                aResult += NS_ConvertUCS2toUTF8(s);
                aResult += "\"]";
                break;
            }

            aResult = "[nsISupports ";
            aResult += nsPrintfCString("%p", mISupports);
            aResult += "]";
        } while (0);
        break;

    case eString:
        aResult = "[string \"";
        aResult += NS_ConvertUCS2toUTF8(mString);
        aResult += "\"]";
        break;

    case eInteger:
        aResult = "[integer ";
        aResult += nsPrintfCString("%d", mInteger);
        aResult += "]";
        break;
    }
}
#endif


//----------------------------------------------------------------------
//
// VariableSet
//


VariableSet::VariableSet()
    : mVariables(nsnull), mCount(0), mCapacity(0)
{
}

VariableSet::~VariableSet()
{
    delete[] mVariables;
}

nsresult
VariableSet::Add(PRInt32 aVariable)
{
    if (Contains(aVariable))
        return NS_OK;

    if (mCount >= mCapacity) {
        PRInt32 capacity = mCapacity + 4;
        PRInt32* variables = new PRInt32[capacity];
        if (! variables)
            return NS_ERROR_OUT_OF_MEMORY;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            variables[i] = mVariables[i];

        delete[] mVariables;

        mVariables = variables;
        mCapacity = capacity;
    }

    mVariables[mCount++] = aVariable;
    return NS_OK;
}

nsresult
VariableSet::Remove(PRInt32 aVariable)
{
    PRInt32 i = 0;
    while (i < mCount) {
        if (aVariable == mVariables[i])
            break;

        ++i;
    }

    if (i >= mCount)
        return NS_OK;

    --mCount;

    while (i < mCount) {
        mVariables[i] = mVariables[i + 1];
        ++i;
    }
        
    return NS_OK;
}

PRBool
VariableSet::Contains(PRInt32 aVariable) const
{
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        if (aVariable == mVariables[i])
            return PR_TRUE;
    }

    return PR_FALSE;
}

//----------------------------------------------------------------------=

nsresult
MemoryElementSet::Add(MemoryElement* aElement)
{
    for (ConstIterator element = First(); element != Last(); ++element) {
        if (*element == *aElement) {
            // We've already got this element covered. Since Add()
            // assumes ownership, and we aren't going to need this,
            // just nuke it.
            delete aElement;
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
    if (HasAssignmentFor(aAssignment.mVariable))
        return NS_ERROR_UNEXPECTED;

    List* list = new List;
    if (! list)
        return NS_ERROR_OUT_OF_MEMORY;

    list->mAssignment = aAssignment;
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

PRBool
nsAssignmentSet::HasAssignment(PRInt32 aVariable, const Value& aValue) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable && assignment->mValue == aValue)
            return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
nsAssignmentSet::HasAssignmentFor(PRInt32 aVariable) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable)
            return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
nsAssignmentSet::GetAssignmentFor(PRInt32 aVariable, Value* aValue) const
{
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        if (assignment->mVariable == aVariable) {
            *aValue = assignment->mValue;
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

PRBool
nsAssignmentSet::Equals(const nsAssignmentSet& aSet) const
{
    if (aSet.mAssignments == mAssignments)
        return PR_TRUE;

    // If they have a different number of assignments, then they're different.
    if (Count() != aSet.Count())
        return PR_FALSE;

    // XXX O(n^2)! Ugh!
    for (ConstIterator assignment = First(); assignment != Last(); ++assignment) {
        Value value;
        if (! aSet.GetAssignmentFor(assignment->mVariable, &value))
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
    const Instantiation* inst = NS_STATIC_CAST(const Instantiation*, aKey);

    PLHashNumber result = 0;

    nsAssignmentSet::ConstIterator last = inst->mAssignments.Last();
    for (nsAssignmentSet::ConstIterator assignment = inst->mAssignments.First(); assignment != last; ++assignment)
        result ^= assignment->Hash();

    return result;
}


PRIntn
Instantiation::Compare(const void* aLeft, const void* aRight)
{
    const Instantiation* left  = NS_STATIC_CAST(const Instantiation*, aLeft);
    const Instantiation* right = NS_STATIC_CAST(const Instantiation*, aRight);

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


PRBool
InstantiationSet::HasAssignmentFor(PRInt32 aVariable) const
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
// RootNode
//

nsresult
RootNode::Propagate(const InstantiationSet& aInstantiations, void* aClosure)
{
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("RootNode[%p]: Propagate() begin", this));

    ReteNodeSet::Iterator last = mKids.Last();
    for (ReteNodeSet::Iterator kid = mKids.First(); kid != last; ++kid)
        kid->Propagate(aInstantiations, aClosure);

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("RootNode[%p]: Propagate() end", this));

    return NS_OK;
}

nsresult
RootNode::Constrain(InstantiationSet& aInstantiations, void* aClosure)
{
    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("RootNode[%p]: Constrain()", this));

    return NS_OK;
}


nsresult
RootNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return NS_OK;
}


PRBool
RootNode::HasAncestor(const ReteNode* aNode) const
{
    return aNode == this;
}

//----------------------------------------------------------------------
//
// JoinNode
//
//   A node that performs a join in the network.
//

JoinNode::JoinNode(InnerNode* aLeftParent,
                   PRInt32 aLeftVariable,
                   InnerNode* aRightParent,
                   PRInt32 aRightVariable,
                   Operator aOperator)
    : mLeftParent(aLeftParent),
      mLeftVariable(aLeftVariable),
      mRightParent(aRightParent),
      mRightVariable(aRightVariable),
      mOperator(aOperator)
{
}

nsresult
JoinNode::Propagate(const InstantiationSet& aInstantiations, void* aClosure)
{
    // the add will have been propagated down from one of the parent
    // nodes: either the left or the right. Test the other node for
    // matches.
    nsresult rv;

    PRBool hasLeftAssignment = aInstantiations.HasAssignmentFor(mLeftVariable);
    PRBool hasRightAssignment = aInstantiations.HasAssignmentFor(mRightVariable);

    NS_ASSERTION(hasLeftAssignment ^ hasRightAssignment, "there isn't exactly one assignment specified");
    if (! (hasLeftAssignment ^ hasRightAssignment))
        return NS_ERROR_UNEXPECTED;

    InstantiationSet instantiations = aInstantiations;
    InnerNode* test = hasLeftAssignment ? mRightParent : mLeftParent;

    {
        // extend the assignments
        InstantiationSet::Iterator last = instantiations.Last();
        for (InstantiationSet::Iterator inst = instantiations.First(); inst != last; ++inst) {
            if (hasLeftAssignment) {
                // the left is bound
                Value leftValue;
                inst->mAssignments.GetAssignmentFor(mLeftVariable, &leftValue);
                rv = inst->AddAssignment(mRightVariable, leftValue);
            }
            else {
                // the right is bound
                Value rightValue;
                inst->mAssignments.GetAssignmentFor(mRightVariable, &rightValue);
                rv = inst->AddAssignment(mLeftVariable, rightValue);
            }

            if (NS_FAILED(rv)) return rv;
        }
    }

    if (! instantiations.Empty()) {
        // propagate consistency checking back up the tree
        rv = test->Constrain(instantiations, aClosure);
        if (NS_FAILED(rv)) return rv;

        ReteNodeSet::Iterator last = mKids.Last();
        for (ReteNodeSet::Iterator kid = mKids.First(); kid != last; ++kid)
            kid->Propagate(instantiations, aClosure);
    }

    return NS_OK;
}


nsresult
JoinNode::GetNumBound(InnerNode* aAncestor, const InstantiationSet& aInstantiations, PRInt32* aBoundCount)
{
    // Compute the number of variables for an ancestor that are bound
    // in the current instantiation set.
    nsresult rv;

    VariableSet vars;
    rv = aAncestor->GetAncestorVariables(vars);
    if (NS_FAILED(rv)) return rv;

    PRInt32 count = 0;
    for (PRInt32 i = vars.GetCount() - 1; i >= 0; --i) {
        if (aInstantiations.HasAssignmentFor(vars.GetVariableAt(i)))
            ++count;
    }

    *aBoundCount = count;
    return NS_OK;
}


nsresult
JoinNode::Bind(InstantiationSet& aInstantiations, PRBool* aDidBind)
{
    // Try to use the instantiation set to bind the unbound join
    // variable. If successful, aDidBind <= PR_TRUE.
    nsresult rv;

    PRBool hasLeftAssignment = aInstantiations.HasAssignmentFor(mLeftVariable);
    PRBool hasRightAssignment = aInstantiations.HasAssignmentFor(mRightVariable);

    NS_ASSERTION(! (hasLeftAssignment && hasRightAssignment), "there is more than one assignment specified");
    if (hasLeftAssignment && hasRightAssignment)
        return NS_ERROR_UNEXPECTED;

    if (hasLeftAssignment || hasRightAssignment) {
        InstantiationSet::Iterator last = aInstantiations.Last();
        for (InstantiationSet::Iterator inst = aInstantiations.First(); inst != last; ++inst) {
            if (hasLeftAssignment) {
                // the left is bound
                Value leftValue;
                inst->mAssignments.GetAssignmentFor(mLeftVariable, &leftValue);
                rv = inst->AddAssignment(mRightVariable, leftValue);
            }
            else {
                // the right is bound
                Value rightValue;
                inst->mAssignments.GetAssignmentFor(mRightVariable, &rightValue);
                rv = inst->AddAssignment(mLeftVariable, rightValue);
            }

            if (NS_FAILED(rv)) return rv;
        }

        *aDidBind = PR_TRUE;
    }
    else {
        *aDidBind = PR_FALSE;
    }

    return NS_OK;
}

nsresult
JoinNode::Constrain(InstantiationSet& aInstantiations, void* aClosure)
{
    if (aInstantiations.Empty())
        return NS_OK;

    nsresult rv;
    PRBool didBind;

    rv = Bind(aInstantiations, &didBind);
    if (NS_FAILED(rv)) return rv;

    PRInt32 numLeftBound;
    rv = GetNumBound(mLeftParent, aInstantiations, &numLeftBound);
    if (NS_FAILED(rv)) return rv;

    PRInt32 numRightBound;
    rv = GetNumBound(mRightParent, aInstantiations, &numRightBound);
    if (NS_FAILED(rv)) return rv;

    InnerNode *first, *last;
    if (numLeftBound > numRightBound) {
        first = mLeftParent;
        last = mRightParent;
    }
    else {
        first = mRightParent;
        last = mLeftParent;
    }

    rv = first->Constrain(aInstantiations, aClosure);
    if (NS_FAILED(rv)) return rv;

    if (! didBind) {
        rv = Bind(aInstantiations, &didBind);
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(didBind, "uh oh, still no assignment");
    }

    rv = last->Constrain(aInstantiations, aClosure);
    if (NS_FAILED(rv)) return rv;

    if (! didBind) {
        // sort throught the full cross product
        NS_NOTYETIMPLEMENTED("write me");
    }

    return NS_OK;
}


nsresult
JoinNode::GetAncestorVariables(VariableSet& aVariables) const
{
    nsresult rv;

    rv = mLeftParent->GetAncestorVariables(aVariables);
    if (NS_FAILED(rv)) return rv;

    rv = mRightParent->GetAncestorVariables(aVariables);
    if (NS_FAILED(rv)) return rv;

    
    if (mLeftVariable) {
        rv = aVariables.Add(mLeftVariable);
        if (NS_FAILED(rv)) return rv;
    }

    if (mRightVariable) {
        rv = aVariables.Add(mRightVariable);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


PRBool
JoinNode::HasAncestor(const ReteNode* aNode) const
{
    if (aNode == this) {
        return PR_TRUE;
    }
    else if (mLeftParent->HasAncestor(aNode) || mRightParent->HasAncestor(aNode)) {
        return PR_TRUE;
    }
    else {
        return PR_FALSE;
    }
}

//----------------------------------------------------------------------
//
// TestNode
//
//   to do:
//     - FilterInstantiations() is poorly named
//


TestNode::TestNode(InnerNode* aParent)
    : mParent(aParent)
{
}


nsresult
TestNode::Propagate(const InstantiationSet& aInstantiations, void* aClosure)
{
    nsresult rv;

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Propagate() begin", this));

    InstantiationSet instantiations = aInstantiations;
    rv = FilterInstantiations(instantiations, aClosure);
    if (NS_FAILED(rv)) return rv;

    if (! instantiations.Empty()) {
        ReteNodeSet::Iterator last = mKids.Last();
        for (ReteNodeSet::Iterator kid = mKids.First(); kid != last; ++kid) {
            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("TestNode[%p]: Propagate() passing to child %p", this, kid.operator->()));

            kid->Propagate(instantiations, aClosure);
        }
    }

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Propagate() end", this));

    return NS_OK;
}


nsresult
TestNode::Constrain(InstantiationSet& aInstantiations, void* aClosure)
{
    nsresult rv;

    PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
           ("TestNode[%p]: Constrain() begin", this));

    rv = FilterInstantiations(aInstantiations, aClosure);
    if (NS_FAILED(rv)) return rv;

    if (! aInstantiations.Empty()) {
        // if we still have instantiations, then ride 'em on up to the
        // parent to narrow them.

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("TestNode[%p]: Constrain() passing to parent %p", this, mParent));

        rv = mParent->Constrain(aInstantiations, aClosure);
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


nsresult
TestNode::GetAncestorVariables(VariableSet& aVariables) const
{
    return mParent->GetAncestorVariables(aVariables);
}


PRBool
TestNode::HasAncestor(const ReteNode* aNode) const
{
    return aNode == this ? PR_TRUE : mParent->HasAncestor(aNode);
}


//----------------------------------------------------------------------0

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

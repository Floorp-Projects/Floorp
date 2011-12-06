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

  A rule discrimination network implementation based on ideas from
  RETE and TREAT.

  RETE is described in Charles Forgy, "Rete: A Fast Algorithm for the
  Many Patterns/Many Objects Match Problem", Artificial Intelligence
  19(1): pp. 17-37, 1982.

  TREAT is described in Daniel P. Miranker, "TREAT: A Better Match
  Algorithm for AI Production System Matching", AAAI 1987: pp. 42-47.

  --

  TO DO:

  . nsAssignmentSet::List objects are allocated by the gallon. We
    should make it so that these are always allocated from a pool,
    maybe owned by the nsRuleNetwork?

 */

#ifndef nsRuleNetwork_h__
#define nsRuleNetwork_h__

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsFixedSizeAllocator.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "plhash.h"
#include "pldhash.h"
#include "nsCRT.h"
#include "nsIRDFNode.h"

class nsIRDFResource;
class nsXULTemplateResultSetRDF;
class nsXULTemplateQueryProcessorRDF;

//----------------------------------------------------------------------

/**
 * A memory element that supports an instantiation. A memory element holds a
 * set of nodes involved in an RDF test such as <member> or <triple> test. A
 * memory element is created when a specific test matches. The query processor
 * maintains a map between the memory elements and the results they eventually
 * matched. When an assertion is removed from the graph, this map is consulted
 * to determine which results will no longer match.
 */
class MemoryElement {
protected:
    MemoryElement() { MOZ_COUNT_CTOR(MemoryElement); }
    virtual ~MemoryElement() { MOZ_COUNT_DTOR(MemoryElement); }
public:

    static bool Init();

    static bool gPoolInited;
    static nsFixedSizeAllocator gPool;

    virtual void Destroy() = 0;
    virtual const char* Type() const = 0;
    virtual PLHashNumber Hash() const = 0;
    virtual bool Equals(const MemoryElement& aElement) const = 0;

    bool operator==(const MemoryElement& aMemoryElement) const {
        return Equals(aMemoryElement);
    }

    bool operator!=(const MemoryElement& aMemoryElement) const {
        return !Equals(aMemoryElement);
    }
};

//----------------------------------------------------------------------

/**
 * A collection of memory elements
 */
class MemoryElementSet {
public:
    class ConstIterator;
    friend class ConstIterator;

protected:
    class List {
    public:
        List() { MOZ_COUNT_CTOR(MemoryElementSet::List); }

        ~List() {
            MOZ_COUNT_DTOR(MemoryElementSet::List);
            mElement->Destroy();
            NS_IF_RELEASE(mNext); }

        PRInt32 AddRef() { return ++mRefCnt; }

        PRInt32 Release() {
            PRInt32 refcnt = --mRefCnt;
            if (refcnt == 0) delete this;
            return refcnt; }

        MemoryElement* mElement;
        PRInt32        mRefCnt;
        List*          mNext;
    };

    List* mElements;

public:
    MemoryElementSet() : mElements(nsnull) {
        MOZ_COUNT_CTOR(MemoryElementSet); }

    MemoryElementSet(const MemoryElementSet& aSet) : mElements(aSet.mElements) {
        MOZ_COUNT_CTOR(MemoryElementSet);
        NS_IF_ADDREF(mElements); }

    MemoryElementSet& operator=(const MemoryElementSet& aSet) {
        NS_IF_RELEASE(mElements);
        mElements = aSet.mElements;
        NS_IF_ADDREF(mElements);
        return *this; }
        
    ~MemoryElementSet() {
        MOZ_COUNT_DTOR(MemoryElementSet);
        NS_IF_RELEASE(mElements); }

public:
    class ConstIterator {
    public:
        ConstIterator(List* aElementList) : mCurrent(aElementList) {
            NS_IF_ADDREF(mCurrent); }

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {
            NS_IF_ADDREF(mCurrent); }

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            NS_IF_RELEASE(mCurrent);
            mCurrent = aConstIterator.mCurrent;
            NS_IF_ADDREF(mCurrent);
            return *this; }

        ~ConstIterator() { NS_IF_RELEASE(mCurrent); }

        ConstIterator& operator++() {
            List* next = mCurrent->mNext;
            NS_RELEASE(mCurrent);
            mCurrent = next;
            NS_IF_ADDREF(mCurrent);
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            List* next = mCurrent->mNext;
            NS_RELEASE(mCurrent);
            mCurrent = next;
            NS_IF_ADDREF(mCurrent);
            return result; }

        const MemoryElement& operator*() const {
            return *mCurrent->mElement; }

        const MemoryElement* operator->() const {
            return mCurrent->mElement; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        List* mCurrent;
    };

    ConstIterator First() const { return ConstIterator(mElements); }
    ConstIterator Last() const { return ConstIterator(nsnull); }

    // N.B. that the set assumes ownership of the element
    nsresult Add(MemoryElement* aElement);
};

//----------------------------------------------------------------------

/**
 * An assignment of a value to a variable
 */
class nsAssignment {
public:
    const nsCOMPtr<nsIAtom> mVariable;
    nsCOMPtr<nsIRDFNode> mValue;

    nsAssignment(nsIAtom* aVariable, nsIRDFNode* aValue)
        : mVariable(aVariable),
          mValue(aValue)
        { MOZ_COUNT_CTOR(nsAssignment); }

    nsAssignment(const nsAssignment& aAssignment)
        : mVariable(aAssignment.mVariable),
          mValue(aAssignment.mValue)
        { MOZ_COUNT_CTOR(nsAssignment); }

    ~nsAssignment() { MOZ_COUNT_DTOR(nsAssignment); }

    bool operator==(const nsAssignment& aAssignment) const {
        return mVariable == aAssignment.mVariable && mValue == aAssignment.mValue; }

    bool operator!=(const nsAssignment& aAssignment) const {
        return mVariable != aAssignment.mVariable || mValue != aAssignment.mValue; }

    PLHashNumber Hash() const {
        // XXX I have no idea if this hashing function is good or not // XXX change this
        PLHashNumber temp = PLHashNumber(NS_PTR_TO_INT32(mValue.get())) >> 2; // strip alignment bits
        return (temp & 0xffff) | NS_PTR_TO_INT32(mVariable.get()); }
};


//----------------------------------------------------------------------

/**
 * A collection of value-to-variable assignments that minimizes
 * copying by sharing subsets when possible.
 */
class nsAssignmentSet {
public:
    class ConstIterator;
    friend class ConstIterator;

protected:
    class List {
    public:
        List(const nsAssignment &aAssignment) : mAssignment(aAssignment) {
            MOZ_COUNT_CTOR(nsAssignmentSet::List); }

        ~List() {
            MOZ_COUNT_DTOR(nsAssignmentSet::List);
            NS_IF_RELEASE(mNext); }

        PRInt32 AddRef() { return ++mRefCnt; }

        PRInt32 Release() {
            PRInt32 refcnt = --mRefCnt;
            if (refcnt == 0) delete this;
            return refcnt; }

        nsAssignment mAssignment;
        PRInt32 mRefCnt;
        List*   mNext;
    };

    List* mAssignments;

public:
    nsAssignmentSet()
        : mAssignments(nsnull)
        { MOZ_COUNT_CTOR(nsAssignmentSet); }

    nsAssignmentSet(const nsAssignmentSet& aSet)
        : mAssignments(aSet.mAssignments) {
        MOZ_COUNT_CTOR(nsAssignmentSet);
        NS_IF_ADDREF(mAssignments); }

    nsAssignmentSet& operator=(const nsAssignmentSet& aSet) {
        NS_IF_RELEASE(mAssignments);
        mAssignments = aSet.mAssignments;
        NS_IF_ADDREF(mAssignments);
        return *this; }

    ~nsAssignmentSet() {
        MOZ_COUNT_DTOR(nsAssignmentSet);
        NS_IF_RELEASE(mAssignments); }

public:
    class ConstIterator {
    public:
        ConstIterator(List* aAssignmentList) : mCurrent(aAssignmentList) {
            NS_IF_ADDREF(mCurrent); }

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {
            NS_IF_ADDREF(mCurrent); }

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            NS_IF_RELEASE(mCurrent);
            mCurrent = aConstIterator.mCurrent;
            NS_IF_ADDREF(mCurrent);
            return *this; }

        ~ConstIterator() { NS_IF_RELEASE(mCurrent); }

        ConstIterator& operator++() {
            List* next = mCurrent->mNext;
            NS_RELEASE(mCurrent);
            mCurrent = next;
            NS_IF_ADDREF(mCurrent);
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            List* next = mCurrent->mNext;
            NS_RELEASE(mCurrent);
            mCurrent = next;
            NS_IF_ADDREF(mCurrent);
            return result; }

        const nsAssignment& operator*() const {
            return mCurrent->mAssignment; }

        const nsAssignment* operator->() const {
            return &mCurrent->mAssignment; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        List* mCurrent;
    };

    ConstIterator First() const { return ConstIterator(mAssignments); }
    ConstIterator Last() const { return ConstIterator(nsnull); }

public:
    /**
     * Add an assignment to the set
     * @param aElement the assigment to add
     * @return NS_OK if all is well, NS_ERROR_OUT_OF_MEMORY if memory
     *   could not be allocated for the addition.
     */
    nsresult Add(const nsAssignment& aElement);

    /**
     * Determine if the assignment set contains the specified variable
     * to value assignment.
     * @param aVariable the variable for which to lookup the binding
     * @param aValue the value to query
     * @return true if aVariable is bound to aValue; false otherwise.
     */
    bool HasAssignment(nsIAtom* aVariable, nsIRDFNode* aValue) const;

    /**
     * Determine if the assignment set contains the specified assignment
     * @param aAssignment the assignment to search for
     * @return true if the set contains the assignment, false otherwise.
     */
    bool HasAssignment(const nsAssignment& aAssignment) const {
        return HasAssignment(aAssignment.mVariable, aAssignment.mValue); }

    /**
     * Determine whether the assignment set has an assignment for the
     * specified variable.
     * @param aVariable the variable to query
     * @return true if the assignment set has an assignment for the variable,
     *   false otherwise.
     */
    bool HasAssignmentFor(nsIAtom* aVariable) const;

    /**
     * Retrieve the assignment for the specified variable
     * @param aVariable the variable to query
     * @param aValue an out parameter that will receive the value assigned
     *   to the variable, if any.
     * @return true if the variable has an assignment, false
     *   if there was no assignment for the variable.
     */
    bool GetAssignmentFor(nsIAtom* aVariable, nsIRDFNode** aValue) const;

    /**
     * Count the number of assignments in the set
     * @return the number of assignments in the set
     */
    PRInt32 Count() const;

    /**
     * Determine if the set is empty
     * @return true if the assignment set is empty, false otherwise.
     */
    bool IsEmpty() const { return mAssignments == nsnull; }

    bool Equals(const nsAssignmentSet& aSet) const;
    bool operator==(const nsAssignmentSet& aSet) const { return Equals(aSet); }
    bool operator!=(const nsAssignmentSet& aSet) const { return !Equals(aSet); }
};


//----------------------------------------------------------------------

/**
 * A collection of variable-to-value bindings, with the memory elements
 * that support those bindings. Essentially, an instantiation is the
 * collection of variables and values assigned to those variables for a single
 * result. For each RDF rule in the rule network, each instantiation is
 * examined and either extended with additional bindings specified by the RDF
 * rule, or removed if the rule doesn't apply (for instance if a node has no
 * children). When an instantiation gets to the last node of the rule network,
 * which is always an nsInstantiationNode, a result is created for it.
 *
 * An instantiation object is typically created by "extending" another
 * instantiation object. That is, using the copy constructor, and
 * adding bindings and support to the instantiation.
 */
class Instantiation
{
public:
    /**
     * The variable-to-value bindings
     */
    nsAssignmentSet  mAssignments;

    /**
     * The memory elements that support the bindings.
     */
    MemoryElementSet mSupport;

    Instantiation() { MOZ_COUNT_CTOR(Instantiation); }

    Instantiation(const Instantiation& aInstantiation)
        : mAssignments(aInstantiation.mAssignments),
          mSupport(aInstantiation.mSupport) {
        MOZ_COUNT_CTOR(Instantiation); }

    Instantiation& operator=(const Instantiation& aInstantiation) {
        mAssignments = aInstantiation.mAssignments;
        mSupport  = aInstantiation.mSupport;
        return *this; }

    ~Instantiation() { MOZ_COUNT_DTOR(Instantiation); }

    /**
     * Add the specified variable-to-value assignment to the instantiation's
     * set of assignments.
     * @param aVariable the variable to which is being assigned
     * @param aValue the value that is being assigned
     * @return NS_OK if no errors, NS_ERROR_OUT_OF_MEMORY if there
     *   is not enough memory to perform the operation
     */
    nsresult AddAssignment(nsIAtom* aVariable, nsIRDFNode* aValue) {
        mAssignments.Add(nsAssignment(aVariable, aValue));
        return NS_OK; }

    /**
     * Add a memory element to the set of memory elements that are
     * supporting the instantiation
     * @param aMemoryElement the memory element to add to the
     *   instantiation's set of support
     * @return NS_OK if no errors occurred, NS_ERROR_OUT_OF_MEMORY
     *   if there is not enough memory to perform the operation.
     */
    nsresult AddSupportingElement(MemoryElement* aMemoryElement) {
        mSupport.Add(aMemoryElement);
        return NS_OK; }

    bool Equals(const Instantiation& aInstantiation) const {
        return mAssignments == aInstantiation.mAssignments; }

    bool operator==(const Instantiation& aInstantiation) const {
        return Equals(aInstantiation); }

    bool operator!=(const Instantiation& aInstantiation) const {
        return !Equals(aInstantiation); }

    static PLHashNumber Hash(const void* aKey);
    static PRIntn Compare(const void* aLeft, const void* aRight);
};


//----------------------------------------------------------------------

/**
 * A collection of intantiations
 */
class InstantiationSet
{
public:
    InstantiationSet();
    InstantiationSet(const InstantiationSet& aInstantiationSet);
    InstantiationSet& operator=(const InstantiationSet& aInstantiationSet);

    ~InstantiationSet() {
        MOZ_COUNT_DTOR(InstantiationSet);
        Clear(); }

    class ConstIterator;
    friend class ConstIterator;

    class Iterator;
    friend class Iterator;

    friend class nsXULTemplateResultSetRDF; // so it can get to the List

protected:
    class List {
    public:
        Instantiation mInstantiation;
        List*         mNext;
        List*         mPrev;

        List() { MOZ_COUNT_CTOR(InstantiationSet::List); }
        ~List() { MOZ_COUNT_DTOR(InstantiationSet::List); }
    };

    List mHead;

public:
    class ConstIterator {
    protected:
        friend class Iterator; // XXXwaterson so broken.
        List* mCurrent;

    public:
        ConstIterator(List* aList) : mCurrent(aList) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        ConstIterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        ConstIterator operator--(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        const Instantiation& operator*() const {
            return mCurrent->mInstantiation; }

        const Instantiation* operator->() const {
            return &mCurrent->mInstantiation; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(const_cast<List*>(&mHead)); }

    class Iterator : public ConstIterator {
    public:
        Iterator(List* aList) : ConstIterator(aList) {}

        Iterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        Iterator operator++(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        Iterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        Iterator operator--(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        Instantiation& operator*() const {
            return mCurrent->mInstantiation; }

        Instantiation* operator->() const {
            return &mCurrent->mInstantiation; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

        friend class InstantiationSet;
    };

    Iterator First() { return Iterator(mHead.mNext); }
    Iterator Last() { return Iterator(&mHead); }

    bool Empty() const { return First() == Last(); }

    Iterator Append(const Instantiation& aInstantiation) {
        return Insert(Last(), aInstantiation); }

    Iterator Insert(Iterator aBefore, const Instantiation& aInstantiation);

    Iterator Erase(Iterator aElement);

    void Clear();

    bool HasAssignmentFor(nsIAtom* aVariable) const;
};

//----------------------------------------------------------------------

/**
 * A abstract base class for all nodes in the rule network
 */
class ReteNode
{
public:
    ReteNode() {}
    virtual ~ReteNode() {}

    /**
     * Propagate a set of instantiations "down" through the
     * network. Each instantiation is a partial set of
     * variable-to-value assignments, along with the memory elements
     * that support it.
     *
     * The node must evaluate each instantiation, and either 1)
     * extend it with additional assignments and memory-element
     * support, or 2) remove it from the set because it is
     * inconsistent with the constraints that this node applies.
     *
     * The node must then pass the resulting instantiation set along
     * to any of its children in the network. (In other words, the
     * node must recursively call Propagate() on its children. We
     * should fix this to make the algorithm interruptable.)
     *
     * See TestNode::Propagate for details about instantiation set ownership
     *
     * @param aInstantiations the set of instantiations to propagate
     *   down through the network.
     * @param aIsUpdate true if updating, false for first generation
     * @param aTakenInstantiations true if the ownership over aInstantiations
     *                             has been taken from the caller. If false,
     *                             the caller owns it.
     * @return NS_OK if no errors occurred.
     */
    virtual nsresult Propagate(InstantiationSet& aInstantiations,
                               bool aIsUpdate, bool& aTakenInstantiations) = 0;
};

//----------------------------------------------------------------------

/**
 * A collection of nodes in the rule network
 */
class ReteNodeSet
{
public:
    ReteNodeSet();
    ~ReteNodeSet();

    nsresult Add(ReteNode* aNode);
    nsresult Clear();

    class Iterator;

    class ConstIterator {
    public:
        ConstIterator(ReteNode** aNode) : mCurrent(aNode) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            ++mCurrent;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            ++mCurrent;
            return result; }

        const ReteNode* operator*() const {
            return *mCurrent; }

        const ReteNode* operator->() const {
            return *mCurrent; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

    protected:
        friend class Iterator; // XXXwaterson this is so wrong!
        ReteNode** mCurrent;
    };

    ConstIterator First() const { return ConstIterator(mNodes); }
    ConstIterator Last() const { return ConstIterator(mNodes + mCount); }

    class Iterator : public ConstIterator {
    public:
        Iterator(ReteNode** aNode) : ConstIterator(aNode) {}

        Iterator& operator++() {
            ++mCurrent;
            return *this; }

        Iterator operator++(int) {
            Iterator result(*this);
            ++mCurrent;
            return result; }

        ReteNode* operator*() const {
            return *mCurrent; }

        ReteNode* operator->() const {
            return *mCurrent; }

        bool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        bool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    Iterator First() { return Iterator(mNodes); }
    Iterator Last() { return Iterator(mNodes + mCount); }

    PRInt32 Count() const { return mCount; }

protected:
    ReteNode** mNodes;
    PRInt32 mCount;
    PRInt32 mCapacity;
};

//----------------------------------------------------------------------

/**
 * A node that applies a test condition to a set of instantiations.
 *
 * This class provides implementations of Propagate() and Constrain()
 * in terms of one simple operation, FilterInstantiations(). A node
 * that is a "simple test node" in a rule network should derive from
 * this class, and need only implement FilterInstantiations().
 */
class TestNode : public ReteNode
{
public:
    TestNode(TestNode* aParent);

    /**
     * Retrieve the test node's parent
     * @return the test node's parent
     */
    TestNode* GetParent() const { return mParent; }

    /**
     * Calls FilterInstantiations() on the instantiation set, and if
     * the resulting set isn't empty, propagates the new set down to
     * each of the test node's children.
     *
     * Note that the caller of Propagate is responsible for deleting
     * aInstantiations if necessary as described below.
     *
     * Propagate may be called in update or non-update mode as indicated
     * by the aIsUpdate argument. Non-update mode is used when initially
     * generating results, whereas update mode is used when the datasource
     * changes and new results might be available.
     *
     * The last node in a chain of TestNodes is always an nsInstantiationNode.
     * In non-update mode, this nsInstantiationNode will cache the results
     * in the query using the SetCachedResults method. The query processor
     * takes these cached results and creates a nsXULTemplateResultSetRDF
     * which is the enumeration returned to the template builder. This
     * nsXULTemplateResultSetRDF owns the instantiations and they will be
     * deleted when the nsXULTemplateResultSetRDF goes away.
     *
     * In update mode, the nsInstantiationNode node will iterate over the
     * instantiations itself and callback to the builder to update any matches
     * and generated content. If no instantiations match, then the builder
     * will never be called.
     *
     * Thus, the difference between update and non-update modes is that in
     * update mode, the results and instantiations have been already handled
     * whereas in non-update mode they are expected to be returned in an
     * nsXULTemplateResultSetRDF for further processing by the builder.
     *
     * Regardless, aTakenInstantiations will be set to true if the
     * ownership over aInstantiations has been transferred to a result set.
     * If set to false, the caller is still responsible for aInstantiations.
     * aTakenInstantiations will be set properly even if an error occurs.
     */
    virtual nsresult Propagate(InstantiationSet& aInstantiations,
                               bool aIsUpdate, bool& aTakenInstantiations);

    /**
     * This is called by a child node on its parent to allow the
     * parent's constraints to apply to the set of instantiations.
     *
     * A node must iterate through the set of instantiations, and for
     * each instantiation, either 1) extend the instantiation by
     * adding variable-to-value assignments and memory element support
     * for those assignments, or 2) remove the instantiation because
     * it is inconsistent.
     *
     * The node must then pass the resulting set of instantiations up
     * to its parent (by recursive call; we should make this iterative
     * & interruptable at some point.)
     * 
     * @param aInstantiations the set of instantiations that must
     *   be constrained
     * @return NS_OK if no errors occurred
     */
    virtual nsresult Constrain(InstantiationSet& aInstantiations);

    /**
     * Given a set of instantiations, filter out any that are
     * inconsistent with the test node's test, and append
     * variable-to-value assignments and memory element support for
     * those which do pass the test node's test.
     *
     * @param aInstantiations the set of instantiations to be
     *        filtered
     * @param aCantHandleYet [out] true if the instantiations do not contain
     *        enough information to constrain the data. May be null if this
     *        isn't important to the caller.
     * @return NS_OK if no errors occurred.
     */
    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations,
                                          bool* aCantHandleYet) const = 0;
    //XXX probably better named "ApplyConstraints" or "Discrminiate" or something

    /**
     * Add another node as a child of this node.
     * @param aNode the node to add.
     * @return NS_OK if no errors occur.
     */
    nsresult AddChild(ReteNode* aNode) { return mKids.Add(aNode); }

    /**
     * Remove all the children of this node
     * @return NS_OK if no errors occur.
     */
    nsresult RemoveAllChildren() { return mKids.Clear(); }

protected:
    TestNode* mParent;
    ReteNodeSet mKids;
};

#endif // nsRuleNetwork_h__

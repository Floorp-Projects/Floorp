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
#include "nsIContent.h"
#include "plhash.h"
#include "pldhash.h"
#include "nsCRT.h"

class nsIRDFResource;
class nsIRDFNode;

//----------------------------------------------------------------------

/**
 * A type-safe value that can be bound to a variable in the rule
 * network.
 */
class Value {
public:
    enum Type { eUndefined, eISupports, eString, eInteger };

protected:
    Type mType;

    union {
        nsISupports* mISupports;
        PRUnichar*   mString;
        PRInt32      mInteger;
    };

    PRBool Equals(const Value& aValue) const;
    PRBool Equals(nsISupports* aISupports) const;
    PRBool Equals(const PRUnichar* aString) const;
    PRBool Equals(PRInt32 aInteger) const;

    void Clear();

public:
    Value() : mType(eUndefined) {
        MOZ_COUNT_CTOR(Value); }

    Value(const Value& aValue);
    Value(nsISupports* aISupports);
    Value(const PRUnichar* aString);
    Value(PRInt32 aInteger);

    Value& operator=(const Value& aValue);
    Value& operator=(nsISupports* aISupports);
    Value& operator=(const PRUnichar* aString);
    Value& operator=(PRInt32 aInteger);

    ~Value();

    PRBool operator==(const Value& aValue) const { return Equals(aValue); }
    PRBool operator==(nsISupports* aISupports) const { return Equals(aISupports); }
    PRBool operator==(const PRUnichar* aString) const { return Equals(aString); }
    PRBool operator==(PRInt32 aInteger) const { return Equals(aInteger); }

    PRBool operator!=(const Value& aValue) const { return !Equals(aValue); }
    PRBool operator!=(nsISupports* aISupports) const { return !Equals(aISupports); }
    PRBool operator!=(const PRUnichar* aString) const { return !Equals(aString); }
    PRBool operator!=(PRInt32 aInteger) const { return !Equals(aInteger); }

    /**
     * Get the value's type
     * @return the value's type
     */
    Type GetType() const { return mType; }

    /**
     * Treat the Value as an nsISupports. (Note that the result
     * is _not_ addref'd.)
     * @return the value as an nsISupports, or null if the value is
     *   not an nsISupports.
     */
    operator nsISupports*() const;

    /**
     * Treat the value as a Unicode string.
     * @return the value as a Unicode string, or null if the value
     *   is not a Unicode string.
     */
    operator const PRUnichar*() const;

    /**
     * Treat the value as an integer.
     * @return the value as an integer, or zero if the value is
     *   not an integer
     */
    operator PRInt32() const;

    PLHashNumber Hash() const;

#ifdef DEBUG
    void ToCString(nsACString& aResult);
#endif
};

#ifdef DEBUG
extern nsISupports*
value_to_isupports(const nsIID& aIID, const Value& aValue);

#  define VALUE_TO_ISUPPORTS(type, v) \
        NS_STATIC_CAST(type*, value_to_isupports(NS_GET_IID(type), (v)))
#else
#  define VALUE_TO_ISUPPORTS(type, v) \
        NS_STATIC_CAST(type*, NS_STATIC_CAST(nsISupports*, (v)))
#endif

// Convenience wrappers for |Value::operator nsISupports*()|. In a
// debug build, they expand to versions that will call QI() and verify
// that the types are kosher. In an optimized build, they'll just cast
// n' go. Rock on!
#define VALUE_TO_IRDFRESOURCE(v) VALUE_TO_ISUPPORTS(nsIRDFResource, (v))
#define VALUE_TO_IRDFNODE(v)     VALUE_TO_ISUPPORTS(nsIRDFNode, (v))
#define VALUE_TO_ICONTENT(v)     VALUE_TO_ISUPPORTS(nsIContent, (v))

//----------------------------------------------------------------------

/**
 * A set of variables
 */
class VariableSet
{
public:
    VariableSet();
    ~VariableSet();

    /**
     * Add a variable to the set
     * @param aVariable the variable to add
     * @returns NS_OK, unless something went wrong.
     */
    nsresult Add(PRInt32 aVariable);

    /**
     * Remove a variable from the set
     * @param aVariable the variable to remove
     * @returns NS_OK, unless something went wrong.
     */
    nsresult Remove(PRInt32 aVariable);

    /**
     * Determine if the set contains a variable
     * @param aVariable the variable to test
     * @return PR_TRUE if the set contains the variable, PR_FALSE otherwise.
     */
    PRBool Contains(PRInt32 aVariable) const;

    /**
     * Determine the number of variables in the set
     * @return the number of variables in the set
     */
    PRInt32 GetCount() const { return mCount; }

    /**
     * Get the <i>i</i>th variable in the set
     * @param aIndex the index to retrieve
     * @return the <i>i</i>th variable in the set, or -1 if no such
     *   variable exists.
     */
    PRInt32 GetVariableAt(PRInt32 aIndex) const { return mVariables[aIndex]; }

protected:
    PRInt32* mVariables;
    PRInt32 mCount;
    PRInt32 mCapacity;
};

//----------------------------------------------------------------------

/**
 * A memory element that supports an instantiation
 */
class MemoryElement {
public:
    MemoryElement() {}
    virtual ~MemoryElement() {}

    virtual const char* Type() const = 0;
    virtual PLHashNumber Hash() const = 0;
    virtual PRBool Equals(const MemoryElement& aElement) const = 0;
    virtual MemoryElement* Clone(void* aPool) const = 0;

    PRBool operator==(const MemoryElement& aMemoryElement) const {
        return Equals(aMemoryElement);
    }

    PRBool operator!=(const MemoryElement& aMemoryElement) const {
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
            delete mElement;
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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
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
    PRInt32 mVariable;
    Value   mValue;

    nsAssignment() : mVariable(-1), mValue()
        { MOZ_COUNT_CTOR(nsAssignment); }

    nsAssignment(PRInt32 aVariable, const Value& aValue)
        : mVariable(aVariable),
          mValue(aValue)
        { MOZ_COUNT_CTOR(nsAssignment); }

    nsAssignment(const nsAssignment& aAssignment)
        : mVariable(aAssignment.mVariable),
          mValue(aAssignment.mValue)
        { MOZ_COUNT_CTOR(nsAssignment); }

    ~nsAssignment() { MOZ_COUNT_DTOR(nsAssignment); }

    nsAssignment& operator=(const nsAssignment& aAssignment) {
        mVariable = aAssignment.mVariable;
        mValue    = aAssignment.mValue;
        return *this; }

    PRBool operator==(const nsAssignment& aAssignment) const {
        return mVariable == aAssignment.mVariable && mValue == aAssignment.mValue; }

    PRBool operator!=(const nsAssignment& aAssignment) const {
        return mVariable != aAssignment.mVariable || mValue != aAssignment.mValue; }

    PLHashNumber Hash() const {
        // XXX I have no idea if this hashing function is good or not
        return (mValue.Hash() & 0xffff) | (mVariable << 16); }
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
        List() { MOZ_COUNT_CTOR(nsAssignmentSet::List); }

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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
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
     * @return PR_TRUE if aVariable is bound to aValue; PR_FALSE otherwise.
     */
    PRBool HasAssignment(PRInt32 aVariable, const Value& aValue) const;

    /**
     * Determine if the assignment set contains the specified assignment
     * @param aAssignment the assignment to search for
     * @return PR_TRUE if the set contains the assignment, PR_FALSE otherwise.
     */
    PRBool HasAssignment(const nsAssignment& aAssignment) const {
        return HasAssignment(aAssignment.mVariable, aAssignment.mValue); }

    /**
     * Determine whether the assignment set has an assignment for the
     * specified variable.
     * @param aVariable the variable to query
     * @return PR_TRUE if the assignment set has an assignment for the variable,
     *   PR_FALSE otherwise.
     */
    PRBool HasAssignmentFor(PRInt32 aVariable) const;

    /**
     * Retrieve the assignment for the specified variable
     * @param aVariable the variable to query
     * @param aValue an out parameter that will receive the value assigned
     *   to the variable, if any.
     * @return PR_TRUE if the variable has an assignment, PR_FALSE
     *   if there was no assignment for the variable.
     */
    PRBool GetAssignmentFor(PRInt32 aVariable, Value* aValue) const;

    /**
     * Count the number of assignments in the set
     * @return the number of assignments in the set
     */
    PRInt32 Count() const;

    /**
     * Determine if the set is empty
     * @return PR_TRUE if the assignment set is empty, PR_FALSE otherwise.
     */
    PRBool IsEmpty() const { return mAssignments == nsnull; }

    PRBool Equals(const nsAssignmentSet& aSet) const;
    PRBool operator==(const nsAssignmentSet& aSet) const { return Equals(aSet); }
    PRBool operator!=(const nsAssignmentSet& aSet) const { return !Equals(aSet); }
};


//----------------------------------------------------------------------

/**
 * A collection of varible-to-value bindings, with the memory elements
 * that support those bindings.
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
    nsresult AddAssignment(PRInt32 aVariable, const Value& aValue) {
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

    PRBool Equals(const Instantiation& aInstantiation) const {
        return mAssignments == aInstantiation.mAssignments; }

    PRBool operator==(const Instantiation& aInstantiation) const {
        return Equals(aInstantiation); }

    PRBool operator!=(const Instantiation& aInstantiation) const {
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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(NS_CONST_CAST(List*, &mHead)); }

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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

        friend class InstantiationSet;
    };

    Iterator First() { return Iterator(mHead.mNext); }
    Iterator Last() { return Iterator(&mHead); }

    PRBool Empty() const { return First() == Last(); }

    Iterator Append(const Instantiation& aInstantiation) {
        return Insert(Last(), aInstantiation); }

    Iterator Insert(Iterator aBefore, const Instantiation& aInstantiation);

    Iterator Erase(Iterator aElement);

    void Clear();

    PRBool HasAssignmentFor(PRInt32 aVariable) const;

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
     * @param aInstantiations the set of instantiations to propagate
     *   down through the network.
     * @param aClosure any application-specific information that
     *   needs to be passed through the network.
     * @return NS_OK if no errors occurred.
     */
    virtual nsresult Propagate(const InstantiationSet& aInstantiations, void* aClosure) = 0;
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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
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

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    Iterator First() { return Iterator(mNodes); }
    Iterator Last() { return Iterator(mNodes + mCount); }

protected:
    ReteNode** mNodes;
    PRInt32 mCount;
    PRInt32 mCapacity;
};

//----------------------------------------------------------------------

/**
 * An abstract base class for an "inner node" in the rule
 * network. Adds support for children and "upward" queries.
 */
class InnerNode : public ReteNode
{
public:
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
     * @param aClosure application-specific information that needs to
     *   be passed through the network.
     * @return NS_OK if no errors occurred
     */
    virtual nsresult Constrain(InstantiationSet& aInstantiations, void* aClosure) = 0;

    /**
     * Retrieve the set of variables that are introduced by this node
     * and any of its ancestors. To correctly implement this method, a
     * node must add any variables that it introduces to the variable
     * set, and then recursively call GetAncestorVariables() on its
     * parent (or parents).
     *
     * @param aVariables The variable set to which the callee will add
     *   its variables, and its ancestors variables.
     * @return NS_OK if no errors occur.
     */
    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const = 0;

    /**
     * Determine if this node has another node as its direct ancestor.
     * @param aNode the node to look for.
     * @return PR_TRUE if aNode is a direct ancestor of this node, PR_FALSE
     *   otherwise.
     */
    virtual PRBool HasAncestor(const ReteNode* aNode) const = 0;

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
    ReteNodeSet mKids;
};

//----------------------------------------------------------------------

/**
 * The root node in the rule network.
 */
class RootNode : public InnerNode
{
public:
    // "downward" propogations
    virtual nsresult Propagate(const InstantiationSet& aInstantiations, void* aClosure);

    // "upward" propogations
    virtual nsresult Constrain(InstantiationSet& aInstantiations, void* aClosure);

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool HasAncestor(const ReteNode* aNode) const;
};

//----------------------------------------------------------------------

/**
 * A node that joins to paths from the root node, and binds a
 * variable from the left ancestor to a variable in the right
 * ancestor.
 */
class JoinNode : public InnerNode
{
public:
    enum Operator { eEquality };

    JoinNode(InnerNode* aLeftParent,
             PRInt32 aLeftVariable,
             InnerNode* aRightParent,
             PRInt32 aRightVariable,
             Operator aOperator);

    // "downward" propogations
    virtual nsresult Propagate(const InstantiationSet& aInstantiations, void* aClosure);

    // "upward" propogations
    virtual nsresult Constrain(InstantiationSet& aInstantiations, void* aClosure);

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool HasAncestor(const ReteNode* aNode) const;

protected:
    InnerNode* mLeftParent;
    PRInt32 mLeftVariable;
    InnerNode* mRightParent;
    PRInt32 mRightVariable;
    Operator mOperator;

    static nsresult GetNumBound(InnerNode* aAncestor, const InstantiationSet& aInstantiations, PRInt32* aBoundCount);

    nsresult Bind(InstantiationSet& aInstantiations, PRBool* aDidBind);
};


//----------------------------------------------------------------------

/**
 * A node that applies a test condition to a set of instantiations.
 *
 * This class provides implementations of Propagate() and Constrain()
 * in terms of one simple operation, FilterInstantiations(). A node
 * that is a "simple test node" in a rule network should derive from
 * this class, and need only implement FilterInstantiations() and
 * GetAncestorVariables().
 */
class TestNode : public InnerNode
{
public:
    TestNode(InnerNode* aParent);

    /**
     * Retrieve the test node's parent
     * @return the test node's parent
     */
    InnerNode* GetParent() const { return mParent; }

    /**
     * Calls FilterInstantiations() on the instantiation set, and if
     * the resulting set isn't empty, propagates the new set down to
     * each of the test node's children.
     */
    virtual nsresult Propagate(const InstantiationSet& aInstantiations, void* aClosure);

    /**
     * Calls FilterInstantiations() on the instantiation set, and if
     * the resulting set isn't empty, propagates the new set up to the
     * test node's parent.
     */
    virtual nsresult Constrain(InstantiationSet& aInstantiations, void* aClosure);

    /**
     * Given a set of instantiations, filter out any that are
     * inconsistent with the test node's test, and append
     * variable-to-value assignments and memory element support for
     * those which do pass the test node's test.
     *
     * @param aInstantiations the set of instantiations to be
     *   filtered
     * @param aClosure application-specific data that is to be passed
     *   through the network.
     * @return NS_OK if no errors occurred.
     */
    virtual nsresult FilterInstantiations(InstantiationSet& aInstantiations, void* aClosure) const = 0; //XXX probably better named "ApplyConstraints" or "Discrminiate" or something

    virtual nsresult GetAncestorVariables(VariableSet& aVariables) const;

    virtual PRBool HasAncestor(const ReteNode* aNode) const;

protected:
    InnerNode* mParent;
};

//----------------------------------------------------------------------

class nsRuleNetwork
{
public:
    struct SymtabEntry {
        PLDHashEntryHdr mHdr;
        PRUnichar*      mSymbol;
        PRInt32         mVariable;
    };
    
    nsRuleNetwork() { Init(); }
    ~nsRuleNetwork() { Finish(); }

    /**
     * Remove all the nodes from the network. The nodes will be 
     * destroyed
     * @return NS_OK if no errors occur
     */ 
    void Clear() { Finish(); Init(); }

    /**
     * Add a node to the network. The network assumes ownership of the
     * node; it will be destroyed when the network is destroyed, or if
     * Clear() is called.
     *
     * @param aNode the node to add to the network
     * @return NS_OK if no errors occur
     */
    nsresult AddNode(ReteNode* aNode) { return mNodes.Add(aNode); }

    /**
     * Retrieve the root node in the rule network
     * @return the root node in the rule network
     */
    RootNode* GetRoot() { return &mRoot; };

    /**
     * Create an unnamed variable
     */
    PRInt32 CreateAnonymousVariable() { return ++mNextVariable; }

    /**
     * Assign a symbol to a variable
     */
    void PutSymbol(const PRUnichar* aSymbol, PRInt32 aVariable) {
        NS_PRECONDITION(LookupSymbol(aSymbol) == 0, "symbol already defined");

        SymtabEntry* entry =
            NS_REINTERPRET_CAST(SymtabEntry*,
                                PL_DHashTableOperate(&mSymtab,
                                                     aSymbol,
                                                     PL_DHASH_ADD));

        if (entry) {
            entry->mSymbol   = nsCRT::strdup(aSymbol);
            entry->mVariable = aVariable;
        } };
                                
    /**
     * Lookup the variable associated with the symbol
     */
    PRInt32 LookupSymbol(const PRUnichar* aSymbol, PRBool aCreate = PR_FALSE) {
        SymtabEntry* entry =
            NS_REINTERPRET_CAST(SymtabEntry*,
                                PL_DHashTableOperate(&mSymtab,
                                                     aSymbol,
                                                     PL_DHASH_LOOKUP));

        if (PL_DHASH_ENTRY_IS_BUSY(&entry->mHdr))
            return entry->mVariable;

        PRInt32 result = 0;
        if (aCreate) {
            result = CreateAnonymousVariable();
            PutSymbol(aSymbol, result);
        }

        return result; }

protected:
    /**
     * The root node in the network
     */
    RootNode mRoot;

    /**
     * Other nodes in the network
     */
    ReteNodeSet mNodes;

    void Init();
    void Finish();

    /**
     * Symbol table, mapping symbolic names to variable identifiers
     */
    PLDHashTable mSymtab;

    /**
     * The next available variable identifier
     */
    PRInt32 mNextVariable;

    static PLDHashTableOps gOps;
};



#endif // nsRuleNetwork_h__

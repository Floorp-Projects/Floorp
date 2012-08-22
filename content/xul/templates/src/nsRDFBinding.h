/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFBinding_h__
#define nsRDFBinding_h__

#include "nsAutoPtr.h"
#include "nsIAtom.h"
#include "nsIRDFResource.h"
#include "nsISupportsImpl.h"

class nsXULTemplateResultRDF;
class nsBindingValues;

/*
 * Classes related to storing bindings for RDF handling.
 */

/*
 * a  <binding> descriptors
 */
class RDFBinding {

public:

    nsCOMPtr<nsIAtom>        mSubjectVariable;
    nsCOMPtr<nsIRDFResource> mPredicate;
    nsCOMPtr<nsIAtom>        mTargetVariable;

    // indicates whether a binding is dependant on the result from a
    // previous binding
    bool                     mHasDependency;

    RDFBinding*              mNext;

private:

    friend class RDFBindingSet;

    RDFBinding(nsIAtom* aSubjectVariable,
               nsIRDFResource* aPredicate,
               nsIAtom* aTargetVariable)
      : mSubjectVariable(aSubjectVariable),
        mPredicate(aPredicate),
        mTargetVariable(aTargetVariable),
        mHasDependency(false),
        mNext(nullptr)
    {
        MOZ_COUNT_CTOR(RDFBinding);
    }

    ~RDFBinding()
    {
        MOZ_COUNT_DTOR(RDFBinding);
    }
};

/*
 * a collection of <binding> descriptors. This object is refcounted by
 * nsBindingValues objects and the query processor.
 */
class RDFBindingSet
{
protected:

    // the number of bindings
    int32_t mCount;

    // pointer to the first binding in a linked list
    RDFBinding* mFirst;

public:

    RDFBindingSet()
        : mCount(0),
          mFirst(nullptr)
    {
        MOZ_COUNT_CTOR(RDFBindingSet);
    }

    ~RDFBindingSet();

    NS_INLINE_DECL_REFCOUNTING(RDFBindingSet)

    int32_t Count() const { return mCount; }

    /*
     * Add a binding (aRef -> aPredicate -> aVar) to the set
     */
    nsresult
    AddBinding(nsIAtom* aVar, nsIAtom* aRef, nsIRDFResource* aPredicate);

    /*
     * Return true if the binding set contains a binding which would cause
     * the result to need resynchronizing for an RDF triple. The member
     * variable may be supplied as an optimization since bindings most
     * commonly use the member variable as the subject. If aMemberVariable
     * is set, aSubject must be the value of the member variable for the
     * result. The supplied binding values aBindingValues must be values
     * using this binding set (that is aBindingValues->GetBindingSet() == this)
     *
     * @param aSubject subject of the RDF triple
     * @param aPredicate predicate of the RDF triple
     * @param aTarget target of the RDF triple
     * @param aMemberVariable member variable for the query for the binding
     * @param aResult result to synchronize
     * @param aBindingValues the values for the bindings for the result
     */
    bool
    SyncAssignments(nsIRDFResource* aSubject,
                    nsIRDFResource* aPredicate,
                    nsIRDFNode* aTarget,
                    nsIAtom* aMemberVariable,
                    nsXULTemplateResultRDF* aResult,
                    nsBindingValues& aBindingValues);

    /*
     * The query processor maintains a map of subjects to an array of results.
     * This is used such that when a new assertion is added to the RDF graph,
     * the results associated with the subject of that triple may be checked
     * to see if their bindings have changed. The AddDependencies method adds
     * these subject dependencies to the map.
     */
    void
    AddDependencies(nsIRDFResource* aSubject,
                    nsXULTemplateResultRDF* aResult);

    /*
     * Remove the results from the dependencies map when results are deleted.
     */
    void
    RemoveDependencies(nsIRDFResource* aSubject,
                       nsXULTemplateResultRDF* aResult);

    /*
     * The nsBindingValues classes stores an array of values, one for each
     * target symbol that could be set by the bindings in the set.
     * LookupTargetIndex determines the index into the array for a given
     * target symbol.
     */
    int32_t
    LookupTargetIndex(nsIAtom* aTargetVariable, RDFBinding** aBinding);
};

/*
 * A set of values of bindings. This object is used once per result.
 * This stores a reference to the binding set and an array of node values.
 * Since the binding set is used once per query and the values are
 * used once per result, we reduce size by only storing the value array's
 * length in the binding set. This is possible since the array is always
 * a fixed length for a particular binding set.
 *
 * XXX ndeakin We may want to revisit this later since it makes the code
 *             more complicated.
 */
class nsBindingValues
{
protected:

    // the binding set
    nsRefPtr<RDFBindingSet> mBindings;

    /*
     * A set of values for variable bindings. To look up a binding value,
     * scan through the binding set in mBindings for the right target atom.
     * Its index will correspond to the index in this array. The size of this
     * array is determined by the RDFBindingSet's Count().
     */
    nsCOMPtr<nsIRDFNode>* mValues;

public:

    nsBindingValues()
      : mBindings(nullptr),
        mValues(nullptr)
    {
        MOZ_COUNT_CTOR(nsBindingValues);
    }

    ~nsBindingValues();


    /**
     * Clear the binding set, to be called when the nsBindingValues is deleted
     * or a new binding set is being set.
     */
    void ClearBindingSet();

    RDFBindingSet* GetBindingSet() { return mBindings; }

    /**
     * Set the binding set to use. This needs to be called once a rule matches
     * since it is then known which bindings will apply.
     */
    nsresult SetBindingSet(RDFBindingSet* aBindings);

    nsCOMPtr<nsIRDFNode>* ValuesArray() { return mValues; }

    /*
     * Retrieve the assignment for a particular variable
     */
    void
    GetAssignmentFor(nsXULTemplateResultRDF* aResult,
                     nsIAtom* aVar,
                     nsIRDFNode** aValue);

    /*
     * Remove depenedencies the bindings have on particular resources
     */
    void
    RemoveDependencies(nsIRDFResource* aSubject,
                       nsXULTemplateResultRDF* aResult);
};

#endif // nsRDFBinding_h__

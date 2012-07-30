/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsXULTemplateResultRDF.h"
#include "nsRDFBinding.h"

#ifdef DEBUG
#include "nsXULContentUtils.h"
#endif

RDFBindingSet::~RDFBindingSet()
{
    while (mFirst) {
        RDFBinding* doomed = mFirst;
        mFirst = mFirst->mNext;
        delete doomed;
    }

    MOZ_COUNT_DTOR(RDFBindingSet);
}

nsresult
RDFBindingSet::AddBinding(nsIAtom* aVar, nsIAtom* aRef, nsIRDFResource* aPredicate)
{
    RDFBinding* newbinding = new RDFBinding(aRef, aPredicate, aVar);
    if (! newbinding)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mFirst) {
        RDFBinding* binding = mFirst;

        while (binding) { 
            // the binding is dependant on the calculation of a previous binding
            if (binding->mSubjectVariable == aVar)
                newbinding->mHasDependency = true;

            // if the target variable is already used in a binding, ignore it
            // since it won't be useful for anything
            if (binding->mTargetVariable == aVar) {
                delete newbinding;
                return NS_OK;
            }

            // add the binding at the end of the list
            if (! binding->mNext) {
                binding->mNext = newbinding;
                break;
            }

            binding = binding->mNext;
        }
    }
    else {
        mFirst = newbinding;
    }

    mCount++;

    return NS_OK;
}

bool
RDFBindingSet::SyncAssignments(nsIRDFResource* aSubject,
                               nsIRDFResource* aPredicate,
                               nsIRDFNode* aTarget,
                               nsIAtom* aMemberVariable,
                               nsXULTemplateResultRDF* aResult,
                               nsBindingValues& aBindingValues)
{
    NS_ASSERTION(aBindingValues.GetBindingSet() == this,
                 "nsBindingValues not for this RDFBindingSet");
    NS_PRECONDITION(aResult, "Must have result");

    bool needSync = false;
    nsCOMPtr<nsIRDFNode>* valuesArray = aBindingValues.ValuesArray();
    if (!valuesArray)
        return false;

    RDFBinding* binding = mFirst;
    PRInt32 count = 0;

    // QI for proper comparisons just to be safe
    nsCOMPtr<nsIRDFNode> subjectnode = do_QueryInterface(aSubject);

    // iterate through the bindings looking for ones that would match the RDF
    // nodes that were involved in a change
    nsCOMPtr<nsIRDFNode> value;
    while (binding) {
        if (aPredicate == binding->mPredicate) {
            // if the source of the binding is the member variable, optimize
            if (binding->mSubjectVariable == aMemberVariable) {
                valuesArray[count] = aTarget;
                needSync = true;
            }
            else {
                aResult->GetAssignment(binding->mSubjectVariable, getter_AddRefs(value));
                if (value == subjectnode) {
                    valuesArray[count] = aTarget;
                    needSync = true;
                }
            }
        }

        binding = binding->mNext;
        count++;
    }

    return needSync;
}

void
RDFBindingSet::AddDependencies(nsIRDFResource* aSubject,
                               nsXULTemplateResultRDF* aResult)
{
    NS_PRECONDITION(aResult, "Must have result");

    // iterate through the bindings and add binding dependencies to the
    // processor

    nsXULTemplateQueryProcessorRDF* processor = aResult->GetProcessor();
    if (! processor)
        return;

    nsCOMPtr<nsIRDFNode> value;

    RDFBinding* binding = mFirst;
    while (binding) {
        aResult->GetAssignment(binding->mSubjectVariable, getter_AddRefs(value));

        nsCOMPtr<nsIRDFResource> valueres = do_QueryInterface(value);
        if (valueres)
            processor->AddBindingDependency(aResult, valueres);

        binding = binding->mNext;
    }
}

void
RDFBindingSet::RemoveDependencies(nsIRDFResource* aSubject,
                                  nsXULTemplateResultRDF* aResult)
{
    NS_PRECONDITION(aResult, "Must have result");

    // iterate through the bindings and remove binding dependencies from the
    // processor

    nsXULTemplateQueryProcessorRDF* processor = aResult->GetProcessor();
    if (! processor)
        return;

    nsCOMPtr<nsIRDFNode> value;

    RDFBinding* binding = mFirst;
    while (binding) {
        aResult->GetAssignment(binding->mSubjectVariable, getter_AddRefs(value));

        nsCOMPtr<nsIRDFResource> valueres = do_QueryInterface(value);
        if (valueres)
            processor->RemoveBindingDependency(aResult, valueres);

        binding = binding->mNext;
    }
}

PRInt32
RDFBindingSet::LookupTargetIndex(nsIAtom* aTargetVariable, RDFBinding** aBinding)
{
    PRInt32 idx = 0;
    RDFBinding* binding = mFirst;

    while (binding) {
        if (binding->mTargetVariable == aTargetVariable) {
            *aBinding = binding;
            return idx;
        }
        idx++;
        binding = binding->mNext;
    }

    return -1;
}

nsBindingValues::~nsBindingValues()
{
    ClearBindingSet();
    MOZ_COUNT_DTOR(nsBindingValues);
}

void
nsBindingValues::ClearBindingSet()
{
    if (mBindings && mValues) {
        delete [] mValues;
        mValues = nullptr;
    }

    mBindings = nullptr;
}

nsresult
nsBindingValues::SetBindingSet(RDFBindingSet* aBindings)
{
    ClearBindingSet();

    PRInt32 count = aBindings->Count();
    if (count) {
        mValues = new nsCOMPtr<nsIRDFNode>[count];
        if (!mValues)
            return NS_ERROR_OUT_OF_MEMORY;

        mBindings = aBindings;
    }
    else {
        mValues = nullptr;
    }

    return NS_OK;
}

void
nsBindingValues::GetAssignmentFor(nsXULTemplateResultRDF* aResult,
                                  nsIAtom* aVar,
                                  nsIRDFNode** aValue)
{
    *aValue = nullptr;

    // assignments are calculated lazily when asked for. The only issue is
    // when a binding has no value in the RDF graph, it will be checked again
    // every time.

    if (mBindings && mValues) {
        RDFBinding* binding;
        PRInt32 idx = mBindings->LookupTargetIndex(aVar, &binding);
        if (idx >= 0) {
            *aValue = mValues[idx];
            if (*aValue) {
                NS_ADDREF(*aValue);
            }
            else {
                nsXULTemplateQueryProcessorRDF* processor = aResult->GetProcessor();
                if (! processor)
                    return;

                nsIRDFDataSource* ds = processor->GetDataSource();
                if (! ds)
                    return;

                nsCOMPtr<nsIRDFNode> subjectValue;
                aResult->GetAssignment(binding->mSubjectVariable,
                                       getter_AddRefs(subjectValue));
                if (subjectValue) {
                    nsCOMPtr<nsIRDFResource> subject = do_QueryInterface(subjectValue);
                    ds->GetTarget(subject, binding->mPredicate, true, aValue);
                    if (*aValue)
                        mValues[idx] = *aValue;
                }
            }
        }
    }
}

void
nsBindingValues::RemoveDependencies(nsIRDFResource* aSubject,
                                    nsXULTemplateResultRDF* aResult)
{
    if (mBindings)
        mBindings->RemoveDependencies(aSubject, aResult);
}

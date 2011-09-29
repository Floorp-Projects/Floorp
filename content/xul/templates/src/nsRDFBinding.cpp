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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
                newbinding->mHasDependency = PR_TRUE;

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
        return PR_FALSE;

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
                needSync = PR_TRUE;
            }
            else {
                aResult->GetAssignment(binding->mSubjectVariable, getter_AddRefs(value));
                if (value == subjectnode) {
                    valuesArray[count] = aTarget;
                    needSync = PR_TRUE;
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
        mValues = nsnull;
    }

    mBindings = nsnull;
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
        mValues = nsnull;
    }

    return NS_OK;
}

void
nsBindingValues::GetAssignmentFor(nsXULTemplateResultRDF* aResult,
                                  nsIAtom* aVar,
                                  nsIRDFNode** aValue)
{
    *aValue = nsnull;

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
                    ds->GetTarget(subject, binding->mPredicate, PR_TRUE, aValue);
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

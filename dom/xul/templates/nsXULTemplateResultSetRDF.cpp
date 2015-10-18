/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTemplateResultSetRDF.h"
#include "nsXULTemplateQueryProcessorRDF.h"

NS_IMPL_ISUPPORTS(nsXULTemplateResultSetRDF, nsISimpleEnumerator)

NS_IMETHODIMP
nsXULTemplateResultSetRDF::HasMoreElements(bool *aResult)
{
    *aResult = true;

    nsCOMPtr<nsIRDFNode> node;

    if (! mInstantiations || ! mQuery) {
        *aResult = false;
        return NS_OK;
    }

    if (mCheckedNext) {
        if (!mCurrent || mCurrent == &(mInstantiations->mHead))
            *aResult = false;
        return NS_OK;
    }

    mCheckedNext = true;
                
    do {
        if (mCurrent) {
            mCurrent = mCurrent->mNext;
            if (mCurrent == &(mInstantiations->mHead)) {
                *aResult = false;
                return NS_OK;
            }
        }
        else {
            *aResult = ! mInstantiations->Empty();
            if (*aResult)
                mCurrent = mInstantiations->mHead.mNext;
        }

        // get the value of the member variable. If it is not set, skip
        // the result and move on to the next result
        if (mCurrent) {
            mCurrent->mInstantiation.mAssignments.
                GetAssignmentFor(mQuery->mMemberVariable, getter_AddRefs(node));
        }

        // only resources may be used as results
        mResource = do_QueryInterface(node);
    } while (! mResource);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultSetRDF::GetNext(nsISupports **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mCurrent || !mCheckedNext)
        return NS_ERROR_FAILURE;

    RefPtr<nsXULTemplateResultRDF> nextresult =
        new nsXULTemplateResultRDF(mQuery, mCurrent->mInstantiation, mResource);
    if (!nextresult)
        return NS_ERROR_OUT_OF_MEMORY;

    // add the supporting memory elements to the processor's map. These are
    // used to remove the results when an assertion is removed from the graph
    mProcessor->AddMemoryElements(mCurrent->mInstantiation, nextresult);

    mCheckedNext = false;

    nextresult.forget(aResult);

    return NS_OK;
}

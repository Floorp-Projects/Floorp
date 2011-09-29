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

#include "nsXULTemplateResultSetRDF.h"
#include "nsXULTemplateQueryProcessorRDF.h"

NS_IMPL_ISUPPORTS1(nsXULTemplateResultSetRDF, nsISimpleEnumerator)

NS_IMETHODIMP
nsXULTemplateResultSetRDF::HasMoreElements(bool *aResult)
{
    *aResult = PR_TRUE;

    nsCOMPtr<nsIRDFNode> node;

    if (! mInstantiations || ! mQuery) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    if (mCheckedNext) {
        if (!mCurrent || mCurrent == &(mInstantiations->mHead))
            *aResult = PR_FALSE;
        return NS_OK;
    }

    mCheckedNext = PR_TRUE;
                
    do {
        if (mCurrent) {
            mCurrent = mCurrent->mNext;
            if (mCurrent == &(mInstantiations->mHead)) {
                *aResult = PR_FALSE;
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

    nsRefPtr<nsXULTemplateResultRDF> nextresult =
        new nsXULTemplateResultRDF(mQuery, mCurrent->mInstantiation, mResource);
    if (!nextresult)
        return NS_ERROR_OUT_OF_MEMORY;

    // add the supporting memory elements to the processor's map. These are
    // used to remove the results when an assertion is removed from the graph
    mProcessor->AddMemoryElements(mCurrent->mInstantiation, nextresult);

    mCheckedNext = PR_FALSE;

    *aResult = nextresult;
    NS_ADDREF(*aResult);

    return NS_OK;
}

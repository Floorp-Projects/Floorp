/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsString.h"
#include "nsXULTemplateResultStorage.h"

NS_IMPL_ISUPPORTS1(nsXULTemplateResultStorage, nsIXULTemplateResult)

nsXULTemplateResultStorage::nsXULTemplateResultStorage(nsXULTemplateResultSetStorage* aResultSet)
{
    static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
    nsCOMPtr<nsIRDFService> rdfService = do_GetService(kRDFServiceCID);
    rdfService->GetAnonymousResource(getter_AddRefs(mNode));
    mResultSet = aResultSet;
    if (aResultSet) {
        mResultSet->FillColumnValues(mValues);
    }
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetIsContainer(bool* aIsContainer)
{
    *aIsContainer = false;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetIsEmpty(bool* aIsEmpty)
{
    *aIsEmpty = true;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetMayProcessChildren(bool* aMayProcessChildren)
{
    *aMayProcessChildren = false;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetId(nsAString& aId)
{
    const char* uri = nullptr;
    mNode->GetValueConst(&uri);

    aId.Assign(NS_ConvertUTF8toUTF16(uri));

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetResource(nsIRDFResource** aResource)
{
    *aResource = mNode;
    NS_IF_ADDREF(*aResource);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetType(nsAString& aType)
{
    aType.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateResultStorage::GetBindingFor(nsIAtom* aVar, nsAString& aValue)
{
    NS_ENSURE_ARG_POINTER(aVar);

    aValue.Truncate();
    if (!mResultSet) {
        return NS_OK;
    }

    int32_t idx = mResultSet->GetColumnIndex(aVar);
    if (idx < 0) {
        return NS_OK;
    }

    nsIVariant * value = mValues[idx];
    if (value) {
        value->GetAsAString(aValue);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::GetBindingObjectFor(nsIAtom* aVar, nsISupports** aValue)
{
    NS_ENSURE_ARG_POINTER(aVar);

    if (mResultSet) {
        int32_t idx = mResultSet->GetColumnIndex(aVar);
        if (idx >= 0) {
            *aValue = mValues[idx];
            NS_IF_ADDREF(*aValue);
            return NS_OK;
        }
    }
    *aValue = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::RuleMatched(nsISupports* aQuery, nsIDOMNode* aRuleNode)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultStorage::HasBeenRemoved()
{
    return NS_OK;
}

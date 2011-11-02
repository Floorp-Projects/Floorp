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
 *     Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
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

#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsString.h"
#include "nsXULTemplateResultStorage.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS1(nsXULTemplateResultStorage, nsIXULTemplateResult)

nsXULTemplateResultStorage::nsXULTemplateResultStorage(nsXULTemplateResultSetStorage* aResultSet)
{
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
    const char* uri = nsnull;
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

    PRInt32 idx = mResultSet->GetColumnIndex(aVar);
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
        PRInt32 idx = mResultSet->GetColumnIndex(aVar);
        if (idx >= 0) {
            *aValue = mValues[idx];
            NS_IF_ADDREF(*aValue);
            return NS_OK;
        }
    }
    *aValue = nsnull;
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

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Mitch Stoltz <mstoltz@netscape.com>
 */

/*describes principals which combine one or more principals*/
#include "nsAggregatePrincipal.h"

static NS_DEFINE_IID(kICertificatePrincipalIID, NS_ICERTIFICATEPRINCIPAL_IID);

NS_IMPL_QUERY_INTERFACE4(nsAggregatePrincipal, nsIAggregatePrincipal, 
                         nsICertificatePrincipal, nsICodebasePrincipal, nsIPrincipal)

NSBASEPRINCIPALS_ADDREF(nsAggregatePrincipal);
NSBASEPRINCIPALS_RELEASE(nsAggregatePrincipal);

////////////////////////////////////////////////
// Methods implementing nsIAggregatePrincipal //
////////////////////////////////////////////////
NS_IMETHODIMP 
nsAggregatePrincipal::GetCertificate(nsIPrincipal** result)
{
    *result = mCertificatePrincipal;
    NS_IF_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP 
nsAggregatePrincipal::GetCodebase(nsIPrincipal** result)
{
    *result = mCodebasePrincipal;
    NS_IF_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP 
nsAggregatePrincipal::SetCertificate(nsIPrincipal* aCertificate)
{
    if(!aCertificate)
    {
        mCertificate = nsnull;
        mCertificatePrincipal = nsnull;
        mMasterPrincipal = mCodebasePrincipal;
        return NS_OK;
    }
    if (NS_FAILED(aCertificate->QueryInterface(NS_GET_IID(nsICertificatePrincipal),
                                               (void**)&mCertificate)))
        return NS_ERROR_FAILURE;
    mCertificatePrincipal = aCertificate;
    mMasterPrincipal = aCertificate;
    return NS_OK;
}

NS_IMETHODIMP 
nsAggregatePrincipal::SetCodebase(nsIPrincipal* aCodebase)
{
    if(!aCodebase)
    {
        mCodebase = nsnull;
        mCodebasePrincipal = nsnull;
        return NS_OK;
    }
    if (NS_FAILED(aCodebase->QueryInterface(NS_GET_IID(nsICodebasePrincipal),
                                               (void**)&mCodebase)))
        return NS_ERROR_FAILURE;
    mCodebasePrincipal = aCodebase;
    if (!mCertificatePrincipal)
        mMasterPrincipal = aCodebase;
    return NS_OK;
}

NS_IMETHODIMP 
nsAggregatePrincipal::Intersect(nsIPrincipal* other)
{
    NS_ASSERTION(mCodebase, "Principal without codebase");

    PRBool sameCert = PR_FALSE;
    if (NS_FAILED(mCertificatePrincipal->Equals(other, &sameCert)))
        return NS_ERROR_FAILURE;
    if (!sameCert)
        SetCertificate(nsnull);
    return NS_OK;
}

///////////////////////////////////////
// Methods implementing nsIPrincipal //
///////////////////////////////////////
NS_IMETHODIMP 
nsAggregatePrincipal::ToString(char **result)
{
    return mMasterPrincipal->ToString(result);
}

NS_IMETHODIMP 
nsAggregatePrincipal::ToUserVisibleString(char **result)
{
    return mMasterPrincipal->ToUserVisibleString(result);
}

NS_IMETHODIMP
nsAggregatePrincipal::Equals(nsIPrincipal * other, PRBool * result)
{
    if (this == other) {
        *result = PR_TRUE;
        return NS_OK;
    }
    if (!other) {
        *result = PR_FALSE;
        return NS_OK;
    }
    nsresult rv;
    nsCOMPtr<nsIAggregatePrincipal> otherAgg = 
        do_QueryInterface(other, &rv);
    if (NS_FAILED(rv))
    {
        *result = PR_FALSE;
        return NS_OK;
    }
    //-- Two aggregates are equal if both codebase and certificate are equal
    *result = PR_FALSE;
    PRBool certEqual, cbEqual;
    rv = mCertificatePrincipal->Equals(other, &certEqual);
    if(NS_FAILED(rv)) return rv;
    rv = mCodebasePrincipal->Equals(other, &cbEqual);
    if(NS_FAILED(rv)) return rv;
    *result = certEqual && cbEqual;
    return NS_OK;
}

NS_IMETHODIMP
nsAggregatePrincipal::HashValue(PRUint32 *result)
{
    return mMasterPrincipal->HashValue(result);
}

NS_IMETHODIMP 
nsAggregatePrincipal::CanEnableCapability(const char *capability,
                                          PRInt16 *result)
{
    return mMasterPrincipal->CanEnableCapability(capability, result);
}

NS_IMETHODIMP
nsAggregatePrincipal::SetCanEnableCapability(const char *capability, 
                                             PRInt16 canEnable)
{
    return mMasterPrincipal->SetCanEnableCapability(capability, canEnable);
}   

NS_IMETHODIMP
nsAggregatePrincipal::IsCapabilityEnabled(const char *capability, void *annotation, 
                                          PRBool *result)
{
    return mMasterPrincipal->IsCapabilityEnabled(capability, annotation, result);
}

NS_IMETHODIMP
nsAggregatePrincipal::EnableCapability(const char *capability, void **annotation)
{
    return mMasterPrincipal->EnableCapability(capability, annotation);
}

NS_IMETHODIMP
nsAggregatePrincipal::RevertCapability(const char *capability, void **annotation)
{
    return mMasterPrincipal->RevertCapability(capability, annotation);
}

NS_IMETHODIMP
nsAggregatePrincipal::DisableCapability(const char *capability, void **annotation)
{
    return mMasterPrincipal->DisableCapability(capability, annotation);
}

NS_IMETHODIMP
nsAggregatePrincipal::Save(nsSupportsHashtable* aPrincipals, nsIPref *prefs)
{
    return mMasterPrincipal->Save(aPrincipals, prefs);
}

/////////////////////////////////////////////
// Constructor, Destructor, initialization //
/////////////////////////////////////////////

nsAggregatePrincipal::nsAggregatePrincipal() : mCertificate(nsnull),
                                               mCodebase(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsAggregatePrincipal::~nsAggregatePrincipal()
{
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
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
 * The Original Code is nsChromeURL.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "nsChromeURL.h"
#include "nsIStandardURL.h"
#include "nsNetUtil.h"
#include "nsIChromeRegistry.h"
#include "nsNetCID.h"
#include "nsMemory.h"
#include "nsComponentManagerUtils.h"
#include "nsString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsAutoPtr.h"

// This comes from nsChromeRegistry.cpp
extern nsIChromeRegistry* gChromeRegistry;

static NS_DEFINE_CID(kChromeURLCID, NS_CHROMEURL_CID);

nsresult
nsChromeURL::Init(const nsACString &aSpec, const char *aCharset,
                  nsIURI *aBaseURI)
{
    nsresult rv;
    nsCOMPtr<nsIStandardURL> url =
        do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, aCharset,
                   aBaseURI);
    NS_ENSURE_SUCCESS(rv, rv);

    mChromeURL = do_QueryInterface(url, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Canonify the "chrome:" URL; e.g., so that we collapse
    // "chrome://navigator/content/" and "chrome://navigator/content"
    // and "chrome://navigator/content/navigator.xul".

    // Try the global cache first.
    if (!gChromeRegistry) {
        // The service has not been instantiated yet; let's do that now.
        nsCOMPtr<nsIChromeRegistry> reg =
            do_GetService(NS_CHROMEREGISTRY_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ASSERTION(gChromeRegistry, "yikes, gChromeRegistry not set");
    }

    return gChromeRegistry->Canonify(mChromeURL);

    // Construction of |mConvertedURI| must happen lazily since this
    // method can be called during nsChromeRegistry::Init, when
    // nsChromeRegistry::ConvertChromeURL will fail.
}

// interface nsISupports

NS_IMPL_ADDREF(nsChromeURL)
NS_IMPL_RELEASE(nsChromeURL)
NS_INTERFACE_MAP_BEGIN(nsChromeURL)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsIURI)
    NS_INTERFACE_MAP_ENTRY(nsIURL)
    NS_INTERFACE_MAP_ENTRY(nsIChromeURL)
    NS_INTERFACE_MAP_ENTRY(nsIStandardURL)
    NS_INTERFACE_MAP_ENTRY(nsISerializable)
    NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
    if (aIID.Equals(nsChromeURL::GetIID())) // NS_CHROMEURL_IMPL_IID
        foundInterface = NS_REINTERPRET_CAST(nsISupports*, this);
    else
NS_INTERFACE_MAP_END

#define FORWARD_METHOD(method_, params_, args_)                                \
  NS_IMETHODIMP nsChromeURL::method_ params_                                  \
  {                                                                           \
    return mChromeURL->method_ args_;                                         \
  }

// interface nsIURI

FORWARD_METHOD(GetSpec, (nsACString & aSpec), (aSpec))
FORWARD_METHOD(SetSpec, (const nsACString & aSpec), (aSpec))
FORWARD_METHOD(GetPrePath, (nsACString & aPrePath), (aPrePath))
FORWARD_METHOD(GetScheme, (nsACString & aScheme), (aScheme))
FORWARD_METHOD(SetScheme, (const nsACString & aScheme), (aScheme))
FORWARD_METHOD(GetUserPass, (nsACString & aUserPass), (aUserPass))
FORWARD_METHOD(SetUserPass, (const nsACString & aUserPass), (aUserPass))
FORWARD_METHOD(GetUsername, (nsACString & aUsername), (aUsername))
FORWARD_METHOD(SetUsername, (const nsACString & aUsername), (aUsername))
FORWARD_METHOD(GetPassword, (nsACString & aPassword), (aPassword))
FORWARD_METHOD(SetPassword, (const nsACString & aPassword), (aPassword))
FORWARD_METHOD(GetHostPort, (nsACString & aHostPort), (aHostPort))
FORWARD_METHOD(SetHostPort, (const nsACString & aHostPort), (aHostPort))
FORWARD_METHOD(GetHost, (nsACString & aHost), (aHost))
FORWARD_METHOD(SetHost, (const nsACString & aHost), (aHost))
FORWARD_METHOD(GetPort, (PRInt32 *aPort), (aPort))
FORWARD_METHOD(SetPort, (PRInt32 aPort), (aPort))
FORWARD_METHOD(GetPath, (nsACString & aPath), (aPath))
FORWARD_METHOD(SetPath, (const nsACString & aPath), (aPath))

/*
 * Overriding Equals is the main point of nsChromeURL's existence.  But
 * be careful because nsStandardURL equals QIs its |other| argument to
 * nsStandardURL, which we don't implement.
 */
NS_IMETHODIMP nsChromeURL::Equals(nsIURI *other, PRBool *_retval)
{
    NS_ENSURE_ARG_POINTER(other);
    NS_PRECONDITION(_retval, "null out param");

    *_retval = PR_FALSE;

    nsRefPtr<nsChromeURL> otherChrome;
    CallQueryInterface(other,
                   NS_STATIC_CAST(nsChromeURL**, getter_AddRefs(otherChrome)));
    if (!otherChrome) {
        return NS_OK; // false
    }

    PRBool equal;
    nsresult rv = mChromeURL->Equals(otherChrome->mChromeURL, &equal);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!equal) {
        return NS_OK; // false
    }

    if (!mConvertedURI) {
        nsCOMPtr<nsIURI> tmp;
        rv = GetConvertedURI(getter_AddRefs(tmp));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    if (!otherChrome->mConvertedURI) {
        nsCOMPtr<nsIURI> tmp;
        rv = otherChrome->GetConvertedURI(getter_AddRefs(tmp));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mConvertedURI->Equals(otherChrome->mConvertedURI, _retval);
}

FORWARD_METHOD(SchemeIs, (const char *scheme, PRBool *_retval), (scheme, _retval))

NS_IMETHODIMP nsChromeURL::Clone(nsIURI **_retval)
{
    nsIChromeURL *cloneI;
    nsresult rv = nsChromeURL::ReConvert(&cloneI);
    NS_ENSURE_SUCCESS(rv, rv);
    nsChromeURL *clone = NS_STATIC_CAST(nsChromeURL*, cloneI);

    if (mConvertedURI)
        rv = mConvertedURI->Clone(getter_AddRefs(clone->mConvertedURI));

    if (NS_FAILED(rv)) {
        NS_RELEASE(clone);
        return rv;
    }
    *_retval = clone;
    return NS_OK;
}

FORWARD_METHOD(Resolve, (const nsACString & relativePath, nsACString & _retval), (relativePath, _retval))
FORWARD_METHOD(GetAsciiSpec, (nsACString & aAsciiSpec), (aAsciiSpec))
FORWARD_METHOD(GetAsciiHost, (nsACString & aAsciiHost), (aAsciiHost))
FORWARD_METHOD(GetOriginCharset, (nsACString & aOriginCharset), (aOriginCharset))

// interface nsIURL
FORWARD_METHOD(GetFilePath, (nsACString & aFilePath), (aFilePath))
FORWARD_METHOD(SetFilePath, (const nsACString & aFilePath), (aFilePath))
FORWARD_METHOD(GetParam, (nsACString & aParam), (aParam))
FORWARD_METHOD(SetParam, (const nsACString & aParam), (aParam))
FORWARD_METHOD(GetQuery, (nsACString & aQuery), (aQuery))
FORWARD_METHOD(SetQuery, (const nsACString & aQuery), (aQuery))
FORWARD_METHOD(GetRef, (nsACString & aRef), (aRef))
FORWARD_METHOD(SetRef, (const nsACString & aRef), (aRef))
FORWARD_METHOD(GetDirectory, (nsACString & aDirectory), (aDirectory))
FORWARD_METHOD(SetDirectory, (const nsACString & aDirectory), (aDirectory))
FORWARD_METHOD(GetFileName, (nsACString & aFileName), (aFileName))
FORWARD_METHOD(SetFileName, (const nsACString & aFileName), (aFileName))
FORWARD_METHOD(GetFileBaseName, (nsACString & aFileBaseName), (aFileBaseName))
FORWARD_METHOD(SetFileBaseName, (const nsACString & aFileBaseName), (aFileBaseName))
FORWARD_METHOD(GetFileExtension, (nsACString & aFileExtension), (aFileExtension))
FORWARD_METHOD(SetFileExtension, (const nsACString & aFileExtension), (aFileExtension))

/*
 * override because nsStandardURL QIs aURIToCompare to nsStandardURL,
 * which we don't implement.
 */
NS_IMETHODIMP nsChromeURL::GetCommonBaseSpec(nsIURI *aURIToCompare, nsACString & _retval)
{
    NS_ENSURE_ARG_POINTER(aURIToCompare);

    nsChromeURL *otherChromeURL;
    nsresult rv;
    if (NS_SUCCEEDED(CallQueryInterface(aURIToCompare, &otherChromeURL))) {
        rv = mChromeURL->GetCommonBaseSpec(otherChromeURL->mChromeURL, _retval);
        NS_RELEASE(otherChromeURL);
    } else {
        rv = mChromeURL->GetCommonBaseSpec(aURIToCompare, _retval);
    }
    return rv;
}

/*
 * override because nsStandardURL QIs aURIToCompare to nsStandardURL,
 * which we don't implement.
 */
NS_IMETHODIMP nsChromeURL::GetRelativeSpec(nsIURI *aURIToCompare, nsACString & _retval)
{
    NS_ENSURE_ARG_POINTER(aURIToCompare);

    nsChromeURL *otherChromeURL;
    nsresult rv;
    if (NS_SUCCEEDED(CallQueryInterface(aURIToCompare, &otherChromeURL))) {
        rv = mChromeURL->GetRelativeSpec(otherChromeURL->mChromeURL, _retval);
        NS_RELEASE(otherChromeURL);
    } else {
        rv = mChromeURL->GetRelativeSpec(aURIToCompare, _retval);
    }
    return rv;
}

// interface nsIChromeURL

NS_IMETHODIMP nsChromeURL::GetConvertedURI(nsIURI **aConvertedURI)
{
    if (!mConvertedURI) {
        NS_ENSURE_TRUE(gChromeRegistry, NS_ERROR_UNEXPECTED);

        // From now on, mutation is forbidden, since it wouldn't be
        // reflected in mConvertedURI, and the whole point of nsChromeURL
        // is to keep track of when mConvertedURI changes.
        nsCOMPtr<nsIStandardURL> standardURL = do_QueryInterface(mChromeURL);
        standardURL->SetMutable(PR_FALSE);

        // Resolve the "chrome:" URL to a URL with a different scheme.
        nsCAutoString spec;
        nsresult rv = gChromeRegistry->ConvertChromeURL(mChromeURL, spec);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = NS_NewURI(getter_AddRefs(mConvertedURI), spec);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*aConvertedURI = mConvertedURI);
    return NS_OK;
}

// NOTE: This function is used to implement Clone.
NS_IMETHODIMP nsChromeURL::ReConvert(nsIChromeURL **aResult)
{
    *aResult = nsnull;

    nsRefPtr<nsChromeURL> result = new nsChromeURL;
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = mChromeURL->Clone(NS_REINTERPRET_CAST(nsIURI**,
                                      NS_STATIC_CAST(nsIURL**,
                                        getter_AddRefs(result->mChromeURL))));
    NS_ASSERTION(nsCOMPtr<nsIURL>(do_QueryInterface(
                     NS_STATIC_CAST(nsISupports*, result->mChromeURL)))
                 == result->mChromeURL, "invalid cast");
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ADDREF(*aResult = result);
    return NS_OK;
}

// interface nsIStandardURL

NS_IMETHODIMP nsChromeURL::Init(PRUint32 aUrlType, PRInt32 aDefaultPort, const nsACString & aSpec, const char *aOriginCharset, nsIURI *aBaseURI)
{
    NS_NOTREACHED("nsChromeURL::Init");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsChromeURL::GetMutable(PRBool *aMutable)
{
    nsCOMPtr<nsIStandardURL> url = do_QueryInterface(mChromeURL);
    return url->GetMutable(aMutable);
}

NS_IMETHODIMP nsChromeURL::SetMutable(PRBool aMutable)
{
    nsCOMPtr<nsIStandardURL> url = do_QueryInterface(mChromeURL);
    return url->SetMutable(aMutable);
}

// interface nsISerializable

NS_IMETHODIMP nsChromeURL::Read(nsIObjectInputStream *aInputStream)
{
    nsresult rv =
        aInputStream->ReadObject(PR_TRUE, getter_AddRefs(mChromeURL));
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_ReadOptionalObject(aInputStream, PR_TRUE,
                                 getter_AddRefs(mConvertedURI));
}

NS_IMETHODIMP nsChromeURL::Write(nsIObjectOutputStream *aOutputStream)
{
    nsresult rv = aOutputStream->WriteCompoundObject(mChromeURL,
                                                     NS_GET_IID(nsIURL),
                                                     PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
    // XXX Should this be optional, or should we resolve before writing?
    return NS_WriteOptionalCompoundObject(aOutputStream, mConvertedURI,
                                          NS_GET_IID(nsIURI), PR_TRUE);
}

// interface nsIClassInfo

NS_IMETHODIMP nsChromeURL::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    *count = 0;
    *array = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
{
    *_retval = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetClassID(nsCID * *aClassID)
{
    *aClassID = (nsCID*) nsMemory::Alloc(sizeof(nsCID));
    if (!*aClassID)
            return NS_ERROR_OUT_OF_MEMORY;
    return GetClassIDNoAlloc(*aClassID);
}

NS_IMETHODIMP nsChromeURL::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::MAIN_THREAD_ONLY;
    return NS_OK;
}

NS_IMETHODIMP nsChromeURL::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    *aClassIDNoAlloc = kChromeURLCID;
    return NS_OK;
}

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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc.  Portions created by Sun are
 * Copyright (C) 2001 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
 *
 * Contributor(s): 
 */

#include "nsAbLDAPDirectory.h"
#include "nsAbLDAPProperties.h"
#include "nsAbUtils.h"

#include "nsAbQueryStringToExpression.h"

#include "nsAbBaseCID.h"
#include "nsIAddrBookSession.h"

#include "nsIRDFService.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#include "nsAutoLock.h"


nsAbLDAPDirectory::nsAbLDAPDirectory() :
    nsAbDirectoryRDFResource(),
    nsAbLDAPDirectoryQuery (),
    mInitialized(PR_FALSE),
    mInitializedConnection(PR_FALSE),
    mPerformingQuery(PR_FALSE),
    mContext(0),
    mLock(0)
{
}

nsAbLDAPDirectory::~nsAbLDAPDirectory()
{
    if (mLock)
        PR_DestroyLock (mLock);
}

NS_IMPL_ISUPPORTS_INHERITED3(nsAbLDAPDirectory, nsAbDirectoryRDFResource, nsIAbDirectory, nsIAbDirectoryQuery, nsIAbDirectorySearch)


nsresult nsAbLDAPDirectory::Initiate ()
{
    if (mIsQueryURI == PR_FALSE)
        return NS_ERROR_FAILURE;

    if (mInitialized == PR_TRUE)
        return NS_OK;

    nsresult rv;

    mLock = PR_NewLock ();
    if(!mLock)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = nsAbQueryStringToExpression::Convert (mQueryString.get (),
        getter_AddRefs(mExpression));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InitiateConnection ();

    mInitialized = PR_TRUE;
    return rv;
}

nsresult nsAbLDAPDirectory::InitiateConnection ()
{
    if (mInitializedConnection == PR_TRUE)
        return NS_OK;

    nsresult rv;

    mURL = do_CreateInstance(NS_LDAPURL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString uriString (mURINoQuery);
    uriString.ReplaceSubstring ("moz-abldapdirectory:", "ldap:");

    rv = mURL->SetSpec(uriString.get ());
    NS_ENSURE_SUCCESS(rv, rv);

    mConnection = do_CreateInstance(NS_LDAPCONNECTION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedConnection = PR_TRUE;
    return rv;
}


/* 
 *
 * nsIAbDirectory methods
 *
 */

NS_IMETHODIMP nsAbLDAPDirectory::GetOperations(PRInt32 *aOperations)
{
    *aOperations = nsIAbDirectory::opSearch;
    return NS_OK;
}

NS_IMETHODIMP nsAbLDAPDirectory::GetChildNodes(nsIEnumerator* *result)
{
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array)
            return NS_ERROR_OUT_OF_MEMORY;

    return array->Enumerate(result);
}

NS_IMETHODIMP nsAbLDAPDirectory::GetChildCards(nsIEnumerator** result)
{
    nsresult rv;
    
    // Start the search
    rv = StartSearch ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array)
            return NS_ERROR_OUT_OF_MEMORY;

    return array->Enumerate(result);
}

NS_IMETHODIMP nsAbLDAPDirectory::HasCard(nsIAbCard* card, PRBool* hasCard)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsVoidKey key (NS_STATIC_CAST(void* ,card));

    // Enter lock
    nsAutoLock lock (mLock);

    *hasCard = mCache.Exists (&key);
    if (*hasCard == PR_FALSE && mPerformingQuery == PR_TRUE)
            return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
}

NS_IMETHODIMP nsAbLDAPDirectory::GetTotalCards(PRBool subDirectoryCount,
        PRUint32 *_retval)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);
    // Enter lock
    nsAutoLock lock (mLock);
    *_retval = NS_STATIC_CAST(PRUint32 ,mCache.Count ());

    return NS_OK;
}



/* 
 *
 * nsAbLDAPDirectoryQuery methods
 *
 */

nsresult nsAbLDAPDirectory::GetLDAPConnection (nsILDAPConnection** connection)
{
    nsresult rv;

    rv = InitiateConnection ();
    NS_ENSURE_SUCCESS(rv, rv);

    *connection = mConnection;
    NS_IF_ADDREF(*connection);

    return rv;
}

nsresult nsAbLDAPDirectory::GetLDAPURL (nsILDAPURL** url)
{
    nsresult rv;

    rv = InitiateConnection ();
    NS_ENSURE_SUCCESS(rv, rv);

    *url = mURL;
    NS_IF_ADDREF(*url);

    return rv;
}

nsresult nsAbLDAPDirectory::CreateCard (nsILDAPURL* uri, const char* dn, nsIAbCard** card)
{
    nsresult rv;

    nsXPIDLCString cardUri;
    rv = CreateCardURI (uri, dn, getter_Copies (cardUri));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> res;
    rv = gRDFService->GetResource(cardUri, getter_AddRefs(res));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = res->QueryInterface(NS_GET_IID(nsIAbCard), NS_REINTERPRET_CAST(void**, card));
    NS_IF_ADDREF(*card);

    return rv;
}

nsresult nsAbLDAPDirectory::CreateCardURI (nsILDAPURL* uri, const char* dn, char** cardUri)
{
    nsresult rv;

    nsXPIDLCString host;
    rv = uri->GetHost(getter_Copies (host));
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 port;
    rv = uri->GetPort(&port);
    NS_ENSURE_SUCCESS(rv, rv);

    *cardUri = PR_smprintf("moz-abldapcard://%s:%d/%s", host.get (), port, dn);
    if(!cardUri)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}



/* 
 *
 * nsIAbDirectorySearch methods
 *
 */

NS_IMETHODIMP nsAbLDAPDirectory::StartSearch ()
{
    nsresult rv;
    
    if (mIsQueryURI == PR_FALSE ||
        mQueryString.Length () == 0)
        return NS_OK;


    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = StopSearch ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbDirectoryQueryArguments> arguments;
    NS_NewIAbDirectoryQueryArguments (getter_AddRefs(arguments));

    rv = arguments->SetExpression (mExpression);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the return properties to
    // return nsIAbCard interfaces
    nsCStringArray properties;
    properties.AppendCString (nsCAutoString ("card:nsIAbCard"));
    CharPtrArrayGuard returnProperties (PR_FALSE);
    rv = CStringArrayToCharPtrArray::Convert (properties,returnProperties.GetSizeAddr(),
                    returnProperties.GetArrayAddr(), PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = arguments->SetReturnProperties (returnProperties.GetSize(), returnProperties.GetArray());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = arguments->SetQuerySubDirectories (PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the the query listener
    nsCOMPtr<nsIAbDirectoryQueryResultListener> queryListener;
    nsAbDirSearchListener* _queryListener =
        new nsAbDirSearchListener (this);
    queryListener = _queryListener;

    // Perform the query
    rv = DoQuery (arguments, queryListener, 100, 0, &mContext);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Enter lock
    nsAutoLock lock (mLock);
    mPerformingQuery = PR_TRUE;
    mCache.Reset ();

    return rv;
}

NS_IMETHODIMP nsAbLDAPDirectory::StopSearch ()
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    // Enter lock
    {
        nsAutoLock lockGuard (mLock);
        if (mPerformingQuery == PR_FALSE)
            return NS_OK;
        mPerformingQuery = PR_FALSE;
    }
    // Exit lock

    rv = StopQuery (mContext);

    return rv;
}


/* 
 *
 * nsAbDirSearchListenerContext methods
 *
 */
nsresult nsAbLDAPDirectory::OnSearchFinished (PRInt32 result)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoLock lock (mLock);
    mPerformingQuery = PR_FALSE;

    return NS_OK;
}

nsresult nsAbLDAPDirectory::OnSearchFoundCard (nsIAbCard* card)
{
    nsresult rv;

    rv = Initiate ();
    NS_ENSURE_SUCCESS(rv, rv);

    nsVoidKey key (NS_STATIC_CAST(void* ,card));
    // Enter lock
    {
        nsAutoLock lock (mLock);
        mCache.Put (&key, card);
    }
    // Exit lock

    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);;
    if(NS_SUCCEEDED(rv))
        abSession->NotifyDirectoryItemAdded(this, card);

    return NS_OK;
}


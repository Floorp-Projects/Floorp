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
 * The Initial Developer of the Original Code is
 * <rdayal@netscape.com>
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsCOMPtr.h"
#include "nsAbLDAPReplicationQuery.h"
#include "nsAbLDAPReplicationService.h"
#include "nsAbLDAPProcessReplicationData.h"
#include "nsILDAPURL.h"
#include "nsAbBaseCID.h"
#include "nsAbLDAPProperties.h"
#include "nsProxiedService.h"
#include "nsLDAP.h"
#include "nsAbUtils.h"
#include "nsDirPrefs.h"


NS_IMPL_ISUPPORTS1(nsAbLDAPReplicationQuery, nsIAbLDAPReplicationQuery)

nsAbLDAPReplicationQuery::nsAbLDAPReplicationQuery()
 : mDirServer(nsnull),
   mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsAbLDAPReplicationQuery::~nsAbLDAPReplicationQuery()
{
    DIR_DeleteServer(mDirServer);
}

nsresult nsAbLDAPReplicationQuery::InitLDAPData()
{
    mDirServer = (DIR_Server *) PR_Calloc(1, sizeof(DIR_Server));
    if (!mDirServer) return NS_ERROR_NULL_POINTER;

    DIR_InitServerWithType(mDirServer, LDAPDirectory);
    // since DeleteServer frees the prefName make a copy of prefName string
    mDirServer->prefName = nsCRT::strdup(mDirPrefName.get());
    DIR_GetPrefsForOneServer(mDirServer, PR_FALSE, PR_FALSE);

    // this is done here to take care of the problem related to bug # 99124.
    // earlier versions of Mozilla could have the fileName associated with the directory
    // to be abook.mab which is the profile's personal addressbook. If the pref points to
    // it generate a new filename and set it as the replication filename.
    if (!nsCRT::strcasecmp(mDirServer->fileName,kPersonalAddressbook) || !mDirServer->fileName)
        DIR_SetServerFileName(mDirServer, mDirServer->serverName);
    // use the dir server filename for replication
    PR_FREEIF(mDirServer->replInfo->fileName);
    // since DeleteServer frees the replInfo->fileName make a copy of string
    // if we donot do this the DeleteServer functions crashes.
    mDirServer->replInfo->fileName = nsCRT::strdup(mDirServer->fileName);

    nsresult rv = NS_OK;

    mURL = do_CreateInstance(NS_LDAPURL_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return rv;

    rv = mURL->SetSpec(nsDependentCString(mDirServer->uri));
    if (NS_FAILED(rv)) 
        return rv;

    mConnection = do_CreateInstance(NS_LDAPCONNECTION_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return rv;

    mOperation = do_CreateInstance(NS_LDAPOPERATION_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return rv;

    return rv;
}

nsresult nsAbLDAPReplicationQuery::ConnectToLDAPServer(nsILDAPURL * aURL)
{
    if (!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    nsCAutoString host;
    rv = aURL->GetHost(host);
    if (NS_FAILED(rv)) 
        return rv;
    if (host.IsEmpty())
        return NS_ERROR_UNEXPECTED;

    PRInt32 port;
    rv = aURL->GetPort(&port);
    if (NS_FAILED(rv)) 
        return rv;
    if (!port)
        return NS_ERROR_UNEXPECTED;

    nsXPIDLCString dn;
    rv = aURL->GetDn(getter_Copies(dn));
    if (NS_FAILED(rv))
        return rv;
    if (!dn.get())
        return NS_ERROR_UNEXPECTED;

    // Initiate LDAP message listener to the current thread
    nsCOMPtr<nsILDAPMessageListener> listener;
    rv = NS_GetProxyForObject(NS_CURRENT_EVENTQ,
                  NS_GET_IID(nsILDAPMessageListener), 
                  NS_STATIC_CAST(nsILDAPMessageListener*, mDataProcessor),
                  PROXY_SYNC | PROXY_ALWAYS, 
                  getter_AddRefs(listener));
    if (!listener) 
        return NS_ERROR_FAILURE;

    // initialize the LDAP connection
    rv = mConnection->Init(host.get(), port, NS_ConvertUTF8toUCS2(dn).get(), listener);

    return rv;    
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::Init(const nsACString & aPrefName, nsIWebProgressListener *aProgressListener)
{
    if (aPrefName.IsEmpty())
        return NS_ERROR_UNEXPECTED;

    mDirPrefName = aPrefName;

    nsresult rv = NS_OK;

    rv = InitLDAPData();
    if (NS_FAILED(rv)) 
        return rv;

    mDataProcessor =  do_CreateInstance(NS_ABLDAP_PROCESSREPLICATIONDATA_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return rv;

    // 'this' initialized
    mInitialized = PR_TRUE;

    rv = mDataProcessor->Init(this, aProgressListener);
    if (NS_FAILED(rv)) 
        return rv;

    return rv;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::DoReplicationQuery()
{
    return ConnectToLDAPServer(mURL);
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::QueryAllEntries()
{
    if (!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK; 

    // get the search filter associated with the directory server url; 
    //
    nsXPIDLCString urlFilter;
    rv = mURL->GetFilter(getter_Copies(urlFilter));
    if (NS_FAILED(rv)) 
        return rv;

    nsXPIDLCString dn;
    rv = mURL->GetDn(getter_Copies(dn));
    if (NS_FAILED(rv)) 
        return rv;
    if (!dn.get())
        return NS_ERROR_UNEXPECTED;

    PRInt32 scope;
    rv = mURL->GetScope(&scope);
    if (NS_FAILED(rv)) 
        return rv;

    CharPtrArrayGuard attributes;
    rv = mURL->GetAttributes(attributes.GetSizeAddr(), attributes.GetArrayAddr());
    if (NS_FAILED(rv)) 
        return rv;

    rv = mOperation->SearchExt(NS_ConvertUTF8toUCS2(dn).get(), scope, 
                               NS_ConvertUTF8toUCS2(urlFilter).get(), 
                               attributes.GetSize(), attributes.GetArray(),
                               0, 0);
    return rv;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::CancelQuery()
{
    if (!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = mDataProcessor->Abort();
    return rv;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::Done(PRBool aSuccess)
{
   if (!mInitialized) 
       return NS_ERROR_NOT_INITIALIZED;
   
   nsresult rv = NS_OK;
   nsCOMPtr<nsIAbLDAPReplicationService> replicationService = 
                            do_GetService(NS_ABLDAP_REPLICATIONSERVICE_CONTRACTID, &rv);
   if (NS_SUCCEEDED(rv))
      replicationService->Done(aSuccess);

   return rv;
}


NS_IMETHODIMP nsAbLDAPReplicationQuery::GetOperation(nsILDAPOperation * *aOperation)
{
   NS_ENSURE_ARG_POINTER(aOperation);
   if (!mInitialized) 
       return NS_ERROR_NOT_INITIALIZED;
   
   NS_IF_ADDREF(*aOperation = mOperation);

   return NS_OK;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::GetConnection(nsILDAPConnection * *aConnection)
{
   NS_ENSURE_ARG_POINTER(aConnection);
   if (!mInitialized) 
       return NS_ERROR_NOT_INITIALIZED;

   NS_IF_ADDREF(*aConnection = mConnection); 

   return NS_OK;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::GetReplicationURL(nsILDAPURL * *aReplicationURL)
{
   NS_ENSURE_ARG_POINTER(aReplicationURL);
   if (!mInitialized) 
       return NS_ERROR_NOT_INITIALIZED;

   NS_IF_ADDREF(*aReplicationURL = mURL); 

   return NS_OK;
}

NS_IMETHODIMP nsAbLDAPReplicationQuery::GetReplicationInfo(DIR_ReplicationInfo * *aReplicationInfo)
{
   NS_ENSURE_ARG_POINTER(aReplicationInfo);
   if (!mInitialized) 
       return NS_ERROR_NOT_INITIALIZED;

   *aReplicationInfo = mDirServer->replInfo; 

   return NS_OK;
}

/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rajiv Dayal <rdayal@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsCOMPtr.h"
#include "nsAbLDAPChangeLogQuery.h"
#include "nsAbLDAPReplicationService.h"
#include "nsAbLDAPChangeLogData.h"
#include "nsAbLDAPProperties.h"
#include "nsAutoLock.h"
#include "nsAbUtils.h"
#include "prprf.h"
#include "nsDirPrefs.h"
#include "nsAbBaseCID.h"
#include "nsPrintfCString.h"


NS_IMPL_ISUPPORTS_INHERITED1(nsAbLDAPChangeLogQuery, nsAbLDAPReplicationQuery, nsIAbLDAPChangeLogQuery)

nsAbLDAPChangeLogQuery::nsAbLDAPChangeLogQuery()
{
  NS_INIT_ISUPPORTS();
}

nsAbLDAPChangeLogQuery::~nsAbLDAPChangeLogQuery()
{

}

// this is to be defined only till this is not hooked to SSL to get authDN and authPswd
#define USE_AUTHDLG

NS_IMETHODIMP nsAbLDAPChangeLogQuery::Init(const nsACString & aPrefName, nsIWebProgressListener *aProgressListener)
{
    if(aPrefName.IsEmpty()) 
        return NS_ERROR_UNEXPECTED;

    mDirPrefName = aPrefName;

    nsresult rv = InitLDAPData();
    if(NS_FAILED(rv)) 
        return rv;

    // create the ChangeLog Data Processor
    mDataProcessor =  do_CreateInstance(NS_ABLDAP_PROCESSCHANGELOGDATA_CONTRACTID, &rv);
    if(NS_FAILED(rv)) 
        return rv;

    // 'this' initialized
    mInitialized = PR_TRUE;

    return mDataProcessor->Init(this, aProgressListener);
}

NS_IMETHODIMP nsAbLDAPChangeLogQuery::DoReplicationQuery()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

#ifdef USE_AUTHDLG
    return ConnectToLDAPServer(mURL, NS_LITERAL_STRING(""));
#else
    mDataProcessor->PopulateAuthData();
    return ConnectToLDAPServer(mURL, mAuthDN);
#endif
}

NS_IMETHODIMP nsAbLDAPChangeLogQuery::QueryAuthDN(const nsAString & aValueUsedToFindDn)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK; 

    CharPtrArrayGuard attributes;
    *attributes.GetSizeAddr() = 2;
    *attributes.GetArrayAddr() = NS_STATIC_CAST(char **, nsMemory::Alloc((*attributes.GetSizeAddr()) * sizeof(char *)));
    attributes.GetArray()[0] = ToNewCString(nsDependentCString(DIR_GetFirstAttributeString(mDirServer, cn)));
    attributes.GetArray()[1] = nsnull;
    
    nsAutoString filter;
    filter.AssignWithConversion(DIR_GetFirstAttributeString(mDirServer, auth));
    filter.AppendWithConversion(NS_LITERAL_CSTRING("=").get());
    filter += aValueUsedToFindDn;

    nsXPIDLCString dn;
    rv = mURL->GetDn(getter_Copies(dn));
    if(NS_FAILED(rv)) 
        return rv;

    return mOperation->SearchExt(NS_ConvertUTF8toUCS2(dn).get(), nsILDAPURL::SCOPE_SUBTREE, 
                               filter.get(), 
                               attributes.GetSize(), attributes.GetArray(),
                               0, 0);
}

NS_IMETHODIMP nsAbLDAPChangeLogQuery::QueryRootDSE()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    return mOperation->SearchExt(nsnull, nsILDAPURL::SCOPE_BASE, 
                               NS_LITERAL_STRING("objectclass=*").get(), 
                               MozillaLdapPropertyRelator::rootDSEAttribCount, 
                               MozillaLdapPropertyRelator::changeLogRootDSEAttribs,
                               0, 0);
}

NS_IMETHODIMP nsAbLDAPChangeLogQuery::QueryChangeLog(const nsAString & aChangeLogDN, PRInt32 aLastChangeNo)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;
    if(aChangeLogDN.IsEmpty()) 
        return NS_ERROR_UNEXPECTED;

    // make sure that the filter here just have one condition 
    // and should not be enclosed in enclosing brackets.
    // also condition '>' doesnot work, it should be '>='/
    nsCAutoString filter (NS_LITERAL_CSTRING("changenumber>="));
    filter.AppendInt(mDirServer->replInfo->lastChangeNumber+1);

    return mOperation->SearchExt(PromiseFlatString(aChangeLogDN).get(), 
                               nsILDAPURL::SCOPE_ONELEVEL,
                               NS_ConvertUTF8toUCS2(filter).get(), 
                               MozillaLdapPropertyRelator::changeLogEntryAttribCount, 
                               MozillaLdapPropertyRelator::changeLogEntryAttribs,
                               0, 0);
}

NS_IMETHODIMP nsAbLDAPChangeLogQuery::QueryChangedEntries(const nsAString & aChangedEntryDN)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;
    if(aChangedEntryDN.IsEmpty()) 
        return NS_ERROR_UNEXPECTED;

    nsXPIDLCString urlFilter;
    nsresult rv = mURL->GetFilter(getter_Copies(urlFilter));
    if(NS_FAILED(rv)) 
        return rv;

    PRInt32 scope;
    rv = mURL->GetScope(&scope);
    if(NS_FAILED(rv)) 
        return rv;

    CharPtrArrayGuard attributes;
    rv = mURL->GetAttributes(attributes.GetSizeAddr(), attributes.GetArrayAddr());
    if(NS_FAILED(rv)) 
        return rv;

    return mOperation->SearchExt(PromiseFlatString(aChangedEntryDN).get(), scope, 
                               NS_ConvertUTF8toUCS2(urlFilter).get(), 
                               attributes.GetSize(), attributes.GetArray(),
                               0, 0);
}


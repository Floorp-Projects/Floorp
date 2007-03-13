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
 * Rajiv Dayal <rdayal@netscape.com>
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
#include "nsAbLDAPReplicationService.h"
#include "nsAbLDAPReplicationQuery.h"
#include "nsAbBaseCID.h"
#include "nsIWebProgressListener.h"

// XXX Change log replication doesn't work. Bug 311632 should fix it.
//#include "nsAbLDAPChangeLogQuery.h"
#include "nsIAbLDAPReplicationData.h"

/*** implementation of the service ******/

NS_IMPL_ISUPPORTS1(nsAbLDAPReplicationService, nsIAbLDAPReplicationService)

nsAbLDAPReplicationService::nsAbLDAPReplicationService() 
    : mReplicating(PR_FALSE)
{
}

nsAbLDAPReplicationService::~nsAbLDAPReplicationService()
{
}

/* void startReplication(in string aURI, in nsIWebProgressListener progressListener); */
NS_IMETHODIMP nsAbLDAPReplicationService::StartReplication(const nsACString & aPrefName, nsIWebProgressListener *progressListener)
{
    if(aPrefName.IsEmpty())
        return NS_ERROR_UNEXPECTED;

#ifdef DEBUG_rdayal
    printf("Start Replication called");
#endif

    // makes sure to allow only one replication at a time
    if(mReplicating)
        return NS_ERROR_FAILURE;

    mPrefName = aPrefName;

    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;

    switch(DecideProtocol())
    {
        case nsIAbLDAPProcessReplicationData::kDefaultDownloadAll :
            mQuery = do_CreateInstance(NS_ABLDAP_REPLICATIONQUERY_CONTRACTID, &rv);
            break ;
// XXX Change log replication doesn't work. Bug 311632 should fix it.
//        case nsIAbLDAPProcessReplicationData::kChangeLogProtocol :
//            mQuery = do_CreateInstance (NS_ABLDAP_CHANGELOGQUERY_CONTRACTID, &rv);
//            break ;
        default :
            break;
    }

            if(NS_SUCCEEDED(rv) && mQuery)
            {
               rv = mQuery->Init(aPrefName, progressListener);
               if(NS_SUCCEEDED(rv))
               {
                   rv = mQuery->DoReplicationQuery();
                   if(NS_SUCCEEDED(rv))
                   {
                       mReplicating = PR_TRUE;
                       return rv;
                   }
               }
            }

    if(progressListener && NS_FAILED(rv))
        progressListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, PR_FALSE);
       
    return rv;
}

/* void cancelReplication(in string aURI); */
NS_IMETHODIMP nsAbLDAPReplicationService::CancelReplication(const nsACString & aPrefName)
{
    if(aPrefName.IsEmpty())
        return NS_ERROR_UNEXPECTED;

    nsresult rv = NS_ERROR_FAILURE;

    if(aPrefName == mPrefName)
    {
        if(mQuery && mReplicating)
            rv = mQuery->CancelQuery();  
    }

    // if query has been cancelled successfully
    if(NS_SUCCEEDED(rv))
        Done(PR_FALSE);

    return rv;
}

NS_IMETHODIMP nsAbLDAPReplicationService::Done(PRBool aSuccess)
{
    mReplicating = PR_FALSE;
    if(mQuery)
        mQuery = nsnull;  // release query obj

    return NS_OK;
}


// XXX: This method should query the RootDSE for the changeLog attribute, 
// if it exists ChangeLog protocol is supported.
PRInt32 nsAbLDAPReplicationService::DecideProtocol()
{
  // do the changeLog, it will decide if there is a need to replicate all
  // entries or only update existing DB and will do the approprite thing.
  //
  // XXX: Bug 231965 changed this from kChangeLogProtocol to
  // kDefaultDownloadAll because of a problem with ldap replication not
  // working correctly. We need to change this back at some stage (bug 311632).
  return nsIAbLDAPProcessReplicationData::kDefaultDownloadAll;
}



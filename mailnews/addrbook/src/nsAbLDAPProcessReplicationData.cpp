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


#include "nsAbLDAPProcessReplicationData.h"
#include "nsLDAP.h"
#include "nsIAbCard.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"
#include "nsAbUtils.h"
#include "nsAbMDBCard.h"
#include "nsAbLDAPCard.h"
#include "nsFileSpec.h"
#include "nsAbLDAPProperties.h"
#include "nsAbLDAPReplicationQuery.h"
#include "nsProxiedService.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAbLDAPProcessReplicationData, nsIAbLDAPProcessReplicationData, nsILDAPMessageListener)

nsAbLDAPProcessReplicationData::nsAbLDAPProcessReplicationData()
 : mState(kQueryNotStarted),
   mProtocol(-1),
   mCount(0),
   mReplInfo(nsnull),
   mInitialized(PR_FALSE),
   mDBOpen(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsAbLDAPProcessReplicationData::~nsAbLDAPProcessReplicationData()
{
  /* destructor code */
  if(mDBOpen && mReplicationDB) 
      mReplicationDB->Close(PR_FALSE);
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::Init(nsIAbLDAPReplicationQuery *query, nsIWebProgressListener *progressListener)
{
   NS_ENSURE_ARG_POINTER(query);

   nsresult rv = NS_OK;

   mQuery = query;

   rv = mQuery->GetReplicationInfo(&mReplInfo);
   if(NS_FAILED(rv)) {
       mQuery = nsnull;
       return rv;   
   }
   if(!mReplInfo) {
       mQuery = nsnull;
       return NS_ERROR_FAILURE;   
   }

   mListener = progressListener;

   mInitialized = PR_TRUE;

   return rv;
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::GetReplicationState(PRInt32 *aReplicationState) 
{
    NS_ENSURE_ARG_POINTER(aReplicationState);

    *aReplicationState = mState; 
    return NS_OK; 
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::GetProtocolUsed(PRInt32 *aProtocolUsed) 
{
    NS_ENSURE_ARG_POINTER(aProtocolUsed);

    *aProtocolUsed = mProtocol; 
    return NS_OK; 
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::OnLDAPInit(nsresult aStatus)
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    // Make sure that the Init() worked properly
    if(NS_FAILED(aStatus)) {
        Done(PR_FALSE);
        return rv;
    }

    nsCOMPtr<nsILDAPMessageListener> listener;
    rv = NS_GetProxyForObject(NS_CURRENT_EVENTQ,
                  NS_GET_IID(nsILDAPMessageListener), 
                  NS_STATIC_CAST(nsILDAPMessageListener*, this),
                  PROXY_SYNC | PROXY_ALWAYS, 
                  getter_AddRefs(listener));
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    nsCOMPtr<nsILDAPOperation> operation;
    rv = mQuery->GetOperation(getter_AddRefs(operation));
    if(NS_FAILED(rv)) {
       Done(PR_FALSE);
       return rv;   
    }

    nsCOMPtr<nsILDAPConnection> connection;
    rv = mQuery->GetConnection(getter_AddRefs(connection));
    if(NS_FAILED(rv)) {
       Done(PR_FALSE);
       return rv;  
    }

    rv = operation->Init(connection, listener);
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    // Bind
    rv = operation->SimpleBind(nsnull);
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    return rv;
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    PRInt32 messageType;
    rv = aMessage->GetType(&messageType);
    if(NS_FAILED(rv)) {
#ifdef DEBUG_rdayal
        printf("LDAP Replication : OnLDAPMessage : couldnot GetType \n");
#endif
        Done(PR_FALSE);
        return rv;
    }

    switch(messageType)
    {
    case nsILDAPMessage::RES_BIND:
        rv = OnLDAPBind(aMessage);
        break;
    case nsILDAPMessage::RES_SEARCH_ENTRY:
        rv = OnLDAPSearchEntry(aMessage);
        break;
    case nsILDAPMessage::RES_SEARCH_RESULT:
        rv = OnLDAPSearchResult(aMessage);
    default:
        // for messageTypes we donot handle return NS_OK to LDAP and move ahead.
        rv = NS_OK;
        break;
    }

    return rv;
}

NS_IMETHODIMP nsAbLDAPProcessReplicationData::Abort()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    nsCOMPtr<nsILDAPOperation> operation;
    rv = mQuery->GetOperation(getter_AddRefs(operation));
    if(operation && mState != kQueryNotStarted)
    {
        rv = operation->Abandon();
        if(NS_SUCCEEDED(rv))
            mState = kQueryNotStarted;
    }

    if(mReplicationDB && mDBOpen) {
        mReplicationDB->ForceClosed(); // force close since we need to delete the file.
        mDBOpen = PR_FALSE;

        // delete the unsaved replication file
        if(mReplicationFile) {
            rv = mReplicationFile->Remove(PR_FALSE);
            if(NS_SUCCEEDED(rv)) {
                // now put back the backed up replicated file if aborted
                if(mBackupReplicationFile) 
                    rv = mBackupReplicationFile->MoveTo(nsnull, mReplInfo->fileName);
            }
        }
    }


    Done(PR_FALSE);

    return rv;
}

nsresult nsAbLDAPProcessReplicationData::OnLDAPBind(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    PRInt32 errCode;

    nsresult rv = aMessage->GetErrorCode(&errCode);
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    if(errCode != nsILDAPErrors::SUCCESS) {
        Done(PR_FALSE);
        return NS_ERROR_FAILURE;
    }

    rv = CreateABForReplicatedDir();
    if(NS_FAILED(rv)) {
        // donot call Done here since CreateABForReplicationDir calls it.
        return rv;
    }

    rv = mQuery->QueryAllEntries();
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    mState = kReplicatingAll;

    if(mListener && NS_SUCCEEDED(rv))
        mListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, PR_TRUE);

    return rv;
}

nsresult nsAbLDAPProcessReplicationData::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;
    // since this runs on the main thread and is single threaded, this will 
    // take care of entries returned by LDAP Connection thread after Abort.
    if(!mReplicationDB || !mDBOpen) 
        return NS_ERROR_FAILURE;

    nsresult rv;       


    nsAbLDAPCard card;


    PRBool hasSetCardProperty = PR_FALSE;
    rv = MozillaLdapPropertyRelator::createCardPropertyFromLDAPMessage(aMessage,
                                                                     &card, &hasSetCardProperty);
    if(NS_FAILED(rv)) {
        Abort();
        return rv;
    }

    if(hasSetCardProperty == PR_FALSE)
    {
        Abort();
        return NS_ERROR_FAILURE;
    }


#ifdef DEBUG_rdayal
        nsXPIDLString firstName;
        rv = card.GetFirstName(getter_Copies(firstName));
        NS_ENSURE_SUCCESS(rv,rv);
        nsXPIDLString lastName;
        rv = card.GetLastName(getter_Copies(lastName));
        NS_ENSURE_SUCCESS(rv,rv);
        nsCAutoString name;
        name.AssignWithConversion(firstName);
        name.Append("  ");
        name.AppendWithConversion(lastName);
        printf("\n LDAPReplication :: got card #: %d, name: %s \n", mCount, name.get());
#endif


    nsCOMPtr<nsIAbMDBCard> dbCard;
    dbCard = do_CreateInstance(NS_ABMDBCARD_CONTRACTID, &rv);
    if(NS_FAILED(rv)) {
        Abort();
        return rv;
    }

    nsCOMPtr<nsIAbCard> newCard;
    newCard = do_QueryInterface(dbCard, &rv);
    if(NS_FAILED(rv)) {
        Abort();
        return rv;
    }

    rv = newCard->Copy(&card);
    if(NS_FAILED(rv)) {
        Abort();
        return rv;
    }

    rv = mReplicationDB->CreateNewCardAndAddToDB(newCard, PR_FALSE);
    if(NS_FAILED(rv)) {
        Abort();
        return rv;
    }

    mCount ++;

    if(!(mCount % 10))  // inform the listener every 10 entries
    {
        mListener->OnProgressChange(nsnull,nsnull,mCount, -1, mCount, -1);
        // in case if the LDAP Connection thread is starved and causes problem
        // uncomment this one and try.
        // PR_Sleep(PR_INTERVAL_NO_WAIT); // give others a chance
    }

    return rv;
}


nsresult nsAbLDAPProcessReplicationData::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
#ifdef DEBUG_rdayal
    printf("LDAP Replication : Got Results for Completion");
#endif

    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;
    PRInt32 errorCode;
    rv = aMessage->GetErrorCode(&errorCode);

    if(NS_SUCCEEDED(rv)) {
        // We are done with the LDAP search for all entries.
        if(errorCode == nsILDAPErrors::SUCCESS || errorCode == nsILDAPErrors::SIZELIMIT_EXCEEDED) {
            Done(PR_TRUE);
            if(mReplicationDB && mDBOpen) {
                rv = mReplicationDB->Close(PR_TRUE);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB Close on Success failed");
                mDBOpen = PR_FALSE;
                // once we have saved the new replication file, delete the backup file
                if(mBackupReplicationFile)
                {
                    rv = mBackupReplicationFile->Remove(PR_FALSE);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "Replication BackupFile Remove on Success failed");
                }
            }
            return NS_OK;
        }
    }

    // in case if GetErrorCode returned error or errorCode is not SUCCESS / SIZELIMIT_EXCEEDED
    if(mReplicationDB && mDBOpen) {
        // if error result is returned close the DB without saving ???
        // should we commit anyway ??? whatever is returned is not lost then !!
        rv = mReplicationDB->ForceClosed(); // force close since we need to delete the file.
        NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB ForceClosed on Failure failed");
        mDBOpen = PR_FALSE;
        // if error result is returned remove the replicated file
        if(mReplicationFile) {
            rv = mReplicationFile->Remove(PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication File Remove on Failure failed");
            if(NS_SUCCEEDED(rv)) {
                // now put back the backed up replicated file
                if(mBackupReplicationFile) 
                {
                    rv = mBackupReplicationFile->MoveTo(nsnull, mReplInfo->fileName);
                    NS_ASSERTION(NS_SUCCEEDED(rv), "Replication Backup File Move back on Failure failed");
                }
            }
        }
        Done(PR_FALSE);
    }

    return NS_OK;
}

nsresult nsAbLDAPProcessReplicationData::CreateABForReplicatedDir()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    if(!mReplInfo->fileName) {
        Done(PR_FALSE);
        return NS_ERROR_FAILURE;
    }

    nsFileSpec* dbPath;
    rv = abSession->GetUserProfileDirectory(&dbPath);
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    (*dbPath) += mReplInfo->fileName;

    // if the AB DB already exists backup existing one, 
    // in case if the user cancels or Abort put back the backed up file
    if(dbPath->Exists()) {
        // get nsIFile for nsFileSpec from abSession, why use a obsolete class if not required!
        rv = NS_FileSpecToIFile(dbPath, getter_AddRefs(mReplicationFile));
        if(NS_FAILED(rv))  {
            delete dbPath;
            Done(PR_FALSE);
            return rv;
        }
        // create the backup file object same as replication file object
        // we create a backup file here since we need to cleanup the existing file
        // and then commit so instead of deleting existing cards we just clone the
        // existing one for a much better performance.
        rv = mReplicationFile->Clone(getter_AddRefs(mBackupReplicationFile));
        if(NS_FAILED(rv))  {
            delete dbPath;
            Done(PR_FALSE);
            return rv;
        }
        nsCAutoString backupFile(mReplInfo->fileName); 
        backupFile += BACKUP_EXTENSION;
        rv = mBackupReplicationFile->MoveTo(nsnull, backupFile.get());
        if (rv == NS_ERROR_FILE_ALREADY_EXISTS)
        {
            // create a temp nsILocalFile object for the existing backup file to delete it
            nsCOMPtr<nsILocalFile> existingBackupFile;
            nsXPIDLCString backupFilePath;
            rv = mBackupReplicationFile->GetPath(getter_Copies(backupFilePath));
            if (NS_SUCCEEDED(rv))
            {
                backupFilePath += BACKUP_EXTENSION;
                rv = NS_NewLocalFile(backupFilePath.get(), PR_TRUE, getter_AddRefs(existingBackupFile));
                // remove the existing backup file and move the cloned replication file to backup file
                if (NS_SUCCEEDED(rv))
                {
                    rv = existingBackupFile->Remove(PR_FALSE);
                    if (NS_SUCCEEDED(rv))
                        rv = mBackupReplicationFile->MoveTo(nsnull, backupFile.get());
                }
            }
        }
        // this will take care of all failed cases for moving to backup file above
        if(NS_FAILED(rv))  {
            delete dbPath;
            Done(PR_FALSE);
            return rv;
        }
    }

    nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
             do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
    if(NS_FAILED(rv)) {
        delete dbPath;
        Done(PR_FALSE);
        return rv;
    }
    
    rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(mReplicationDB), PR_TRUE);
    delete dbPath;
    if(NS_FAILED(rv)) {
        Done(PR_FALSE);
        return rv;
    }

    mDBOpen = PR_TRUE;  // replication DB is now Open
    return rv;
}

void nsAbLDAPProcessReplicationData::Done(PRBool aSuccess)
{
   if(!mInitialized) 
       return;

   mQuery->Done(aSuccess);

   if(mListener)
       mListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_STOP, aSuccess);

   // since this is called when all is done here, either on success, failure or abort
   // releas the query now.
   mQuery = nsnull;
}

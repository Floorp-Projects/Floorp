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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rajiv Dayal <rdayal@netscape.com>
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

#include "nsAbLDAPChangeLogData.h"
#include "nsAbLDAPChangeLogQuery.h"
#include "nsLDAP.h"
#include "nsIAbCard.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"
#include "nsAbUtils.h"
#include "nsAbMDBCard.h"
#include "nsAbLDAPCard.h"
#include "nsFileSpec.h"
#include "nsAbLDAPProperties.h"
#include "nsProxiedService.h"
#include "nsAutoLock.h"
#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsIWindowWatcher.h"
#include "nsUnicharUtils.h"

// defined here since to be used 
// only locally to this file.
enum UpdateOp {
 NO_OP,
 ENTRY_ADD,
 ENTRY_DELETE,
 ENTRY_MODIFY
};

nsAbLDAPProcessChangeLogData::nsAbLDAPProcessChangeLogData()
: mUseChangeLog(PR_FALSE),
  mChangeLogEntriesCount(0),
  mEntriesAddedQueryCount(0)
{
   mRootDSEEntry.firstChangeNumber = 0;
   mRootDSEEntry.lastChangeNumber = 0;
}

nsAbLDAPProcessChangeLogData::~nsAbLDAPProcessChangeLogData()
{

}

NS_IMETHODIMP nsAbLDAPProcessChangeLogData::Init(nsIAbLDAPReplicationQuery * query, nsIWebProgressListener *progressListener)
{
   NS_ENSURE_ARG_POINTER(query);

   // here we are assuming that the caller will pass a nsAbLDAPChangeLogQuery object,
   // an implementation derived from the implementation of nsIAbLDAPReplicationQuery.
   nsresult rv = NS_OK;
   mChangeLogQuery = do_QueryInterface(query, &rv);
   if(NS_FAILED(rv))
       return rv;
   
   // call the parent's Init now
   return nsAbLDAPProcessReplicationData::Init(query, progressListener);
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPBind(nsILDAPMessage *aMessage)
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

    switch(mState) {
    case kAnonymousBinding :
        rv = GetAuthData();
        if(NS_SUCCEEDED(rv)) 
            rv = mChangeLogQuery->QueryAuthDN(mAuthUserID);
        if(NS_SUCCEEDED(rv)) 
            mState = kSearchingAuthDN;
        break;
    case kAuthenticatedBinding :
        rv = mChangeLogQuery->QueryRootDSE();
        if(NS_SUCCEEDED(rv)) 
            mState = kSearchingRootDSE;
        break;
    } //end of switch

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;       

    switch(mState)
    {
    case kSearchingAuthDN :
        {
            nsCAutoString authDN;
            rv = aMessage->GetDn(authDN);
            if(NS_SUCCEEDED(rv) && !authDN.IsEmpty())
                mAuthDN = authDN.get();
        }
        break;
    case kSearchingRootDSE:
        rv = ParseRootDSEEntry(aMessage);
        break;
    case kFindingChanges:
        rv = ParseChangeLogEntries(aMessage);
        break;
    // fall through since we only add (for updates we delete and add)
    case kReplicatingChanges:
    case kReplicatingAll :
        return nsAbLDAPProcessReplicationData::OnLDAPSearchEntry(aMessage);
    }

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    PRInt32 errorCode;
    
    nsresult rv = aMessage->GetErrorCode(&errorCode);

    if(NS_SUCCEEDED(rv))
    {
        if(errorCode == nsILDAPErrors::SUCCESS || errorCode == nsILDAPErrors::SIZELIMIT_EXCEEDED) {
            switch(mState) {
            case kSearchingAuthDN :
                rv = OnSearchAuthDNDone();
                break;
            case kSearchingRootDSE:
             {
                // before starting the changeLog check the DB file, if its not there or bogus
                // we need to create a new one and set to all.
                nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
                if(NS_FAILED(rv)) 
                    break;
                nsFileSpec* dbPath;
                rv = abSession->GetUserProfileDirectory(&dbPath);
                if(NS_FAILED(rv)) 
                    break;
                (*dbPath) += mDirServerInfo->replInfo->fileName;
                if (!dbPath->Exists() || !dbPath->GetFileSize()) 
                    mUseChangeLog = PR_FALSE;
                delete dbPath;

                // open / create the AB here since it calls Done,
                // just return from here. 
                if (mUseChangeLog)
                   rv = OpenABForReplicatedDir(PR_FALSE);
                else
                   rv = OpenABForReplicatedDir(PR_TRUE);
                if (NS_FAILED(rv))
                   return rv;
                
                // now start the appropriate query
                rv = OnSearchRootDSEDone();
                break;
             }
            case kFindingChanges:
                rv = OnFindingChangesDone();
                // if success we return from here since
                // this changes state to kReplicatingChanges
                // and it falls thru into the if clause below.
                if (NS_SUCCEEDED(rv))
                    return rv;
                break;
            case kReplicatingAll :
                return nsAbLDAPProcessReplicationData::OnLDAPSearchResult(aMessage);
            } // end of switch
        }
        else
            rv = NS_ERROR_FAILURE;
        // if one of the changed entry in changelog is not found, 
        // continue with replicating the next one.
        if(mState == kReplicatingChanges)
            rv = OnReplicatingChangeDone();
    } // end of outer if

    if(NS_FAILED(rv))
        Abort();

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::GetAuthData()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (!wwatch)
        return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIAuthPrompt> dialog;
    nsresult rv = wwatch->GetNewAuthPrompter(0, getter_AddRefs(dialog));
    if (NS_FAILED(rv))
        return rv;
    if (!dialog) 
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsILDAPURL> url;
    rv = mQuery->GetReplicationURL(getter_AddRefs(url));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString serverUri;
    rv = url->GetSpec(serverUri);
    if (NS_FAILED(rv)) 
        return rv;

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED (rv)) 
        return rv ;
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle("chrome://messenger/locale/addressbook/addressBook.properties", getter_AddRefs(bundle));
    if (NS_FAILED (rv)) 
        return rv ;

    nsXPIDLString title;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("AuthDlgTitle").get(), getter_Copies(title));
    if (NS_FAILED (rv)) 
        return rv ;

    nsXPIDLString desc;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("AuthDlgDesc").get(), getter_Copies(desc));
    if (NS_FAILED (rv)) 
        return rv ;

    nsXPIDLString username;
    nsXPIDLString password;
    PRBool btnResult = PR_FALSE;
	rv = dialog->PromptUsernameAndPassword(title, desc, 
                                            NS_ConvertUTF8toUCS2(serverUri).get(), 
                                            nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                            getter_Copies(username), getter_Copies(password), 
                                            &btnResult);
    if(NS_SUCCEEDED(rv) && btnResult) {
        CopyUTF16toUTF8(username, mAuthUserID);
        CopyUTF16toUTF8(password, mAuthPswd);
        mDirServerInfo->enableAuth=PR_TRUE;
        mDirServerInfo->savePassword=PR_TRUE;
    }
    else
        rv = NS_ERROR_FAILURE;

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnSearchAuthDNDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsCOMPtr<nsILDAPURL> url;
    nsresult rv = mQuery->GetReplicationURL(getter_AddRefs(url));
    if(NS_SUCCEEDED(rv))
        rv = mQuery->ConnectToLDAPServer(url, mAuthDN);
    if(NS_SUCCEEDED(rv)) {
        mState = kAuthenticatedBinding;
        PR_FREEIF(mDirServerInfo->authDn);
        mDirServerInfo->authDn=ToNewCString(mAuthDN);
    }

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::ParseRootDSEEntry(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // populate the RootDSEChangeLogEntry
    CharPtrArrayGuard attrs;
    nsresult rv = aMessage->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    // no attributes !!!
    if(NS_FAILED(rv)) 
        return rv;

    for(PRInt32 i=attrs.GetSize()-1; i >= 0; i--) {
        PRUnicharPtrArrayGuard vals;
        rv = aMessage->GetValues(attrs.GetArray()[i], vals.GetSizeAddr(), vals.GetArrayAddr());
        if(NS_FAILED(rv))
            continue;
        if(vals.GetSize()) {
            if (!PL_strcasecmp(attrs[i], "changelog"))
                CopyUTF16toUTF8(vals[0], mRootDSEEntry.changeLogDN);
            if (!PL_strcasecmp(attrs[i], "firstChangeNumber"))
                mRootDSEEntry.firstChangeNumber = atol(NS_LossyConvertUCS2toASCII(vals[0]).get());
            if (!PL_strcasecmp(attrs[i], "lastChangeNumber"))
                mRootDSEEntry.lastChangeNumber = atol(NS_LossyConvertUCS2toASCII(vals[0]).get());
            if (!PL_strcasecmp(attrs[i], "dataVersion"))
                CopyUTF16toUTF8(vals[0], mRootDSEEntry.dataVersion);
        }
    }

    if((mRootDSEEntry.lastChangeNumber > 0) 
        && (mDirServerInfo->replInfo->lastChangeNumber < mRootDSEEntry.lastChangeNumber)
        && (mDirServerInfo->replInfo->lastChangeNumber > mRootDSEEntry.firstChangeNumber) 
      )
        mUseChangeLog = PR_TRUE;

    if(mRootDSEEntry.lastChangeNumber && (mDirServerInfo->replInfo->lastChangeNumber == mRootDSEEntry.lastChangeNumber)) {
        Done(PR_TRUE); // we are up to date no need to replicate, db not open yet so call Done
        return NS_OK;
    }

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnSearchRootDSEDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    if(mUseChangeLog) {
        rv = mChangeLogQuery->QueryChangeLog(mRootDSEEntry.changeLogDN, mRootDSEEntry.lastChangeNumber);
        if (NS_FAILED(rv))
           return rv;
        mState = kFindingChanges;
        if(mListener)
            mListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, PR_FALSE);
    }
    else {
        rv = mQuery->QueryAllEntries();
        if (NS_FAILED(rv))
           return rv;
        mState = kReplicatingAll;
        if(mListener)
            mListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, PR_TRUE);
    }

    mDirServerInfo->replInfo->lastChangeNumber = mRootDSEEntry.lastChangeNumber;
    PR_FREEIF(mDirServerInfo->replInfo->dataVersion);
    mDirServerInfo->replInfo->dataVersion = ToNewCString(mRootDSEEntry.dataVersion);

    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::ParseChangeLogEntries(nsILDAPMessage *aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    // populate the RootDSEChangeLogEntry
    CharPtrArrayGuard attrs;
    nsresult rv = aMessage->GetAttributes(attrs.GetSizeAddr(), attrs.GetArrayAddr());
    // no attributes
    if(NS_FAILED(rv)) 
        return rv;

    nsAutoString targetDN;
    UpdateOp operation = NO_OP;
    for(PRInt32 i = attrs.GetSize()-1; i >= 0; i--) {
        PRUnicharPtrArrayGuard vals;
        rv = aMessage->GetValues(attrs.GetArray()[i], vals.GetSizeAddr(), vals.GetArrayAddr());
        if(NS_FAILED(rv))
            continue;
        if(vals.GetSize()) {
            if (!PL_strcasecmp(attrs[i], "targetdn"))
                targetDN = vals[0];
            if (!PL_strcasecmp(attrs[i], "changetype")) {
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("add"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_ADD;
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("modify"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_MODIFY;
                if (!Compare(nsDependentString(vals[0]), NS_LITERAL_STRING("delete"), nsCaseInsensitiveStringComparator()))
                    operation = ENTRY_DELETE;
            }
        }
    }

    mChangeLogEntriesCount++;
    if(!(mChangeLogEntriesCount % 10)) { // inform the listener every 10 entries
        mListener->OnProgressChange(nsnull,nsnull,mChangeLogEntriesCount, -1, mChangeLogEntriesCount, -1);
        // in case if the LDAP Connection thread is starved and causes problem
        // uncomment this one and try.
        // PR_Sleep(PR_INTERVAL_NO_WAIT); // give others a chance
    }

#ifdef DEBUG_rdayal
    printf ("ChangeLog Replication : Updated Entry : %s for OpType : %u\n", 
                                    NS_ConvertUCS2toUTF8(targetDN).get(), operation);
#endif

    switch(operation) {
    case ENTRY_ADD:
        // add the DN to the add list if not already in the list
        if(!(mEntriesToAdd.IndexOf(targetDN) >= 0))
            mEntriesToAdd.AppendString(targetDN);
        break;
    case ENTRY_DELETE:
        // donot check the return here since delete may fail if 
        // entry deleted in changelog doesnot exist in DB 
        // for e.g if the user specifies a filter, so go next entry
        DeleteCard(targetDN);
        break;
    case ENTRY_MODIFY:
        // for modify, delte the entry from DB and add updated entry
        // we do this since we cannot access the changes attribs of changelog
        rv = DeleteCard(targetDN);
        if (NS_SUCCEEDED(rv)) 
            if(!(mEntriesToAdd.IndexOf(targetDN) >= 0))
                mEntriesToAdd.AppendString(targetDN);
        break;
    default:
        // should not come here, would come here only
        // if the entry is not a changeLog entry
        NS_WARNING("nsAbLDAPProcessChangeLogData::ParseChangeLogEntries"
           "Not an changelog entry");
    }

    // go ahead processing the next entry,  a modify or delete DB operation 
    // can 'correctly' fail if the entry is not present in the DB. 
    // eg : in case a filter is specified.
    return NS_OK;
}

nsresult nsAbLDAPProcessChangeLogData::OnFindingChangesDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

#ifdef DEBUG_rdayal
    printf ("ChangeLog Replication : Finding Changes Done \n");
#endif

    nsresult rv = NS_OK;

    // no entries to add/update (for updates too we delete and add) entries,
    // we took care of deletes in ParseChangeLogEntries, all Done!
    mEntriesAddedQueryCount = mEntriesToAdd.Count();
    if(mEntriesAddedQueryCount <= 0) {
        if(mReplicationDB && mDBOpen) {
            // close the DB, no need to commit since we have not made
            // any changes yet to the DB.
            rv = mReplicationDB->Close(PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB Close(no commit) on Success failed");
            mDBOpen = PR_FALSE;
            // once are done with the replication file, delete the backup file
            if(mBackupReplicationFile) {
                rv = mBackupReplicationFile->Remove(PR_FALSE);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Replication BackupFile Remove on Success failed");
            }
        }
        Done(PR_TRUE);
        return NS_OK;
    }

    // decrement the count first to get the correct array element
    mEntriesAddedQueryCount--;
    rv = mChangeLogQuery->QueryChangedEntries(NS_ConvertUCS2toUTF8(*(mEntriesToAdd[mEntriesAddedQueryCount])));
    if (NS_FAILED(rv))
        return rv;

    if(mListener && NS_SUCCEEDED(rv))
        mListener->OnStateChange(nsnull, nsnull, nsIWebProgressListener::STATE_START, PR_TRUE);

    mState = kReplicatingChanges;
    return rv;
}

nsresult nsAbLDAPProcessChangeLogData::OnReplicatingChangeDone()
{
    if(!mInitialized) 
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;

    if(!mEntriesAddedQueryCount)
    {
        if(mReplicationDB && mDBOpen) {
            rv = mReplicationDB->Close(PR_TRUE); // commit and close the DB
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication DB Close (commit) on Success failed");
            mDBOpen = PR_FALSE;
        }
        // once we done with the replication file, delete the backup file
        if(mBackupReplicationFile) {
            rv = mBackupReplicationFile->Remove(PR_FALSE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Replication BackupFile Remove on Success failed");
        }
        Done(PR_TRUE);  // all data is recieved
        return NS_OK;
    }

    // remove the entry already added from the list and query the next one
    if(mEntriesAddedQueryCount < mEntriesToAdd.Count() && mEntriesAddedQueryCount >= 0)
        mEntriesToAdd.RemoveStringAt(mEntriesAddedQueryCount);
    mEntriesAddedQueryCount--;
    rv = mChangeLogQuery->QueryChangedEntries(NS_ConvertUCS2toUTF8(*(mEntriesToAdd[mEntriesAddedQueryCount])));

    return rv;
}


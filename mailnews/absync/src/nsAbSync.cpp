/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsAbSync.h"
#include "prmem.h"
#include "nsAbSyncCID.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "nsIAddrBookSession.h"
#include "nsAbBaseCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "nsSyncDecoderRing.h"
#include "plstr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "nsMsgI18N.h"
#include "nsIScriptGlobalObject.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"

static NS_DEFINE_CID(kCAbSyncPostEngineCID, NS_ABSYNC_POST_ENGINE_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAbCardPropertyCID, NS_ABCARDPROPERTY_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAbSync, nsIAbSync)

nsAbSync::nsAbSync()
{
  NS_INIT_ISUPPORTS();

  // For listener array stuff...
  mListenerArrayCount = 0;
  mListenerArray = nsnull;
  mStringBundle = nsnull;
  mRootDocShell = nsnull;
  mUserName = nsnull;

  InternalInit();
  InitSchemaColumns();
}

void
nsAbSync::InternalInit()
{
  /* member initializers and constructor code */
  mCurrentState = nsIAbSyncState::nsIAbSyncIdle;
  mTransactionID = 100;
  mPostEngine = nsnull;

  mAbSyncPort = 5000;
  mAbSyncAddressBook = nsnull;
  mAbSyncAddressBookFileName = nsnull;
  mHistoryFile = nsnull;
  mOldSyncMapingTable = nsnull;
  mNewSyncMapingTable = nsnull;
  mNewServerTable = nsnull;

  mLastChangeNum = 1;

  mLocale.AssignWithConversion("");
  mDeletedRecordTags = nsnull;
  mDeletedRecordValues = nsnull;
  mNewRecordTags = nsnull;
  mNewRecordValues = nsnull;

  mPhoneTypes = nsnull;
  mPhoneValues = nsnull;

  mLockFile = nsnull;
  mLastSyncFailed = PR_FALSE;

  mCrashTableSize = 0;
  mCrashTable = nsnull;
}

nsresult
nsAbSync::CleanServerTable(nsVoidArray *aArray)
{
  if (!aArray)
    return NS_OK;

  for (PRInt32 i=0; i<aArray->Count(); i++)
  {
    syncMappingRecord *tRec = (syncMappingRecord *)aArray->ElementAt(i);
    if (!tRec)
      continue;

    nsCRT::free((char *)tRec);
  }

  delete aArray;
  return NS_OK;
}

nsresult
nsAbSync::InternalCleanup(nsresult aResult)
{
  /* cleanup code */
  DeleteListeners();

  PR_FREEIF(mAbSyncAddressBook);
  PR_FREEIF(mAbSyncAddressBookFileName);

  PR_FREEIF(mOldSyncMapingTable);
  PR_FREEIF(mNewSyncMapingTable);

  PR_FREEIF(mCrashTable);

  CleanServerTable(mNewServerTable);

  if (mHistoryFile)
    mHistoryFile->CloseStream();

  if (mLockFile)
  {
    mLockFile->CloseStream();

    if (NS_SUCCEEDED(aResult))
      mLockFile->Delete(PR_FALSE);
  }

  if (mDeletedRecordTags)
  {
    delete mDeletedRecordTags;
    mDeletedRecordTags = nsnull;
  }

  if (mDeletedRecordValues)
  {
    delete mDeletedRecordValues;
    mDeletedRecordValues = nsnull;
  }

  if (mNewRecordTags)
  {
    delete mNewRecordTags;
    mNewRecordTags = nsnull;
  }

  if (mNewRecordValues)
  {
    delete mNewRecordValues;
    mNewRecordValues = nsnull;
  }

  if (mPhoneTypes)
  {
    delete mPhoneTypes;
    mPhoneTypes = nsnull;
  }

  if (mPhoneValues)
  {
    delete mPhoneValues;
    mPhoneValues = nsnull;
  }

  return NS_OK;
}

nsAbSync::~nsAbSync()
{
  if (mPostEngine)
    mPostEngine->RemovePostListener((nsIAbSyncPostListener *)this);

  InternalCleanup(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsAbSync::InitSchemaColumns()
{
  // First, setup the local address book fields...
  mSchemaMappingList[0].abField = kFirstNameColumn;
  mSchemaMappingList[1].abField = kLastNameColumn;
  mSchemaMappingList[2].abField = kDisplayNameColumn;
  mSchemaMappingList[3].abField = kNicknameColumn;
  mSchemaMappingList[4].abField = kPriEmailColumn;
  mSchemaMappingList[5].abField = k2ndEmailColumn;
  mSchemaMappingList[6].abField = kPreferMailFormatColumn;
  mSchemaMappingList[7].abField = kWorkPhoneColumn;
  mSchemaMappingList[8].abField = kHomePhoneColumn;
  mSchemaMappingList[9].abField = kFaxColumn;
  mSchemaMappingList[10].abField = kPagerColumn;
  mSchemaMappingList[11].abField = kCellularColumn;
  mSchemaMappingList[12].abField = kHomeAddressColumn;
  mSchemaMappingList[13].abField = kHomeAddress2Column;
  mSchemaMappingList[14].abField = kHomeCityColumn;
  mSchemaMappingList[15].abField = kHomeStateColumn;
  mSchemaMappingList[16].abField = kHomeZipCodeColumn;
  mSchemaMappingList[17].abField = kHomeCountryColumn;
  mSchemaMappingList[18].abField = kWorkAddressColumn;
  mSchemaMappingList[19].abField = kWorkAddress2Column;
  mSchemaMappingList[20].abField = kWorkCityColumn;
  mSchemaMappingList[21].abField = kWorkStateColumn;
  mSchemaMappingList[22].abField = kWorkZipCodeColumn;
  mSchemaMappingList[23].abField = kWorkCountryColumn;
  mSchemaMappingList[24].abField = kJobTitleColumn;
  mSchemaMappingList[25].abField = kDepartmentColumn;
  mSchemaMappingList[26].abField = kCompanyColumn;
  mSchemaMappingList[27].abField = kWebPage1Column;
  mSchemaMappingList[28].abField = kWebPage2Column;
  mSchemaMappingList[29].abField = kBirthYearColumn;
  mSchemaMappingList[30].abField = kBirthMonthColumn;
  mSchemaMappingList[31].abField = kBirthDayColumn;
  mSchemaMappingList[32].abField = kCustom1Column;
  mSchemaMappingList[33].abField = kCustom2Column;
  mSchemaMappingList[34].abField = kCustom3Column;
  mSchemaMappingList[35].abField = kCustom4Column;
  mSchemaMappingList[36].abField = kNotesColumn;
  mSchemaMappingList[37].abField = kLastModifiedDateColumn;

  // Now setup the server fields...
  mSchemaMappingList[0].serverField = kServerFirstNameColumn;
  mSchemaMappingList[1].serverField = kServerLastNameColumn;
  mSchemaMappingList[2].serverField = kServerDisplayNameColumn;
  mSchemaMappingList[3].serverField = kServerNicknameColumn;
  mSchemaMappingList[4].serverField = kServerPriEmailColumn;
  mSchemaMappingList[5].serverField = kServer2ndEmailColumn;
  mSchemaMappingList[6].serverField = kServerPlainTextColumn;
  mSchemaMappingList[7].serverField = kServerWorkPhoneColumn;
  mSchemaMappingList[8].serverField = kServerHomePhoneColumn;
  mSchemaMappingList[9].serverField = kServerFaxColumn;
  mSchemaMappingList[10].serverField = kServerPagerColumn;
  mSchemaMappingList[11].serverField = kServerCellularColumn;
  mSchemaMappingList[12].serverField = kServerHomeAddressColumn;
  mSchemaMappingList[13].serverField = kServerHomeAddress2Column;
  mSchemaMappingList[14].serverField = kServerHomeCityColumn;
  mSchemaMappingList[15].serverField = kServerHomeStateColumn;
  mSchemaMappingList[16].serverField = kServerHomeZipCodeColumn;
  mSchemaMappingList[17].serverField = kServerHomeCountryColumn;
  mSchemaMappingList[18].serverField = kServerWorkAddressColumn;
  mSchemaMappingList[19].serverField = kServerWorkAddress2Column;
  mSchemaMappingList[20].serverField = kServerWorkCityColumn;
  mSchemaMappingList[21].serverField = kServerWorkStateColumn;
  mSchemaMappingList[22].serverField = kServerWorkZipCodeColumn;
  mSchemaMappingList[23].serverField = kServerWorkCountryColumn;
  mSchemaMappingList[24].serverField = kServerJobTitleColumn;
  mSchemaMappingList[25].serverField = kServerDepartmentColumn;
  mSchemaMappingList[26].serverField = kServerCompanyColumn;
  mSchemaMappingList[27].serverField = kServerWebPage1Column;
  mSchemaMappingList[28].serverField = kServerWebPage2Column;
  mSchemaMappingList[29].serverField = kServerBirthYearColumn;
  mSchemaMappingList[30].serverField = kServerBirthMonthColumn;
  mSchemaMappingList[31].serverField = kServerBirthDayColumn;
  mSchemaMappingList[32].serverField = kServerCustom1Column;
  mSchemaMappingList[33].serverField = kServerCustom2Column;
  mSchemaMappingList[34].serverField = kServerCustom3Column;
  mSchemaMappingList[35].serverField = kServerCustom4Column;
  mSchemaMappingList[36].serverField = kServerNotesColumn;
  mSchemaMappingList[37].serverField = kServerLastModifiedDateColumn;

  return NS_OK;
}

nsresult
nsAbSync::DisplayErrorMessage(const PRUnichar * msg)
{
  nsresult rv = NS_OK;

  if ((!msg) || (!*msg))
    return NS_ERROR_INVALID_ARG;

  if (mRootDocShell)
  {
    nsCOMPtr<nsIPrompt> dialog;
    dialog = do_GetInterface(mRootDocShell, &rv);
    if (dialog)
    {
      rv = dialog->Alert(nsnull, msg);
      rv = NS_OK;
    }
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  // If we failed before, fall back to the non-parented modal dialog
  if (NS_FAILED(rv))
  {
    nsCOMPtr<nsIPrompt> dialog;
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch)
      wwatch->GetNewPrompter(0, getter_AddRefs(dialog));

    if (!dialog)
      return NS_ERROR_FAILURE;
    rv = dialog->Alert(nsnull, msg);
  }

  return rv;
}

nsresult
nsAbSync::SetDOMWindow(nsIDOMWindowInternal *aWindow)
{
	if (!aWindow)
		return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface((nsISupports *)aWindow));
  nsCOMPtr<nsIDocShell> docShell;
  if (globalScript)
    globalScript->GetDocShell(getter_AddRefs(docShell));
  
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));  
  if(docShellAsItem)
  {
    nsCOMPtr<nsIDocShellTreeItem> rootAsItem;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(rootAsItem));
    
    nsCOMPtr<nsIDocShellTreeNode> rootAsNode(do_QueryInterface(rootAsItem));
    nsCOMPtr<nsIDocShell> temp_docShell(do_QueryInterface(rootAsItem));
    mRootDocShell = temp_docShell;
  }

  return rv;
}

/* void SetAbSyncUser (in string aUser); */
NS_IMETHODIMP 
nsAbSync::SetAbSyncUser(const char *aUser)
{
  if (aUser)
    mUserName = nsCRT::strdup(aUser);
  return NS_OK;
}

/* void AddSyncListener (in nsIAbSyncListener aListener); */
NS_IMETHODIMP nsAbSync::AddSyncListener(nsIAbSyncListener *aListener)
{
  if ( (mListenerArrayCount > 0) || mListenerArray )
  {
    ++mListenerArrayCount;
    mListenerArray = (nsIAbSyncListener **) 
                  PR_Realloc(*mListenerArray, sizeof(nsIAbSyncListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      mListenerArray[mListenerArrayCount - 1] = aListener;
      return NS_OK;
    }
  }
  else
  {
    mListenerArrayCount = 1;
    mListenerArray = (nsIAbSyncListener **) PR_Malloc(sizeof(nsIAbSyncListener *) * mListenerArrayCount);
    if (!mListenerArray)
      return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::memset(mListenerArray, 0, (sizeof(nsIAbSyncListener *) * mListenerArrayCount));
  
    mListenerArray[0] = aListener;
    NS_ADDREF(mListenerArray[0]);
    return NS_OK;
  }
}

/* void RemoveSyncListener (in nsIAbSyncListener aListener); */
NS_IMETHODIMP nsAbSync::RemoveSyncListener(nsIAbSyncListener *aListener)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] == aListener)
    {
      NS_RELEASE(mListenerArray[i]);
      mListenerArray[i] = nsnull;
      return NS_OK;
    }

  return NS_ERROR_INVALID_ARG;
}

nsresult
nsAbSync::DeleteListeners()
{
  if ( (mListenerArray) && (*mListenerArray) )
  {
    PRInt32 i;
    for (i=0; i<mListenerArrayCount; i++)
    {
      NS_RELEASE(mListenerArray[i]);
    }
    
    PR_FREEIF(mListenerArray);
  }

  mListenerArrayCount = 0;
  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnStartAuthOperation(void)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartAuthOperation();

  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnStopAuthOperation(nsresult aStatus, const PRUnichar *aMsg, const char *aCookie)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopAuthOperation(aStatus, aMsg, aCookie);

  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnStartSync(PRInt32 aTransactionID, const PRUint32 aMsgSize)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStartOperation(aTransactionID, aMsgSize);

  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnProgress(aTransactionID, aProgress, aProgressMax);

  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnStatus(PRInt32 aTransactionID, const PRUnichar *aMsg)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStatus(aTransactionID, aMsg);

  return NS_OK;
}

nsresult
nsAbSync::NotifyListenersOnStopSync(PRInt32 aTransactionID, nsresult aStatus, 
                                    const PRUnichar *aMsg)
{
  PRInt32 i;
  for (i=0; i<mListenerArrayCount; i++)
    if (mListenerArray[i] != nsnull)
      mListenerArray[i]->OnStopOperation(aTransactionID, aStatus, aMsg);

  return NS_OK;
}


/**
  * Notify the observer that the AB Sync Authorization operation has begun. 
  *
  */
NS_IMETHODIMP 
nsAbSync::OnStartAuthOperation(void)
{
  NotifyListenersOnStartAuthOperation();
  return NS_OK;
}

/**
   * Notify the observer that the AB Sync operation has been completed.  
   * 
   * This method is called regardless of whether the the operation was 
   * successful.
   * 
   *  aTransactionID    - the ID for this particular request
   *  aStatus           - Status code for the sync request
   *  aMsg              - A text string describing the error (if any).
   *  aCookie           - hmmm...cooookies!
   */
NS_IMETHODIMP 
nsAbSync::OnStopAuthOperation(nsresult aStatus, const PRUnichar *aMsg, const char *aCookie)
{
  NotifyListenersOnStopAuthOperation(aStatus, aMsg, aCookie);
  return NS_OK;
}

/* void OnStartOperation (in PRInt32 aTransactionID, in PRUint32 aMsgSize); */
NS_IMETHODIMP nsAbSync::OnStartOperation(PRInt32 aTransactionID, PRUint32 aMsgSize)
{
  NotifyListenersOnStartSync(aTransactionID, aMsgSize);
  return NS_OK;
}

/* void OnProgress (in PRInt32 aTransactionID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP nsAbSync::OnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  NotifyListenersOnProgress(aTransactionID, aProgress, aProgressMax);
  return NS_OK;
}

/* void OnStatus (in PRInt32 aTransactionID, in wstring aMsg); */
NS_IMETHODIMP nsAbSync::OnStatus(PRInt32 aTransactionID, const PRUnichar *aMsg)
{
  NotifyListenersOnStatus(aTransactionID, aMsg);
  return NS_OK;
}

/* void OnStopOperation (in PRInt32 aTransactionID, in nsresult aStatus, in wstring aMsg, out string aProtocolResponse); */
NS_IMETHODIMP nsAbSync::OnStopOperation(PRInt32 aTransactionID, nsresult aStatus, const PRUnichar *aMsg, const char *aProtocolResponse)
{
  nsresult    rv = aStatus;

  //
  // Now, figure out what the server told us to do with the sync operation.
  //
  if ( (aProtocolResponse) && (NS_SUCCEEDED(aStatus)) )
    rv = ProcessServerResponse(aProtocolResponse);

  NotifyListenersOnStopSync(aTransactionID, rv, aMsg);
  InternalCleanup(aStatus);

  mCurrentState = nsIAbSyncState::nsIAbSyncIdle;

#ifdef DEBUG_rhp
  printf("ABSYNC: OnStopOperation: Status = %d\n", aStatus);
#endif
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
//
// This is the implementation of the actual sync logic. 
//
////////////////////////////////////////////////////////////////////////////////////////

/* PRInt32 GetCurrentState (); */
NS_IMETHODIMP nsAbSync::GetCurrentState(PRInt32 *_retval)
{
  *_retval = mCurrentState;
  return NS_OK;
}

/* void PerformAbSync (out PRInt32 aTransactionID); */
NS_IMETHODIMP nsAbSync::CancelAbSync()
{
  if (!mPostEngine)
    return NS_ERROR_FAILURE;

  return mPostEngine->CancelAbSync();
}

/* void PerformAbSync (out PRInt32 aTransactionID); */
NS_IMETHODIMP nsAbSync::PerformAbSync(nsIDOMWindowInternal *aDOMWindow, PRInt32 *aTransactionID)
{
  nsresult      rv;
  char          *protocolRequest = nsnull;
  char          *prefixStr = nsnull;
  char          *clientIDStr = nsnull;

  // We'll need later...
  SetDOMWindow(aDOMWindow);

  // If we are already running...don't let anything new start...
  if (mCurrentState != nsIAbSyncState::nsIAbSyncIdle)
    return NS_ERROR_FAILURE;

  // Make sure everything is set back to NULL's
  InternalInit();

  // Ok, now get the server/port number we should be hitting with 
  // this request as well as the local address book we will be 
  // syncing with...
  //
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
  NS_ENSURE_SUCCESS(rv, rv);

  prefs->CopyCharPref("mail.absync.address_book",     &mAbSyncAddressBook);
  prefs->GetIntPref  ("mail.absync.last_change",      &mLastChangeNum);
  if (NS_FAILED(prefs->GetIntPref("mail.absync.port", &mAbSyncPort)))
    mAbSyncPort = 5000;

  // More sanity...
  if (mLastChangeNum == 0)
    mLastChangeNum = 1;

  // Get the string arrays setup for the phone numbers...
  mPhoneTypes = new nsStringArray();
  mPhoneValues = new nsStringArray();

  // Ok, we need to see if a particular address book was in the prefs
  // If not, then we will use the default, but if there was one specified, 
  // we need to do a prefs lookup and get the file name of interest
  // The pref format is: user_pref("ldap_2.servers.SherrysAddressBook.filename", "abook-1.mab");
  //
  if ( (mAbSyncAddressBook) && (*mAbSyncAddressBook) )
  {
    nsCString prefId("ldap_2.servers.");
    prefId.Append(mAbSyncAddressBook);
    prefId.Append(".filename");

    prefs->CopyCharPref(prefId.get(), &mAbSyncAddressBookFileName);
  }

  mTransactionID++;

  // First, analyze the situation locally and build up a string that we
  // will need to post.
  //
  rv = AnalyzeTheLocalAddressBook();
  if (NS_FAILED(rv))
    goto EarlyExit;

  // We can keep this object around for reuse...
  if (!mPostEngine)
  {
    rv = nsComponentManager::CreateInstance(kCAbSyncPostEngineCID, NULL, NS_GET_IID(nsIAbSyncPostEngine), getter_AddRefs(mPostEngine));
    NS_ENSURE_SUCCESS(rv, rv);

    mPostEngine->AddPostListener((nsIAbSyncPostListener *)this);
  }

  rv = mPostEngine->BuildMojoString(mRootDocShell, &clientIDStr);
  if (NS_FAILED(rv) || (!clientIDStr))
    goto EarlyExit;

  if (mPostString.IsEmpty())
    prefixStr = PR_smprintf("last=%u&protocol=%s&client=%s&ver=%s", 
                            mLastChangeNum, ABSYNC_PROTOCOL, 
                            clientIDStr, ABSYNC_VERSION);
  else
    prefixStr = PR_smprintf("last=%u&protocol=%s&client=%s&ver=%s&", 
                            mLastChangeNum, ABSYNC_PROTOCOL, 
                            clientIDStr, ABSYNC_VERSION);

  if (!prefixStr)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    OnStopOperation(mTransactionID, NS_ERROR_OUT_OF_MEMORY, nsnull, nsnull);
    goto EarlyExit;
  }

  mPostString.Insert(NS_ConvertASCIItoUCS2(prefixStr), 0);
  nsCRT::free(prefixStr);

  protocolRequest = ToNewCString(mPostString);
  if (!protocolRequest)
    goto EarlyExit;

  // Ok, FIRE!
  rv = mPostEngine->SendAbRequest(nsnull, mAbSyncPort, protocolRequest, mTransactionID, mRootDocShell, mUserName);
  if (NS_SUCCEEDED(rv))
  {
    mCurrentState = nsIAbSyncState::nsIAbSyncRunning;
  }
  else
  {
    OnStopOperation(mTransactionID, rv, nsnull, nsnull);
    goto EarlyExit;
  }

EarlyExit:
  PR_FREEIF(protocolRequest);
  PR_FREEIF(clientIDStr);

  if (NS_FAILED(rv))
    InternalCleanup(rv);
  return rv;
}

NS_IMETHODIMP 
nsAbSync::OpenAB(char *aAbName, nsIAddrDatabase **aDatabase)
{
	if (!aDatabase)
    return NS_ERROR_FAILURE;

	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
    if (!aAbName)
      (*dbPath) += "abook.mab";
    else
      (*dbPath) += aAbName;

		nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
		         do_GetService(kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, aDatabase, PR_TRUE);
	}
  else
    rv = NS_ERROR_FAILURE;

  delete dbPath;

  return rv;
}

NS_IMETHODIMP    
nsAbSync::GenerateProtocolForCard(nsIAbCard *aCard, PRBool aAddId, nsString &protLine)
{
  PRUnichar     *aName = nsnull;
  nsString      tProtLine;
  PRInt32       phoneCount = 1;
  PRBool        foundPhone = PR_FALSE;
  const char    *phoneType;

  protLine = NS_ConvertASCIItoUCS2("");

  if (aAddId)
  {
    PRUint32    aKey;
	  nsresult rv = NS_OK;

    nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(aCard, &rv)); 
    NS_ENSURE_SUCCESS(rv, rv);
    if (NS_FAILED(dbcard->GetKey(&aKey)))
      return NS_ERROR_FAILURE;

#ifdef DEBUG_rhp
  printf("ABSYNC: GENERATING PROTOCOL FOR CARD - Address Book Card Key: %d\n", aKey);
#endif

    char *tVal = PR_smprintf("%d", (aKey * -1));
    if (tVal)
    {
      tProtLine.Append(NS_ConvertASCIItoUCS2("%26cid%3D"));
      tProtLine.Append(NS_ConvertASCIItoUCS2(tVal));
      nsCRT::free(tVal);
    }
  }

  for (PRInt32 i=0; i<kMaxColumns; i++)
  {
    if (NS_SUCCEEDED(aCard->GetCardValue(mSchemaMappingList[i].abField, &aName)) && (aName) && (*aName))
    {
      // These are unknown tags we are omitting...
      if (!nsCRT::strncasecmp(mSchemaMappingList[i].serverField, "OMIT:", 5))
        continue;

      // Reset this flag...
      foundPhone = PR_FALSE;
      // If this is a phone number, we have to special case this because
      // phone #'s are handled differently than all other tags...sigh!
      //
      if (!nsCRT::strncasecmp(mSchemaMappingList[i].abField, kWorkPhoneColumn, nsCRT::strlen(kWorkPhoneColumn)))
      {
        foundPhone = PR_TRUE;
        phoneType = ABSYNC_WORK_PHONE_TYPE;
      }
      else if (!nsCRT::strncasecmp(mSchemaMappingList[i].abField, kHomePhoneColumn, nsCRT::strlen(kHomePhoneColumn)))
      {
        foundPhone = PR_TRUE;
        phoneType = ABSYNC_HOME_PHONE_TYPE;
      }
      else if (!nsCRT::strncasecmp(mSchemaMappingList[i].abField, kFaxColumn, nsCRT::strlen(kFaxColumn)))
      {
        foundPhone = PR_TRUE;
        phoneType = ABSYNC_FAX_PHONE_TYPE;
      }
      else if (!nsCRT::strncasecmp(mSchemaMappingList[i].abField, kPagerColumn, nsCRT::strlen(kPagerColumn)))
      {
        foundPhone = PR_TRUE;
        phoneType = ABSYNC_PAGER_PHONE_TYPE;
      }
      else if (!nsCRT::strncasecmp(mSchemaMappingList[i].abField, kCellularColumn, nsCRT::strlen(kCellularColumn)))
      {
        foundPhone = PR_TRUE;
        phoneType = ABSYNC_CELL_PHONE_TYPE;
      }

      if (foundPhone)
      {
        char *pVal = PR_smprintf("phone%d", phoneCount);
        if (pVal)
        {
          char *utfString = ToNewUTF8String(nsDependentString(aName));

          // Now, URL Encode the value string....
          char *myTStr = nsEscape(utfString, url_Path);
          if (myTStr)
          {
            PR_FREEIF(utfString);
            utfString = myTStr;
          }

          tProtLine.Append(NS_ConvertASCIItoUCS2("&"));
          tProtLine.Append(NS_ConvertASCIItoUCS2(pVal));
          tProtLine.Append(NS_ConvertASCIItoUCS2("="));
          if (utfString)
          {
            tProtLine.Append(NS_ConvertASCIItoUCS2(utfString));
            PR_FREEIF(utfString);
          }
          else
            tProtLine.Append(aName);
          tProtLine.Append(NS_ConvertASCIItoUCS2("&"));
          tProtLine.Append(NS_ConvertASCIItoUCS2(pVal));
          tProtLine.Append(NS_ConvertASCIItoUCS2("_type="));
          tProtLine.Append(NS_ConvertASCIItoUCS2(phoneType));
          PR_FREEIF(pVal);
          phoneCount++;
        }
      }
      else    // Good ole' normal tag...
      {
        char *utfString = ToNewUTF8String(nsDependentString(aName));

        // Now, URL Encode the value string....
        char *myTStr = nsEscape(utfString, url_Path);
        if (myTStr)
        {
          PR_FREEIF(utfString);
          utfString = myTStr;
        }

        tProtLine.Append(NS_ConvertASCIItoUCS2("&"));
        tProtLine.Append(NS_ConvertASCIItoUCS2(mSchemaMappingList[i].serverField));
        tProtLine.Append(NS_ConvertASCIItoUCS2("="));
        if (utfString)
        {
          tProtLine.Append(NS_ConvertASCIItoUCS2(utfString));
          PR_FREEIF(utfString);
        }
        else
          tProtLine.Append(aName);
      }

      PR_FREEIF(aName);
    }
  }

  if (!tProtLine.IsEmpty())
  {
    // Now, check if this is that flag for the plain text email selection...if so,
    // then tack this information on as well...
    PRUint32 format = nsIAbPreferMailFormat::unknown;
    if (NS_SUCCEEDED(aCard->GetPreferMailFormat(&format)))
    {
      if (format != nsIAbPreferMailFormat::html)
        aName = ToNewUnicode(NS_LITERAL_STRING("0"));
      else  
        aName = ToNewUnicode(NS_LITERAL_STRING("1"));
    
      // Just some sanity...
      if (aName)
      {
        char *utfString = ToNewUTF8String(nsDependentString(aName));
      
        // Now, URL Encode the value string....
        char *myTStr = nsEscape(utfString, url_Path);
        if (myTStr)
        {
          PR_FREEIF(utfString);
          utfString = myTStr;
        }
      
        tProtLine.Append(NS_ConvertASCIItoUCS2("&"));
        tProtLine.Append(NS_ConvertASCIItoUCS2(kServerPlainTextColumn));
        tProtLine.Append(NS_ConvertASCIItoUCS2("="));
        if (utfString)
        {
          tProtLine.Append(NS_ConvertASCIItoUCS2(utfString));
          PR_FREEIF(utfString);
        }
        else
          tProtLine.Append(aName);
      
        PR_FREEIF(aName);
      }
    }

    char *tLine = ToNewCString(tProtLine);
    if (!tLine)
      return NS_ERROR_OUT_OF_MEMORY;

    char *escData = nsEscape(tLine, url_Path);
    if (escData)
    {
      tProtLine = NS_ConvertASCIItoUCS2(escData);
    }

    PR_FREEIF(tLine);
    PR_FREEIF(escData);
    protLine = tProtLine;
  }

  return NS_OK;
}

long
GetCRC(char *str)
{
  cm_t      crcModel;
  p_cm_t    p = &crcModel;
  
  /*
    Name   : Crc-32
    Width  : 32
    Poly   : 04C11DB7
    Init   : FFFFFFFF
    RefIn  : True
    RefOut : True
    XorOut : FFFFFFFF
    Check  : CBF43926
  */
  p->cm_width = 32;
  p->cm_poly = 0x4C11DB7;
  p->cm_init = 0xFFFFFFFF;
  p->cm_refin = PR_TRUE;
  p->cm_refot = PR_TRUE;
  p->cm_xorot = 0xFFFFFFFF;

  char *pChar = str;
  cm_ini( p);
  for (PRUint32 i = 0; i < nsCRT::strlen(str); i++, pChar++)
    cm_nxt( p, *pChar);
  
  return( cm_crc( p) );
}

PRBool          
nsAbSync::ThisCardHasChanged(nsIAbCard *aCard, syncMappingRecord *newSyncRecord, nsString &protLine)
{
  syncMappingRecord   *historyRecord = nsnull;
  PRUint32            counter = 0;
  nsString            tempProtocolLine;

  // First, null out the protocol return line
  protLine = NS_ConvertASCIItoUCS2("");

  // Use the localID for this entry to lookup the old history record in the 
  // cached array
  //
  if (mOldSyncMapingTable)
  {
    while (counter < mOldTableSize)
    {
      if (mOldSyncMapingTable[counter].localID == newSyncRecord->localID)
      {
        historyRecord = &(mOldSyncMapingTable[counter]);
        break;
      }

      counter++;
    }
  }

  //
  // Now, build up the compare protocol line for this entry.
  //
  if (NS_FAILED(GenerateProtocolForCard(aCard, PR_FALSE, tempProtocolLine)))
    return PR_FALSE;

  if (tempProtocolLine.IsEmpty())
    return PR_FALSE;

  // Get the CRC for this temp entry line...
  char *tLine = ToNewCString(tempProtocolLine);
  if (!tLine)
    return PR_FALSE;
  newSyncRecord->CRC = GetCRC(tLine);
  nsCRT::free(tLine);

  //
  // #define     SYNC_MODIFIED     0x0001    // Must modify record on server
  // #define     SYNC_ADD          0x0002    // Must add record to server   
  // #define     SYNC_DELETED      0x0004    // Must delete record from server
  // #define     SYNC_RETRY        0x0008    // Sent to server but failed...must retry!
  // #define     SYNC_RENUMBER     0x0010    // Renumber on the server
  //
  // If we have a history record, we need to carry over old items!
  if (historyRecord)
  {
    newSyncRecord->serverID = historyRecord->serverID;
    historyRecord->flags |= SYNC_PROCESSED;
  }

  // If this is a record that has changed from comparing CRC's, then
  // when can generate a line that should be tacked on to the output
  // going up to the server
  //
  if ( (!historyRecord) || (historyRecord->CRC != newSyncRecord->CRC) )
  {      
    PRUint32    aKey;

    if (!historyRecord)
    {
      newSyncRecord->flags |= SYNC_ADD;

	    nsresult rv = NS_OK;
      nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(aCard, &rv)); 
      NS_ENSURE_SUCCESS(rv, rv);
      if (NS_FAILED(dbcard->GetKey(&aKey)))
        return PR_FALSE;
      
      // Ugh...this should never happen...BUT??
      if (aKey <= 0)
        return PR_FALSE;

      // Needs to be negative, so make it so!
      char *tVal = PR_smprintf("%d", (aKey * -1));
      if (tVal)
      {
        protLine.Append(NS_ConvertASCIItoUCS2("%26cid%3D"));
        protLine.Append(NS_ConvertASCIItoUCS2(tVal));
        protLine.Append(tempProtocolLine);
        nsCRT::free(tVal);
      }
      else
      {
        return PR_FALSE;
      }
    }
    else
    {
      newSyncRecord->flags |= SYNC_MODIFIED;

      char *tVal2 = PR_smprintf("%d", historyRecord->serverID);
      if (tVal2)
      {
        protLine.Append(NS_ConvertASCIItoUCS2("%26id%3D"));
        protLine.Append(NS_ConvertASCIItoUCS2(tVal2));
        protLine.Append(tempProtocolLine);
        nsCRT::free(tVal2);
      }
      else
      {
        return PR_FALSE;
      }
    }

    return PR_TRUE;
  }
  else  // This is the same record as before.
  {
    return PR_FALSE;
  }

  return PR_FALSE;
}

char*
BuildSyncTimestamp(void)
{
  static char       result[75] = "";
	PRExplodedTime    now;
  char              buffer[128] = "";

  // Generate envelope line in format of:  From - Sat Apr 18 20:01:49 1998
  //
  // Use PR_FormatTimeUSEnglish() to format the date in US English format,
	// then figure out what our local GMT offset is, and append it (since
	// PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
	// per RFC 1123 (superceding RFC 822.)
  //
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
	PR_FormatTimeUSEnglish(buffer, sizeof(buffer),
						   "%a %b %d %H:%M:%S %Y",
						   &now);
  
  // This value must be in ctime() format, with English abbreviations.
	// PL_strftime("... %c ...") is no good, because it is localized.
  //
  PL_strcpy(result, "Last - ");
  PL_strcpy(result + 7, buffer);
  PL_strcpy(result + 7 + 24, CRLF);
  return result;
}

NS_IMETHODIMP
nsAbSync::AnalyzeAllRecords(nsIAddrDatabase *aDatabase, nsIAbDirectory *directory)
{
  nsresult                rv = NS_OK;
  nsIEnumerator           *cardEnum = nsnull;
  nsCOMPtr<nsISupports>   obj = nsnull;
  PRBool                  exists = PR_FALSE;
  PRUint32                readCount = 0;
  PRInt32                 readSize = 0;
  PRUint32                workCounter = 0;
  nsString                singleProtocolLine;

  // Init size vars...
  mOldTableSize = 0;
  mNewTableSize = 0;
  PR_FREEIF(mOldSyncMapingTable);
  PR_FREEIF(mNewSyncMapingTable);
  CleanServerTable(mNewServerTable);
  mCurrentPostRecord = 1;

#ifdef DEBUG_rhp
  printf("ABSYNC: AnalyzeAllRecords:\n");
#endif

  //
  // First thing we need to do is open the absync.dat file
  // to compare against the last sync operation.
  // 
  // we want <profile>/absync.dat
  //

  nsCOMPtr<nsIFile> historyFile;  
  nsCOMPtr<nsIFile> lockFile;
  
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(historyFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = historyFile->Append("absync.dat");
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Convert the rest of the code to use
  // nsIFile and avoid this conversion hack.
  do
  {
    nsXPIDLCString pathBuf;
    rv = historyFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) break;
    rv = NS_NewFileSpec(getter_AddRefs(mHistoryFile));
    if (NS_FAILED(rv)) break;
    rv = mHistoryFile->SetNativePath(pathBuf);
  }
  while (0);

  NS_ASSERTION(NS_SUCCEEDED(rv),"ab sync:  failed to specify history file");
  if (NS_FAILED(rv)) 
  {
    rv = NS_ERROR_FAILURE;
    goto GetOut;
  }

  // Now see if we actually have an old table to work with?
  // If not, this is the first sync operation.
  //
  //mOldSyncMapingTable = nsnull;
  //mNewSyncMapingTable = nsnull;
  mHistoryFile->Exists(&exists);
  
  // Do this here to be used in case of a crash recovery situation...
  rv = aDatabase->EnumerateCards(directory, &cardEnum);
  if (NS_FAILED(rv) || (!cardEnum))
  {
    rv = NS_ERROR_FAILURE;
    goto GetOut;
  }

  //
  // Now, lets get a lot smarter about failures that may happen during the sync
  // operation. If this happens, we should do all that is possible to prevent 
  // duplicate entries from getting stored in an address book. To that end, we
  // will create an "absync.lck" file that will only exist during the synchronization
  // operation. The only time the lock file should be on disk at this point is if the
  // last sync operation failed. If so, we should build a compare list of the current
  // address book and only insert entries that don't have matching CRC's
  //
  // Note: be very tolerant if any of this stuff fails ...
  
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(lockFile));
  if (NS_SUCCEEDED(rv)) 
  {
    rv = lockFile->Append((const char *)"absync.lck");
    if (NS_SUCCEEDED(rv)) 
    {
    
      // TODO: Convert the rest of the code to use
      // nsIFile and avoid this conversion hack.
      do
      {
        nsXPIDLCString pathBuf;
        rv = lockFile->GetPath(getter_Copies(pathBuf));
        if (NS_FAILED(rv)) break;
        rv = NS_NewFileSpec(getter_AddRefs(mLockFile));
        if (NS_FAILED(rv)) break;
        rv = mLockFile->SetNativePath(pathBuf);
      }
      while (0);
      
    if (NS_SUCCEEDED(rv)) 
    {
      PRBool      tExists = PR_FALSE;

      mLockFile->Exists(&tExists);
      if (tExists)
      {
        mLastSyncFailed = PR_TRUE;

        // Ok, we got here which means that the last operation must have failed and
        // we should build a table of the CRC's of the address book we have locally
        // and prevent us from adding the same person twice. Hope this works.
        //
        cardEnum->First();
        do
        {
          mCrashTableSize++;
        } while (NS_SUCCEEDED(cardEnum->Next()));

        mCrashTable = (syncMappingRecord *) PR_MALLOC(mCrashTableSize * sizeof(syncMappingRecord));
        if (!mCrashTable)
        {
          mCrashTableSize = 0;
        }
        else
        {
          // Init the memory!
          nsCRT::memset(mCrashTable, 0, (mCrashTableSize * sizeof(syncMappingRecord)) );
          nsString        tProtLine; 

          rv = NS_OK;  
          cardEnum->First();
          do
          {
            if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
              break;
            else
            {
              nsCOMPtr<nsIAbCard> card;
              card = do_QueryInterface(obj, &rv);
              if ( NS_SUCCEEDED(rv) && (card) )
              {
                // First, we need to fill out the localID for this entry. This should
                // be the ID from the local database for this card entry
                //
                PRUint32    aKey;
	              nsresult rv = NS_OK;
                nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv)); 
                if (NS_FAILED(rv) || !dbcard)
                  continue;
                if (NS_FAILED(dbcard->GetKey(&aKey)))
                  continue;

                // Ugh...this should never happen...BUT??
                if (aKey <= 0)
                  continue;

                // Ok, now get the data for this record....
                mCrashTable[workCounter].localID = aKey;
                if (NS_SUCCEEDED(GenerateProtocolForCard(card, PR_FALSE, tProtLine)))
                {
                  char    *tCRCLine = ToNewCString(tProtLine);
                  if (tCRCLine)
                  {
                    mCrashTable[workCounter].CRC = GetCRC(tCRCLine);
                    PR_FREEIF(tCRCLine);
                  }
                }
              }
            }

            workCounter++;
          } while (NS_SUCCEEDED(cardEnum->Next()));
        }
      }
      else  // If here, create the lock file
      {
        if (NS_SUCCEEDED(mLockFile->OpenStreamForWriting()))
        {
          char      *tMsg = BuildSyncTimestamp();
          PRInt32   tWriteSize;

          mLockFile->Write(tMsg, nsCRT::strlen(tMsg), &tWriteSize);
          mLockFile->CloseStream();
        }
      }
    }
  }  
  }  

  // If the old table exists, then we need to load it up!
  if (exists)
  {
    if (NS_SUCCEEDED(mHistoryFile->GetFileSize(&mOldTableSize)))
    {
      if (NS_FAILED(mHistoryFile->OpenStreamForReading()))
      {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto GetOut;
      }

      mOldSyncMapingTable = (syncMappingRecord *) PR_MALLOC(mOldTableSize);
      if (!mOldSyncMapingTable)
      {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto GetOut;
      }
      
      // Init the memory!
      nsCRT::memset(mOldSyncMapingTable, 0, mOldTableSize);

      // Now get the number of records in the table size!
      mOldTableSize /= sizeof(syncMappingRecord);

      // Now read the history file into memory!
      while (readCount < mOldTableSize)
      {
        //if (NS_FAILED(mHistoryFile->Read((char **)&(mOldSyncMapingTable[readCount]), 
        //                                 sizeof(syncMappingRecord), &readSize))
        //                                 || (readSize != sizeof(syncMappingRecord)))
        syncMappingRecord   *tRecord = &mOldSyncMapingTable[readCount];
        if (NS_FAILED(mHistoryFile->Read((char **)&tRecord, 
                                         sizeof(syncMappingRecord), &readSize))
                                         || (readSize != sizeof(syncMappingRecord)))
        {
          rv = NS_ERROR_OUT_OF_MEMORY;
          goto GetOut;
        }

#ifdef DEBUG_rhp
  printf("------ Entry #%d --------\n", readCount);
  printf("Old Sync Table: %d\n", mOldSyncMapingTable[readCount].serverID);
  printf("Old Sync Table: %d\n", mOldSyncMapingTable[readCount].localID);
  printf("Old Sync Table: %d\n", mOldSyncMapingTable[readCount].CRC);
  printf("Old Sync Table: %d\n", mOldSyncMapingTable[readCount].flags);
#endif

        readCount++;
      }
    }
  }

  //
  // Now create the NEW sync mapping table that we will use for this
  // current operation. First, we have to count the total number of
  // entries...ugh.
  //
  cardEnum->First();
  do
  {
    mNewTableSize++;
  } while (NS_SUCCEEDED(cardEnum->Next()));

  mNewSyncMapingTable = (syncMappingRecord *) PR_MALLOC(mNewTableSize * sizeof(syncMappingRecord));
  if (!mNewSyncMapingTable)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;;
    goto GetOut;
  }

  // Init the memory!
  nsCRT::memset(mNewSyncMapingTable, 0, (mNewTableSize * sizeof(syncMappingRecord)) );

  rv = NS_OK;  
  workCounter =0;
  cardEnum->First();
  do
  {
    if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
      break;
    else
    {
      nsCOMPtr<nsIAbCard> card;
      card = do_QueryInterface(obj, &rv);
      if ( NS_SUCCEEDED(rv) && (card) )
      {
        // First, we need to fill out the localID for this entry. This should
        // be the ID from the local database for this card entry
        //
        PRUint32    aKey;
	      nsresult rv = NS_OK;
        nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv)); 
        if (NS_FAILED(rv) || !dbcard)
          continue;
        if (NS_FAILED(dbcard->GetKey(&aKey)))
          continue;

        // Ugh...this should never happen...BUT??
        if (aKey <= 0)
          continue;

        mNewSyncMapingTable[workCounter].localID = aKey;

        if (ThisCardHasChanged(card, &(mNewSyncMapingTable[workCounter]), singleProtocolLine))
        {
          // If we get here, we should look at the flags in the mNewSyncMapingTable to see
          // what we should add to the protocol header area and then tack on the singleProtcolLine
          // we got back from this call.
          //
          // Need the separator for multiple operations...
          if (!mPostString.IsEmpty())
            mPostString.Append(NS_ConvertASCIItoUCS2("&"));

          if (mNewSyncMapingTable[workCounter].flags & SYNC_ADD)
          {
#ifdef DEBUG_rhp
  char *t = ToNewCString(singleProtocolLine);
  printf("ABSYNC: ADDING Card: %s\n", t);
  PR_FREEIF(t);
#endif
            char *tVal3 = PR_smprintf("%d", mCurrentPostRecord);
            if (tVal3)
            {
              mPostString.Append(NS_ConvertASCIItoUCS2(tVal3));
              mPostString.Append(NS_ConvertASCIItoUCS2("="));
            }

            mPostString.Append(NS_ConvertASCIItoUCS2(SYNC_ESCAPE_ADDUSER));
            mPostString.Append(singleProtocolLine);

            PR_FREEIF(tVal3);
            mCurrentPostRecord++;
          }
          else if (mNewSyncMapingTable[workCounter].flags & SYNC_MODIFIED)
          {
#ifdef DEBUG_rhp
  char *t = ToNewCString(singleProtocolLine);
  printf("ABSYNC: MODIFYING Card: %s\n", t);
  PR_FREEIF(t);
#endif
            char *tVal4 = PR_smprintf("%d", mCurrentPostRecord);
            if (tVal4)
            {
              mPostString.Append(NS_ConvertASCIItoUCS2(tVal4));
              mPostString.Append(NS_ConvertASCIItoUCS2("="));
            }

            mPostString.Append(NS_ConvertASCIItoUCS2(SYNC_ESCAPE_MOD));
            mPostString.Append(singleProtocolLine);
            PR_FREEIF(tVal4);
            mCurrentPostRecord++;
          }
        }
      }
    }

    workCounter++;
  } while (NS_SUCCEEDED(cardEnum->Next()));

  //
  // Now, when we get here, we should go through the old history and see if
  // there are records we need to delete since we didn't touch them when comparing
  // against the current address book
  //
  readCount = 0;
  while (readCount < mOldTableSize)
  {
    if (!(mOldSyncMapingTable[readCount].flags && SYNC_PROCESSED))
    {
      // Need the separator for multiple operations...
      if (!mPostString.IsEmpty())
        mPostString.Append(NS_ConvertASCIItoUCS2("&"));

      char *tVal = PR_smprintf("%d", mOldSyncMapingTable[readCount].serverID);
      if (tVal)
      {
#ifdef DEBUG_rhp
  printf("ABSYNC: DELETING Card: %d\n", mOldSyncMapingTable[readCount].serverID);
#endif

        char *tVal2 = PR_smprintf("%d", mCurrentPostRecord);
        if (tVal2)
        {
          mPostString.Append(NS_ConvertASCIItoUCS2(tVal2));
          mPostString.Append(NS_ConvertASCIItoUCS2("="));
        }

        mPostString.Append(NS_ConvertASCIItoUCS2(SYNC_ESCAPE_DEL));
        mPostString.Append(NS_ConvertASCIItoUCS2("%26id="));
        mPostString.Append(NS_ConvertASCIItoUCS2(tVal));
        nsCRT::free(tVal);
        nsCRT::free(tVal2);
        mCurrentPostRecord++;
      }
    }

    readCount++;
  }

GetOut:
  if (cardEnum)
    delete cardEnum;

  if (mHistoryFile)
    mHistoryFile->CloseStream();

  if (NS_FAILED(rv))
  {
    mOldTableSize = 0;
    mNewTableSize = 0;
    PR_FREEIF(mOldSyncMapingTable);
    PR_FREEIF(mNewSyncMapingTable);
  }

  // Ok, get out!
  return rv;
}

nsresult
nsAbSync::AnalyzeTheLocalAddressBook()
{
  // Time to build the mPostString 
  //
  nsresult        rv = NS_OK;
  nsIAddrDatabase *aDatabase = nsnull;
 
  // Get the address book entry
  nsCOMPtr <nsIRDFResource>     resource = nsnull;
  nsCOMPtr <nsIAbDirectory>     directory = nsnull;

  // Init to null...
  mPostString.Assign(NS_ConvertASCIItoUCS2(""));

  // Now, open the database...for now, make it the default
  rv = OpenAB(mAbSyncAddressBookFileName, &aDatabase);
  if (NS_FAILED(rv))
    return rv;

  // Get the RDF service...
  nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // this should not be hardcoded to abook.mab
  // this works for any address book...not sure why
  // absync on go againt abook.mab - candice
  rv = rdfService->GetResource(kPersonalAddressbookUri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) 
    goto EarlyExit;
  
  // query interface 
  directory = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // Ok, Now we need to analyze the records in the database against
  // the sync history file and see what has changed. If it has changed,
  // then we need to create the mPostString protocol stuff to make 
  // the changes
  //
  rv = AnalyzeAllRecords(aDatabase, directory);

EarlyExit:
  // Database is open...make sure to close it
  if (aDatabase)
  {
    aDatabase->Close(PR_TRUE);
  }
  NS_IF_RELEASE(aDatabase);
  return rv;
}

nsresult
nsAbSync::PatchHistoryTableWithNewID(PRInt32 clientID, PRInt32 serverID, PRInt32 aMultiplier)
{
  for (PRUint32 i = 0; i < mNewTableSize; i++)
  {
    if (mNewSyncMapingTable[i].localID == (clientID * aMultiplier))
    {
#ifdef DEBUG_rhp
  printf("ABSYNC: PATCHING History Table - Client: %d - Server: %d\n", clientID, serverID);
#endif

      mNewSyncMapingTable[i].serverID = serverID;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

///////////////////////////////////////////////
// The following is for protocol parsing
///////////////////////////////////////////////
#define     SERVER_ERROR              "err "

#define     SERVER_OP_RETURN          "~op_return"
#define     SERVER_OP_RETURN_LOCALE   "dlocale="
#define     SERVER_OP_RETURN_RENAME   "op=ren"
#define     SERVER_OP_RETURN_CID      "cid="
#define     SERVER_OP_RETURN_SID      "sid="

#define     SERVER_NEW_RECORDS        "~new_records_section "

#define     SERVER_DELETED_RECORDS    "~deleted_records_section "

#define     SERVER_LAST_CHANGED       "~last_chg"

nsresult
nsAbSync::ExtractInteger(char *aLine, char *aTag, char aDelim, PRInt32 *aRetVal)
{
  *aRetVal = 0;

  if ((!aLine) || (!aTag))
    return NS_ERROR_FAILURE;

  char *fLoc = PL_strstr(aLine, aTag);
  if (!fLoc)
    return NS_ERROR_FAILURE;

  fLoc += nsCRT::strlen(aTag);
  if (!*fLoc)
    return NS_ERROR_FAILURE;

  char *endLoc = fLoc;
  while ( (*endLoc) && (*endLoc != aDelim) )
    endLoc++;

  // Terminate it temporarily...
  char  saveLoc = '\0';
  if (*endLoc)
  {
    saveLoc = *endLoc;
    *endLoc = '\0';
  }

  *aRetVal = atoi(fLoc);
  *endLoc = saveLoc;
  return NS_OK;
}

char *
nsAbSync::ExtractCharacterString(char *aLine, char *aTag, char aDelim)
{
  if ((!aLine) || (!aTag))
    return nsnull;

  char *fLoc = PL_strstr(aLine, aTag);
  if (!fLoc)
    return nsnull;

  fLoc += nsCRT::strlen(aTag);
  if (!*fLoc)
    return nsnull;

  char *endLoc = fLoc;
  while ( (*endLoc) && (*endLoc != aDelim) )
    endLoc++;

  // Terminate it temporarily...
  char  saveLoc = '\0';
  if (*endLoc)
  {
    saveLoc = *endLoc;
    *endLoc = '\0';
  }

  char *returnValue = nsCRT::strdup(fLoc);
  *endLoc = saveLoc;
  return returnValue;
}

// Return true if the server returned an error...
PRBool
nsAbSync::ErrorFromServer(char **errString)
{
  if (!nsCRT::strncasecmp(mProtocolOffset, SERVER_ERROR, nsCRT::strlen(SERVER_ERROR)))
  {
    *errString = mProtocolOffset + nsCRT::strlen(SERVER_ERROR);
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

// If this returns true, we are done with the data...
PRBool
nsAbSync::EndOfStream()
{
  if ( (!*mProtocolOffset) )
    return PR_TRUE;
  else
    return PR_FALSE;
}

nsresult        
nsAbSync::ProcessOpReturn()
{
  char  *workLine = nsnull;

  while ((workLine = ExtractCurrentLine()) != nsnull)
  {
    if (!*workLine)   // end of this section
      break;

    // Find the right tag and do something with it!
    // First a locale for the data
    if (!nsCRT::strncasecmp(workLine, SERVER_OP_RETURN_LOCALE, 
                            nsCRT::strlen(SERVER_OP_RETURN_LOCALE)))
    {
      char *locale = workLine;
      locale += nsCRT::strlen(SERVER_OP_RETURN_LOCALE);
      if (*locale)
        mLocale = NS_ConvertASCIItoUCS2(locale);
    }
    // this is for renaming records from the server...
    else if (!nsCRT::strncasecmp(workLine, SERVER_OP_RETURN_RENAME, 
                                 nsCRT::strlen(SERVER_OP_RETURN_RENAME)))
    {
      char *renop = workLine;
      renop += nsCRT::strlen(SERVER_OP_RETURN_RENAME);
      if (*renop)
      {
        nsresult  rv = NS_OK;
        PRInt32   clientID, serverID;

        rv  = ExtractInteger(renop, SERVER_OP_RETURN_CID, ' ', &clientID);
        rv += ExtractInteger(renop, SERVER_OP_RETURN_SID, ' ', &serverID);
        if (NS_SUCCEEDED(rv))
        {
          PatchHistoryTableWithNewID(clientID, serverID, -1);
        }
      }
    }

    PR_FREEIF(workLine);
  }

  return NS_OK;
}

nsresult
nsAbSync::LocateClientIDFromServerID(PRInt32 aServerID, PRInt32 *aClientID)
{
  PRUint32   i;

  for (i=0; i<mOldTableSize; i++)
  {
    if (mOldSyncMapingTable[i].serverID == aServerID)
    {
      *aClientID = mOldSyncMapingTable[i].localID;
      return NS_OK;
    }
  }

  for (i=0; i<mNewTableSize; i++)
  {
    if (mNewSyncMapingTable[i].serverID == aServerID)
    {
      *aClientID = mNewSyncMapingTable[i].localID;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsAbSync::DeleteCardByServerID(PRInt32 aServerID)
{
  nsIEnumerator           *cardEnum = nsnull;
  nsCOMPtr<nsISupports>   obj = nsnull;
  PRUint32                aKey;

  // First off, find the aServerID in the history database and find
  // the local client ID for this server ID
  //
  PRInt32   clientID;
  if (NS_FAILED(LocateClientIDFromServerID(aServerID, &clientID)))
  {
    return NS_ERROR_FAILURE;
  }

  // Time to find the entry to delete!
  //
  nsresult        rv = NS_OK;
  nsIAddrDatabase *aDatabase = nsnull;
 
  // Get the address book entry
  nsCOMPtr <nsIRDFResource>     resource = nsnull;
  nsCOMPtr <nsIAbDirectory>     directory = nsnull;

  // Now, open the database...for now, make it the default
  rv = OpenAB(mAbSyncAddressBookFileName, &aDatabase);
  if (NS_FAILED(rv))
    return rv;

  // Get the RDF service...
  nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // this should not be hardcoded to abook.mab
  // this works for any address book...not sure why
  // absync on go againt abook.mab - candice
  rv = rdfService->GetResource(kPersonalAddressbookUri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) 
    goto EarlyExit;
  
  // query interface 
  directory = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  rv = aDatabase->EnumerateCards(directory, &cardEnum);
  if (NS_FAILED(rv) || (!cardEnum))
  {
    rv = NS_ERROR_FAILURE;
    goto EarlyExit;
  }

  //
  // Now we have to find the entry and delete it from the database!
  //
  cardEnum->First();
  do
  {
    if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
      continue;
    else
    {
      nsCOMPtr<nsIAbCard> card;
      card = do_QueryInterface(obj, &rv);

	    nsresult rv = NS_OK;
      nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv)); 
      if (NS_FAILED(rv) || !dbcard)
        continue;
      if (NS_FAILED(dbcard->GetKey(&aKey)))
        continue;

      if ((PRInt32) aKey == clientID)
      {
        rv = aDatabase->DeleteCard(card, PR_TRUE);
        break;
      }
    }

  } while (NS_SUCCEEDED(cardEnum->Next()));

EarlyExit:
  if (cardEnum)
    delete cardEnum;

  // Database is open...make sure to close it
  if (aDatabase)
  {
    aDatabase->Close(PR_TRUE);
  }
  NS_IF_RELEASE(aDatabase);
  return rv;
}

nsresult        
nsAbSync::DeleteRecord()
{
  PRInt32     i = 0;
  nsresult    rv = NS_ERROR_FAILURE;

  for (i=0; i<mDeletedRecordValues->Count(); i+=mDeletedRecordTags->Count())
  {
    nsString *val = mDeletedRecordValues->StringAt(i);
    if ( (!val) || val->IsEmpty() )
      continue;

    PRInt32   aErrorCode;
    PRInt32 delID = val->ToInteger(&aErrorCode);
    if (NS_FAILED(aErrorCode))
      continue;

    rv = DeleteCardByServerID(delID);
  }

  return rv;
}

nsresult        
nsAbSync::DeleteList()
{
  PRInt32     i = 0;
  nsresult    rv = NS_ERROR_FAILURE;

  for (i=0; i<mDeletedRecordValues->Count(); i+=mDeletedRecordTags->Count())
  {
    nsString *val = mDeletedRecordValues->StringAt(i);
    if ( (!val) || val->IsEmpty() )
      continue;

    PRInt32   aErrorCode;
    PRInt32 delID = val->ToInteger(&aErrorCode);
    if (NS_FAILED(aErrorCode))
      continue;

    // RICHIE_TODO - Ok, delete a list by a server ID
  }

  return rv;
}

nsresult        
nsAbSync::DeleteGroup()
{
  PRInt32     i = 0;
  nsresult    rv = NS_ERROR_FAILURE;

  for (i=0; i<mDeletedRecordValues->Count(); i+=mDeletedRecordTags->Count())
  {
    nsString *val = mDeletedRecordValues->StringAt(i);
    if ( (!val) || val->IsEmpty() )
      continue;

    PRInt32   aErrorCode;
    PRInt32 delID = val->ToInteger(&aErrorCode);
    if (NS_FAILED(aErrorCode))
      continue;

    // RICHIE_TODO - Ok, delete a group by a server ID
  }

  return rv;
}

nsresult        
nsAbSync::ProcessDeletedRecords()
{
  char      *workLine;
  nsresult  rv = NS_OK;

  // Ok, first thing we need to do is get all of the tags for 
  // deleted records.
  //
  mDeletedRecordTags = new nsStringArray();
  if (!mDeletedRecordTags)
    return NS_ERROR_OUT_OF_MEMORY;

  mDeletedRecordValues = new nsStringArray();
  if (!mDeletedRecordValues)
    return NS_ERROR_OUT_OF_MEMORY;

  while ((workLine = ExtractCurrentLine()) != nsnull)
  {
    if (!*workLine)   // end of this section
      break;

    mDeletedRecordTags->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    PR_FREEIF(workLine);
  }

  // Now, see what the next line is...if its a CRLF, then we
  // really don't have anything to do here and we can just return
  //
  while ((workLine = ExtractCurrentLine()) != nsnull)
  {
    if (!*workLine)
     break;
    
    // Ok, if we are here, then we need to loop and get the values
    // for the tags in question starting at the second since the 
    // first has already been eaten
    //
    mDeletedRecordValues->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    for (PRInt32 i=0; i<mDeletedRecordTags->Count(); i++)
    {
      workLine = ExtractCurrentLine();
      if (!workLine)
        return NS_ERROR_FAILURE;
      
      mDeletedRecordValues->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    }
  }
  
  // Ok, now that we are here, we check to see if we have anything
  // in the records array. If we do, then deal with it!
  //
  // If nothing in the array...just return!
  if (mDeletedRecordValues->Count() == 0)
    return NS_OK;

  PRInt32 tType = DetermineTagType(mDeletedRecordTags);
  switch (tType) 
  {
  case SYNC_SINGLE_USER_TYPE:
    rv += DeleteRecord();
    break;

  case SYNC_MAILLIST_TYPE:
    rv += DeleteList();
    break;

  case SYNC_GROUP_TYPE:
    rv += DeleteGroup();
    break;
    
  case SYNC_UNKNOWN_TYPE:
  default:
    rv = NS_ERROR_FAILURE;
    break;
  }

  return rv;
}

nsresult        
nsAbSync::ProcessLastChange()
{
  char      *aLine = nsnull;
 
  if (EndOfStream())
    return NS_ERROR_FAILURE;

  if ( (aLine = ExtractCurrentLine()) != nsnull)
  {
    if (*aLine)
    {
      mLastChangeNum = atoi(aLine);
      PR_FREEIF(aLine);
    }
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

char *        
nsAbSync::ExtractCurrentLine()
{
  nsString    extractString; 

  while ( (*mProtocolOffset) && 
          ( (*mProtocolOffset != nsCRT::CR) && (*mProtocolOffset != nsCRT::LF) )
        )
  {
    extractString.Append(PRUnichar(*mProtocolOffset));
    mProtocolOffset++;
  }

  if (!*mProtocolOffset)
    return nsnull;
  else
  {
    while ( (*mProtocolOffset) && 
            (*mProtocolOffset == nsCRT::CR) )
            mProtocolOffset++;

    if (*mProtocolOffset == nsCRT::LF)
      mProtocolOffset++;

    char *tString = ToNewCString(extractString);
    if (tString)
    {
      char *ret = nsUnescape(tString);
      return ret;
    }
    else
      return nsnull;
  }
}

nsresult        
nsAbSync::AdvanceToNextLine()
{
  // First, find first nsCRT::CR or nsCRT::LF...
  while ( (*mProtocolOffset) && 
          ( (*mProtocolOffset != nsCRT::CR) && (*mProtocolOffset != nsCRT::LF) )
        )
  {
    mProtocolOffset++;
  }

  // now, make sure we are past the LF...
  if (*mProtocolOffset)
  {
    while ( (*mProtocolOffset) && 
            (*mProtocolOffset != nsCRT::LF) )
        mProtocolOffset++;
    
    if (*mProtocolOffset == nsCRT::LF)
      mProtocolOffset++;
  }

  return NS_OK;   // at end..but this is ok...
}

nsresult
nsAbSync::AdvanceToNextSection()
{
  // If we are sitting on a section...bump past it...
  mProtocolOffset++;
  while (!EndOfStream() && *mProtocolOffset != '~')
    AdvanceToNextLine();
  return NS_OK;
}

// See if we are sitting on a particular tag...and eat if if we are
PRBool          
nsAbSync::TagHit(const char *aTag, PRBool advanceToNextLine)
{
  if ((!aTag) || (!*aTag))
    return PR_FALSE;
  
  if (!nsCRT::strncasecmp(mProtocolOffset, aTag, nsCRT::strlen(aTag)))
  {
    if (advanceToNextLine)
      AdvanceToNextLine();
    else
      mProtocolOffset += nsCRT::strlen(aTag);
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

PRBool
nsAbSync::ParseNextSection()
{
  nsresult      rv = NS_OK;

  if (TagHit(SERVER_OP_RETURN, PR_TRUE))
    rv = ProcessOpReturn();
  else if (TagHit(SERVER_NEW_RECORDS, PR_TRUE))
    rv = ProcessNewRecords();
  else if (TagHit(SERVER_DELETED_RECORDS, PR_TRUE))
    rv = ProcessDeletedRecords();
  else if (TagHit(SERVER_LAST_CHANGED, PR_TRUE))
    rv = ProcessLastChange();
  else    // We shouldn't get here...but if we do...
    rv = AdvanceToNextSection();

  // If this is a failure, then get to the next section!
  if (NS_FAILED(rv))
    AdvanceToNextSection();

  return PR_TRUE;
}

//
// This is the response side of getting things back from the server
//
nsresult
nsAbSync::ProcessServerResponse(const char *aProtocolResponse)
{
  nsresult        rv = NS_OK;
  PRUint32        writeCount = 0;
  PRInt32         writeSize = 0;
  char            *errorString; 
  PRBool          parseOk = PR_TRUE;

  // If no response, then this is a problem...
  if (!aProtocolResponse)
  {
    PRUnichar   *outValue = GetString(NS_ConvertASCIItoUCS2("syncInvalidResponse").get());
    DisplayErrorMessage(outValue);
    PR_FREEIF(outValue);
    return NS_ERROR_FAILURE;
  }

  // Assign the vars...
  mProtocolResponse = (char *)aProtocolResponse;
  mProtocolOffset = (char *)aProtocolResponse;

  if (ErrorFromServer(&errorString))
  {
    PRUnichar   *outValue = GetString(NS_ConvertASCIItoUCS2("syncServerError").get());
    PRUnichar   *msgValue;

    msgValue = nsTextFormatter::smprintf(outValue, errorString);

    DisplayErrorMessage(msgValue);

    PR_FREEIF(outValue);
    PR_FREEIF(msgValue);

    return NS_ERROR_FAILURE;
  }

  while ( (!EndOfStream()) && (parseOk) )
  {
    parseOk = ParseNextSection();
  }

  // Now, write out the history file with the new information
  //
  if (!mHistoryFile)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto ExitEarly;
  }

  if (NS_FAILED(mHistoryFile->OpenStreamForWriting()))
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto ExitEarly;
  }

  // Ok, this handles the entries that we knew about before we started.
  while (writeCount < mNewTableSize)
  {
    // Sanity one more time...
    if (mNewSyncMapingTable[writeCount].serverID != 0)
    {
      if (NS_FAILED(mHistoryFile->Write((char *)&(mNewSyncMapingTable[writeCount]), 
                                       sizeof(syncMappingRecord), &writeSize))
                                       || (writeSize != sizeof(syncMappingRecord)))
      {
        rv = NS_ERROR_OUT_OF_MEMORY;;
        goto ExitEarly;
      }
    }

    writeCount++;
  }

  // These are entries that we got back from the server that are new
  // to us!
  writeCount = 0;
  if (mNewServerTable)
  {
    while (writeCount < (PRUint32) mNewServerTable->Count())
    {
      syncMappingRecord *tRec = (syncMappingRecord *)mNewServerTable->ElementAt(writeCount);
      if (!tRec)
        continue;

      if (NS_FAILED(mHistoryFile->Write((char *)(tRec), 
                                       sizeof(syncMappingRecord), &writeSize))
                                       || (writeSize != sizeof(syncMappingRecord)))
      {
        rv = NS_ERROR_OUT_OF_MEMORY;;
        goto ExitEarly;
      }

      writeCount++;
    }
  }

  if (mHistoryFile)
    mHistoryFile->CloseStream();

ExitEarly:
  if (mLastChangeNum > 1)
  {
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    if (NS_SUCCEEDED(rv) && prefs) 
    {
      prefs->SetIntPref("mail.absync.last_change", mLastChangeNum);
    }
  }

  return NS_OK;
}

PRInt32
nsAbSync::DetermineTagType(nsStringArray *aArray)
{
  PRBool    gotRecordID = PR_FALSE;
  PRBool    gotListID = PR_FALSE;
  PRBool    gotGroupID = PR_FALSE;

  for (PRInt32 i = 0; i<aArray->Count(); i++)
  {
    nsString *val = mNewRecordTags->StringAt(0);
    if ( (!val) || (val->IsEmpty()) )
      continue;

    if (val->CompareWithConversion("record_id") == 0)
      gotRecordID = PR_TRUE;
    else if (val->CompareWithConversion("list_id") == 0)
      gotListID = PR_TRUE;
    else if (val->CompareWithConversion("group_id") == 0)
      gotGroupID = PR_TRUE;
  }

  if (gotGroupID)
    return SYNC_GROUP_TYPE;
  else if (gotListID)
    return SYNC_MAILLIST_TYPE;
  else if (gotRecordID)
    return SYNC_SINGLE_USER_TYPE;
  else
    // If we get here, don't have a clue!
    return SYNC_UNKNOWN_TYPE;
}

nsresult        
nsAbSync::ProcessNewRecords()
{
  char      *workLine;
  nsresult  rv = NS_OK;

  // Ok, first thing we need to do is get all of the tags for 
  // new records. These are the server tags for address book fields.
  //
  mNewRecordTags = new nsStringArray();
  if (!mNewRecordTags)
    return NS_ERROR_OUT_OF_MEMORY;

  mNewRecordValues = new nsStringArray();
  if (!mNewRecordValues)
    return NS_ERROR_OUT_OF_MEMORY;

  while ((workLine = ExtractCurrentLine()) != nsnull)
  {
    if (!*workLine)   // end of this section
      break;

    mNewRecordTags->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    PR_FREEIF(workLine);
  }

  // Now, see what the next line is...if its a CRLF, then we
  // really don't have anything to do here and we can just return
  //
  while ((workLine = ExtractCurrentLine()) != nsnull)
  {
    if (!*workLine)
     break;
    
    // Ok, if we are here, then we need to loop and get the values
    // for the tags in question starting at the second since the 
    // first has already been eaten
    //
    mNewRecordValues->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    PR_FREEIF(workLine);
    for (PRInt32 i=0; i<(mNewRecordTags->Count()-1); i++)
    {
      workLine = ExtractCurrentLine();
      if (!workLine)
        return NS_ERROR_FAILURE;
      
      mNewRecordValues->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
      PR_FREEIF(workLine);
    }

    // Eat the CRLF!
    workLine = ExtractCurrentLine();
    PR_FREEIF(workLine);
  }
  
  // Ok, now that we are here, we need to figure out what type
  // of addition to the address book this is. 
  //
  // But first...sanity! If nothing in the array...just return!
  if (mNewRecordValues->Count() == 0)
    return NS_OK;

  PRInt32 tType = DetermineTagType(mNewRecordTags);
  switch (tType) 
  {
  case SYNC_SINGLE_USER_TYPE:
    rv = AddNewUsers();
    break;

  case SYNC_MAILLIST_TYPE:
    break;

  case SYNC_GROUP_TYPE:
    break;
    
  case SYNC_UNKNOWN_TYPE:
  default:
    return NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult
nsAbSync::FindCardByClientID(PRInt32           aClientID,
                             nsIAddrDatabase  *aDatabase,
                             nsIAbDirectory   *directory,
                             nsIAbCard        **aReturnCard)
{
  nsIEnumerator           *cardEnum = nsnull;
  nsCOMPtr<nsISupports>   obj = nsnull;
  PRUint32                aKey;
  nsresult                rv = NS_ERROR_FAILURE;

  // Time to find the entry to play with!
  //
  *aReturnCard = nsnull;
  rv = aDatabase->EnumerateCards(directory, &cardEnum);
  if (NS_FAILED(rv) || (!cardEnum))
  {
    rv = NS_ERROR_FAILURE;
    goto EarlyExit;
  }

  //
  // Now we have to find the entry and return it!
  //
  cardEnum->First();
  do
  {
    if (NS_FAILED(cardEnum->CurrentItem(getter_AddRefs(obj))))
      continue;
    else
    {
      nsCOMPtr<nsIAbCard> card;
      card = do_QueryInterface(obj, &rv);

      nsresult rv=NS_OK;
      nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv)); 
      if (NS_FAILED(rv) || !dbcard)
        continue;
      if (NS_FAILED(dbcard->GetKey(&aKey)))
        continue;

      // Found IT!
      if ((PRInt32) aKey == aClientID)
      {
        *aReturnCard = card;
        rv = NS_OK;
        break;
      }
    }

  } while (NS_SUCCEEDED(cardEnum->Next()));

EarlyExit:
  if (cardEnum)
    delete cardEnum;

  return rv;
}

//
// This routine will be used to hunt through an index of CRC's to see if this
// address book card has already been added to the local database. If so, don't
// duplicate!
// 
PRBool
nsAbSync::CardAlreadyInAddressBook(nsIAbCard        *newCard,
                                   PRInt32          *aClientID,
                                   ulong            *aRetCRC)
{
  PRUint32   i;
  nsString  tProtLine;
  PRBool    rVal = PR_FALSE;

  // First, generate a protocol line for this new card...
  if (NS_FAILED(GenerateProtocolForCard(newCard, PR_FALSE, tProtLine)))
    return PR_FALSE;
 
  char    *tCRCLine = ToNewCString(tProtLine);
  if (!tCRCLine)
    return PR_FALSE;

  ulong   workCRC = GetCRC(tCRCLine);
  for (i=0; i<mCrashTableSize; i++)
  {
    if (mCrashTable[i].CRC == workCRC)
    {
      *aClientID = mCrashTable[i].localID;
      *aRetCRC = workCRC;
      rVal = PR_TRUE;
    }
  }
  
  PR_FREEIF(tCRCLine);
  return rVal;
}

// 
// This will look for an entry in the address book for this particular person and return 
// the address book card and the client ID if found...otherwise, it will return ZERO
// 
PRInt32
nsAbSync::HuntForExistingABEntryInServerRecord(PRInt32          aPersonIndex, 
                                               nsIAddrDatabase  *aDatabase,
                                               nsIAbDirectory   *directory,
                                               PRInt32          *aServerID, 
                                               nsIAbCard        **newCard)
{
  PRInt32   clientID;
  PRInt32   j;

  //
  // Ok, first thing is to find out what the server ID is for this person?
  //
  *aServerID = 0;
  *newCard = nsnull;
  for (j = 0; j < mNewRecordTags->Count(); j++)
  {
    nsString *val = mNewRecordValues->StringAt((aPersonIndex*(mNewRecordTags->Count())) + j);
    if ( (val) && (!val->IsEmpty()) )
    {
      // See if this is the record_id...
      nsString *tagVal = mNewRecordTags->StringAt(j);
      if (tagVal->CompareWithConversion("record_id") == 0)
      {
        PRInt32 errorCode;
        *aServerID = val->ToInteger(&errorCode);
        break;
      }      
    }
  }

  // Hmm...no server ID...not good...well, better return anyway
  if (*aServerID == 0)
    return 0;

  // If didn't find the client ID, better bail out!
  if (NS_FAILED(LocateClientIDFromServerID(*aServerID, &clientID)))
    return 0;

  // Now, we have the clientID, need to find the address book card in the
  // database for this client ID...
  if (NS_FAILED(FindCardByClientID(clientID, aDatabase, directory, newCard)))
  {
    *aServerID = 0;
    return 0;
  }
  else
    return clientID;
}

nsresult        
nsAbSync::AddNewUsers()
{
  nsresult        rv = NS_OK;
  nsIAddrDatabase *aDatabase = nsnull;
  PRInt32         addCount = 0;
  PRInt32         i,j;
  PRInt32         serverID;
  PRInt32         localID;
  nsCOMPtr<nsIAbCard> newCard;
  nsIAbCard       *tCard = nsnull;
  nsString        tempProtocolLine;
  PRBool          isNewCard = PR_TRUE;
 
  // Get the address book entry
  nsCOMPtr <nsIRDFResource>     resource = nsnull;
  nsCOMPtr <nsIAbDirectory>     directory = nsnull;

  // Do a sanity check...if the numbers don't add up, then 
  // return an error
  //
  if ( (mNewRecordValues->Count() % mNewRecordTags->Count()) != 0)
    return NS_ERROR_FAILURE;

  // Get the add count...
  addCount = mNewRecordValues->Count() / mNewRecordTags->Count();

  // Now, open the database...for now, make it the default
  rv = OpenAB(mAbSyncAddressBookFileName, &aDatabase);
  if (NS_FAILED(rv))
    return rv;

  // Get the RDF service...
  nsCOMPtr<nsIRDFService> rdfService(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // this should not be hardcoded to abook.mab
  // this works for any address book...not sure why
  // absync on go againt abook.mab - candice
  rv = rdfService->GetResource(kPersonalAddressbookUri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) 
    goto EarlyExit;
  
  // query interface 
  directory = do_QueryInterface(resource, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // Create the new array for history if it is still null...
  if (!mNewServerTable)
  {
    mNewServerTable = new nsVoidArray();
    if (!mNewServerTable)
    {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto EarlyExit;
    }
  }

  //
  // 
  // Create the new card that we will eventually add to the 
  // database...
  //
  for (i = 0; i < addCount; i++)
  {    
    serverID = 0;     // for safety

    // 
    // Ok, if we are here, then we need to figure out if this is really a NEW card
    // or just an update of an existing card that is already on this client. 
    //
    // NOTE: i is the entry of the person we are working on!
    //
    localID = HuntForExistingABEntryInServerRecord(i, aDatabase, directory, &serverID, &tCard);
    if ( (localID > 0) && (nsnull != tCard))
    {
      // This is an existing entry in the local address book
      // We need to dig out the card entry or just assign something here
      newCard = tCard;
      isNewCard = PR_FALSE;
    }
    else
    {
      // This is a new entry!
      rv = nsComponentManager::CreateInstance(kAbCardPropertyCID, nsnull, NS_GET_IID(nsIAbCard), 
                                              getter_AddRefs(newCard));
      isNewCard = PR_TRUE;
    }

    if (NS_FAILED(rv) || !newCard)
    {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto EarlyExit;
    }

    for (j = 0; j < mNewRecordTags->Count(); j++)
    {
      nsString *val = mNewRecordValues->StringAt((i*(mNewRecordTags->Count())) + j);
      if ( (val) && (!val->IsEmpty()) )
      {
        // See if this is the record_id, keep it around for later...
        nsString *tagVal = mNewRecordTags->StringAt(j);
        if (tagVal->CompareWithConversion("record_id") == 0)
        {
          PRInt32 errorCode;
          serverID = val->ToInteger(&errorCode);

#ifdef DEBUG_rhp
  printf("ABSYNC: ADDING Card: %d\n", serverID);
#endif
        }

        // Ok, "val" could still be URL Encoded, so we need to decode
        // first and then pass into the call...
        //
        char *myTStr = ToNewCString(*val);
        if (myTStr)
        {
          char *ret = nsUnescape(myTStr);
          if (ret)
          {
            val->AssignWithConversion(ret);
            PR_FREEIF(ret);
          }
        }
        AddValueToNewCard(newCard, mNewRecordTags->StringAt(j), val);
      }
    }

    // Now do the phone numbers...they are special???
    ProcessPhoneNumbersTheyAreSpecial(newCard);

    // Ok, now that we are here, we should check if this is a recover from a crash situation.
    // If it is, then we should try to find the CRC for this entry in the local address book
    // and tweak a new flag if it is already there.
    //
    PRBool    cardAlreadyThere = PR_FALSE;
    ulong     tempCRC;

    if (mLastSyncFailed)
      cardAlreadyThere = CardAlreadyInAddressBook(newCard, &localID, &tempCRC);

    // Ok, now, lets be extra nice and see if we should build a display name for the
    // card.
    //
    PRUnichar   *tDispName = nsnull;
    if (NS_SUCCEEDED(newCard->GetCardValue(kDisplayNameColumn, &tDispName)))
    {
      if ( (!tDispName) || (!*tDispName) )
      {
        PRUnichar   *tFirstName = nsnull;
        PRUnichar   *tLastName = nsnull;
        nsString    tFullName;

        newCard->GetCardValue(kFirstNameColumn, &tFirstName);
        newCard->GetCardValue(kLastNameColumn, &tLastName);
  
        if (tFirstName)
        {
          tFullName.Append(tFirstName);
          if (tLastName)
            tFullName.Append(NS_ConvertASCIItoUCS2(" "));
        }

        if (tLastName)
          tFullName.Append(tLastName);

        PR_FREEIF(tFirstName);
        PR_FREEIF(tLastName);

        // Ok, now we should add a display name...
        newCard->SetDisplayName(tFullName.get());
      }
      else
        PR_FREEIF(tDispName);
    }

    // Ok, now we need to modify or add the card! ONLY IF ITS NOT THERE ALREADY
    if (!cardAlreadyThere)
    {
      if (!isNewCard)
        rv = aDatabase->EditCard(newCard, PR_TRUE);
      else
      {
        PRUint32  tID;
        rv = aDatabase->CreateNewCardAndAddToDBWithKey(newCard, PR_TRUE, &tID);
        localID = (PRInt32) tID;
      }
    }

    //
    // Now, calculate the NEW CRC for the new or updated card...
    //
    syncMappingRecord *newSyncRecord = (syncMappingRecord *)PR_Malloc(sizeof(syncMappingRecord));
    if (newSyncRecord)
    {
      if (NS_FAILED(GenerateProtocolForCard(newCard, PR_FALSE, tempProtocolLine)))
        continue;

      // Get the CRC for this temp entry line...
      char *tLine = ToNewCString(tempProtocolLine);
      if (!tLine)
        continue;

      if (!isNewCard)
      {
        // First try to patch the old table if this is an old card...if that
        // fails, then flip the flag to TRUE and have a new record created
        // in newSyncRecord
        //
        if (NS_FAILED(PatchHistoryTableWithNewID(localID, serverID, 1)))
          isNewCard = PR_TRUE;
      }

      if (isNewCard)
      {
        nsCRT::memset(newSyncRecord, 0, sizeof(syncMappingRecord));
        newSyncRecord->CRC = GetCRC(tLine);
        newSyncRecord->serverID = serverID;
        newSyncRecord->localID = localID;
        mNewServerTable->AppendElement((void *)newSyncRecord);
      }
      else
      {
        PR_FREEIF(newSyncRecord);
      }

      nsCRT::free(tLine);
    }

    newCard = nsnull;
  }

EarlyExit:
  // Database is open...make sure to close it
  if (aDatabase)
  {
    aDatabase->Close(PR_TRUE);
  }
  NS_IF_RELEASE(aDatabase);  
  return rv;
}

PRInt32 
nsAbSync::GetTypeOfPhoneNumber(nsString tagName)
{  
  nsString compValue = tagName;
  compValue.Append(NS_ConvertASCIItoUCS2("_type"));

  for (PRInt32 i=0; i<mPhoneTypes->Count(); i++)
  {
    nsString *val = mPhoneTypes->StringAt(i);
    if ( (!val) || val->IsEmpty() )
      continue;

    if (!compValue.CompareWithConversion(*val, PR_TRUE, compValue.Length()))
    {
      nsString phoneType;

      phoneType.Assign(*val);
      PRInt32 loc = phoneType.FindChar('=');
      if (loc == -1)
        continue;
      
      phoneType.Cut(0, loc+1);
      if (!phoneType.CompareWithConversion(ABSYNC_HOME_PHONE_TYPE, PR_TRUE, nsCRT::strlen(ABSYNC_HOME_PHONE_TYPE)))
        return ABSYNC_HOME_PHONE_ID;
      else if (!phoneType.CompareWithConversion(ABSYNC_WORK_PHONE_TYPE, PR_TRUE, nsCRT::strlen(ABSYNC_WORK_PHONE_TYPE)))
        return ABSYNC_WORK_PHONE_ID;
      else if (!phoneType.CompareWithConversion(ABSYNC_FAX_PHONE_TYPE, PR_TRUE, nsCRT::strlen(ABSYNC_FAX_PHONE_TYPE)))
        return ABSYNC_FAX_PHONE_ID;
      else if (!phoneType.CompareWithConversion(ABSYNC_PAGER_PHONE_TYPE, PR_TRUE, nsCRT::strlen(ABSYNC_PAGER_PHONE_TYPE)))
        return ABSYNC_PAGER_PHONE_ID;
      else if (!phoneType.CompareWithConversion(ABSYNC_CELL_PHONE_TYPE, PR_TRUE, nsCRT::strlen(ABSYNC_CELL_PHONE_TYPE)))
        return ABSYNC_CELL_PHONE_ID;
    }
  }

  // If we get here...drop back to the defaults...why, oh why is it this way?
  //
  if (!tagName.CompareWithConversion("phone1"))
    return ABSYNC_HOME_PHONE_ID;
  else if (!tagName.CompareWithConversion("phone2"))
    return ABSYNC_WORK_PHONE_ID;
  else if (!tagName.CompareWithConversion("phone3"))
    return ABSYNC_FAX_PHONE_ID;
  else if (!tagName.CompareWithConversion("phone4"))
    return ABSYNC_PAGER_PHONE_ID;
  else if (!tagName.CompareWithConversion("phone5"))
    return ABSYNC_CELL_PHONE_ID;

  return 0;
}

nsresult
nsAbSync::ProcessPhoneNumbersTheyAreSpecial(nsIAbCard *aCard)
{
  PRInt32   i;
  nsString  tagName;
  nsString  phoneNumber;
  PRInt32   loc;

  for (i=0; i<mPhoneValues->Count(); i++)
  {
    nsString *val = mPhoneValues->StringAt(i);
    if ( (!val) || val->IsEmpty() )
      continue;

    tagName.Assign(*val);
    phoneNumber.Assign(*val);
    loc = tagName.FindChar('=');
    if (loc == -1)
      continue;

    tagName.Cut(loc, (tagName.Length() - loc));
    phoneNumber.Cut(0, loc+1);

    PRInt32 pType = GetTypeOfPhoneNumber(tagName);
    if (pType == 0)
      continue;
    
    if (pType == ABSYNC_PAGER_PHONE_ID)
      aCard->SetPagerNumber(phoneNumber.get());
    else if (pType == ABSYNC_HOME_PHONE_ID)
      aCard->SetHomePhone(phoneNumber.get());
    else if (pType == ABSYNC_WORK_PHONE_ID)
      aCard->SetWorkPhone(phoneNumber.get());
    else if (pType == ABSYNC_FAX_PHONE_ID)
      aCard->SetFaxNumber(phoneNumber.get());
    else if (pType == ABSYNC_CELL_PHONE_ID)
      aCard->SetCellularNumber(phoneNumber.get());
  }

  return NS_OK;
}

nsresult
nsAbSync::AddValueToNewCard(nsIAbCard *aCard, nsString *aTagName, nsString *aTagValue)
{
  nsresult  rv = NS_OK;

  // 
  // First, we are going to special case the phone entries because they seem to
  // be handled differently than all other tags. All other tags are unique with no specifiers
  // as to their type...this is not the case with phone numbers.
  //
  PRUnichar aChar = '_';
  nsString  outValue;
  char      *tValue = nsnull;

  tValue = ToNewCString(*aTagValue);
  if (tValue)
  {
    rv = nsMsgI18NConvertToUnicode(nsCAutoString("UTF-8"), nsCAutoString(tValue), outValue);
    if (NS_SUCCEEDED(rv))
      aTagValue->Assign(outValue);
    PR_FREEIF(tValue);
  }

  if (!aTagName->CompareWithConversion("phone", PR_TRUE, 5))
  {
    nsString      tempVal;
    tempVal.Append(*aTagName);
    tempVal.Append(NS_ConvertASCIItoUCS2("="));
    tempVal.Append(*aTagValue);

    if (aTagName->FindChar(aChar) != -1)
      mPhoneTypes->AppendString(tempVal);
    else
      mPhoneValues->AppendString(tempVal);

    return NS_OK;
  }

  // Ok, we need to figure out what the tag name from the server maps to and assign
  // this value the new nsIAbCard
  //
  if (!aTagName->CompareWithConversion(kServerFirstNameColumn))
    aCard->SetFirstName(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerLastNameColumn))
    aCard->SetLastName(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerDisplayNameColumn))
    aCard->SetDisplayName(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerNicknameColumn))
    aCard->SetNickName(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerPriEmailColumn))
  {
#ifdef DEBUG_rhp
  char *t = ToNewCString(*aTagValue);
  printf("Email: %s\n", t);
  PR_FREEIF(t);
#endif
    aCard->SetPrimaryEmail(aTagValue->get());
  }
  else if (!aTagName->CompareWithConversion(kServer2ndEmailColumn))
    aCard->SetSecondEmail(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeAddressColumn))
    aCard->SetHomeAddress(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeAddress2Column))
    aCard->SetHomeAddress2(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeCityColumn))
    aCard->SetHomeCity(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeStateColumn))
    aCard->SetHomeState(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeZipCodeColumn))
    aCard->SetHomeZipCode(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerHomeCountryColumn))
    aCard->SetHomeCountry(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkAddressColumn))
    aCard->SetWorkAddress(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkAddress2Column))
    aCard->SetWorkAddress2(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkCityColumn))
    aCard->SetWorkCity(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkStateColumn))
    aCard->SetWorkState(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkZipCodeColumn))
    aCard->SetWorkZipCode(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWorkCountryColumn))
    aCard->SetWorkCountry(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerJobTitleColumn))
    aCard->SetJobTitle(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerNotesColumn))
    aCard->SetNotes(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWebPage1Column))
    aCard->SetWebPage1(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerDepartmentColumn))
    aCard->SetDepartment(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerCompanyColumn))
    aCard->SetCompany(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerWebPage2Column))
    aCard->SetWebPage2(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerCustom1Column))
    aCard->SetCustom1(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerCustom2Column))
    aCard->SetCustom2(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerCustom3Column))
    aCard->SetCustom3(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerCustom4Column))
    aCard->SetCustom4(aTagValue->get());
  else if (!aTagName->CompareWithConversion(kServerPlainTextColumn))
  {
    // This is plain text pref...have to add a little logic.
    if (!aTagValue->CompareWithConversion("1"))
      aCard->SetPreferMailFormat(nsIAbPreferMailFormat::html);
    else if (!aTagValue->CompareWithConversion("0"))
      aCard->SetPreferMailFormat(nsIAbPreferMailFormat::unknown);
  }

  return rv;
}

#define AB_STRING_URL       "chrome://messenger/locale/addressbook/absync.properties"

PRUnichar *
nsAbSync::GetString(const PRUnichar *aStringName)
{
	nsresult    res = NS_OK;
  PRUnichar   *ptrv = nsnull;

	if (!mStringBundle)
	{
		static const char propertyURL[] = AB_STRING_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = 
		         do_GetService(kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(mStringBundle));
		}
	}

	if (mStringBundle)
		res = mStringBundle->GetStringFromName(aStringName, &ptrv);

  if ( NS_SUCCEEDED(res) && (ptrv) )
    return ptrv;
  else
    return nsCRT::strdup(aStringName);
}

/************ UNUSED FOR NOW
aCard->SetBirthYear(aTagValue->get());
aCard->SetBirthMonth(aTagValue->get());
aCard->SetBirthDay(aTagValue->get());
char *kServerBirthYearColumn = "OMIT:BirthYear";
char *kServerBirthMonthColumn = "OMIT:BirthMonth";
char *kServerBirthDayColumn = "OMIT:BirthDay";
**********************************************/

/************* FOR MAILING LISTS
aCard->SetIsMailList(aTagValue->get());
************* FOR MAILING LISTS ***************/

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEscape.h"
#include "nsSyncDecoderRing.h"

static NS_DEFINE_CID(kCAbSyncPostEngineCID, NS_ABSYNC_POST_ENGINE_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAbSync, nsIAbSync)

nsAbSync::nsAbSync()
{
  NS_INIT_ISUPPORTS();

  /* member initializers and constructor code */
  mListenerArray = nsnull;
  mListenerArrayCount = 0;
  mCurrentState = nsIAbSyncState::nsIAbSyncIdle;
  mTransactionID = 100;
  mPostEngine = nsnull;

  mAbSyncServer = nsnull;
  mAbSyncPort = 5000;
  mAbSyncAddressBook = nsnull;
  mAbSyncAddressBookFileName = nsnull;
  mHistoryFile = nsnull;
  mOldSyncMapingTable = nsnull;
  mNewSyncMapingTable = nsnull;
  mNewServerTable = nsnull;

  mLastChangeNum = 1;
  mUserName = nsString("RHPizzarro").ToNewCString();

  InitSchemaColumns();
}

nsresult
nsAbSync::InternalCleanup()
{
  /* cleanup code */
  DeleteListeners();

  PR_FREEIF(mAbSyncServer);
  PR_FREEIF(mAbSyncAddressBook);
  PR_FREEIF(mAbSyncAddressBookFileName);

  PR_FREEIF(mOldSyncMapingTable);
  PR_FREEIF(mNewSyncMapingTable);
  PR_FREEIF(mNewServerTable);

  if (mHistoryFile)
    mHistoryFile->CloseStream();

  PR_FREEIF(mUserName);

  return NS_OK;
}

nsAbSync::~nsAbSync()
{

  if (mPostEngine)
    mPostEngine->RemovePostListener((nsIAbSyncPostListener *)this);

  InternalCleanup();
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
  mSchemaMappingList[6].abField = kPlainTextColumn;
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
nsAbSync::NotifyListenersOnStartSync(PRInt32 aTransactionID, PRUint32 aMsgSize)
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
  //
  // Now, figure out what the server told us to do with the sync operation.
  //
  if (aProtocolResponse)
    ProcessServerResponse(aProtocolResponse);

  NotifyListenersOnStopSync(aTransactionID, aStatus, aMsg);
  InternalCleanup();

  mCurrentState = nsIAbSyncState::nsIAbSyncIdle;
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
NS_IMETHODIMP nsAbSync::PerformAbSync(PRInt32 *aTransactionID)
{
  nsresult      rv;
  char          *postSpec = nsnull;
  char          *protocolRequest = nsnull;
  char          *prefixStr = nsnull;

  // If we are already running...don't let anything new start...
  if (mCurrentState != nsIAbSyncState::nsIAbSyncIdle)
    return NS_ERROR_FAILURE;

  // Ok, now get the server/port number we should be hitting with 
  // this request as well as the local address book we will be 
  // syncing with...
  //
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || !prefs) 
    return NS_ERROR_FAILURE;

  prefs->CopyCharPref("mail.absync.server",           &mAbSyncServer);
  prefs->CopyCharPref("mail.absync.address_book",     &mAbSyncAddressBook);
  prefs->GetIntPref  ("mail.absync.last_change",      &mLastChangeNum);
  if (NS_FAILED(prefs->GetIntPref("mail.absync.port", &mAbSyncPort)))
    mAbSyncPort = 5000;

  // Did we get sane values...
  if (!mAbSyncServer)
  {
    rv = NS_ERROR_FAILURE;
    goto EarlyExit;
  }

  postSpec = PR_smprintf("http://%s", mAbSyncServer);
  if (!postSpec)  
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto EarlyExit;
  }

  // Ok, we need to see if a particular address book was in the prefs
  // If not, then we will use the default, but if there was one specified, 
  // we need to do a prefs lookup and get the file name of interest
  // The pref format is: user_pref("ldap_2.servers.Richie.filename", "abook-1.mab");
  //
  if ( (mAbSyncAddressBook) && (*mAbSyncAddressBook) )
  {
    nsCString prefId = "ldap_2.servers.";
    prefId.Append(mAbSyncAddressBook);
    prefId.Append(".filename");

    prefs->CopyCharPref(prefId, &mAbSyncAddressBookFileName);
  }

  mTransactionID++;

  // First, analyze the situation locally and build up a string that we
  // will need to post.
  //
  rv = AnalyzeTheLocalAddressBook();
  if (NS_FAILED(rv))
    goto EarlyExit;

  //
  // if we have nothing in mPostString, then we can just return OK...no 
  // sync was needed
  //
  if (mPostString == "")
  {
    rv = NS_OK;
    OnStopOperation(mTransactionID, NS_OK, nsnull, nsnull);
    goto EarlyExit;
  }

  // We can keep this object around for reuse...
  if (!mPostEngine)
  {
    rv = nsComponentManager::CreateInstance(kCAbSyncPostEngineCID, NULL, NS_GET_IID(nsIAbSyncPostEngine), getter_AddRefs(mPostEngine));
    if ( NS_FAILED(rv) || (!mPostEngine) )
      return NS_ERROR_FAILURE;

    mPostEngine->AddPostListener((nsIAbSyncPostListener *)this);
  }

  // Ok, add the header to this protocol string information...
  prefixStr = PR_smprintf("last=%u&protocol=%d&client=2&ver=Demo&", mLastChangeNum, ABSYNC_PROTOCOL);
  if (!prefixStr)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    OnStopOperation(mTransactionID, NS_OK, nsnull, nsnull);
    goto EarlyExit;
  }

  mPostString.Insert(prefixStr, 0);
  nsCRT::free(prefixStr);

  protocolRequest = mPostString.ToNewCString();
  if (!protocolRequest)
    goto EarlyExit;

  // Ok, FIRE!
  rv = mPostEngine->SendAbRequest(postSpec, mAbSyncPort, protocolRequest, mTransactionID);
  if (NS_SUCCEEDED(rv))
    mCurrentState = nsIAbSyncState::nsIAbSyncRunning;

EarlyExit:
  PR_FREEIF(protocolRequest);
  PR_FREEIF(postSpec);

  if (NS_FAILED(rv))
    InternalCleanup();
  return rv;
}

NS_IMETHODIMP 
nsAbSync::OpenAB(char *aAbName, nsIAddrDatabase **aDatabase)
{
	if (!aDatabase)
    return NS_ERROR_FAILURE;

	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
    if (!aAbName)
      (*dbPath) += "abook.mab";
    else
      (*dbPath) += aAbName;

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(aDatabase), PR_TRUE);
	}
  else
    rv = NS_ERROR_FAILURE;

	return rv;
}

NS_IMETHODIMP    
nsAbSync::GenerateProtocolForCard(nsIAbCard *aCard, PRBool aAddId, nsString &protLine)
{
  PRUnichar     *aName = nsnull;
  nsString      tProtLine = "";

  if (aAddId)
  {
    PRUint32    aKey;
    if (NS_FAILED(aCard->GetKey(&aKey)))
      return NS_ERROR_FAILURE;

    char *tVal = PR_smprintf("%d", (aKey * -1));
    if (tVal)
    {
      tProtLine.Append("%26cid%3D");
      tProtLine.Append(tVal);
      nsCRT::free(tVal);
    }
  }

  for (PRInt32 i=0; i<kMaxColumns; i++)
  {
    if (NS_SUCCEEDED(aCard->GetCardValue(mSchemaMappingList[i].abField, &aName)) && (aName) && (*aName))
    {
      if (nsCRT::strncasecmp(mSchemaMappingList[i].serverField, "OMIT:", 5))
      {
        tProtLine.Append("&");
        tProtLine.Append(mSchemaMappingList[i].serverField);
        tProtLine.Append("=");
        tProtLine.Append(aName);
      }
    }
  }

  if (tProtLine != "")
  {
    char *tLine = tProtLine.ToNewCString();
    if (!tLine)
      return NS_ERROR_OUT_OF_MEMORY;

    char *escData = nsEscape(tLine, url_Path);
    if (escData)
    {
      tProtLine = escData;
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
  p->cm_refin = TRUE;
  p->cm_refot = TRUE;
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
  nsString            tempProtocolLine = "";

  // First, null out the protocol return line
  protLine = "";

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

  // Get the CRC for this temp entry line...
  char *tLine = tempProtocolLine.ToNewCString();
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
    if (NS_FAILED(aCard->GetKey(&aKey)))
      return PR_FALSE;
    
    // Needs to be negative, so make it so!
    char *tVal = PR_smprintf("%d", (aKey * -1));
    if (tVal)
    {
      protLine.Append("%26cid%3D");
      protLine.Append(tVal);
      protLine.Append(tempProtocolLine);
      nsCRT::free(tVal);
    }
    else
      return PR_FALSE;

    if (!historyRecord)
      newSyncRecord->flags |= SYNC_ADD;
    else
      newSyncRecord->flags |= SYNC_MODIFIED;
    
    return PR_TRUE;
  }
  else  // This is the same record as before.
  {
    return PR_FALSE;
  }

  return PR_FALSE;
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
  nsString                singleProtocolLine = "";

  // Init size vars...
  mOldTableSize = 0;
  mNewTableSize = 0;
  mNewServerTableSize = 0;
  PR_FREEIF(mOldSyncMapingTable);
  PR_FREEIF(mNewSyncMapingTable);
  PR_FREEIF(mNewServerTable);
  mCurrentPostRecord = 1;
  mNewServerTableSize = 0;

  //
  // First thing we need to do is open the absync.dat file
  // to compare against the last sync operation.
  // 
  // we want <profile>/absync.dat
  //
  NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  rv = locator->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(mHistoryFile));
  if (NS_FAILED(rv)) 
    return rv;

  rv = mHistoryFile->AppendRelativeUnixPath((const char *)"absync.dat");
  NS_ASSERTION(NS_SUCCEEDED(rv),"ab sync:  failed to append history filename");
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
        rv = NS_ERROR_OUT_OF_MEMORY;;
        goto GetOut;
      }
      
      // Init the memory!
      nsCRT::memset(mOldSyncMapingTable, 0, mOldTableSize);

      // Now get the number of records in the table size!
      mOldTableSize /= sizeof(syncMappingRecord);

      // Now read the history file into memory!
      while (readCount < mOldTableSize)
      {
        if (NS_FAILED(mHistoryFile->Read((char **)&(mOldSyncMapingTable[readCount]), 
                                         sizeof(syncMappingRecord), &readSize))
                                         || (readSize != sizeof(syncMappingRecord)))
        {
          rv = NS_ERROR_OUT_OF_MEMORY;;
          goto GetOut;
        }

        readCount++;
      }
    }
  }

  rv = aDatabase->EnumerateCards(directory, &cardEnum);
  if (NS_FAILED(rv) || (!cardEnum))
  {
    rv = NS_ERROR_FAILURE;
    goto GetOut;
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
        if (NS_FAILED(card->GetKey(&aKey)))
          continue;

        mNewSyncMapingTable[workCounter].localID = aKey;

        if (ThisCardHasChanged(card, &(mNewSyncMapingTable[workCounter]), singleProtocolLine))
        {
          // If we get here, we should look at the flags in the mNewSyncMapingTable to see
          // what we should add to the protocol header area and then tack on the singleProtcolLine
          // we got back from this call.
          //
          if (mNewSyncMapingTable[workCounter].flags && SYNC_ADD)
          {
            char *tVal3 = PR_smprintf("%d", mCurrentPostRecord);
            if (tVal3)
            {
              mPostString.Append(tVal3);
              mPostString.Append("=");
            }

            mPostString.Append(SYNC_ESCAPE_ADDUSER);
            mPostString.Append(singleProtocolLine);

            PR_FREEIF(tVal3);
            mCurrentPostRecord++;
          }
          else if (mNewSyncMapingTable[workCounter].flags && SYNC_MODIFIED)
          {
            char *tVal4 = PR_smprintf("%d", mCurrentPostRecord);
            if (tVal4)
            {
              mPostString.Append(tVal4);
              mPostString.Append("=");
            }

            mPostString.Append(SYNC_ESCAPE_MOD);
            mPostString.Append(singleProtocolLine);
            PR_FREEIF(tVal4);
            mCurrentPostRecord++;
          }
        }
      }
    }

    workCounter++;
  } while (NS_SUCCEEDED(cardEnum->Next()));

  delete cardEnum;

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
      char *tVal = PR_smprintf("%d", mOldSyncMapingTable[readCount].serverID);
      if (tVal)
      {
        char *tVal2 = PR_smprintf("%d", mCurrentPostRecord);
        if (tVal2)
        {
          mPostString.Append(tVal2);
          mPostString.Append("=");
        }

        mPostString.Append(SYNC_ESCAPE_DEL);
        mPostString.Append("%26id=");
        mPostString.Append(tVal);
        nsCRT::free(tVal);
        nsCRT::free(tVal2);
        mCurrentPostRecord++;
      }
    }

    readCount++;
  }

GetOut:
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
  nsIAbCard                     *urlArgCard = nsnull;
  PRUnichar                     *workEmail = nsnull;
  char                          *charEmail = nsnull;
  PRUnichar                     *workAb = nsnull;
  char                          *charAb = nsnull;

  // Init to null...
  mPostString = "";

  // Now, open the database...for now, make it the default
  rv = OpenAB(mAbSyncAddressBookFileName, &aDatabase);
  if (NS_FAILED(rv))
    return rv;

  // Get the RDF service...
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
  if (NS_FAILED(rv)) 
    goto EarlyExit;

  // this should not be hardcoded to abook.mab
  // RICHIE - this works for any address book...not sure why
  rv = rdfService->GetResource("abdirectory://abook.mab", getter_AddRefs(resource));
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
  
  NS_IF_RELEASE(urlArgCard);
  PR_FREEIF(charEmail);
  PR_FREEIF(charAb);
  return rv;
}

///////////////////////////////////////////////
// The following is for protocol parsing
///////////////////////////////////////////////

#define     SERVER_OP_RETURN          "~op_return"
#define     SERVER_NEW_RECORDS        "~new_records_section "
#define     SERVER_DELETED_RECORDS    "~deleted_records_section "
#define     SERVER_LAST_CHANGED       "~last_chg"

// Return true if the server returned an error...
PRBool
nsAbSync::ErrorFromServer(char **errString)
{
  if (!nsCRT::strncasecmp(mProtocolOffset, "err ", 4))
  {
    *errString = mProtocolOffset + 4;
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

// If this returns true, we are done with the data...
PRBool
nsAbSync::DecoderDone()
{
  if ( (!*mProtocolOffset) )
    return PR_TRUE;
  else
    return PR_FALSE;
}

nsresult        
nsAbSync::ProcessOpReturn()
{
  return NS_OK;
}

nsresult        
nsAbSync::ProcessNewRecords()
{
  return NS_OK;
}

nsresult        
nsAbSync::ProcessDeletedRecords()
{
  return NS_OK;
}

nsresult        
nsAbSync::ProcessLastChange()
{
  char      *aLine = nsnull;

  if (NS_FAILED(AdvanceToNextLine()))
    return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(ExtractCurrentLine(&aLine)))
  {
    if (aLine)
    {
      mLastChangeNum = nsCRT::atoi(nsString(aLine).GetUnicode());
      PR_FREEIF(aLine);
    }
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

nsresult        
nsAbSync::ExtractCurrentLine(char   **aLine)
{
  nsString    extractString = ""; 

  *aLine = nsnull;
  while (*mProtocolOffset)
  {
    while ( (*mProtocolOffset != CR) && (*mProtocolOffset == LF) )
    {
      extractString.Append(*mProtocolOffset);
      mProtocolOffset++;
    }
  }

  if (!*mProtocolOffset)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

nsresult        
nsAbSync::AdvanceToNextLine()
{
  while (*mProtocolOffset)
  {
    while ( (*mProtocolOffset != CR) && (*mProtocolOffset != LF) )
      mProtocolOffset++;
  }

  if (!*mProtocolOffset)
    return NS_ERROR_FAILURE;

  while (*mProtocolOffset)
  {
    while ( (*mProtocolOffset == CR) || (*mProtocolOffset == LF) )
      mProtocolOffset++;
  }

  if (!*mProtocolOffset)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

PRBool
nsAbSync::ParseNextSection()
{
  nsresult      rv = NS_OK;

  while ( (*mProtocolOffset == CR) || (*mProtocolOffset == LF) )
    mProtocolOffset++;

  if (!nsCRT::strncasecmp(mProtocolOffset, SERVER_OP_RETURN, nsCRT::strlen(SERVER_OP_RETURN)))
    rv = ProcessOpReturn();
  else if (!nsCRT::strncasecmp(mProtocolOffset, SERVER_NEW_RECORDS, nsCRT::strlen(SERVER_NEW_RECORDS)))
    rv = ProcessNewRecords();
  else if (!nsCRT::strncasecmp(mProtocolOffset, SERVER_DELETED_RECORDS, nsCRT::strlen(SERVER_DELETED_RECORDS)))
    rv = ProcessDeletedRecords();
  else if (!nsCRT::strncasecmp(mProtocolOffset, SERVER_LAST_CHANGED, nsCRT::strlen(SERVER_LAST_CHANGED)))
    rv = ProcessLastChange();

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
    printf("\7RICHIE: Server returned invalid response!\n");
    return NS_ERROR_FAILURE;
  }

  // Assign the vars...
  mProtocolResponse = (char *)aProtocolResponse;
  mProtocolOffset = (char *)aProtocolResponse;

  if (ErrorFromServer(&errorString))
  {
    printf("\7RICHIE: Server error: %s\n", errorString);
    return NS_ERROR_FAILURE;
  }

  while ( (!DecoderDone()) && (parseOk) )
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

  /***********RICHIE
  if (NS_FAILED(mHistoryFile->OpenStreamForWriting()))
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto ExitEarly;
  }

  //
  // Now, when we get here, we should go through the old history and see if
  // there are records we need to delete since we didn't touch them when comparing
  // against the current address book
  //

  // Ok, this handles the entries that we knew about before we started.
  while (writeCount < mNewTableSize)
  {
    
    if (NS_FAILED(mHistoryFile->Write((char *)&(mNewSyncMapingTable[writeCount]), 
                                     sizeof(syncMappingRecord), &writeSize))
                                     || (writeSize != sizeof(syncMappingRecord)))
    {
      rv = NS_ERROR_OUT_OF_MEMORY;;
      goto ExitEarly;
    }

    writeCount++;
  }

  // These are entries that we got back from the server that are new
  // to us!
  writeCount = 0;
  while (writeCount < mNewServerTableSize)
  {
    
    if (NS_FAILED(mHistoryFile->Write((char *)&(mNewServerTable[writeCount]), 
                                     sizeof(syncMappingRecord), &writeSize))
                                     || (writeSize != sizeof(syncMappingRecord)))
    {
      rv = NS_ERROR_OUT_OF_MEMORY;;
      goto ExitEarly;
    }

    writeCount++;
  }

  if (mHistoryFile)
    mHistoryFile->CloseStream();
*************** RICHIE */

ExitEarly:
  if (mLastChangeNum > 1)
  {
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
    if (NS_SUCCEEDED(rv) && prefs) 
    {
      prefs->SetIntPref("mail.absync.last_change", mLastChangeNum);
    }
  }

  return NS_OK;
}

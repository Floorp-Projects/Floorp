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
#include "plstr.h"
#include "nsString.h"

static NS_DEFINE_CID(kCAbSyncPostEngineCID, NS_ABSYNC_POST_ENGINE_CID); 
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kRDFServiceCID,  NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kAbCardPropertyCID, NS_ABCARDPROPERTY_CID);

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsAbSync, nsIAbSync)

nsAbSync::nsAbSync()
{
  NS_INIT_ISUPPORTS();

  InternalInit();
  InitSchemaColumns();
}

void
nsAbSync::InternalInit()
{
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
  mUserName = nsCString("RHPizzarro").ToNewCString();

  mLocale.AssignWithConversion("");
  mDeletedRecordTags = nsnull;
  mDeletedRecordValues = nsnull;
  mNewRecordTags = nsnull;
  mNewRecordValues = nsnull;
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

  if (mDeletedRecordTags)
    delete mDeletedRecordTags;
  if (mDeletedRecordValues)
    delete mDeletedRecordValues;

  if (mNewRecordTags)
    delete mNewRecordTags;
  if (mNewRecordValues)
    delete mNewRecordValues;

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

  // Make sure everything is set back to NULL's
  InternalInit();

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
  if (mPostString.IsEmpty())
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
  prefixStr = PR_smprintf("last=%u&protocol=%d&client=seamonkey&ver=%s&", mLastChangeNum, ABSYNC_PROTOCOL, ABSYNC_VERSION);
  if (!prefixStr)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    OnStopOperation(mTransactionID, NS_OK, nsnull, nsnull);
    goto EarlyExit;
  }

  mPostString.Insert(NS_ConvertASCIItoUCS2(prefixStr), 0);
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
  nsString      tProtLine;

  if (aAddId)
  {
    PRUint32    aKey;
    if (NS_FAILED(aCard->GetKey(&aKey)))
      return NS_ERROR_FAILURE;

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
      if (nsCRT::strncasecmp(mSchemaMappingList[i].serverField, "OMIT:", 5))
      {
        tProtLine.Append(NS_ConvertASCIItoUCS2("&"));
        tProtLine.Append(NS_ConvertASCIItoUCS2(mSchemaMappingList[i].serverField));
        tProtLine.Append(NS_ConvertASCIItoUCS2("="));
        tProtLine.Append(aName);
      }
    }
  }

  if (!tProtLine.IsEmpty())
  {
    char *tLine = tProtLine.ToNewCString();
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
      protLine.Append(NS_ConvertASCIItoUCS2("%26cid%3D"));
      protLine.Append(NS_ConvertASCIItoUCS2(tVal));
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
  nsString                singleProtocolLine;

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
              mPostString.Append(NS_ConvertASCIItoUCS2(tVal3));
              mPostString.Append(NS_ConvertASCIItoUCS2("="));
            }

            mPostString.Append(NS_ConvertASCIItoUCS2(SYNC_ESCAPE_ADDUSER));
            mPostString.Append(singleProtocolLine);

            PR_FREEIF(tVal3);
            mCurrentPostRecord++;
          }
          else if (mNewSyncMapingTable[workCounter].flags && SYNC_MODIFIED)
          {
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
      char *tVal = PR_smprintf("%d", mOldSyncMapingTable[readCount].serverID);
      if (tVal)
      {
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
  return rv;
}

nsresult
nsAbSync::PatchHistoryTableWithNewID(PRInt32 clientID, PRInt32 serverID)
{
  for (PRUint32 i = 0; i < mNewTableSize; i++)
  {
    if (mNewSyncMapingTable[i].localID == (clientID * -1))
    {
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
  char  *workLine;

  while (workLine = ExtractCurrentLine())
  {
    if (!*workLine)   // end of this section
      break;

    // Find the right tag and do something with it!
    // First a locale for the data
    if (!nsCRT::strncasecmp(workLine, SERVER_OP_RETURN_LOCALE, 
                            nsCRT::strlen(SERVER_OP_RETURN_LOCALE)))
    {
      char *locale = workLine += nsCRT::strlen(SERVER_OP_RETURN_LOCALE);
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
          PatchHistoryTableWithNewID(clientID, serverID);
        }
      }
    }

    if (*workLine)
      nsCRT::free(workLine);
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

      if (NS_FAILED(card->GetKey(&aKey)))
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

  while (workLine = ExtractCurrentLine())
  {
    if (!*workLine)   // end of this section
      break;

    mDeletedRecordTags->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    PR_FREEIF(workLine);
  }

  // Now, see what the next line is...if its a CRLF, then we
  // really don't have anything to do here and we can just return
  //
  while (workLine = ExtractCurrentLine())
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
          ( (*mProtocolOffset != CR) && (*mProtocolOffset != LF) )
        )
  {
    extractString.Append(NS_ConvertASCIItoUCS2(mProtocolOffset), 1);
    mProtocolOffset++;
  }

  if (!*mProtocolOffset)
    return nsnull;
  else
  {
    while ( (*mProtocolOffset) && 
            (*mProtocolOffset == CR) )
            mProtocolOffset++;

    if (*mProtocolOffset == LF)
      mProtocolOffset++;

    char *tString = extractString.ToNewCString();
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
  // First, find first CR or LF...
  while ( (*mProtocolOffset) && 
          ( (*mProtocolOffset != CR) && (*mProtocolOffset != LF) )
        )
  {
    mProtocolOffset++;
  }

  // now, make sure we are past the LF...
  if (*mProtocolOffset)
  {
    while ( (*mProtocolOffset) && 
            (*mProtocolOffset != LF) )
        mProtocolOffset++;
    
    if (*mProtocolOffset == LF)
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
nsAbSync::TagHit(char *aTag, PRBool advanceToNextLine)
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
    printf("\7RICHIE_GUI: Server returned invalid response!\n");
    return NS_ERROR_FAILURE;
  }

  // Assign the vars...
  mProtocolResponse = (char *)aProtocolResponse;
  mProtocolOffset = (char *)aProtocolResponse;

  if (ErrorFromServer(&errorString))
  {
    printf("\7RICHIE_GUI: Server error: %s\n", errorString);
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

  while (workLine = ExtractCurrentLine())
  {
    if (!*workLine)   // end of this section
      break;

    mNewRecordTags->AppendString(nsString(NS_ConvertASCIItoUCS2(workLine)));
    PR_FREEIF(workLine);
  }

  // Now, see what the next line is...if its a CRLF, then we
  // really don't have anything to do here and we can just return
  //
  while (workLine = ExtractCurrentLine())
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
nsAbSync::AddNewUsers()
{
  nsresult        rv = NS_OK;
  nsIAddrDatabase *aDatabase = nsnull;
  PRInt32         addCount = 0;
  PRInt32         i,j;
  nsCOMPtr<nsIAbCard> newCard;
 
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

  //
  // 
  // Create the new card that we will eventually add to the 
  // database...
  //
  for (i = 0; i < addCount; i++)
  {
    rv = nsComponentManager::CreateInstance(kAbCardPropertyCID, nsnull, NS_GET_IID(nsIAbCard), 
                                            getter_AddRefs(newCard));
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
        AddValueToNewCard(newCard, mNewRecordTags->StringAt(j), val);
      }
    }

    // Ok, now we need to add the card!
    rv = aDatabase->CreateNewCardAndAddToDB(newCard, PR_TRUE);
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

nsresult
nsAbSync::AddValueToNewCard(nsIAbCard *aCard, nsString *aTagName, nsString *aTagValue)
{
  nsresult  rv = NS_OK;


  return rv;
}

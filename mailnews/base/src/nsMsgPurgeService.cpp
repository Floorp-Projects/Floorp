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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Navin Gupta <naving@netscape.com> (Original Author)
 *
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

#include "nsMsgPurgeService.h"
#include "nsCRT.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgBaseCID.h"
#include "nsMsgUtils.h"
#include "nsMsgSearchCore.h"
#include "msgCore.h"
#include "nsISpamSettings.h"
#include "nsIMsgSearchTerm.h"
#include "nsIMsgHdr.h"
#include "nsIRDFService.h"
#include "nsIFileSpec.h"
#include "nsIMsgProtocolInfo.h"

#define ONE_DAY_IN_MILLISECONDS 86400000000
#define MIN_DELAY_BETWEEN_PURGES 300000000  //minimum delay between two consecutive purges

NS_IMPL_ISUPPORTS3(nsMsgPurgeService, nsIMsgPurgeService, nsIIncomingServerListener, nsIMsgSearchNotify)

void OnPurgeTimer(nsITimer *timer, void *aPurgeService)
{
  nsMsgPurgeService *purgeService = (nsMsgPurgeService*)aPurgeService;
  purgeService->PerformPurge();		
}

nsMsgPurgeService::nsMsgPurgeService()
{
  NS_INIT_ISUPPORTS();

  mPurgeArray = nsnull;
  mHaveShutdown = PR_FALSE;
}

nsMsgPurgeService::~nsMsgPurgeService()
{
  if (mPurgeTimer) {
    mPurgeTimer->Cancel();
  }
  
  PRInt32 count = mPurgeArray->Count();
  PRInt32 i;
  for(i=0; i < count; i++)
  {
    nsPurgeEntry *purgeEntry = (nsPurgeEntry*)mPurgeArray->ElementAt(i);
    delete purgeEntry;
  }
  delete mPurgeArray;
  
  if(!mHaveShutdown)
  {
    Shutdown();
  }
}

NS_IMETHODIMP nsMsgPurgeService::Init()
{
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
  do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    accountManager->AddIncomingServerListener(this);

  if(mHaveShutdown) //in turbo mode on profile change we don't need to do anything below this
  {
    mHaveShutdown = PR_FALSE;
    return NS_OK;
  }

  mPurgeArray = new nsVoidArray();
  if(!mPurgeArray)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::Shutdown()
{
  if (mPurgeTimer) 
  {
    mPurgeTimer->Cancel();
    mPurgeTimer = nsnull;
  }
  nsresult rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    accountManager->RemoveIncomingServerListener(this);
  }
  
  mHaveShutdown = PR_TRUE;
  return NS_OK;
}

nsresult nsMsgPurgeService::AddServer(nsIMsgIncomingServer *server)
{
  if (FindServer(server) != -1)
    return NS_OK;

  //Only add it if it hasn't been added already
  nsXPIDLCString type;
  nsresult rv = server->GetType(getter_Copies(type));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCAutoString contractid(NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX);
  contractid.Append(type);
  
  nsCOMPtr<nsIMsgProtocolInfo> protocolInfo =
    do_GetService(contractid.get(), &rv);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  
  PRBool canGetIncomingMessages = PR_FALSE;
  protocolInfo->GetCanGetIncomingMessages(&canGetIncomingMessages);

  if (!canGetIncomingMessages)
    return NS_OK;

  nsCOMPtr <nsISpamSettings> spamSettings;
  rv = server->GetSpamSettings(getter_AddRefs(spamSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool purgeSpam;
  spamSettings->GetPurge(&purgeSpam);
  if (!purgeSpam)
    return NS_OK;

  nsXPIDLCString junkFolderURI;
  spamSettings->GetSpamFolderURI(getter_Copies(junkFolderURI));
  if (junkFolderURI.IsEmpty())  
    return NS_OK;

  //we cannot do GetExistingFolder because folder tree has not been built yet.
  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(junkFolderURI.get(), getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgFolder> junkFolder = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
    
  nsTime lastPurgeTime=0;
  nsXPIDLCString lastPurgeTimeString;
  rv = junkFolder->GetStringProperty("lastPurgeTime", getter_Copies(lastPurgeTimeString));
  if (NS_FAILED(rv))  //it is ok to fail, junk folder may not exist
     return NS_OK;
  
  if (!lastPurgeTimeString.IsEmpty())
  {
    PRInt64 theTime;
    PR_ParseTimeString(lastPurgeTimeString.get(), PR_FALSE, &theTime);
    lastPurgeTime = theTime;
  }
  
  nsPurgeEntry *purgeEntry = new nsPurgeEntry;
  if (!purgeEntry)
    return NS_ERROR_OUT_OF_MEMORY;
  
  purgeEntry->server = server;
  purgeEntry->folderURI = junkFolderURI.get();
  nsTime nextPurgeTime;             
  if (lastPurgeTime)
  {
    nextPurgeTime = lastPurgeTime;
    nextPurgeTime += ONE_DAY_IN_MILLISECONDS;
    nsTime currentTime;
    if ( nextPurgeTime < currentTime)
      nextPurgeTime = currentTime;
  }
   
  rv = SetNextPurgeTime(purgeEntry, nextPurgeTime);
  NS_ENSURE_SUCCESS(rv, rv);      
  AddPurgeEntry(purgeEntry);
  SetupNextPurge();
  return NS_OK;
}

nsresult nsMsgPurgeService::RemoveServer(nsIMsgIncomingServer *server)
{
  PRInt32 pos = FindServer(server);
  if(pos != -1)
  {
    nsPurgeEntry *purgeEntry = (nsPurgeEntry*)mPurgeArray->ElementAt(pos);
    mPurgeArray->RemoveElementAt(pos);
    delete purgeEntry;
  }
  
  //Should probably reset purge time if this was the server that gets purged next.
	return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::OnServerLoaded(nsIMsgIncomingServer *server)
{
  NS_ENSURE_ARG_POINTER(server); 
  return AddServer(server);
}

NS_IMETHODIMP nsMsgPurgeService::OnServerUnloaded(nsIMsgIncomingServer *server)
{
  NS_ENSURE_ARG_POINTER(server); 
  return RemoveServer(server);
}

NS_IMETHODIMP nsMsgPurgeService::OnServerChanged(nsIMsgIncomingServer *server)
{
  // nothing required.  If the hostname or username changed
  // the next time purge fires, we'll ping the right server
  return NS_OK;
}


PRInt32 nsMsgPurgeService::FindServer(nsIMsgIncomingServer *server)
{
  PRInt32 count = mPurgeArray->Count();
  for(PRInt32 i = 0; i < count; i++)
  {
    nsPurgeEntry *purgeEntry = (nsPurgeEntry*)mPurgeArray->ElementAt(i);
    if(server == purgeEntry->server.get())
      return i;
  }
  return -1;
}

nsresult nsMsgPurgeService::AddPurgeEntry(nsPurgeEntry *purgeEntry)
{
  PRInt32 i;
  PRInt32 count = mPurgeArray->Count();
  for(i = 0; i < count; i++)
  {
    nsPurgeEntry *current = (nsPurgeEntry*)mPurgeArray->ElementAt(i);
    if(purgeEntry->nextPurgeTime < current->nextPurgeTime)
      break;
    
  }
  mPurgeArray->InsertElementAt(purgeEntry, i);
  return NS_OK;
}

nsresult nsMsgPurgeService::SetNextPurgeTime(nsPurgeEntry *purgeEntry, nsTime startTime)
{
  nsIMsgIncomingServer *server = purgeEntry->server; 
  if(!server)
    return NS_ERROR_FAILURE;
  
  purgeEntry->nextPurgeTime = startTime;
  purgeEntry->nextPurgeTime += MIN_DELAY_BETWEEN_PURGES * (mPurgeArray->Count()+1);  //let us stagger them out by 5 mins if they happen to start at same time
  return NS_OK;
}

nsresult nsMsgPurgeService::SetupNextPurge()
{
  if(mPurgeArray->Count() > 0)
  {
    //Get the next purge entry
    nsPurgeEntry *purgeEntry = (nsPurgeEntry*)mPurgeArray->ElementAt(0);
    nsTime currentTime;
    nsInt64 purgeDelay;
    nsInt64 ms(1000);
    if(currentTime > purgeEntry->nextPurgeTime)
      purgeDelay = MIN_DELAY_BETWEEN_PURGES;
    else
      purgeDelay = purgeEntry->nextPurgeTime - currentTime;
    //Convert purgeDelay into milliseconds
    nsInt64 timeInMS = purgeDelay / ms;
    PRUint32 timeInMSUint32 = (PRUint32)timeInMS;
    //Can't currently reset a timer when it's in the process of
    //calling Notify. So, just release the timer here and create a new one.
    if(mPurgeTimer)
    {
      mPurgeTimer->Cancel();
    }
    mPurgeTimer = do_CreateInstance("@mozilla.org/timer;1");
    mPurgeTimer->InitWithFuncCallback(OnPurgeTimer, (void*)this, timeInMSUint32, 
                                     nsITimer::TYPE_ONE_SHOT);
    
  }
  return NS_OK;
}

//This is the function that does a purge on all of the servers whose time it is to purge.
nsresult nsMsgPurgeService::PerformPurge()
{
  nsTime currentTime;
  for(PRInt32 i = 0;i < mPurgeArray->Count(); i++)
  {
    nsPurgeEntry *current = (nsPurgeEntry*)mPurgeArray->ElementAt(i);
    if(current->nextPurgeTime < currentTime)
    {
      PRBool serverBusy = PR_FALSE;
      PRBool serverRequiresPassword = PR_TRUE;
      nsXPIDLCString password;
      // we don't want to prompt the user for password UI so pass in false to
      // the server->GetPassword method. If we don't already know the passsword then 
      // we just won't purge this server
      current->server->GetPassword(getter_Copies(password));
      current->server->GetServerBusy(&serverBusy);
      //Make sure we're logged on before doing a purge
      // and make sure the server isn't already in the middle of downloading new messages
      nsTime nextPurgeTime;
      if(!mSearchSession && !serverBusy && !password.IsEmpty())
      {
        nsresult rv = PurgeJunkFolder(current);
        if (NS_SUCCEEDED(rv))
          nextPurgeTime += ONE_DAY_IN_MILLISECONDS;
        else
          current = nsnull;
      }
      mPurgeArray->RemoveElementAt(i);
      i--; //Because we removed it we need to look at the one that just moved up.

      if (current)
      {
        SetNextPurgeTime(current, nextPurgeTime);
        AddPurgeEntry(current);
      }
    }
    else
      //since we're in purge order, there's no reason to keep checking
      break;
  }
  SetupNextPurge();
  return NS_OK;
}

nsresult nsMsgPurgeService::PurgeJunkFolder(nsPurgeEntry *entry)
{
  nsCOMPtr <nsISpamSettings> spamSettings;
  nsresult rv = entry->server->GetSpamSettings(getter_AddRefs(spamSettings));
  if (spamSettings)
  {
    PRBool purgeSpam;
    spamSettings->GetPurge(&purgeSpam);
    //I didn't want to add another error code; if settings changed from underneath us simply abort - return failure
    if (!purgeSpam)
      return NS_ERROR_FAILURE;   
    nsXPIDLCString junkFolderURI;
    spamSettings->GetSpamFolderURI(getter_Copies(junkFolderURI));

    if (junkFolderURI.IsEmpty())  //fatal error - return failure;
      return NS_ERROR_FAILURE;

    if (!entry->folderURI.Equals(junkFolderURI.get()))  //junkfolder changed, return NS_OK, will purge after a day
    {
      entry->folderURI.Assign(junkFolderURI.get());
      return NS_OK;
    }

    nsCOMPtr<nsIMsgFolder> junkFolder;
    GetExistingFolder(junkFolderURI.get(), getter_AddRefs(junkFolder));
    if (junkFolder)
    {
      //multiple servers can point to same junk folder, so check last purge time before issuing a purge
      //we don't want to remove the entry because junk folder can change for the server
      nsXPIDLCString lastPurgeTimeString;
      junkFolder->GetStringProperty("lastPurgeTime", getter_Copies(lastPurgeTimeString));
      if (!lastPurgeTimeString.IsEmpty())
      {
        PRInt64 theTime;
        PR_ParseTimeString(lastPurgeTimeString.get(), PR_FALSE, &theTime);
        nsTime lastPurgeTime(theTime);
        lastPurgeTime += ONE_DAY_IN_MILLISECONDS;
        nsTime currentTime;
        if (lastPurgeTime > currentTime)
          return NS_OK;              //return NS_OK will purge after a day
      }
      PRInt32 purgeInterval;
      spamSettings->GetPurgeInterval(&purgeInterval);
      return SearchFolderToPurge(junkFolder, purgeInterval);
    }
    else 
    {
     //we couldn't find the junk folder to purge so return failure so that entry gets removed
      return NS_MSG_ERROR_FOLDER_MISSING;
    }
  }
  return rv;
}
    
nsresult nsMsgPurgeService::SearchFolderToPurge(nsIMsgFolder *folder, PRInt32 purgeInterval)
{
  nsresult rv;
  mSearchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mSearchSession->RegisterListener(this);
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server)); //we need to get the folder's server scope because imap can have local junk folder
  NS_ENSURE_SUCCESS(rv, rv);

  nsMsgSearchScopeValue searchScope;
  server->GetSearchScope(&searchScope);

  mSearchSession->AddScopeTerm(searchScope, folder);

  nsCOMPtr <nsIMsgSearchTerm> searchTerm;
  mSearchSession->CreateTerm(getter_AddRefs(searchTerm));
  if (searchTerm)
  {
    searchTerm->SetAttrib(nsMsgSearchAttrib::AgeInDays);
    searchTerm->SetOp(nsMsgSearchOp::IsGreaterThan);
    nsCOMPtr<nsIMsgSearchValue> searchValue;
    searchTerm->GetValue(getter_AddRefs(searchValue));
    if (searchValue)
    {
      searchValue->SetAttrib(nsMsgSearchAttrib::AgeInDays);
      searchValue->SetAge((PRUint32) purgeInterval);
      searchTerm->SetValue(searchValue);
    }
    searchTerm->SetBooleanAnd(PR_FALSE);
    mSearchSession->AppendTerm(searchTerm); 
  }
  
  if (!mHdrsToDelete)  //we are about to search, let us create mHdrsToDelete array
  {
    mHdrsToDelete = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mSearchFolder = folder;
  return mSearchSession->Search(nsnull);
}
    
NS_IMETHODIMP nsMsgPurgeService::OnNewSearch()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::OnSearchHit(nsIMsgDBHdr* msgHdr, nsIMsgFolder *folder)
{
  return mHdrsToDelete->AppendElement(msgHdr);
}

NS_IMETHODIMP nsMsgPurgeService::OnSearchDone(nsresult status)
{
  nsresult rv = NS_OK;
  if (NS_SUCCEEDED(status))
  {
    PRUint32 count;
    mHdrsToDelete->Count(&count);
    if (count > 0)
      rv = mSearchFolder->DeleteMessages(mHdrsToDelete, nsnull, PR_FALSE /*delete storage*/, PR_FALSE /*isMove*/, nsnull, PR_FALSE /*allowUndo*/);

    if (NS_SUCCEEDED(rv))
    {
      char dateBuf[100];
      dateBuf[0] = '\0';
      PRExplodedTime exploded;
      PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
      PR_FormatTimeUSEnglish(dateBuf, sizeof(dateBuf), "%a %b %d %H:%M:%S %Y", &exploded);
      mSearchFolder->SetStringProperty("lastPurgeTime", dateBuf);
    }
  }
  mHdrsToDelete->Clear();
  mSearchSession->UnregisterListener(this);
  mSearchSession = nsnull;  //let us create another search session next time we search, rather than clearing scopes, terms etc.
  mSearchFolder = nsnull;
  return NS_OK;
}

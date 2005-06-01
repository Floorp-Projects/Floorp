/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Navin Gupta <naving@netscape.com> (Original Developer)
 *   Seth Spitzer <sspitzer@netscape.com>
 *   David Bienvenu <bienvenu@mozilla.org>
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

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
#include "nsIMsgFilterPlugin.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "prlog.h"
#include "nsMsgFolderFlags.h"

static PRLogModuleInfo *MsgPurgeLogModule = nsnull;

NS_IMPL_ISUPPORTS2(nsMsgPurgeService, nsIMsgPurgeService, nsIMsgSearchNotify)

void OnPurgeTimer(nsITimer *timer, void *aPurgeService)
{
  nsMsgPurgeService *purgeService = (nsMsgPurgeService*)aPurgeService;
  purgeService->PerformPurge();		
}

nsMsgPurgeService::nsMsgPurgeService()
{
  mHaveShutdown = PR_FALSE;
  mMinDelayBetweenPurges = 480;  // never purge a folder more than once every 8 hours (60 min/hour * 8 hours)
  mPurgeTimerInterval = 5;  // fire the purge timer every 5 minutes, starting 5 minutes after the service is created (when we load accounts)
}

nsMsgPurgeService::~nsMsgPurgeService()
{
  if (mPurgeTimer)
    mPurgeTimer->Cancel();
  
  if(!mHaveShutdown)
    Shutdown();
}

NS_IMETHODIMP nsMsgPurgeService::Init()
{
  nsresult rv;
  
  if (!MsgPurgeLogModule)
    MsgPurgeLogModule = PR_NewLogModule("MsgPurge");

  // these prefs are here to help QA test this feature
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv))
  {
    PRInt32 min_delay;
    rv = prefBranch->GetIntPref("mail.purge.min_delay", &min_delay);
    if (NS_SUCCEEDED(rv) &&  min_delay) 
      mMinDelayBetweenPurges = min_delay;
    
    PRInt32 purge_timer_interval;
    rv = prefBranch->GetIntPref("mail.purge.timer_interval", &purge_timer_interval);
    if (NS_SUCCEEDED(rv) &&  purge_timer_interval) 
      mPurgeTimerInterval = purge_timer_interval;
  }
  
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("mail.purge.min_delay=%d minutes",mMinDelayBetweenPurges));
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("mail.purge.timer_interval=%d minutes",mPurgeTimerInterval));

  // don't start purging right away.
  // because the accounts aren't loaded and because the user might be trying to sign in
  // or startup, etc.
  SetupNextPurge();

  mHaveShutdown = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::Shutdown()
{
  if (mPurgeTimer) 
  {
    mPurgeTimer->Cancel();
    mPurgeTimer = nsnull;
  }
  
  mHaveShutdown = PR_TRUE;
  return NS_OK;
}

nsresult nsMsgPurgeService::SetupNextPurge()
{
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("setting to check again in %d minutes",mPurgeTimerInterval));

  // Convert mPurgeTimerInterval into milliseconds
  PRUint32 timeInMSUint32 = mPurgeTimerInterval * 60000;

  // Can't currently reset a timer when it's in the process of
  // calling Notify. So, just release the timer here and create a new one.
  if(mPurgeTimer)
    mPurgeTimer->Cancel();
  
  mPurgeTimer = do_CreateInstance("@mozilla.org/timer;1");
  mPurgeTimer->InitWithFuncCallback(OnPurgeTimer, (void*)this, timeInMSUint32, 
    nsITimer::TYPE_ONE_SHOT);
   
  return NS_OK;
}

// This is the function that looks for the first folder to purge. It also 
// applies retention settings to any folder that hasn't had retention settings
// applied in mMinDelayBetweenPurges minutes (default, 8 hours).
// However, if we've spent more than .5 seconds in this loop, don't
// apply any more retention settings because it might lock up the UI.
// This might starve folders later on in the hierarchy, since we always
// start at the top, but since we also apply retention settings when you
// open a folder, or when you compact all folders, I think this will do
// for now, until we have a cleanup on shutdown architecture.
nsresult nsMsgPurgeService::PerformPurge()
{
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("performing purge"));

  nsresult rv;
  
  nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  PRBool keepApplyingRetentionSettings = PR_TRUE;

  nsCOMPtr<nsISupportsArray> allServers;
  rv = accountManager->GetAllServers(getter_AddRefs(allServers));
  if (NS_SUCCEEDED(rv) && allServers)
  {
    PRUint32 numServers;
    rv = allServers->Count(&numServers);
    PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("%d servers", numServers));
    nsCOMPtr<nsIMsgFolder> folderToPurge;
    PRIntervalTime startTime = PR_IntervalNow();
    PRInt32 purgeIntervalToUse;
    nsTime oldestPurgeTime = 0; // we're going to pick the least-recently purged folder

    // apply retention settings to folders that haven't had retention settings
    // applied in mMinDelayBetweenPurges minutes (default 8 hours)
    // Because we get last purge time from the folder cache,
    // this code won't open db's for folders until it decides it needs
    // to apply retention settings, and since nsIMsgFolder::ApplyRetentionSettings
    // will close any db's it opens, this code won't leave db's open.
    for (PRUint32 serverIndex=0; serverIndex < numServers; serverIndex++)
    {
      nsCOMPtr <nsIMsgIncomingServer> server =
        do_QueryElementAt(allServers, serverIndex, &rv);
      if (NS_SUCCEEDED(rv) && server)
      {
        if (keepApplyingRetentionSettings)
        {
          nsCOMPtr <nsIMsgFolder> rootFolder;
          rv = server->GetRootFolder(getter_AddRefs(rootFolder));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr <nsISupportsArray> childFolders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv, rv);
          rv = rootFolder->ListDescendents(childFolders);

          PRUint32 cnt =0;  
          childFolders->Count(&cnt);

          nsCOMPtr<nsISupports> supports;
          nsCOMPtr<nsIUrlListener> urlListener;
          nsCOMPtr<nsIMsgFolder> childFolder;

          for (PRUint32 index = 0; index < cnt; index++)
          {
            childFolder = do_QueryElementAt(childFolders, index);
            if (childFolder)
            {
              PRUint32 folderFlags;
              (void) childFolder->GetFlags(&folderFlags);
              if (folderFlags & MSG_FOLDER_FLAG_VIRTUAL)
                continue;
              nsTime curFolderLastPurgeTime=0;
              nsXPIDLCString curFolderLastPurgeTimeString, curFolderUri;
              rv = childFolder->GetStringProperty("LastPurgeTime", getter_Copies(curFolderLastPurgeTimeString));
              if (NS_FAILED(rv))  
                continue; // it is ok to fail, go on to next folder
                
              if (!curFolderLastPurgeTimeString.IsEmpty())
              {
                PRInt64 theTime;
                PR_ParseTimeString(curFolderLastPurgeTimeString.get(), PR_FALSE, &theTime);
                curFolderLastPurgeTime = theTime;
              }
  
              childFolder->GetURI(getter_Copies(curFolderUri));
              PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("%s curFolderLastPurgeTime=%s (if blank, then never)", curFolderUri.get(), curFolderLastPurgeTimeString.get()));

              // check if this folder is due to purge
              // has to have been purged at least mMinDelayBetweenPurges minutes ago
              // we don't want to purge the folders all the time - once a day is good enough
              nsInt64 minDelayBetweenPurges(mMinDelayBetweenPurges);
              nsInt64 microSecondsPerMinute(60000000);
              nsTime nextPurgeTime = curFolderLastPurgeTime + (minDelayBetweenPurges * microSecondsPerMinute);
              nsTime currentTime; // nsTime defaults to PR_Now
              if (nextPurgeTime < currentTime)
              {
                PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("purging %s", curFolderUri.get()));
                childFolder->ApplyRetentionSettings();
              }
              PRIntervalTime elapsedTime;
              LL_SUB(elapsedTime, PR_IntervalNow(), startTime);
              // check if more than 500 milliseconds have elapsed in this purge process
              if (PR_IntervalToMilliseconds(elapsedTime) > 500)
              {
                keepApplyingRetentionSettings = PR_FALSE;
                break;
              }
            }
          }
        }
        nsXPIDLCString type;
        nsresult rv = server->GetType(getter_Copies(type));
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsCAutoString contractid(NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX);
        contractid.Append(type);
        
        nsCOMPtr<nsIMsgProtocolInfo> protocolInfo =
          do_GetService(contractid.get(), &rv);
        NS_ENSURE_SUCCESS(rv, PR_FALSE);

        nsXPIDLCString realHostName;
        server->GetRealHostName(getter_Copies(realHostName));
        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] %s (%s)", serverIndex, realHostName.get(), type.get()));

        nsCOMPtr <nsISpamSettings> spamSettings;
        rv = server->GetSpamSettings(getter_AddRefs(spamSettings));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 spamLevel;
        spamSettings->GetLevel(&spamLevel);
        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] spamLevel=%d (if 0, don't purge)", serverIndex, spamLevel));
        if (!spamLevel)
          continue;
        
        // check if we are set up to purge for this server
        // if not, skip it.
        PRBool purgeSpam;
        spamSettings->GetPurge(&purgeSpam);

        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] purgeSpam=%s (if false, don't purge)", serverIndex, purgeSpam ? "true" : "false"));
        if (!purgeSpam)
          continue;
        
        // check if the spam folder uri is set for this server
        // if not skip it.
        nsXPIDLCString junkFolderURI;
        rv = spamSettings->GetSpamFolderURI(getter_Copies(junkFolderURI));
        NS_ENSURE_SUCCESS(rv,rv);

        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] junkFolderURI=%s (if empty, don't purge)", serverIndex, junkFolderURI.get()));
        if (junkFolderURI.IsEmpty())
          continue;

        // if the junk folder doesn't exist
        // because the folder pane isn't built yet, for example
        // skip this account
        nsCOMPtr<nsIMsgFolder> junkFolder;
        GetExistingFolder(junkFolderURI.get(), getter_AddRefs(junkFolder));

        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] %s exists? %s (if doesn't exist, don't purge)", serverIndex, junkFolderURI.get(), junkFolder ? "true" : "false"));
        if (!junkFolder)
          continue;  
    
        nsTime curJunkFolderLastPurgeTime=0;
        nsXPIDLCString curJunkFolderLastPurgeTimeString;
        rv = junkFolder->GetStringProperty("curJunkFolderLastPurgeTime", getter_Copies(curJunkFolderLastPurgeTimeString));
        if (NS_FAILED(rv))  
          continue; // it is ok to fail, junk folder may not exist
                
        if (!curJunkFolderLastPurgeTimeString.IsEmpty())
        {
          PRInt64 theTime;
          PR_ParseTimeString(curJunkFolderLastPurgeTimeString.get(), PR_FALSE, &theTime);
          curJunkFolderLastPurgeTime = theTime;
        }
  
        PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] %s curJunkFolderLastPurgeTime=%s (if blank, then never)", serverIndex, junkFolderURI.get(), curJunkFolderLastPurgeTimeString.get()));

        // check if this account is due to purge
        // has to have been purged at least mMinDelayBetweenPurges minutes ago
        // we don't want to purge the folders all the time
        nsTime nextPurgeTime = curJunkFolderLastPurgeTime + nsInt64(mMinDelayBetweenPurges * 60000000 /* convert mMinDelayBetweenPurges to into microseconds */);
        nsTime currentTime;
        if (nextPurgeTime < currentTime) 
        {
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] last purge greater than min delay", serverIndex));

          nsCOMPtr <nsIMsgIncomingServer> junkFolderServer;
          rv = junkFolder->GetServer(getter_AddRefs(junkFolderServer));
          NS_ENSURE_SUCCESS(rv,rv);
          
          PRBool serverBusy = PR_FALSE;
          PRBool serverRequiresPassword = PR_TRUE;
          PRBool passwordPromptRequired;
          PRBool canSearchMessages = PR_FALSE;
          junkFolderServer->GetPasswordPromptRequired(&passwordPromptRequired);
          junkFolderServer->GetServerBusy(&serverBusy);
          junkFolderServer->GetServerRequiresPasswordForBiff(&serverRequiresPassword);
          junkFolderServer->GetCanSearchMessages(&canSearchMessages);
          // Make sure we're logged on before doing the search (assuming we need to be)
          // and make sure the server isn't already in the middle of downloading new messages
          // and make sure a search isn't already going on
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] (search in progress? %s)", serverIndex, mSearchSession ? "true" : "false")); 
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] (server busy? %s)", serverIndex, serverBusy ? "true" : "false"));
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] (serverRequiresPassword? %s)", serverIndex, serverRequiresPassword ? "true" : "false"));
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] (passwordPromptRequired? %s)", serverIndex, passwordPromptRequired ? "true" : "false"));
          if (canSearchMessages && !mSearchSession && !serverBusy && (!serverRequiresPassword || !passwordPromptRequired))
          {
            PRInt32 purgeInterval;
            spamSettings->GetPurgeInterval(&purgeInterval);

            if ((oldestPurgeTime == nsInt64(0)) || (curJunkFolderLastPurgeTime < oldestPurgeTime))
            {
              PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] purging! searching for messages older than %d days", serverIndex, purgeInterval));
              oldestPurgeTime = curJunkFolderLastPurgeTime;
              purgeIntervalToUse = purgeInterval;
              folderToPurge = junkFolder;
              // if we've never purged this folder, do it...
              if (curJunkFolderLastPurgeTime == nsInt64(0))
                break;
            }
          }
          else {
            NS_ASSERTION(canSearchMessages, "unexpected, you should be able to search");
            PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] not a good time for this server, try again later", serverIndex));
          }
        }
        else {
          PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("[%d] last purge too recent", serverIndex));
        }
      }
    }
    if (folderToPurge)
      rv = SearchFolderToPurge(folderToPurge, purgeIntervalToUse);
  }
    
  // set up timer to check accounts again
  SetupNextPurge();
  return rv;
}

nsresult nsMsgPurgeService::SearchFolderToPurge(nsIMsgFolder *folder, PRInt32 purgeInterval)
{
  nsresult rv;
  mSearchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mSearchSession->RegisterListener(this);
  
  // update the time we attempted to purge this folder
  char dateBuf[100];
  dateBuf[0] = '\0';
  PRExplodedTime exploded;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
  PR_FormatTimeUSEnglish(dateBuf, sizeof(dateBuf), "%a %b %d %H:%M:%S %Y", &exploded);
  folder->SetStringProperty("curJunkFolderLastPurgeTime", dateBuf);
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("curJunkFolderLastPurgeTime is now %s", dateBuf));

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = folder->GetServer(getter_AddRefs(server)); //we need to get the folder's server scope because imap can have local junk folder
  NS_ENSURE_SUCCESS(rv, rv);

  nsMsgSearchScopeValue searchScope;
  server->GetSearchScope(&searchScope);

  mSearchSession->AddScopeTerm(searchScope, folder);
  
  // look for messages older than the cutoff
  // you can't also search by junk status, see
  // nsMsgPurgeService::OnSearchHit()
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

  // we are about to search
  // create mHdrsToDelete array (if not previously created)
  if (!mHdrsToDelete)  
  {
    mHdrsToDelete = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else
  {
    PRUint32 count;
    mHdrsToDelete->Count(&count);
    NS_ASSERTION(count == 0, "mHdrsToDelete is not empty");
    if (count > 0)
      mHdrsToDelete->Clear();  // this shouldn't happen
  }

  mSearchFolder = folder;
  return mSearchSession->Search(nsnull);
}
    
NS_IMETHODIMP nsMsgPurgeService::OnNewSearch()
{
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("on new search"));
  return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *aFolder)
{
  NS_ENSURE_ARG_POINTER(aMsgHdr);

  nsXPIDLCString messageId;
  nsXPIDLCString author;
  nsXPIDLCString subject;
  
  aMsgHdr->GetMessageId(getter_Copies(messageId));
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("messageId=%s", messageId.get()));
  aMsgHdr->GetSubject(getter_Copies(subject));
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("subject=%s",subject.get()));
  aMsgHdr->GetAuthor(getter_Copies(author));
  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("author=%s",author.get()));

  // double check that the message is junk before adding to 
  // the list of messages to delete
  //
  // note, we can't just search for messages that are junk
  // because not all imap server support keywords 
  // (which we use for the junk score)
  // so the junk status would be in the message db.
  //
  // see bug #194090
  nsXPIDLCString junkScoreStr;
  nsresult rv = aMsgHdr->GetStringProperty("junkscore", getter_Copies(junkScoreStr));
  NS_ENSURE_SUCCESS(rv,rv);

  PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("junkScore=%s (if empty or <= 50, don't add to list delete)", junkScoreStr.get()));

  // if "junkscore" is not set, don't delete the message
  if (junkScoreStr.IsEmpty())
    return NS_OK;

  // I set the cut off at 50. this may change
  // it works for our bayesian plugin, as "0" is good, and "100" is junk
  // but it might need tweaking for other plugins
  if (atoi(junkScoreStr.get()) > 50) {
    PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("added message to delete"));
    return mHdrsToDelete->AppendElement(aMsgHdr);
  }
  else 
    return NS_OK;
}

NS_IMETHODIMP nsMsgPurgeService::OnSearchDone(nsresult status)
{
  nsresult rv = NS_OK;
  if (NS_SUCCEEDED(status))
  {
    PRUint32 count;
    mHdrsToDelete->Count(&count);
    PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("%d messages to delete", count));

    if (count > 0) {
      PR_LOG(MsgPurgeLogModule, PR_LOG_ALWAYS, ("delete messages"));
      rv = mSearchFolder->DeleteMessages(mHdrsToDelete, nsnull, PR_FALSE /*delete storage*/, PR_FALSE /*isMove*/, nsnull, PR_FALSE /*allowUndo*/);
    }
  }
  mHdrsToDelete->Clear();
  mSearchSession->UnregisterListener(this);
  // don't cache the session
  // just create another search session next time we search, rather than clearing scopes, terms etc.
  // we also use mSearchSession to determine if the purge service is "busy"
  mSearchSession = nsnull;  
  mSearchFolder = nsnull;
  return NS_OK;
}

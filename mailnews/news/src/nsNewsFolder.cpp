/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Håkan Waara  <hwaara@chello.se>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "prlog.h"

#include "msgCore.h"    // precompiled header...
#include "nntpCore.h"

#include "nsNewsFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "prprf.h"
#include "prsystem.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsINntpService.h"
#include "nsIFolderListener.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgNewsCID.h"
#include "nsMsgUtils.h"
#include "nsNewsUtils.h"

#include "nsCOMPtr.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsINewsDatabase.h"
#include "nsMsgBaseCID.h"

#include "nsIMsgWindow.h"
#include "nsIDocShell.h"
#include "nsIWebShell.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"

#include "nsXPIDLString.h"

#include "nsIObserverService.h"
#include "nsNetUtil.h"
#include "nsIAuthPrompt.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsINntpUrl.h"
#include "nsNewsSummarySpec.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsReadableUtils.h"
#include "nsNewsDownloader.h"
#include "nsIStringBundle.h"
#include "nsEscape.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);

// ###tw  This really ought to be the most
// efficient file reading size for the current
// operating system.
#define NEWSRC_FILE_BUFFER_SIZE 1024

#define NEWS_SCHEME "news:"
#define SNEWS_SCHEME "snews:"

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

nsMsgNewsFolder::nsMsgNewsFolder(void) : nsMsgLineBuffer(nsnull, PR_FALSE),
     mExpungedBytes(0), mGettingNews(PR_FALSE),
    mInitialized(PR_FALSE), mOptionLines(""), mUnsubscribedNewsgroupLines(""), 
    m_downloadMessageForOfflineUse(PR_FALSE), m_downloadingMultipleMessages(PR_FALSE), 
    mReadSet(nsnull), mGroupUsername(nsnull), mGroupPassword(nsnull), mAsciiName(nsnull)
{
  MOZ_COUNT_CTOR(nsNewsFolder); // double count these for now.
  /* we're parsing the newsrc file, and the line breaks are platform specific.
   * if MSG_LINEBREAK != CRLF, then we aren't looking for CRLF 
   */
  if (PL_strcmp(MSG_LINEBREAK, CRLF))
    SetLookingForCRLF(PR_FALSE);
}

nsMsgNewsFolder::~nsMsgNewsFolder(void)
{
  MOZ_COUNT_DTOR(nsNewsFolder);
  delete mReadSet;
  PR_Free(mGroupUsername);
  PR_Free(mGroupPassword);
  PR_Free(mAsciiName);
}

NS_IMPL_ADDREF_INHERITED(nsMsgNewsFolder, nsMsgDBFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgNewsFolder, nsMsgDBFolder)

NS_IMETHODIMP nsMsgNewsFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  *aInstancePtr = nsnull;
  if (aIID.Equals(NS_GET_IID(nsIMsgNewsFolder)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIMsgNewsFolder*, this);
  }              
  
  if(*aInstancePtr)
  {
    AddRef();
    return NS_OK;
  }
  
  return nsMsgDBFolder::QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsMsgNewsFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;

  PRBool isNewsServer = PR_FALSE;
  rv = GetIsServer(&isNewsServer);
  if (NS_FAILED(rv)) return rv;

  if (isNewsServer) 
  {  
    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = GetNntpServer(getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) return rv;
  
    rv = nntpServer->GetNewsrcFilePath(getter_AddRefs(mNewsrcFilePath));
    if (NS_FAILED(rv)) return rv;
      
    rv = LoadNewsrcFileAndCreateNewsgroups();
    if (NS_FAILED(rv)) return rv;
  }
  else 
  {
    // is not a host, so it has no newsgroups.  (what about categories??)
    rv = NS_OK;
  }
  
  return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::AddNewsgroup(const char *name, const char *setStr, nsIMsgFolder **child)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(child);
  NS_ENSURE_ARG_POINTER(setStr);
  NS_ENSURE_ARG_POINTER(name);
  
  nsCOMPtr <nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv); 
  if (NS_FAILED(rv)) return rv;
  if (!rdf) return NS_ERROR_FAILURE;
  
  nsCOMPtr <nsINntpIncomingServer> nntpServer;
  rv = GetNntpServer(getter_AddRefs(nntpServer));
  if (NS_FAILED(rv)) return rv;
  
  nsCAutoString uri(mURI);
  uri.Append('/');
  // URI should use UTF-8
  // (see RFC2396 Uniform Resource Identifiers (URI): Generic Syntax)
  // since we are forcing it to be latin-1 (IS0-8859-1)
  // we can just assign with conversion
  nsAutoString newsgroupName;
  newsgroupName.AssignWithConversion(name);
  
  nsCAutoString escapedName;
  rv = NS_MsgEscapeEncodeURLPath(newsgroupName, escapedName);
  if (NS_FAILED(rv)) return rv;
  
  rv = nntpServer->AddNewsgroup(escapedName.get());
  if (NS_FAILED(rv)) return rv;
  
  uri.Append(escapedName.get());
  
  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uri, getter_AddRefs(res));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIMsgNewsFolder> newsFolder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv)) return rv;        
  
  // cache this for when we open the db
  rv = newsFolder->SetReadSetFromStr(setStr);

  rv = folder->SetParent(this);
  NS_ENSURE_SUCCESS(rv,rv);
  
  // this what shows up in the UI
  rv = folder->SetName(newsgroupName.get());
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = folder->SetFlag(MSG_FOLDER_FLAG_NEWSGROUP);
  if (NS_FAILED(rv)) return rv;          
  
  PRUint32 numExistingGroups;
  rv = Count(&numExistingGroups);
  NS_ENSURE_SUCCESS(rv,rv);

  // add 1000 to prevent this problem:  1,10,11,2,3,4,5
  rv = folder->SetSortOrder(numExistingGroups + 1000);
  NS_ENSURE_SUCCESS(rv,rv);
  
  //convert to an nsISupports before appending
  nsCOMPtr<nsISupports> folderSupports(do_QueryInterface(folder));
  if(folderSupports)
	mSubFolders->AppendElement(folderSupports);
  *child = folder;
  folder->SetParent(this);
  NS_ADDREF(*child);

  return rv;
}


nsresult nsMsgNewsFolder::ParseFolder(nsFileSpec& path)
{
  NS_ASSERTION(0,"ParseFolder not implemented");
 	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgNewsFolder::Enumerate(nsIEnumerator **result)
{
#if 0
  nsresult rv = NS_OK;
  // for now, news folders contain both messages and folders
  // server is a folder, and it contains folders
  // newsgroup is a folder, and it contains messages
  //
  // eventually the top level server will not be a folder
  // and news folders will only contain messages
  nsIEnumerator* folders;
  nsIEnumerator* messages;
  rv = GetSubFolders(&folders);
  if (NS_FAILED(rv)) return rv;
  rv = GetMessages(nsnull, &messages);
  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
#endif
  NS_ASSERTION(PR_FALSE, "obsolete, right?");
  return NS_ERROR_FAILURE;
}

nsresult
nsMsgNewsFolder::AddDirectorySeparator(nsFileSpec &path)
{
    nsresult rv = NS_OK;
    if (mURI.Equals(kNewsRootURI)) 
    {
    // don't concat the full separator with .sbd
    }
    else 
    {
      // see if there's a dir with the same name ending with .sbd
      // unfortunately we can't just say:
      //          path += sep;
      // here because of the way nsFileSpec concatenates
      nsAutoString str;
      str.AssignWithConversion(nsFilePath(path));
#if 0
      nsAutoString sep;
      rv = nsGetNewsFolderSeparator(sep);
      if (NS_FAILED(rv)) return rv;
      str += sep;
#endif

      path = nsFilePath(str);
    }

    return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetSubFolders(nsIEnumerator* *result)
{
  nsresult rv;
  
  if (!mInitialized) 
  {
    // do this first, so we make sure to do it, even on failure.
    // see bug #70494
    mInitialized = PR_TRUE;
    
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;
    
    nsFileSpec path;
    rv = pathSpec->GetFileSpec(&path);
    if (NS_FAILED(rv)) return rv;
    
    nsFileSpec tempPath(path.GetNativePathCString(), PR_TRUE);
    
    rv = CreateSubFolders(path);
    if (NS_FAILED(rv)) return rv;

    // force ourselves to get initialized from cache
    // Don't care if it fails.  this will fail the first time after 
    // migration, but we continue on.  see #66018
    (void)UpdateSummaryTotals(PR_FALSE);
  }

  rv = mSubFolders->Enumerate(result);
  return rv;
}

//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsMsgNewsFolder::GetDatabase(nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  if (!mDatabase)
  {
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsresult folderOpen = msgDBService->OpenFolderDB(this, PR_TRUE, PR_FALSE, getter_AddRefs(mDatabase));
    
    if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
      folderOpen = msgDBService->OpenFolderDB(this, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase));

    if (NS_FAILED(folderOpen))
      return folderOpen;

    if(mAddListener)
      rv = mDatabase->AddListener(this);
      
    nsCOMPtr<nsINewsDatabase> db = do_QueryInterface(mDatabase, &rv);
    if (NS_FAILED(rv)) 
      return rv;        
      
    rv = db->SetReadSet(mReadSet);
    if (NS_FAILED(rv)) 
      return rv;        

    rv = UpdateSummaryTotals(PR_TRUE);
    if (NS_FAILED(rv)) 
      return rv;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsMsgNewsFolder::UpdateFolder(nsIMsgWindow *aWindow)
{
  nsresult rv = GetDatabase(aWindow);	// want this cached...
  if (NS_SUCCEEDED(rv))
  {
    if (mDatabase)
    {
      nsCOMPtr<nsIMsgRetentionSettings> retentionSettings;
      nsresult rv = GetRetentionSettings(getter_AddRefs(retentionSettings));
      if (NS_SUCCEEDED(rv))
        rv = mDatabase->ApplyRetentionSettings(retentionSettings);
    }
    rv = AutoCompact(aWindow);
    NS_ENSURE_SUCCESS(rv,rv);
    // GetNewMessages has to be the last rv set before we get to the next check, so
    // that we'll have rv set to NS_MSG_ERROR_OFFLINE when offline and send
    // a folder loaded notification to the front end.
    rv = GetNewMessages(aWindow, nsnull);
  }
  if (rv == NS_MSG_ERROR_OFFLINE)
  {
    rv = NS_OK;
    NotifyFolderEvent(mFolderLoadedAtom);
  }

  return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanSubscribe(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;

  PRBool isNewsServer = PR_FALSE;
  nsresult rv = GetIsServer(&isNewsServer);
  if (NS_FAILED(rv)) return rv;
 
  // you can only subscribe to news servers, not news groups
  *aResult = isNewsServer;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanFileMessages(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  // you can't file messages into a news server or news group
  *aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanDeleteMessages(PRBool *aCanDeleteMessages)
{
  NS_ENSURE_ARG_POINTER(aCanDeleteMessages);
  *aCanDeleteMessages = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanCreateSubfolders(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  // you can't create subfolders on a news server or a news group
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanRename(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  // you can't rename a news server or a news group
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetCanCompact(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;
  // you can't compact a news server or a news group
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  nsresult rv = GetDatabase(aMsgWindow);
  *result = nsnull;
    
  if(NS_SUCCEEDED(rv))
    rv = mDatabase->EnumerateMessages(result);

  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::GetFolderURL(char **url)
{
  NS_ENSURE_ARG_POINTER(url);

  nsXPIDLCString hostName;
  nsresult rv = GetHostname(getter_Copies(hostName));
  nsXPIDLCString groupName;
  rv = GetAsciiName(getter_Copies(groupName));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 port;
  PRBool isSecure = PR_FALSE;
  rv = server->GetIsSecure(&isSecure);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = server->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, rv);
  const char *newsScheme = (isSecure) ? SNEWS_SCHEME : NEWS_SCHEME;
  *url = PR_smprintf("%s//%s:%ld/%s", newsScheme, hostName.get(), port, groupName.get());
  return NS_OK;

}

NS_IMETHODIMP nsMsgNewsFolder::SetNewsrcHasChanged(PRBool newsrcHasChanged)
{
    nsresult rv;

    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = GetNntpServer(getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) return rv;

    return nntpServer->SetNewsrcHasChanged(newsrcHasChanged);
}

NS_IMETHODIMP nsMsgNewsFolder::CreateSubfolder(const PRUnichar *uninewsgroupname, nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(uninewsgroupname);
  if (!*uninewsgroupname) return NS_ERROR_FAILURE;
  
  nsCAutoString newsgroupname; 
  newsgroupname.AssignWithConversion(uninewsgroupname);
  
  nsFileSpec path;
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  rv = pathSpec->GetFileSpec(&path);
  
  nsCOMPtr<nsIMsgFolder> child;
  
  // Create an empty database for this mail folder, set its name from the user  
  nsCOMPtr<nsIMsgDatabase> newsDBFactory;
  nsCOMPtr <nsIMsgDatabase> newsDB;
  
  //Now we have a valid directory or we have returned.
  //Make sure the new folder name is valid
  
  // remember, some file systems (like mac) can't handle long file names
  nsCAutoString hashedName = newsgroupname;
  rv = NS_MsgHashIfNecessary(hashedName);
  path += hashedName.get();
  
  //Now let's create the actual new folder
  rv = AddNewsgroup(newsgroupname.get(), "", getter_AddRefs(child));
  
  if (NS_SUCCEEDED(rv))
    SetNewsrcHasChanged(PR_TRUE); // subscribe UI does this - but maybe we got here through auto-subscribe

  if(NS_SUCCEEDED(rv) && child)
    NotifyItemAdded(child);

  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::Delete()
{
  nsresult rv = GetDatabase(nsnull);
  
  if(NS_SUCCEEDED(rv)) 
  {
    mDatabase->ForceClosed();
    mDatabase = nsnull;
  }
  
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  nsFileSpec path;
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;
  
  // delete local store, if it exists
  if (path.Exists())
    path.Delete(PR_FALSE);
  // Remove summary file.	
  nsNewsSummarySpec summarySpec(path);
  summarySpec.Delete(PR_FALSE);
  
  nsCOMPtr <nsINntpIncomingServer> nntpServer;
  rv = GetNntpServer(getter_AddRefs(nntpServer));
  if (NS_FAILED(rv)) return rv;
  
  nsXPIDLString name;
  rv = GetName(getter_Copies(name));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCAutoString escapedName;
  rv = NS_MsgEscapeEncodeURLPath(name, escapedName);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = nntpServer->RemoveNewsgroup(escapedName.get());
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = SetNewsrcHasChanged(PR_TRUE);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::Rename(const PRUnichar *newName, nsIMsgWindow *msgWindow)
{
  NS_ASSERTION(0,"Rename not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::GetAbbreviatedName(PRUnichar * *aAbbreviatedName)
{
  nsresult rv = NS_OK;

  if (!aAbbreviatedName)
    return NS_ERROR_NULL_POINTER;

  rv = nsMsgDBFolder::GetPrettyName(aAbbreviatedName);
  if(NS_FAILED(rv)) return rv;

  // only do this for newsgroup names, not for newsgroup hosts.
  PRBool isNewsServer = PR_FALSE;
  rv = GetIsServer(&isNewsServer);
  if (NS_FAILED(rv)) return rv;
  
  if (!isNewsServer) {  
    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = GetNntpServer(getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) return rv;
    
    PRBool abbreviate = PR_TRUE;
    rv = nntpServer->GetAbbreviate(&abbreviate);
    if (NS_FAILED(rv)) return rv;
    
    if (abbreviate) {
      rv = AbbreviatePrettyName(aAbbreviatedName, 1 /* hardcoded for now */);
    }
  }

  return rv;
}


// original code from Oleg Rekutin
// rekusha@asan.com
// Public domain, created by Oleg Rekutin
//
// takes a newsgroup name, number of words from the end to leave unabberviated
// the newsgroup name, will get reset to the following format:
// x.x.x, where x is the first letter of each word and with the
// exception of last 'fullwords' words, which are left intact.
// If a word has a dash in it, it is abbreviated as a-b, where
// 'a' is the first letter of the part of the word before the
// dash and 'b' is the first letter of the part of the word after
// the dash
nsresult nsMsgNewsFolder::AbbreviatePrettyName(PRUnichar ** prettyName, PRInt32 fullwords)
{
  if (!prettyName)
    return NS_ERROR_NULL_POINTER;
  
  nsAutoString name(*prettyName);
  PRInt32 totalwords = 0; // total no. of words
  
  // get the total no. of words
  PRInt32 pos = 0;
  while(1)
  {
    pos = name.FindChar('.', pos);
    if(pos == -1)
    {
      totalwords++;
      break;
    }
    else
    {
      totalwords++;
      pos++;
    }
  }
  
  // get the no. of words to abbreviate
  PRInt32 abbrevnum = totalwords - fullwords;
  if (abbrevnum < 1)
    return NS_OK; // nothing to abbreviate
  
  // build the ellipsis
  nsAutoString out;
  
  out += name[0];
  
  PRInt32    length = name.Length();
  PRInt32    newword = 0;     // == 2 if done with all abbreviated words
  
  fullwords = 0;
  for (PRInt32 i = 1; i < length; i++) {
    if (newword < 2) {
      switch (name[i]) {
      case '.':
        fullwords++;
        // check if done with all abbreviated words...
        if (fullwords == abbrevnum)
          newword = 2;
        else
          newword = 1;
        break;
      case '-':
        newword = 1;
        break;
      default:
        if (newword)
          newword = 0;
        else
          continue;
      }
    }
    out += name[i];
  }

  if (!prettyName)
    return NS_ERROR_NULL_POINTER;
  // we are going to set *prettyName to something else, so free what was there
  
  PR_Free(*prettyName);
  *prettyName = ToNewUnicode(out);
  
  return (*prettyName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsMsgNewsFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{

  nsresult openErr;
  if(!db || !folderInfo)
    return NS_ERROR_NULL_POINTER; 

  openErr = GetDatabase(nsnull);
  *db = mDatabase;
  if (mDatabase) {
    NS_ADDREF(*db);
    if (NS_SUCCEEDED(openErr))
      openErr = (*db)->GetDBFolderInfo(folderInfo);
  }

  return openErr;
}

/* this used to be MSG_FolderInfoNews::UpdateSummaryFromNNTPInfo() */
NS_IMETHODIMP 
nsMsgNewsFolder::UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total)
{
  nsresult rv = NS_OK;
  PRBool newsrcHasChanged = PR_FALSE;
  PRInt32 oldUnreadMessages = mNumUnreadMessages;
  PRInt32 oldTotalMessages = mNumTotalMessages;
  
  char *setStr = nsnull;
  /* First, mark all of the articles now known to be expired as read. */
  if (oldest > 1) 
  { 
    nsXPIDLCString oldSet;
    mReadSet->Output(getter_Copies(oldSet));
    mReadSet->AddRange(1, oldest - 1);
    rv = mReadSet->Output(&setStr);
    if (setStr && nsCRT::strcmp(setStr, oldSet))
      newsrcHasChanged = PR_TRUE;
  }
  
  /* Now search the newsrc line and figure out how many of these messages are marked as unread. */
  
  /* make sure youngest is a least 1. MSNews seems to return a youngest of 0. */
  if (youngest == 0) 
    youngest = 1;
  
  PRInt32 unread = mReadSet->CountMissingInRange(oldest, youngest);
  NS_ASSERTION(unread >= 0,"CountMissingInRange reported unread < 0");
  if (unread < 0)
    // servers can send us stuff like "211 0 41 40 nz.netstatus"
    // we should handle it gracefully.
    unread = 0;
  
  if (unread > total) 
  {
    /* This can happen when the newsrc file shows more unread than exist in the group (total is not necessarily `end - start'.) */
    unread = total;
    PRInt32 deltaInDB = mNumTotalMessages - mNumUnreadMessages;
    //PRint32 deltaInDB = m_totalInDB - m_unreadInDB;
    /* if we know there are read messages in the db, subtract that from the unread total */
    if (deltaInDB > 0)
      unread -= deltaInDB;
  }
  
  mNumUnreadMessages = unread;
  mNumTotalMessages = total;
#if 0
  m_nntpHighwater = youngest;
  m_nntpTotalArticles = total;
#endif
  
  //Need to notify listeners that total count changed.
  if(oldTotalMessages != mNumTotalMessages)
    NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, mNumTotalMessages);
  
  if(oldUnreadMessages != mNumUnreadMessages) 
    NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, mNumUnreadMessages);
  
  nsCRT::free(setStr);
  setStr = nsnull;
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::UpdateSummaryTotals(PRBool force)
{
  if (!mNotifyCountChanges)
    return NS_OK;
  
  PRInt32 oldUnreadMessages = mNumUnreadMessages;
  PRInt32 oldTotalMessages = mNumTotalMessages;
  //We need to read this info from the database
  nsresult ret = ReadDBFolderInfo(force);
  
  if (NS_SUCCEEDED(ret))
  {
    //Need to notify listeners that total count changed.
    if(oldTotalMessages != mNumTotalMessages) 
      NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, mNumTotalMessages);
    
    if(oldUnreadMessages != mNumUnreadMessages) 
      NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, mNumUnreadMessages);
    
    FlushToFolderCache();
  }
  return ret;
}

NS_IMETHODIMP nsMsgNewsFolder::GetExpungedBytesCount(PRUint32 *count)
{
  if(!count)
    return NS_ERROR_NULL_POINTER;

  *count = mExpungedBytes;

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetDeletable(PRBool *deletable)
{
//  NS_ASSERTION(0,"GetDeletable() not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP nsMsgNewsFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
//  NS_ASSERTION(0,"GetRequiresCleanup not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::GetSizeOnDisk(PRUint32 *size)
{
//  NS_ASSERTION(0, "GetSizeOnDisk not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* this is news, so remember that DeleteMessage is really CANCEL */
NS_IMETHODIMP 
nsMsgNewsFolder::DeleteMessages(nsISupportsArray *messages, nsIMsgWindow *aMsgWindow, 
                                PRBool deleteStorage, PRBool isMove,
                                nsIMsgCopyServiceListener* listener, PRBool allowUndo)
{
  nsresult rv = NS_OK;
 
  NS_ENSURE_ARG_POINTER(messages);
  NS_ENSURE_ARG_POINTER(aMsgWindow);
 
  PRUint32 count = 0;
  rv = messages->Count(&count);
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (count != 1) 
  {
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString alertText;
    rv = bundle->GetStringFromName(NS_LITERAL_STRING("onlyCancelOneMessage").get(), getter_Copies(alertText));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrompt> dialog;
    rv = aMsgWindow->GetPromptDialog(getter_AddRefs(dialog));
    NS_ENSURE_SUCCESS(rv,rv);

    if (dialog) 
    {
      rv = dialog->Alert(nsnull, (const PRUnichar *) alertText);
      NS_ENSURE_SUCCESS(rv,rv);
    }
    // return failure, since the cancel failed
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgDBHdr> msgHdr(do_QueryElementAt(messages, 0));

  // for cancel, we need to
  // turn "newsmessage://sspitzer@news.mozilla.org/netscape.test#5428"
  // into "news://sspitzer@news.mozilla.org/23423@netscape.com"

  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString serverURI;
  rv = server->GetServerURI(getter_Copies(serverURI));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString messageID;
  rv = msgHdr->GetMessageId(getter_Copies(messageID));
  NS_ENSURE_SUCCESS(rv,rv);
  
  // we need to escape the message ID, 
  // it might contain characters which will mess us up later, like #
  // see bug #120502
  char *escapedMessageID = nsEscape(messageID.get(), url_Path);
  if (!escapedMessageID)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCAutoString cancelURL((const char *)serverURI);
  cancelURL += '/';
  cancelURL += escapedMessageID;
  cancelURL += "?cancel";

  PR_Free(escapedMessageID);

  nsXPIDLCString messageURI;
  rv = GetUriForMsg(msgHdr, getter_Copies(messageURI));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = nntpService->CancelMessage(cancelURL.get(), messageURI, nsnull /* consumer */, nsnull, aMsgWindow, nsnull);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::GetNewMessages(nsIMsgWindow *aMsgWindow, nsIUrlListener *aListener)
{
  return GetNewsMessages(aMsgWindow, PR_FALSE, aListener);
}

NS_IMETHODIMP nsMsgNewsFolder::GetNextNMessages(nsIMsgWindow *aMsgWindow)
{
  return GetNewsMessages(aMsgWindow, PR_TRUE, nsnull);
}

nsresult nsMsgNewsFolder::GetNewsMessages(nsIMsgWindow *aMsgWindow, PRBool aGetOld, nsIUrlListener *aUrlListener)
{
  nsresult rv = NS_OK;

  PRBool isNewsServer = PR_FALSE;
  rv = GetIsServer(&isNewsServer);
  if (NS_FAILED(rv)) return rv;
  
  if (isNewsServer) 
    // get new messages only works on a newsgroup, not a news server
    return NS_OK;

  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsINntpIncomingServer> nntpServer;
  rv = GetNntpServer(getter_AddRefs(nntpServer));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr <nsIURI> resultUri;
  rv = nntpService->GetNewNews(nntpServer, mURI.get(), aGetOld, this,
                               aMsgWindow, getter_AddRefs(resultUri));
  if (aUrlListener && NS_SUCCEEDED(rv) && resultUri)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(resultUri));
    if (msgUrl)
      msgUrl->RegisterListener(aUrlListener);
  }
  return rv;
}

nsresult 
nsMsgNewsFolder::LoadNewsrcFileAndCreateNewsgroups()
{
  nsresult rv = NS_OK;
  if (!mNewsrcFilePath) return NS_ERROR_FAILURE;

  PRBool exists = PR_FALSE;

  rv = mNewsrcFilePath->Exists(&exists);
  if (NS_FAILED(rv)) return rv;

  if (!exists)
  	// it is ok for the newsrc file to not exist yet
	return NS_OK;

  char *buffer = nsnull;
  rv = mNewsrcFilePath->OpenStreamForReading();
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt32 numread = 0;

  if (NS_FAILED(m_newsrcInputStream.GrowBuffer(NEWSRC_FILE_BUFFER_SIZE))) 
    return NS_ERROR_FAILURE;

	
  while (1) 
  {
    buffer = m_newsrcInputStream.GetBuffer();
    rv = mNewsrcFilePath->Read(&buffer, NEWSRC_FILE_BUFFER_SIZE, &numread);
    NS_ENSURE_SUCCESS(rv,rv);
    if (numread == 0) {
      break;
    }
    else {
      rv = BufferInput(m_newsrcInputStream.GetBuffer(), numread);
      if (NS_FAILED(rv)) {
        break;
      }
    }
  }

  mNewsrcFilePath->CloseStream();
  
  return rv;
}


PRInt32
nsMsgNewsFolder::HandleLine(char* line, PRUint32 line_size)
{
	return HandleNewsrcLine(line, line_size);
}

PRInt32
nsMsgNewsFolder::HandleNewsrcLine(char* line, PRUint32 line_size)
{
  nsresult rv;
  
  /* guard against blank line lossage */
  if (line[0] == '#' || line[0] == nsCRT::CR || line[0] == nsCRT::LF) return 0;
  
  line[line_size] = 0;
  
  if ((line[0] == 'o' || line[0] == 'O') &&
    !PL_strncasecmp (line, "options", 7)) 
    return RememberLine(line);
  
  char *s = nsnull;
  char *setStr = nsnull;
  char *end = line + line_size;
  
  for (s = line; s < end; s++)
    if ((*s == ':') || (*s == '!'))
      break;
    
    if (*s == 0)
      /* What is this?? Well, don't just throw it away... */
      return RememberLine(line);
    
    PRBool subscribed = (*s == ':');
    setStr = s+1;
    *s = '\0';
    
    if (*line == '\0') 
      return 0;
    
  // previous versions of Communicator poluted the
  // newsrc files with articles
  // (this would happen when you clicked on a link like
  // news://news.mozilla.org/3746EF3F.6080309@netscape.com)
  //
  // legal newsgroup names can't contain @ or %
  // 
  // News group names are structured into parts separated by dots, 
  // for example "netscape.public.mozilla.mail-news". 
  // Each part may be up to 14 characters long, and should consist 
  // only of letters, digits, "+" and "-", with at least one letter
  //
  // @ indicates an article and %40 is @ escaped.
  // previous versions of Communicator also dumped
  // the escaped version into the newsrc file
  //
  // So lines like this in a newsrc file should be ignored:
  // 3746EF3F.6080309@netscape.com:
  // 3746EF3F.6080309%40netscape.com:
  if (PL_strstr(line,"@") || PL_strstr(line,"%40")) 
    // skipping, it contains @ or %40
    subscribed = PR_FALSE;

  if (subscribed) 
  {
    // we're subscribed, so add it
    nsCOMPtr <nsIMsgFolder> child;
    
    rv = AddNewsgroup(line, setStr, getter_AddRefs(child));
    
    if (NS_FAILED(rv)) return -1;
  }
  else {
    rv = RememberUnsubscribedGroup(line, setStr);
    if (NS_FAILED(rv)) return -1;
  }

  return 0;
}


nsresult
nsMsgNewsFolder::RememberUnsubscribedGroup(const char *newsgroup, const char *setStr)
{
  if (newsgroup) 
  {
    mUnsubscribedNewsgroupLines += newsgroup;
    mUnsubscribedNewsgroupLines += "! ";
    if (setStr) 
      mUnsubscribedNewsgroupLines += setStr;
    else
      mUnsubscribedNewsgroupLines += MSG_LINEBREAK;
  }
  return NS_OK;
}

PRInt32
nsMsgNewsFolder::RememberLine(const char* line)
{
  mOptionLines = line;
  mOptionLines += MSG_LINEBREAK;
  
  return 0;
}

nsresult nsMsgNewsFolder::ForgetLine()
{
  mOptionLines = "";
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetGroupUsername(char **aGroupUsername)
{
  NS_ENSURE_ARG_POINTER(aGroupUsername);
  nsresult rv;
  
  if (mGroupUsername) 
  {
    *aGroupUsername = nsCRT::strdup(mGroupUsername);
    rv = NS_OK;
  }
  else 
  {
    rv = NS_ERROR_FAILURE;
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::SetGroupUsername(const char *aGroupUsername)
{
  PR_FREEIF(mGroupUsername);
  
  if (aGroupUsername) 
    mGroupUsername = nsCRT::strdup(aGroupUsername);
  
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetGroupPassword(char **aGroupPassword)
{
    NS_ENSURE_ARG_POINTER(aGroupPassword);
    nsresult rv;

    if (mGroupPassword) 
    {
        *aGroupPassword = nsCRT::strdup(mGroupPassword);
        rv = NS_OK;
    }
    else 
    {
        rv = NS_ERROR_FAILURE;
    }

    return rv;      
}

NS_IMETHODIMP nsMsgNewsFolder::SetGroupPassword(const char *aGroupPassword)
{
  PR_FREEIF(mGroupPassword);
  
  if (aGroupPassword) 
    mGroupPassword = nsCRT::strdup(aGroupPassword);
  
  return NS_OK;    
}

nsresult nsMsgNewsFolder::CreateNewsgroupUsernameUrlForSignon(const char *inUriStr, char **result)
{
    return CreateNewsgroupUrlForSignon(inUriStr, "username", result);
}

nsresult nsMsgNewsFolder::CreateNewsgroupPasswordUrlForSignon(const char *inUriStr, char **result)
{
    return CreateNewsgroupUrlForSignon(inUriStr, "password", result);
}

nsresult nsMsgNewsFolder::CreateNewsgroupUrlForSignon(const char *inUriStr, const char *ref, char **result)
{
    nsresult rv;
    PRInt32 port = 0;

    nsCOMPtr<nsIURL> url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = GetNntpServer(getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) return rv;

    PRBool singleSignon = PR_TRUE;
    rv = nntpServer->GetSingleSignon(&singleSignon);

    if (singleSignon)
    {

      nsXPIDLCString serverURI;
      rv = server->GetServerURI(getter_Copies(serverURI));
      if (NS_FAILED(rv)) return rv;

      rv = url->SetSpec(serverURI);
      if (NS_FAILED(rv)) return rv;
    }
    else
    {
    rv = url->SetSpec(nsDependentCString(inUriStr));
    if (NS_FAILED(rv)) return rv;
    }

    rv = url->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port <= 0) 
    {
        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = GetServer(getter_AddRefs(server));
        if (NS_FAILED(rv)) return rv;

        PRBool isSecure = PR_FALSE;
        rv = server->GetIsSecure(&isSecure);
        if (NS_FAILED(rv)) return rv;

        rv = url->SetPort((isSecure) ? SECURE_NEWS_PORT: NEWS_PORT);
        if (NS_FAILED(rv)) return rv;
    }

    rv = url->SetRef(nsDependentCString(ref));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString spec;
    rv = url->GetSpec(spec);
    if (NS_FAILED(rv)) return rv;

    *result = ToNewCString(spec);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgNewsFolder::ForgetGroupUsername()
{
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = SetGroupUsername(nsnull);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString signonURL;
    rv = CreateNewsgroupUsernameUrlForSignon(mURI.get(), getter_Copies(signonURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), signonURL);

    //this is need to make sure wallet service has been created
    rv = CreateServicesForPasswordManager();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->NotifyObservers(uri, "login-failed", nsnull);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::ForgetGroupPassword()
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = SetGroupPassword(nsnull);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString signonURL;
    rv = CreateNewsgroupPasswordUrlForSignon(mURI.get(), getter_Copies(signonURL));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), signonURL);

    //this is need to make sure wallet service has been created
    rv = CreateServicesForPasswordManager();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = observerService->NotifyObservers(uri, "login-failed", nsnull);
    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetGroupPasswordWithUI(const PRUnichar * aPromptMessage, const
                                       PRUnichar *aPromptTitle,
                                       nsIMsgWindow* aMsgWindow,
                                       char **aGroupPassword)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(aGroupPassword);

  if (!mGroupPassword) 
  {
    // prompt the user for the password
        
    nsCOMPtr<nsIAuthPrompt> dialog;
#ifdef DEBUG_seth
    NS_ASSERTION(aMsgWindow,"no msg window");
#endif
    if (aMsgWindow) 
    {
      nsCOMPtr<nsIDocShell> docShell;

      rv = aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell, &rv));
      if (NS_FAILED(rv)) return rv;

      dialog = do_GetInterface(webShell, &rv);
			if (NS_FAILED(rv)) return rv;
    }
    else 
    {
      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch)
        wwatch->GetNewAuthPrompter(0, getter_AddRefs(dialog));

      if (!dialog) return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(dialog,"we didn't get a net prompt");
    if (dialog) 
    {
      nsXPIDLString uniGroupPassword;

      PRBool okayValue = PR_TRUE;
            
      nsXPIDLCString signonURL;
      rv = CreateNewsgroupPasswordUrlForSignon(mURI.get(), getter_Copies(signonURL));
      if (NS_FAILED(rv)) return rv;

      rv = dialog->PromptPassword(aPromptTitle, aPromptMessage, NS_ConvertASCIItoUCS2(NS_STATIC_CAST(const char*, signonURL)).get(), nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                  getter_Copies(uniGroupPassword), &okayValue);
      if (NS_FAILED(rv)) return rv;

      if (!okayValue) // if the user pressed cancel, just return NULL;
      {
        *aGroupPassword = nsnull;
        return rv;
      }

      // we got a password back...so remember it
      rv = SetGroupPassword(NS_LossyConvertUCS2toASCII(uniGroupPassword).get());
      if (NS_FAILED(rv)) return rv;

    } // if we got a prompt dialog
  } // if the password is empty

  rv = GetGroupPassword(aGroupPassword);
  return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetGroupUsernameWithUI(const PRUnichar * aPromptMessage, const
                                       PRUnichar *aPromptTitle,
                                       nsIMsgWindow* aMsgWindow,
                                       char **aGroupUsername)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  NS_ENSURE_ARG_POINTER(aGroupUsername);
  
  if (!mGroupUsername) 
  {
    // prompt the user for the username
    
    nsCOMPtr<nsIAuthPrompt> dialog;
#ifdef DEBUG_seth
    NS_ASSERTION(aMsgWindow,"no msg window");
#endif
    if (aMsgWindow) 
    {
      // prompt the user for the password
      nsCOMPtr<nsIDocShell> docShell;
      rv = aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
      if (NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell, &rv));
      if (NS_FAILED(rv)) return rv;
      dialog = do_GetInterface(webShell, &rv);
      if (NS_FAILED(rv)) return rv;
    }
    else 
    {
      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
      if (wwatch)
        wwatch->GetNewAuthPrompter(0, getter_AddRefs(dialog));
      
      if (!dialog) return NS_ERROR_FAILURE;
    }
    
    NS_ASSERTION(dialog,"we didn't get a net prompt");
    if (dialog) 
    {
      nsXPIDLString uniGroupUsername;
      
      PRBool okayValue = PR_TRUE;
      
      nsXPIDLCString signonURL;
      rv = CreateNewsgroupUsernameUrlForSignon(mURI.get(), getter_Copies(signonURL));
      if (NS_FAILED(rv)) return rv;
      
      rv = dialog->Prompt(aPromptTitle, aPromptMessage, NS_ConvertASCIItoUCS2(signonURL).get(), 
        nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY, nsnull,
        getter_Copies(uniGroupUsername), &okayValue);
      if (NS_FAILED(rv)) return rv;
      
      if (!okayValue) // if the user pressed cancel, just return NULL;
      {
        *aGroupUsername= nsnull;
        return rv;
      }
      
      // we got a username back, remember it
      rv = SetGroupUsername(NS_LossyConvertUCS2toASCII(uniGroupUsername).get());
      if (NS_FAILED(rv)) return rv;
      
    } // if we got a prompt dialog
  } // if the password is empty
  
  rv = GetGroupUsername(aGroupUsername);
  return rv;
}

nsresult nsMsgNewsFolder::CreateBaseMessageURI(const char *aURI)
{
  nsresult rv;

  rv = nsCreateNewsBaseMessageURI(aURI, &mBaseMessageURI);
  return rv;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetNewsrcLine(char **newsrcLine)
{
  nsresult rv;
  
  if (!newsrcLine) return NS_ERROR_NULL_POINTER;
  
  nsXPIDLCString newsgroupname;
  rv = GetAsciiName(getter_Copies(newsgroupname));
  if (NS_FAILED(rv)) return rv;
  
  nsCAutoString newsrcLineStr;
  newsrcLineStr = (const char *)newsgroupname;
  newsrcLineStr += ":";
  
  nsXPIDLCString setStr;
  if (mReadSet) {
    mReadSet->Output(getter_Copies(setStr));
    if (NS_SUCCEEDED(rv)) 
    {
      newsrcLineStr += " ";
      newsrcLineStr += setStr;
      newsrcLineStr += MSG_LINEBREAK;
    }
  }
  
  *newsrcLine = ToNewCString(newsrcLineStr);
  
  if (!*newsrcLine) return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::SetReadSetFromStr(const char *newsrcLine)
{
  if (!newsrcLine) return NS_ERROR_NULL_POINTER;

  delete mReadSet;

  mReadSet = nsMsgKeySet::Create(newsrcLine);

  if (!mReadSet) return NS_ERROR_OUT_OF_MEMORY;

  // Now that mReadSet is recreated, make sure it's stored in the db as well.
  nsCOMPtr<nsINewsDatabase> db = do_QueryInterface(mDatabase);
  if (db) // it's ok not to have a db here.
    db->SetReadSet(mReadSet);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetUnsubscribedNewsgroupLines(char **aUnsubscribedNewsgroupLines)
{
    if (!aUnsubscribedNewsgroupLines) return NS_ERROR_NULL_POINTER;

    if (!mUnsubscribedNewsgroupLines.IsEmpty())
        *aUnsubscribedNewsgroupLines= ToNewCString(mUnsubscribedNewsgroupLines);
    
    return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetOptionLines(char **optionLines)
{
    if (!optionLines) return NS_ERROR_NULL_POINTER;

    if (!mOptionLines.IsEmpty()) 
        *optionLines = ToNewCString(mOptionLines);
    
    return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::OnReadChanged(nsIDBChangeListener * aInstigator)
{
    return SetNewsrcHasChanged(PR_TRUE);
}


NS_IMETHODIMP
nsMsgNewsFolder::GetAsciiName(char **asciiName)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(asciiName);
  if (!mAsciiName) 
  {
    nsXPIDLString name;
    rv = GetName(getter_Copies(name));
    NS_ENSURE_SUCCESS(rv,rv);
    
    // convert to ASCII
    nsCAutoString tmpStr;
    tmpStr.AssignWithConversion(name);
    
    mAsciiName = nsCRT::strdup(tmpStr.get());
    if (!mAsciiName) return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *asciiName = nsCRT::strdup(mAsciiName);
  if (!*asciiName) return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetNntpServer(nsINntpIncomingServer **result)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(result);

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) 
      return rv;

    if (!server) 
      return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = server->QueryInterface(NS_GET_IID(nsINntpIncomingServer),
                                getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) 
      return rv;

    NS_IF_ADDREF(*result = nntpServer);
    return NS_OK;
}

// this gets called after the message actually gets cancelled
// it removes the cancelled message from the db
NS_IMETHODIMP nsMsgNewsFolder::RemoveMessage(nsMsgKey key)
{
  nsresult rv = GetDatabase(nsnull);
  NS_ENSURE_SUCCESS(rv, rv); // if GetDatabase succeeds, mDatabase will be non-null
  return mDatabase->DeleteMessage(key, nsnull, PR_FALSE);
}

NS_IMETHODIMP nsMsgNewsFolder::CancelComplete()
{
  NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::CancelFailed()
{
  NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetSaveArticleOffline(PRBool *aBool)
{
  NS_ENSURE_ARG(aBool);
  *aBool = m_downloadMessageForOfflineUse;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::SetSaveArticleOffline(PRBool aBool)
{
  m_downloadMessageForOfflineUse = aBool;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow)
{
  nsMsgKeyArray srcKeyArray;

  SetSaveArticleOffline(PR_TRUE); 
  nsresult rv;

  // build up message keys.
  if (mDatabase)
  {
    nsCOMPtr <nsISimpleEnumerator> enumerator;
    rv = mDatabase->EnumerateMessages(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(rv) && enumerator)
    {
      PRBool hasMore;

      while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&hasMore)) && hasMore) 
      {
        nsCOMPtr <nsIMsgDBHdr> pHeader;
        rv = enumerator->GetNext(getter_AddRefs(pHeader));
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (pHeader && NS_SUCCEEDED(rv))
        {
          PRBool shouldStoreMsgOffline = PR_FALSE;
          nsMsgKey msgKey;
          pHeader->GetMessageKey(&msgKey);
          MsgFitsDownloadCriteria(msgKey, &shouldStoreMsgOffline);
          if (shouldStoreMsgOffline)
            srcKeyArray.Add(msgKey);
        }
      }
    }
  }
  DownloadNewsArticlesToOfflineStore *downloadState = new DownloadNewsArticlesToOfflineStore(msgWindow, mDatabase, this);
  if (!downloadState)
    return NS_ERROR_OUT_OF_MEMORY;

  m_downloadingMultipleMessages = PR_TRUE;

  return downloadState->DownloadArticles(msgWindow, this, &srcKeyArray);
}

NS_IMETHODIMP nsMsgNewsFolder::DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *window)
{
  nsMsgKeyArray srcKeyArray;

  SetSaveArticleOffline(PR_TRUE); // ### TODO need to clear this when we've finished
  PRUint32 count = 0;
  PRUint32 i;
  nsresult rv = messages->Count(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // build up message keys.
  for (i = 0; i < count; i++)
  {
    nsMsgKey key;
    nsCOMPtr <nsIMsgDBHdr> msgDBHdr = do_QueryElementAt(messages, i, &rv);
    if (msgDBHdr)
      rv = msgDBHdr->GetMessageKey(&key);
    if (NS_SUCCEEDED(rv))
      srcKeyArray.Add(key);
  }
  DownloadNewsArticlesToOfflineStore *downloadState = new DownloadNewsArticlesToOfflineStore(window, mDatabase, this);
  if (!downloadState)
    return NS_ERROR_OUT_OF_MEMORY;

  m_downloadingMultipleMessages = PR_TRUE;

  return downloadState->DownloadArticles(window, this, &srcKeyArray);
}

// line does not have a line terminator (e.g., CR or CRLF)
NS_IMETHODIMP nsMsgNewsFolder::NotifyDownloadedLine(const char *line, nsMsgKey keyOfArticle)
{
  nsresult rv = NS_OK;
  if (m_downloadMessageForOfflineUse && !m_offlineHeader)
  {
    GetMessageHeader(keyOfArticle, getter_AddRefs(m_offlineHeader));
    rv = StartNewOfflineMessage();
  }

  m_numOfflineMsgLines++;

  if (m_tempMessageStream)
  {
    if (line[0] == '.' && line[1] == 0)
    {
      // end of article.
      if (m_offlineHeader)
        EndNewOfflineMessage();

      if (m_tempMessageStream && !m_downloadingMultipleMessages)
      {
        m_tempMessageStream->Close();
        m_tempMessageStream = nsnull;
      }
    }
    else
    {
      PRUint32 count = 0;
      rv = m_tempMessageStream->Write(line, 
           strlen(line), &count);
      if (NS_SUCCEEDED(rv))
        rv = m_tempMessageStream->Write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &count);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to write to stream");
    }
  }
                                                                                
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;

  rv = GetDatabase(nsnull);
  if (mDatabase)
  {
    nsCOMPtr<nsIMsgRetentionSettings> retentionSettings;
    rv = GetRetentionSettings(getter_AddRefs(retentionSettings));
    if (NS_SUCCEEDED(rv))
      rv = mDatabase->ApplyRetentionSettings(retentionSettings);
  }
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::GetMessageIdForKey(nsMsgKey key, char **result)
{
  nsresult rv;

  if (!mDatabase) return NS_ERROR_UNEXPECTED;

  nsCOMPtr <nsIMsgDBHdr> hdr;
  rv = mDatabase->GetMsgHdrForKey(key, getter_AddRefs(hdr));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!hdr) return NS_ERROR_INVALID_ARG;

  return hdr->GetMessageId(result);
}

NS_IMETHODIMP nsMsgNewsFolder::SetSortOrder(PRInt32 order)
{
  mSortOrder = order;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetSortOrder(PRInt32 *order)
{
  NS_ENSURE_ARG_POINTER(order);
  *order = mSortOrder;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetPersistElided(PRBool *aPersistElided)
{
  nsresult rv;

  PRBool isNewsServer = PR_FALSE;
  rv = GetIsServer(&isNewsServer);
  NS_ENSURE_SUCCESS(rv,rv);
 
  // persist the open / closed state, if not a server
  // this doesn't matter right now, but it will if we ever add categories
  if (!isNewsServer) 
  {
    *aPersistElided = PR_TRUE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = prefBranch->GetBoolPref("news.persist_server_open_state_in_folderpane", aPersistElided);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::Shutdown(PRBool shutdownChildren)
{
  if (mFilterList) 
  {
    // close the filter log stream
    nsresult rv = mFilterList->SetLogStream(nsnull);
    NS_ENSURE_SUCCESS(rv,rv);
    mFilterList = nsnull;
  }

  mInitialized = PR_FALSE;
  if (mReadSet) {
    // the nsINewsDatabase holds a weak ref to the readset,
    // and we outlive the db, so it's safe to delete it here.
    nsCOMPtr<nsINewsDatabase> db = do_QueryInterface(mDatabase);
    if (db)
      db->SetReadSet(nsnull);
    delete mReadSet;
    mReadSet = nsnull;
  }

  return nsMsgDBFolder::Shutdown(shutdownChildren);
}

NS_IMETHODIMP
nsMsgNewsFolder::SetFilterList(nsIMsgFilterList *aFilterList)
{
  if (mIsServer)
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    nsresult rv = GetServer(getter_AddRefs(server));
    NS_ENSURE_SUCCESS(rv,rv);
    return server->SetFilterList(aFilterList);
  }

  mFilterList = aFilterList;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetFilterList(nsIMsgWindow *aMsgWindow, nsIMsgFilterList **aResult)
{
  if (mIsServer)
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    nsresult rv = GetServer(getter_AddRefs(server));
    NS_ENSURE_SUCCESS(rv,rv);
    return server->GetFilterList(aMsgWindow, aResult);
  }

  if (!mFilterList) 
  {
    nsCOMPtr<nsIFileSpec> thisFolder;
    nsresult rv = GetPath(getter_AddRefs(thisFolder));
    NS_ENSURE_SUCCESS(rv, rv);
    
    mFilterFile = do_CreateInstance(NS_FILESPEC_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // in 4.x, the news filter file was
    // C:\Program Files\Netscape\Users\meer\News\host-news.mcom.com\mcom.test.dat
    // where the summary file was 
    // C:\Program Files\Netscape\Users\meer\News\host-news.mcom.com\mcom.test.snm
    // we make the rules file ".dat" in mozilla, so that migration works.
    rv = mFilterFile->FromFileSpec(thisFolder);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // NOTE:
    // we don't we need to call NS_MsgHashIfNecessary()
    // it's already been hashed, if necessary
    nsXPIDLCString filterFileName;
    rv = mFilterFile->GetLeafName(getter_Copies(filterFileName));
    NS_ENSURE_SUCCESS(rv,rv);
    
    filterFileName.Append(".dat");
    
    rv = mFilterFile->SetLeafName(filterFileName.get());
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIMsgFilterService> filterService =
      do_GetService(NS_MSGFILTERSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = filterService->OpenFilterList(mFilterFile, this, aMsgWindow, getter_AddRefs(mFilterList));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  NS_IF_ADDREF(*aResult = mFilterList);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::OnStopRunningUrl(nsIURI *aUrl, nsresult aExitCode)
{
 if (m_tempMessageStream)
  {
    m_tempMessageStream->Close();
    m_tempMessageStream = nsnull;
  }
  m_downloadingMultipleMessages = PR_FALSE;
  return nsMsgDBFolder::OnStopRunningUrl(aUrl, aExitCode);
}


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
 *   jefft@netscape.com
 *   putterman@netscape.com
 *   bienvenu@nventure.com
 *   warren@netscape.com
 *   alecf@netscape.com
 *   sspitzer@netscape.com
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Howard Chu <hyc@highlandsun.com>
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

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "prlog.h"

#include "msgCore.h"    // precompiled header...

#include "nsLocalMailFolder.h"	 
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgMessageFlags.h"
#include "prprf.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsIMailboxService.h"
#include "nsParseMailbox.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgWindow.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgUtils.h"
#include "nsLocalUtils.h"
#include "nsIPop3IncomingServer.h"
#include "nsILocalMailIncomingServer.h"
#include "nsIMsgIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsString.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsMsgUtils.h"
#include "nsICopyMsgStreamListener.h"
#include "nsIMsgCopyService.h"
#include "nsLocalUndoTxn.h"
#include "nsMsgTxn.h"
#include "nsIFileSpec.h"
#include "nsIMessenger.h"
#include "nsMsgBaseCID.h"
#include "nsMsgI18N.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPop3URL.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgFolderCompactor.h"
#include "nsNetCID.h"
#include "nsEscape.h"
#include "nsLocalStringBundle.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsISpamSettings.h"
#include "nsINoIncomingServer.h"
#include "nsNativeCharsetUtils.h"
#include "nsMailHeaders.h"
#include "nsCOMArray.h"
#include "nsILineInputStream.h"
#include "nsIFileStreams.h"
#include "nsAutoPtr.h"

static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

//////////////////////////////////////////////////////////////////////////////
// nsLocal
/////////////////////////////////////////////////////////////////////////////

nsLocalMailCopyState::nsLocalMailCopyState() :
  m_fileStream(nsnull), m_curDstKey(0xffffffff), m_curCopyIndex(0),
  m_totalMsgCount(0), m_dataBufferSize(0), m_leftOver(0),
  m_isMove(PR_FALSE), m_dummyEnvelopeNeeded(PR_FALSE), m_fromLineSeen(PR_FALSE), m_writeFailed(PR_FALSE),
  m_notifyFolderLoaded(PR_FALSE)
{
}

nsLocalMailCopyState::~nsLocalMailCopyState()
{
  PR_Free(m_dataBuffer);
  if (m_fileStream)
  {
    if (m_fileStream->is_open())
    m_fileStream->close();
    delete m_fileStream;
  }
  if (m_messageService)
  {
    nsCOMPtr <nsIMsgFolder> srcFolder = do_QueryInterface(m_srcSupport);
    if (srcFolder && m_message)
    {
      nsXPIDLCString uri;
      srcFolder->GetUriForMsg(m_message, getter_Copies(uri));
    }
  }
}
  
nsLocalFolderScanState::nsLocalFolderScanState() :
  m_fileSpec(nsnull), m_uidl(nsnull)
{
}

nsLocalFolderScanState::~nsLocalFolderScanState()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsMsgLocalMailFolder interface
///////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(void)
  : mHaveReadNameFromDB(PR_FALSE),
    mInitialized(PR_FALSE), mCopyState(nsnull), mType(nsnull),
    mCheckForNewMessagesAfterParsing(PR_FALSE), mNumFilterClassifyRequests(0), 
    m_parsingFolder(PR_FALSE), mDownloadState(DOWNLOAD_STATE_NONE)
{
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsMsgLocalMailFolder,
                             nsMsgDBFolder,
                             nsICopyMessageListener,
                             nsIMsgLocalMailFolder,
                             nsIJunkMailClassificationListener)

////////////////////////////////////////////////////////////////////////////////

static PRBool
nsStringEndsWith(nsString& name, const char *ending)
{
  PRInt32 len = name.Length();
  if (len == 0) return PR_FALSE;

  PRInt32 endingLen = strlen(ending);
  return (len > endingLen && name.RFind(ending, PR_TRUE) == len - endingLen);
}
  
static PRBool
nsShouldIgnoreFile(nsString& name)
{
  PRUnichar firstChar=name.CharAt(0);
  if (firstChar == '.' || firstChar == '#' || name.CharAt(name.Length() - 1) == '~')
    return PR_TRUE;

  if (name.LowerCaseEqualsLiteral("msgfilterrules.dat") ||
      name.LowerCaseEqualsLiteral("rules.dat") || 
      name.LowerCaseEqualsLiteral("filterlog.html") || 
      name.LowerCaseEqualsLiteral("junklog.html") || 
      name.LowerCaseEqualsLiteral("rulesbackup.dat"))
    return PR_TRUE;


  // don't add summary files to the list of folders;
  // don't add popstate files to the list either, or rules (sort.dat). 
  if (nsStringEndsWith(name, ".snm") ||
      name.LowerCaseEqualsLiteral("popstate.dat") ||
      name.LowerCaseEqualsLiteral("sort.dat") ||
      name.LowerCaseEqualsLiteral("mailfilt.log") ||
      name.LowerCaseEqualsLiteral("filters.js") ||
      nsStringEndsWith(name, ".toc"))
    return PR_TRUE;

  // ignore RSS data source files
  if (name.LowerCaseEqualsLiteral("feeds.rdf") ||
      name.LowerCaseEqualsLiteral("feeditems.rdf"))
    return PR_TRUE;

  return (nsStringEndsWith(name,".sbd") || nsStringEndsWith(name,".msf"));
}

NS_IMETHODIMP
nsMsgLocalMailFolder::Init(const char* aURI)
{
  nsresult rv;
  rv = nsMsgDBFolder::Init(aURI);
  if (NS_FAILED(rv)) return rv;

  // XXX - DEADCODE - we don't need to override this, do we?
  return rv;

}

nsresult
nsMsgLocalMailFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;
  nsAutoString currentFolderNameStr;
  nsCOMPtr<nsIMsgFolder> child;

  for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++) 
  {
    nsFileSpec currentFolderPath = dir.Spec();

    char *leafName = currentFolderPath.GetLeafName();
    NS_CopyNativeToUnicode(nsDependentCString(leafName), currentFolderNameStr);
    PR_Free(leafName);

    // here we should handle the case where the current file is a .sbd directory w/o
    // a matching folder file, or a directory w/o the name .sbd
    if (nsShouldIgnoreFile(currentFolderNameStr))
      continue;

    rv = AddSubfolder(currentFolderNameStr, getter_AddRefs(child));  
    if (child)
    { 
      nsXPIDLString folderName;
      child->GetName(getter_Copies(folderName));  // try to get it from cache/db
      if (folderName.IsEmpty())
        child->SetPrettyName(currentFolderNameStr.get());
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::AddSubfolder(const nsAString &name,
                                                 nsIMsgFolder **child)
{
  nsresult rv = nsMsgDBFolder::AddSubfolder(name, child);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIFileSpec> path;
  // need to make sure folder exists...
  (*child)->GetPath(getter_AddRefs(path));
  if (path)
  {
    PRBool exists;
    rv = path->Exists(&exists);
    if (!exists) 
        rv = path->Touch();
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetManyHeadersToDownload(PRBool *retval)
{
  PRBool isLocked;
  // if the folder is locked, we're probably reparsing - let's build the
  // view when we've finished reparsing.
  GetLocked(&isLocked);
  if (isLocked)
  {
    *retval = PR_TRUE;
    return NS_OK;
  }
  else
  {
    return nsMsgDBFolder::GetManyHeadersToDownload(retval);
  }

}
//run the url to parse the mailbox
NS_IMETHODIMP nsMsgLocalMailFolder::ParseFolder(nsIMsgWindow *aMsgWindow, nsIUrlListener *listener)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  nsFileSpec path;
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;
  
  
  nsCOMPtr<nsIMailboxService> mailboxService = 
    do_GetService(kMailboxServiceCID, &rv);
  
  if (NS_FAILED(rv)) return rv; 
  nsMsgMailboxParser *parser = new nsMsgMailboxParser(this);
  if(!parser)
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRBool isLocked;
  nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMsgParseMailMsgState*, parser));
  GetLocked(&isLocked);
  if(!isLocked)
    AcquireSemaphore(supports);
  else
  {
    NS_ASSERTION(PR_FALSE, "Could not get folder lock");
    return NS_MSG_FOLDER_BUSY;
  }
  
  rv = mailboxService->ParseMailbox(aMsgWindow, path, parser, listener, nsnull);
  if (NS_SUCCEEDED(rv))
    m_parsingFolder = PR_TRUE;
  return rv;
}

//we treat failure as null db returned
NS_IMETHODIMP nsMsgLocalMailFolder::GetDatabaseWOReparse(nsIMsgDatabase **aDatabase)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG(aDatabase);
  if (!mDatabase)
  {
    nsCOMPtr <nsIFileSpec> destIFolderSpec;
    rv = GetPath(getter_AddRefs(destIFolderSpec));
    
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDBFactory));
    if (NS_SUCCEEDED(rv) && mailDBFactory)
    {
      rv = mailDBFactory->OpenFolderDB(this, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) getter_AddRefs(mDatabase));
      if (mDatabase && NS_SUCCEEDED(rv))
        mDatabase->AddListener(this);
    }
  }
  *aDatabase = mDatabase;
  NS_IF_ADDREF(*aDatabase);
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::Enumerate(nsIEnumerator* *result)
{
#if 0
  nsresult rv; 

  // local mail folders contain both messages and folders:
  nsCOMPtr<nsIEnumerator> folders;
  nsCOMPtr<nsIEnumerator> messages;
  rv = GetSubFolders(getter_AddRefs(folders));
  if (NS_FAILED(rv)) return rv;
  rv = GetMessages(nsnull, getter_AddRefs(messages));
  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
#endif
  NS_ASSERTION(PR_FALSE, "isn't this obsolete?");
  return NS_ERROR_FAILURE;
}

nsresult
nsMsgLocalMailFolder::AddDirectorySeparator(nsFileSpec &path)
{
    nsAutoString sep;
    nsresult rv = nsGetMailFolderSeparator(sep);
    if (NS_FAILED(rv)) return rv;
    
    // see if there's a dir with the same name ending with .sbd
    // unfortunately we can't just say:
    //          path += sep;
    // here because of the way nsFileSpec concatenates
 
    nsCAutoString str(path.GetNativePathCString());
    str.AppendWithConversion(sep);
    path = str.get();

    return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetSubFolders(nsIEnumerator* *result)
{
  PRBool isServer;
  nsresult rv = GetIsServer(&isServer);

  if (!mInitialized) {
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;
    
    nsFileSpec path;
    rv = pathSpec->GetFileSpec(&path);
    if (NS_FAILED(rv)) return rv;
    
    if (!path.Exists())
      path.CreateDirectory();
    if (!path.IsDirectory())
      AddDirectorySeparator(path);
    
    mInitialized = PR_TRUE;      // need to set this flag here to avoid infinite recursion
    // we have to treat the root folder specially, because it's name
    // doesn't end with .sbd
    PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
    if (path.IsDirectory()) 
    {
      newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
      SetFlag(newFlags);

      PRBool createdDefaultMailboxes = PR_FALSE;
      nsCOMPtr<nsILocalMailIncomingServer> localMailServer;

      if (isServer) 
      {
          nsCOMPtr<nsIMsgIncomingServer> server;
          rv = GetServer(getter_AddRefs(server));
          if (NS_FAILED(rv)) return rv;
          if (!server) return NS_MSG_INVALID_OR_MISSING_SERVER;

          localMailServer = do_QueryInterface(server, &rv);
          if (NS_FAILED(rv)) return rv;
          if (!localMailServer) return NS_MSG_INVALID_OR_MISSING_SERVER;
          
          nsCOMPtr<nsIFileSpec> spec;
          rv = NS_NewFileSpecWithSpec(path, getter_AddRefs(spec));
          if (NS_FAILED(rv)) return rv;
          
          // first create the folders on disk (as empty files)
          rv = localMailServer->CreateDefaultMailboxes(spec);
          NS_ENSURE_SUCCESS(rv, rv);
          if (NS_FAILED(rv)) return rv;
          createdDefaultMailboxes = PR_TRUE;
      }

      // now, discover those folders
      rv = CreateSubFolders(path);
      if (NS_FAILED(rv)) return rv;

      // must happen after CreateSubFolders, or the folders won't exist.
      // don't call this more than necessary, it's expensive
      SetPrefFlag();

      // must happen after CreateSubFolders, or the folders won't exist.
      if (createdDefaultMailboxes && isServer) 
      {
        rv = localMailServer->SetFlagsOnDefaultMailboxes();
        if (NS_FAILED(rv)) return rv;
      }
      /*we need to create all the folders at start-up because if a folder having subfolders is
      closed then the datasource will not ask for subfolders. For IMAP logging onto the 
      server will create imap folders and for news we don't have any 2nd level newsgroup */

      PRUint32 cnt;
      rv = mSubFolders->Count(&cnt);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIEnumerator> enumerator;
        for (PRUint32 i=0; i< cnt;i++)
        {
          nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(mSubFolders, i, &rv);
          if (folder && NS_SUCCEEDED(rv))
          {
            rv = folder->GetSubFolders(getter_AddRefs(enumerator));
            NS_ASSERTION(NS_SUCCEEDED(rv),"GetSubFolders failed");
          }
        }
      }
    }
    UpdateSummaryTotals(PR_FALSE);
  }
  rv = mSubFolders->Enumerate(result);
  return rv;
}


//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsMsgLocalMailFolder::GetDatabase(nsIMsgWindow *aMsgWindow)
{
  nsresult rv = NS_OK;
  if (!mDatabase)
  {
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;
    PRBool exists;
    rv = pathSpec->Exists(&exists);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!exists) return NS_ERROR_NULL_POINTER;  //mDatabase will be null at this point.
    
    nsresult folderOpen = NS_OK;
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    
    rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
    if (NS_SUCCEEDED(rv) && mailDBFactory)
    {
      folderOpen = mailDBFactory->OpenFolderDB(this, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase));
      if(NS_FAILED(folderOpen) &&
        folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
      {
        nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
        nsCOMPtr <nsIDBFolderInfo> transferInfo;
        if (mDatabase)
        {
          mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
          if (dbFolderInfo)
          {
            if (folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
              dbFolderInfo->SetFlags(mFlags);
            dbFolderInfo->GetTransferInfo(getter_AddRefs(transferInfo));
          }
          dbFolderInfo = nsnull;
        }
        if (mDatabase)
        {
          dbFolderInfo = nsnull;
          mDatabase->ForceClosed();
        }
        mDatabase = nsnull;
      
        nsFileSpec dbName;
        rv = pathSpec->GetFileSpec(&dbName);
        NS_ENSURE_SUCCESS(rv, rv);
        nsLocalFolderSummarySpec  summarySpec(dbName);
        // Remove summary file.
        summarySpec.Delete(PR_FALSE);
      
        // if it's out of date then reopen with upgrade.
        if (NS_FAILED(rv = mailDBFactory->OpenFolderDB(this, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase)))
          && rv != NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
          return rv;
        else if (transferInfo && mDatabase)
           SetDBTransferInfo(transferInfo);
      }
      
    }
    
    if(mDatabase)
    {
      
      if(mAddListener)
        mDatabase->AddListener(this);
      
      // if we have to regenerate the folder, run the parser url.
      if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING ||
        folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
      {
        if(NS_FAILED(rv = ParseFolder(aMsgWindow, this)))
        {
          if (rv == NS_MSG_FOLDER_BUSY)
          {
            mDatabase->RemoveListener(this);  //we need to null out the db so that parsing gets kicked off again.
            mDatabase = nsnull;
            ThrowAlertMsg("parsingFolderFailed", aMsgWindow);
          }
          return rv;
        }
        else
          return NS_ERROR_NOT_INITIALIZED;
      }
      else
      {
        // We have a valid database so lets extract necessary info.
        UpdateSummaryTotals(PR_TRUE);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::UpdateFolder(nsIMsgWindow *aWindow)
{
  (void) RefreshSizeOnDisk();
  nsresult rv;

  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  PRBool userNeedsToAuthenticate = PR_FALSE;
  // if we're PasswordProtectLocalCache, then we need to find out if the server is authenticated.
  (void) accountManager->GetUserNeedsToAuthenticate(&userNeedsToAuthenticate);
  if (userNeedsToAuthenticate)
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server)); 
    if (NS_FAILED(rv)) return rv;
    if (!server) 
      return NS_MSG_INVALID_OR_MISSING_SERVER;
    // need to check if this is a pop3 or no mail server to determine which password
    // we should challenge the user with.
    nsCOMPtr<nsIMsgIncomingServer> serverToAuthenticateAgainst;
    nsCOMPtr<nsINoIncomingServer> noIncomingServer = do_QueryInterface(server, &rv);
    if (noIncomingServer)
    {
      nsCOMPtr<nsIMsgAccount> defaultAccount;
      accountManager->GetDefaultAccount(getter_AddRefs(defaultAccount));
      if (defaultAccount)
        defaultAccount->GetIncomingServer(getter_AddRefs(serverToAuthenticateAgainst));
    }
    else
    {
      GetServer(getter_AddRefs(serverToAuthenticateAgainst));
    }
    if (serverToAuthenticateAgainst)
    {
      PRBool passwordMatches = PR_FALSE;
      rv = PromptForCachePassword(serverToAuthenticateAgainst, aWindow, passwordMatches);
      if (!passwordMatches)
        return NS_ERROR_FAILURE;
    }
  }
  //If we don't currently have a database, get it.  Otherwise, the folder has been updated (presumably this
  //changes when we download headers when opening inbox).  If it's updated, send NotifyFolderLoaded.
  if(!mDatabase)
    rv = GetDatabase(aWindow); // this will cause a reparse, if needed.
  else
  {
    PRBool valid;
    rv = mDatabase->GetSummaryValid(&valid);
    // don't notify folder loaded or try compaction if db isn't valid
    // (we're probably reparsing or copying msgs to it)
    if (NS_SUCCEEDED(rv) && valid)
    {
      NotifyFolderEvent(mFolderLoadedAtom);
      rv = AutoCompact(aWindow);
      NS_ENSURE_SUCCESS(rv,rv);
    }
    else if (mCopyState)
      mCopyState->m_notifyFolderLoaded = PR_TRUE; //defer folder loaded notification
    else if (!m_parsingFolder)// if the db was already open, it's probably OK to load it if not parsing
      NotifyFolderEvent(mFolderLoadedAtom);
  }
  PRBool filtersRun;
  // if we have new messages, try the filter plugins.
  if (NS_SUCCEEDED(rv) && (mFlags & MSG_FOLDER_FLAG_GOT_NEW))
    (void) CallFilterPlugins(aWindow, &filtersRun);
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  nsresult rv = GetDatabase(aMsgWindow);

  if(NS_SUCCEEDED(rv))
    return mDatabase->EnumerateMessages(result);
  return rv;
}


NS_IMETHODIMP nsMsgLocalMailFolder::GetFolderURL(char **url)
{
  const char *urlScheme = "mailbox:";
  
  if(!url)
    return NS_ERROR_NULL_POINTER;
  
  nsresult rv;
  
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  nsFileSpec path;
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;
  
  nsCAutoString tmpPath((nsFilePath)path);
  
  nsCAutoString urlStr(urlScheme);
  urlStr.Append(tmpPath);
  
  *url = ToNewCString(urlStr);
  return NS_OK;
  
}

/* Finds the directory associated with this folder.  That is if the path is
   c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
   currently exist then it will create it
  */
nsresult nsMsgLocalMailFolder::CreateDirectoryForFolder(nsFileSpec &path)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;
  
  if(!path.IsDirectory())
  {
    //If the current path isn't a directory, add directory separator
    //and test it out.
    rv = AddDirectorySeparator(path);
    if(NS_FAILED(rv))
      return rv;
    
    //If that doesn't exist, then we have to create this directory
    if(!path.IsDirectory())
    {
      //If for some reason there's a file with the directory separator
      //then we are going to fail.
      if(path.Exists())
      {
        return NS_MSG_COULD_NOT_CREATE_DIRECTORY;
      }
      //otherwise we need to create a new directory.
      else
      {
        nsFileSpec tempPath(path.GetNativePathCString(), PR_TRUE); // create intermediate directories
        path.CreateDirectory();
        //Above doesn't return an error value so let's see if
        //it was created.
        if(!path.IsDirectory())
          return NS_MSG_COULD_NOT_CREATE_DIRECTORY;
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::CreateStorageIfMissing(nsIUrlListener* aUrlListener)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIMsgFolder> msgParent;
  GetParentMsgFolder(getter_AddRefs(msgParent));
  
  // parent is probably not set because *this* was probably created by rdf
  // and not by folder discovery. So, we have to compute the parent.
  if (!msgParent)
  {
    nsCAutoString folderName(mURI);
    
    nsCAutoString uri;
    
    PRInt32 leafPos = folderName.RFindChar('/');
    
    nsCAutoString parentName(folderName);
    
    if (leafPos > 0)
    {
      // If there is a hierarchy, there is a parent.
      // Don't strip off slash if it's the first character
      parentName.Truncate(leafPos);
      // get the corresponding RDF resource
      // RDF will create the folder resource if it doesn't already exist
      nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      
      nsCOMPtr<nsIRDFResource> resource;
      rv = rdf->GetResource(parentName, getter_AddRefs(resource));
      NS_ENSURE_SUCCESS(rv,rv);
      
      msgParent = do_QueryInterface(resource, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
  
  if (msgParent)
  {
    nsXPIDLString folderName;
    GetName(getter_Copies(folderName));
    rv = msgParent->CreateSubfolder(folderName, nsnull);
  }
  return rv;
}

nsresult 
nsMsgLocalMailFolder::CheckIfFolderExists(const PRUnichar *newFolderName, nsIMsgFolder *parentFolder, nsIMsgWindow *msgWindow)
{
  NS_ENSURE_ARG_POINTER(newFolderName);
  NS_ENSURE_ARG_POINTER(parentFolder);
  nsCOMPtr<nsIEnumerator> subfolders;
  nsresult rv = parentFolder->GetSubFolders(getter_AddRefs(subfolders));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = subfolders->First();    //will fail if no subfolders 
  while (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsISupports> supports;
    subfolders->CurrentItem(getter_AddRefs(supports));
    nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(supports);
    nsAutoString folderNameString;
    PRUnichar *folderName;
    if (msgFolder)
      msgFolder->GetName(&folderName);
    folderNameString.Adopt(folderName);
    if (folderNameString.Equals(newFolderName, nsCaseInsensitiveStringComparator()))
    {
      if (msgWindow)
        ThrowAlertMsg("folderExists", msgWindow);
      return NS_MSG_FOLDER_EXISTS;
    }
    rv = subfolders->Next();
  }
  return NS_OK;
}

nsresult
nsMsgLocalMailFolder::CreateSubfolder(const PRUnichar *folderName, nsIMsgWindow *msgWindow )
{
  nsresult rv = CheckIfFolderExists(folderName, this, msgWindow);
  if(NS_FAILED(rv))  //we already throw an alert - no need for an assertion
    return rv;
  
  nsFileSpec path;
  nsCOMPtr<nsIMsgFolder> child;
  //Get a directory based on our current path.
  rv = CreateDirectoryForFolder(path);
  if(NS_FAILED(rv))
    return rv;
  
  //Now we have a valid directory or we have returned.
  //Make sure the new folder name is valid
  nsCAutoString nativeFolderName;
  rv = nsMsgI18NCopyUTF16ToNative(folderName, nativeFolderName);
  if (NS_FAILED(rv) || nativeFolderName.IsEmpty()) {
    ThrowAlertMsg("folderCreationFailed", msgWindow);
    // I'm returning this value so the dialog stays up
    return NS_MSG_FOLDER_EXISTS;
  }
  
  nsCAutoString safeFolderName;
  safeFolderName.Assign(nativeFolderName.get());
  NS_MsgHashIfNecessary(safeFolderName);
  
  path += safeFolderName.get();		
  if (path.Exists()) //check this because localized names are different from disk names
  {
    ThrowAlertMsg("folderExists", msgWindow);
    return NS_MSG_FOLDER_EXISTS;
  }
  
  nsOutputFileStream outputStream(path, PR_WRONLY | PR_CREATE_FILE, 00600);	
  if (outputStream.is_open())
  {
    outputStream.flush();
    outputStream.close();
  }
  
  //GetFlags and SetFlags in AddSubfolder will fail because we have no db at this point but mFlags is set.
  rv = AddSubfolder(nsDependentString(folderName), getter_AddRefs(child));
  if (!child || NS_FAILED(rv))
  {
    path.Delete(PR_FALSE);
    return rv;
  }
		
  // Create an empty database for this mail folder, set its name from the user  
  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
  
  rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
  if (NS_SUCCEEDED(rv) && mailDBFactory)
  {
    nsCOMPtr<nsIMsgDatabase> unusedDB;
    rv = mailDBFactory->OpenFolderDB(child, PR_TRUE, PR_TRUE, getter_AddRefs(unusedDB));
    
    if ((NS_SUCCEEDED(rv) || rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING 
      || rv == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE) && unusedDB)
    {
      //need to set the folder name
      nsCOMPtr<nsIDBFolderInfo> folderInfo;
      rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
      if(NS_SUCCEEDED(rv))
      {
        folderInfo->SetMailboxName(nsDependentString(folderName));
      }
      unusedDB->SetSummaryValid(PR_TRUE);
      unusedDB->Close(PR_TRUE);
    }
    else
    {
      path.Delete(PR_FALSE);
      rv = NS_MSG_CANT_CREATE_FOLDER;
    }
  }
  if(NS_SUCCEEDED(rv))
  {
    //we need to notify explicitly the flag change because it failed when we did AddSubfolder
    child->OnFlagChange(mFlags);
    child->SetPrettyName(folderName);  //because empty trash will create a new trash folder
    NotifyItemAdded(child);
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::CompactAll(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupportsArray> folderArray;
  if (!aFolderArray)
  {
    nsCOMPtr<nsIMsgFolder> rootFolder;
    nsCOMPtr<nsISupportsArray> allDescendents;
    rv = GetRootFolder(getter_AddRefs(rootFolder));  
    if (NS_SUCCEEDED(rv) && rootFolder)
    {
      NS_NewISupportsArray(getter_AddRefs(allDescendents));
      rootFolder->ListDescendents(allDescendents);
      PRUint32 cnt =0;
      rv = allDescendents->Count(&cnt);
      NS_ENSURE_SUCCESS(rv,rv);
      NS_NewISupportsArray(getter_AddRefs(folderArray));
      PRUint32 expungedBytes=0;
      for (PRUint32 i=0; i< cnt;i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(allDescendents->ElementAt(i));
        nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
    
        expungedBytes=0;
        if (folder)
          rv = folder->GetExpungedBytes(&expungedBytes);
    
        NS_ENSURE_SUCCESS(rv,rv);
    
        if (expungedBytes > 0)
          rv = folderArray->AppendElement(supports);
      }
      rv = folderArray->Count(&cnt);
      NS_ENSURE_SUCCESS(rv,rv);
      if (cnt == 0 )
        return NotifyCompactCompleted();
    }
  }
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor =  do_CreateInstance(NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && folderCompactor)
    if (aFolderArray)
       rv = folderCompactor->CompactAll(aFolderArray, aMsgWindow, aCompactOfflineAlso, aOfflineFolderArray);  
    else if (folderArray)
       rv = folderCompactor->CompactAll(folderArray, aMsgWindow, aCompactOfflineAlso, aOfflineFolderArray);  
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor =  do_CreateInstance(NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && folderCompactor)
  {
    PRUint32 expungedBytes = 0;

    GetExpungedBytes(&expungedBytes);
    // check if we need to compact the folder

    if (expungedBytes > 0)
      rv = folderCompactor->Compact(this, PR_FALSE, aMsgWindow);
    else
      rv = NotifyCompactCompleted();
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EmptyTrash(nsIMsgWindow *msgWindow,
                                               nsIUrlListener *aListener)
{
    nsresult rv;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    rv = GetTrashFolder(getter_AddRefs(trashFolder));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 flags;
        nsXPIDLCString trashUri;
        trashFolder->GetURI(getter_Copies(trashUri));
        trashFolder->GetFlags(&flags);
        PRInt32 totalMessages = 0;
        rv = trashFolder->GetTotalMessages(PR_TRUE, &totalMessages);
                  
        if (totalMessages <= 0) 
        {
          nsCOMPtr<nsIEnumerator> aEnumerator;
          rv =trashFolder->GetSubFolders(getter_AddRefs(aEnumerator));
          NS_ENSURE_SUCCESS(rv,rv);
          rv = aEnumerator->First();    //will fail if no subfolders 
          if (NS_FAILED(rv)) return NS_OK;
        }
        nsCOMPtr<nsIMsgFolder> parentFolder;
        rv = trashFolder->GetParentMsgFolder(getter_AddRefs(parentFolder));
        if (NS_SUCCEEDED(rv) && parentFolder)
        {
          nsCOMPtr <nsIDBFolderInfo> transferInfo;
          trashFolder->GetDBTransferInfo(getter_AddRefs(transferInfo)); 

          trashFolder->SetParent(nsnull);
          parentFolder->PropagateDelete(trashFolder, PR_TRUE, msgWindow);
          parentFolder->CreateSubfolder(NS_LITERAL_STRING("Trash").get(),nsnull);
          nsCOMPtr<nsIMsgFolder> newTrashFolder;
          rv = GetTrashFolder(getter_AddRefs(newTrashFolder));
          if (NS_SUCCEEDED(rv) && newTrashFolder) 
          {
            nsCOMPtr <nsIMsgLocalMailFolder> localTrash = do_QueryInterface(newTrashFolder);
            if (localTrash)
              localTrash->RefreshSizeOnDisk();
            newTrashFolder->SetDBTransferInfo(transferInfo);
            // update the summary totals so the front end will
            // show the right thing for the new trash folder
            // see bug #161999
            newTrashFolder->UpdateSummaryTotals(PR_TRUE);
          }
        }
    }
    return rv;
}

nsresult nsMsgLocalMailFolder::IsChildOfTrash(PRBool *result)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  PRBool isServer = PR_FALSE;
  PRUint32 parentFlags = 0;

  if (!result) return rv;
  *result = PR_FALSE;

  rv = GetIsServer(&isServer);
  if (NS_FAILED(rv) || isServer) return rv;

  rv= GetFlags(&parentFlags);  //this is the parent folder
  if (parentFlags & MSG_FOLDER_FLAG_TRASH) 
  {
      *result = PR_TRUE;
      return rv;
  }

  nsCOMPtr<nsIMsgFolder> parentFolder;
  nsCOMPtr<nsIMsgFolder> thisFolder;
  rv = QueryInterface(NS_GET_IID(nsIMsgFolder), (void **)
                      getter_AddRefs(thisFolder));

  while (!isServer && thisFolder) 
  {
    rv = thisFolder->GetParentMsgFolder(getter_AddRefs(parentFolder));
    if (NS_FAILED(rv)) return rv;
    rv = parentFolder->GetIsServer(&isServer);
    if (NS_FAILED(rv) || isServer) return rv;
    rv = parentFolder->GetFlags(&parentFlags);
    if (NS_FAILED(rv)) return rv;
    if (parentFlags & MSG_FOLDER_FLAG_TRASH) 
    {
      *result = PR_TRUE;
      return rv;
    }
    thisFolder = parentFolder;
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Delete()
{
  nsresult rv;
  
  if(mDatabase)
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
  
  nsLocalFolderSummarySpec summarySpec(path);

  //Clean up .sbd folder if it exists.
  if(NS_SUCCEEDED(rv))
  {
    // Remove summary file.
    summarySpec.Delete(PR_FALSE);
      
    //Delete mailbox
    path.Delete(PR_FALSE);
      
    if (!path.IsDirectory())
      AddDirectorySeparator(path);
      
    //If this is a directory, then remove it.
    if (path.IsDirectory())
      path.Delete(PR_TRUE);
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::DeleteSubFolders(
  nsISupportsArray *folders, nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_ERROR_FAILURE;
  PRBool isChildOfTrash;
  rv = IsChildOfTrash(&isChildOfTrash);

  if (isChildOfTrash)
    return nsMsgDBFolder::DeleteSubFolders(folders, msgWindow);

  if (!msgWindow) 
    return NS_ERROR_NULL_POINTER;
 
  nsCOMPtr<nsIMsgFolder> trashFolder;
  rv = GetTrashFolder(getter_AddRefs(trashFolder));
  if (NS_SUCCEEDED(rv))
  {
    // we don't allow multiple folder selection so this is ok.
    nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(folders, 0);
    if (folder)
      trashFolder->CopyFolder(folder, PR_TRUE, msgWindow, nsnull);
  }
  return rv;
}

nsresult nsMsgLocalMailFolder::ConfirmFolderDeletion(nsIMsgWindow *aMsgWindow, PRBool *aResult)
{
  NS_ENSURE_ARG(aResult);
  NS_ENSURE_ARG(aMsgWindow);
  nsCOMPtr<nsIDocShell> docShell;
  aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
  if (docShell)
  {
    PRBool confirmDeletion = PR_TRUE;
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (pPrefBranch)
       pPrefBranch->GetBoolPref("mailnews.confirm.moveFoldersToTrash", &confirmDeletion);
    if (confirmDeletion)
    {
      if (!mMsgStringService)
        mMsgStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_CONTRACTID);
      if (!mMsgStringService) 
        return NS_ERROR_FAILURE;
      nsXPIDLString alertString;
      mMsgStringService->GetStringByID(POP3_MOVE_FOLDER_TO_TRASH, getter_Copies(alertString));
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog)
        dialog->Confirm(nsnull, alertString.get(), aResult);
    }
    else
      *aResult = PR_TRUE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Rename(const PRUnichar *aNewName, nsIMsgWindow *msgWindow)
{
  nsCOMPtr<nsIFileSpec> oldPathSpec;
  nsCOMPtr<nsIAtom> folderRenameAtom;
  nsresult rv = GetPath(getter_AddRefs(oldPathSpec));
  if (NS_FAILED(rv)) 
    return rv;
  nsCOMPtr<nsIMsgFolder> parentFolder;
  rv = GetParentMsgFolder(getter_AddRefs(parentFolder));
  if (NS_FAILED(rv)) 
    return rv;
  nsCOMPtr<nsISupports> parentSupport = do_QueryInterface(parentFolder);
  
  nsFileSpec fileSpec;
  oldPathSpec->GetFileSpec(&fileSpec);
  nsLocalFolderSummarySpec oldSummarySpec(fileSpec);
  nsFileSpec dirSpec;
  
  PRUint32 cnt = 0;
  if (mSubFolders)
    mSubFolders->Count(&cnt);
  
  if (cnt > 0)
    rv = CreateDirectoryForFolder(dirSpec);
  
  // convert from PRUnichar* to char* due to not having Rename(PRUnichar*)
  // function in nsIFileSpec
  
  nsCAutoString convertedNewName;
  if (NS_FAILED(nsMsgI18NCopyUTF16ToNative(aNewName, convertedNewName)))
    return NS_ERROR_FAILURE;
  
  nsCAutoString newDiskName;
  newDiskName.Assign(convertedNewName.get());
  NS_MsgHashIfNecessary(newDiskName);
  
  nsXPIDLCString oldLeafName;
  oldPathSpec->GetLeafName(getter_Copies(oldLeafName));
  
  if (mName.Equals(aNewName, nsCaseInsensitiveStringComparator()))
  {
    if(msgWindow)
      rv = ThrowAlertMsg("folderExists", msgWindow);
    return NS_MSG_FOLDER_EXISTS;
  }
  else
  {
    nsCOMPtr <nsIFileSpec> parentPathSpec;
    parentFolder->GetPath(getter_AddRefs(parentPathSpec));
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsFileSpec parentPath;
    parentPathSpec->GetFileSpec(&parentPath);
    NS_ENSURE_SUCCESS(rv,rv);
    
    if (!parentPath.IsDirectory())
      AddDirectorySeparator(parentPath);
    
    rv = CheckIfFolderExists(aNewName, parentFolder, msgWindow);
    if (NS_FAILED(rv)) 
      return rv;
  }
  
  ForceDBClosed();
  
  nsCAutoString newNameDirStr(newDiskName.get());  //save of dir name before appending .msf 
  
  rv = oldPathSpec->Rename(newDiskName.get());
  if (NS_SUCCEEDED(rv))
  {
    newDiskName += ".msf";
    oldSummarySpec.Rename(newDiskName.get());
  }
  else
  {
    ThrowAlertMsg("folderRenameFailed", msgWindow);
    return rv;
  }
  
  if (NS_SUCCEEDED(rv) && cnt > 0) 
  {
    // rename "*.sbd" directory
    newNameDirStr += ".sbd";
    dirSpec.Rename(newNameDirStr.get());
  }
  
  nsCOMPtr<nsIMsgFolder> newFolder;
  if (parentSupport)
  {
    rv = parentFolder->AddSubfolder(nsDependentString(aNewName), getter_AddRefs(newFolder));
    if (newFolder) 
    {
      newFolder->SetPrettyName(aNewName);
      PRBool changed = PR_FALSE;
      MatchOrChangeFilterDestination(newFolder, PR_TRUE /*caseInsenstive*/, &changed);
      if (changed)
        AlertFilterChanged(msgWindow);
      
      if (cnt > 0)
        newFolder->RenameSubFolders(msgWindow, this);
      
      if (parentFolder)
      {
        SetParent(nsnull);
        parentFolder->PropagateDelete(this, PR_FALSE, msgWindow);
        parentFolder->NotifyItemAdded(newFolder);
      }
      folderRenameAtom = do_GetAtom("RenameCompleted");
      newFolder->NotifyFolderEvent(folderRenameAtom);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder)
{
  nsresult rv =NS_OK;
  mInitialized = PR_TRUE;

  PRUint32 flags;
  oldFolder->GetFlags(&flags);
  SetFlags(flags);

  nsCOMPtr<nsIEnumerator> aEnumerator;
  oldFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  nsCOMPtr<nsISupports> aSupport;
  rv = aEnumerator->First();
  while (NS_SUCCEEDED(rv))
  {
     rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
     nsCOMPtr<nsIMsgFolder>msgFolder = do_QueryInterface(aSupport);
     nsXPIDLString folderName;
     rv = msgFolder->GetName(getter_Copies(folderName));
     nsCOMPtr <nsIMsgFolder> newFolder;
     AddSubfolder(folderName, getter_AddRefs(newFolder));
     if (newFolder)
     {
       newFolder->SetPrettyName(folderName.get());
       PRBool changed = PR_FALSE;
       msgFolder->MatchOrChangeFilterDestination(newFolder, PR_TRUE /*caseInsenstive*/, &changed);
       if (changed)
         msgFolder->AlertFilterChanged(msgWindow);

       newFolder->RenameSubFolders(msgWindow, msgFolder);
     }
     rv = aEnumerator->Next();
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(PRUnichar ** prettyName)
{
  return nsMsgDBFolder::GetPrettyName(prettyName);
}

NS_IMETHODIMP nsMsgLocalMailFolder::SetPrettyName(const PRUnichar *aName)
{
  NS_ENSURE_ARG_POINTER(aName);
  nsresult rv = nsMsgDBFolder::SetPrettyName(aName);
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString folderName;
  rv = GetStringProperty("folderName", getter_Copies(folderName));
  NS_ConvertUCS2toUTF8 utf8FolderName(mName);
  if (!NS_SUCCEEDED(rv) || !folderName.Equals(utf8FolderName.get()))
    return SetStringProperty("folderName", utf8FolderName.get());
  else
    return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetName(PRUnichar **aName)
{
  ReadDBFolderInfo(PR_FALSE);
  return nsMsgDBFolder::GetName(aName);
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
  nsresult openErr=NS_ERROR_UNEXPECTED;
  if(!db || !folderInfo || !mPath)
    return NS_ERROR_NULL_POINTER;	//ducarroz: should we use NS_ERROR_INVALID_ARG?

  nsresult rv;
  if (mDatabase)
  {
    openErr = NS_OK;
  }
  else
  {
    nsCOMPtr<nsIMsgDatabase> mailDBFactory( do_CreateInstance(kCMailDB, &rv) );
    if (NS_SUCCEEDED(rv) && mailDBFactory)
    {
      openErr = mailDBFactory->OpenFolderDB(this, PR_FALSE, PR_FALSE, getter_AddRefs(mDatabase));
    }
  }

  *db = mDatabase;
  NS_IF_ADDREF(*db);
  if (NS_SUCCEEDED(openErr)&& *db)
    openErr = (*db)->GetDBFolderInfo(folderInfo);
  return openErr;
}

NS_IMETHODIMP nsMsgLocalMailFolder::ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element)
{
  NS_ENSURE_ARG_POINTER(element);
  nsresult rv = nsMsgDBFolder::ReadFromFolderCacheElem(element);
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString utf8Name;
  rv = element->GetStringProperty("folderName", getter_Copies(utf8Name));
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(utf8Name, mName);
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::WriteToFolderCacheElem(nsIMsgFolderCacheElement *element)
{
  NS_ENSURE_ARG_POINTER(element);
  nsMsgDBFolder::WriteToFolderCacheElem(element);
  return element->SetStringProperty("folderName", NS_ConvertUCS2toUTF8(mName).get());
}

NS_IMETHODIMP nsMsgLocalMailFolder::UpdateSummaryTotals(PRBool force)
{
  if (!mNotifyCountChanges)
    return NS_OK;
  PRInt32 oldUnreadMessages = mNumUnreadMessages;
  PRInt32 oldTotalMessages = mNumTotalMessages;
  //We need to read this info from the database
  ReadDBFolderInfo(force);
  
  //Need to notify listeners that total count changed.
  if(oldTotalMessages != mNumTotalMessages)
  {
    NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, mNumTotalMessages);
  }
  
  if(oldUnreadMessages != mNumUnreadMessages)
  {
    NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, mNumUnreadMessages);
  }
  
  FlushToFolderCache();
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetDeletable(PRBool *deletable)
{
  if(!deletable)
    return NS_ERROR_NULL_POINTER;

  PRBool isServer;
  GetIsServer(&isServer);
  // These are specified in the "Mail/News Windows" UI spe 
  if (isServer)
    *deletable = PR_FALSE;
  else if (mFlags & MSG_FOLDER_FLAG_INBOX || 
    mFlags & MSG_FOLDER_FLAG_DRAFTS || 
    mFlags & MSG_FOLDER_FLAG_TRASH ||
    mFlags & MSG_FOLDER_FLAG_TEMPLATES ||
    mFlags & MSG_FOLDER_FLAG_JUNK)
    *deletable = PR_FALSE;
  else *deletable =  PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
#ifdef HAVE_PORT
  if (m_expungedBytes > 0)
  {
    PRInt32 purgeThreshhold = m_master->GetPrefs()->GetPurgeThreshhold();
    PRBool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();
    return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
  }
  return PR_FALSE;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::RefreshSizeOnDisk()
{
  PRUint32 oldFolderSize = mFolderSize;
  mFolderSize = 0; // we set this to 0 to force it to get recalculated from disk
  if (NS_SUCCEEDED(GetSizeOnDisk(&mFolderSize)))
    NotifyIntPropertyChanged(kFolderSizeAtom, oldFolderSize, mFolderSize);
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetSizeOnDisk(PRUint32* aSize)
{
    NS_ENSURE_ARG_POINTER(aSize);
    nsresult rv = NS_OK;
    if (!mFolderSize)
    {
      nsCOMPtr <nsIFileSpec> fileSpec;
      rv = GetPath(getter_AddRefs(fileSpec));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = fileSpec->GetFileSize(&mFolderSize);
    }
    *aSize = mFolderSize;
    return rv;
}

nsresult
nsMsgLocalMailFolder::GetTrashFolder(nsIMsgFolder** result)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  
  if (!result) return rv;
  
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv))
  {
    PRUint32 numFolders;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH,
      1, &numFolders, result);
    if (NS_SUCCEEDED(rv) && numFolders != 1)
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::DeleteMessages(nsISupportsArray *messages,
                                     nsIMsgWindow *msgWindow, 
                                     PRBool deleteStorage, PRBool isMove,
                                     nsIMsgCopyServiceListener* listener, PRBool allowUndo)
{
  NS_ENSURE_ARG_POINTER(messages);

  PRUint32 messageCount;
  nsresult rv = messages->Count(&messageCount);
  if (!messageCount)
    return rv;

  PRBool isTrashFolder = mFlags & MSG_FOLDER_FLAG_TRASH;
  if (!deleteStorage && !isTrashFolder)
  {
      nsCOMPtr<nsIMsgFolder> trashFolder;
      rv = GetTrashFolder(getter_AddRefs(trashFolder));
      if (NS_SUCCEEDED(rv))
      {
          nsCOMPtr<nsIMsgCopyService> copyService = 
                   do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
          if (NS_SUCCEEDED(rv))
          {
            return copyService->CopyMessages(this, messages, trashFolder,
                                      PR_TRUE, listener, msgWindow, allowUndo);
          }
      }
      return rv;
  }
  else
  {  	
      rv = GetDatabase(msgWindow);
      if(NS_SUCCEEDED(rv))
      {
          nsCOMPtr<nsISupports> msgSupport;
          MarkMsgsOnPop3Server(messages, POP3_DELETE);

          rv = EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_TRUE /*dbBatching*/);
          if (NS_SUCCEEDED(rv))
          {
            for(PRUint32 i = 0; i < messageCount; i++)
            {
              msgSupport = getter_AddRefs(messages->ElementAt(i));
              if (msgSupport)
                DeleteMessage(msgSupport, msgWindow, PR_TRUE, PR_FALSE);
            }
          }
          else if (rv == NS_MSG_FOLDER_BUSY)
            ThrowAlertMsg("deletingMsgsFailed", msgWindow);

          // we are the source folder here for a move or shift delete
          //enable notifications because that will close the file stream
          // we've been caching, mark the db as valid, and commit it.
          EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE /*dbBatching*/);
          if(!isMove)
            NotifyFolderEvent(NS_SUCCEEDED(rv) ? mDeleteOrMoveMsgCompletedAtom : mDeleteOrMoveMsgFailedAtom);
      }
  }
  return rv;
}

nsresult
nsMsgLocalMailFolder::InitCopyState(nsISupports* aSupport, 
                                    nsISupportsArray* messages,
                                    PRBool isMove,
                                    nsIMsgCopyServiceListener* listener, 
                                    nsIMsgWindow *msgWindow, PRBool isFolder, 
                                    PRBool allowUndo)
{
  nsresult rv = NS_OK;
  nsFileSpec path;
  nsCOMPtr<nsIFileSpec> pathSpec;
  
  NS_ASSERTION(!mCopyState, "already copying a msg into this folder");
  if (mCopyState) 
    return NS_ERROR_FAILURE; // already has a  copy in progress
  
  // get mDatabase set, so we can use it to add new hdrs to this db.
  // calling GetDatabaseWOReparse will set mDatabase - we use the comptr
  // here to avoid doubling the refcnt on mDatabase. We don't care if this
  // fails - we just want to give it a chance. It will definitely fail in
  // nsLocalMailFolder::EndCopy because we will have written data to the folder
  // and changed its size.
  nsCOMPtr <nsIMsgDatabase> msgDB;
  GetDatabaseWOReparse(getter_AddRefs(msgDB));
  PRBool isLocked;
  
  GetLocked(&isLocked);
  if(!isLocked)
    AcquireSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
  else
    return NS_MSG_FOLDER_BUSY;
  
  rv = GetPath(getter_AddRefs(pathSpec));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = pathSpec->GetFileSpec(&path);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mCopyState = new nsLocalMailCopyState();
  if(!mCopyState)
    return NS_ERROR_OUT_OF_MEMORY;
  
  mCopyState->m_dataBuffer = (char*) PR_CALLOC(COPY_BUFFER_SIZE+1);
  if (!mCopyState->m_dataBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  mCopyState->m_dataBufferSize = COPY_BUFFER_SIZE;
  
  //Before we continue we should verify that there is enough diskspace.
  //XXX How do we do this?
  mCopyState->m_fileStream = new nsOutputFileStream(path, PR_WRONLY |
    PR_CREATE_FILE);
  if(!mCopyState->m_fileStream)
    return NS_ERROR_OUT_OF_MEMORY;
  
  //The new key is the end of the file
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
  mCopyState->m_srcSupport = do_QueryInterface(aSupport, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mCopyState->m_messages = do_QueryInterface(messages, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mCopyState->m_curCopyIndex = 0;
  mCopyState->m_isMove = isMove;
  mCopyState->m_isFolder = isFolder;
  mCopyState->m_allowUndo = allowUndo;
  mCopyState->m_msgWindow = msgWindow;
  rv = messages->Count(&mCopyState->m_totalMsgCount);
  if (listener)
    mCopyState->m_listener = do_QueryInterface(listener, &rv);
  mCopyState->m_copyingMultipleMessages = PR_FALSE;
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::OnCopyCompleted(nsISupports *srcSupport, PRBool moveCopySucceeded)
{
  if (mCopyState && mCopyState->m_notifyFolderLoaded)
    NotifyFolderEvent(mFolderLoadedAtom);

  delete mCopyState;
  mCopyState = nsnull;
 
  (void) RefreshSizeOnDisk();
  // we are the destination folder for a move/copy
  if (moveCopySucceeded && mDatabase)
  {
    mDatabase->SetSummaryValid(PR_TRUE);
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
    (void) CloseDBIfFolderNotOpen();
  }

  PRBool haveSemaphore;
  nsresult result;
  result = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this),
    &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));

  nsCOMPtr<nsIMsgCopyService> copyService = 
     do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &result);

  if (NS_SUCCEEDED(result))  //copyService will do listener->OnStopCopy()
    copyService->NotifyCompletion(srcSupport, this, moveCopySucceeded ? NS_OK : NS_ERROR_FAILURE); 
  return NS_OK;
}

nsresult 
nsMsgLocalMailFolder::SortMessagesBasedOnKey(nsISupportsArray *messages, nsMsgKeyArray *aKeyArray, nsIMsgFolder *srcFolder)
{
  nsresult rv = NS_OK;
  PRUint32 numMessages = 0;
  rv = messages->Count(&numMessages);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ASSERTION ((numMessages == aKeyArray->GetSize()), "message array and key array size are not same");
  rv = messages->Clear();
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsCOMPtr<nsIMsgDatabase> db; 
  rv = srcFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && db)
    for (PRUint32 i=0;i < numMessages; i++)
    {
      rv = db->GetMsgHdrForKey(aKeyArray->GetAt(i), getter_AddRefs(msgHdr));
      NS_ENSURE_SUCCESS(rv,rv);
      if (msgHdr)
        messages->AppendElement(msgHdr);
    }
  return rv;
}
                
NS_IMETHODIMP
nsMsgLocalMailFolder::CopyMessages(nsIMsgFolder* srcFolder, nsISupportsArray*
                                   messages, PRBool isMove,
                                   nsIMsgWindow *msgWindow,
                                   nsIMsgCopyServiceListener* listener, 
                                   PRBool isFolder, PRBool allowUndo)
{
  nsCOMPtr<nsISupports> srcSupport = do_QueryInterface(srcFolder);
  PRBool isServer;
  nsresult rv = GetIsServer(&isServer);
  if (NS_SUCCEEDED(rv) && isServer)
  {
    NS_ASSERTION(0, "Destination is the root folder. Cannot move/copy here");
    if (isMove)
      srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);
    return OnCopyCompleted(srcSupport, PR_FALSE);
  }
  
  nsXPIDLCString uri;
  rv = srcFolder->GetURI(getter_Copies(uri));
  nsCAutoString protocolType(uri);
  protocolType.SetLength(protocolType.FindChar(':'));
  
  if (WeAreOffline() && (protocolType.LowerCaseEqualsLiteral("imap") || protocolType.LowerCaseEqualsLiteral("news")))
  {
    PRUint32 numMessages = 0;
    messages->Count(&numMessages);
    for (PRUint32 i = 0; i < numMessages; i++)
    {
      nsCOMPtr<nsIMsgDBHdr> message;
      messages->QueryElementAt(i, NS_GET_IID(nsIMsgDBHdr),(void **)getter_AddRefs(message));
      if(NS_SUCCEEDED(rv) && message)
      {
        nsMsgKey key;
        PRBool hasMsgOffline = PR_FALSE;
        message->GetMessageKey(&key);
        srcFolder->HasMsgOffline(key, &hasMsgOffline);
        if (!hasMsgOffline)
        {
          if (isMove)
            srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);
          ThrowAlertMsg("cantMoveMsgWOBodyOffline", msgWindow);
          return OnCopyCompleted(srcSupport, PR_FALSE);
        }
      }
    }
  }
  
  // don't update the counts in the dest folder until it is all over
  EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_FALSE /*dbBatching*/);  //dest folder doesn't need db batching
  
  rv = InitCopyState(srcSupport, messages, isMove, listener, msgWindow, isFolder, allowUndo);
  if (NS_FAILED(rv))
    return OnCopyCompleted(srcSupport, PR_FALSE);

  if (!protocolType.LowerCaseEqualsLiteral("mailbox"))
  {
    mCopyState->m_dummyEnvelopeNeeded = PR_TRUE;
    nsParseMailMessageState* parseMsgState = new nsParseMailMessageState();
    if (parseMsgState)
    {
      nsCOMPtr<nsIMsgDatabase> msgDb;
      mCopyState->m_parseMsgState = do_QueryInterface(parseMsgState, &rv);
      GetDatabaseWOReparse(getter_AddRefs(msgDb));   
      if (msgDb)
        parseMsgState->SetMailDB(msgDb);
    }
  }
  
  // undo stuff
  if (allowUndo)    //no undo for folder move/copy or or move/copy from search window
  {
    nsLocalMoveCopyMsgTxn* msgTxn = nsnull;
    
    msgTxn = new nsLocalMoveCopyMsgTxn(srcFolder, this, isMove);
    
    if (msgTxn)
      rv =
      msgTxn->QueryInterface(NS_GET_IID(nsLocalMoveCopyMsgTxn),
      getter_AddRefs(mCopyState->m_undoMsgTxn));
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv))
    {
      (void) OnCopyCompleted(srcSupport, PR_FALSE);
    }
    else
    {
      msgTxn->SetMsgWindow(msgWindow);
      if (isMove)
      {
        if (mFlags & MSG_FOLDER_FLAG_TRASH)
          msgTxn->SetTransactionType(nsIMessenger::eDeleteMsg);
        else
          msgTxn->SetTransactionType(nsIMessenger::eMoveMsg);
      }
      else
      {
        msgTxn->SetTransactionType(nsIMessenger::eCopyMsg);
      }
    }
  }
  PRUint32 numMsgs = 0;
  mCopyState->m_messages->Count(&numMsgs);
  if (numMsgs > 1 && ((protocolType.LowerCaseEqualsLiteral("imap") && !WeAreOffline()) || protocolType.LowerCaseEqualsLiteral("mailbox")))
  {
    mCopyState->m_copyingMultipleMessages = PR_TRUE;
    rv = CopyMessagesTo(mCopyState->m_messages, msgWindow, this, isMove);
    if (NS_FAILED(rv))
    {
      NS_ERROR("copy message failed");
      (void) OnCopyCompleted(srcSupport, PR_FALSE);
    }
  }
  else
  {
    nsCOMPtr<nsISupports> msgSupport;
    msgSupport = getter_AddRefs(mCopyState->m_messages->ElementAt(0));
    if (msgSupport)
    {
      rv = CopyMessageTo(msgSupport, this, msgWindow, isMove);
      if (NS_FAILED(rv))
      {
        NS_ASSERTION(PR_FALSE, "copy message failed");
        (void) OnCopyCompleted(srcSupport, PR_FALSE);
      }
    }
  }
  // if this failed immediately, need to turn back on notifications and inform FE.
  if (NS_FAILED(rv))
  {
    if (isMove)
      srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);
    EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_FALSE /*dbBatching*/);  //dest folder doesn't need db batching
  }
  return rv;
}
// for srcFolder that are on different server than the dstFolder. 
nsresult
nsMsgLocalMailFolder::CopyFolderAcrossServer(nsIMsgFolder* srcFolder, nsIMsgWindow *msgWindow,
								  nsIMsgCopyServiceListener *listener )
{
  mInitialized = PR_TRUE;
    
  nsXPIDLString folderName;
  srcFolder->GetName(getter_Copies(folderName));
	
  nsresult rv = CreateSubfolder(folderName, msgWindow);
  if (NS_FAILED(rv)) return rv;

  nsCAutoString escapedFolderName;
  rv = NS_MsgEscapeEncodeURLPath(folderName, escapedFolderName);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgFolder> newFolder;
  nsCOMPtr<nsIMsgFolder> newMsgFolder;

  rv = FindSubFolder(escapedFolderName, getter_AddRefs(newMsgFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsISimpleEnumerator> messages;
  rv = srcFolder->GetMessages(msgWindow, getter_AddRefs(messages));

  nsCOMPtr<nsISupportsArray> msgSupportsArray;
  NS_NewISupportsArray(getter_AddRefs(msgSupportsArray));

  PRBool hasMoreElements;
  nsCOMPtr<nsISupports> aSupport;

  if (messages)
    messages->HasMoreElements(&hasMoreElements);
  
  while (hasMoreElements && NS_SUCCEEDED(rv))
  {
    rv = messages->GetNext(getter_AddRefs(aSupport));
    rv = msgSupportsArray->AppendElement(aSupport);
    messages->HasMoreElements(&hasMoreElements);
  }
  
  PRUint32 numMsgs=0;
  msgSupportsArray->Count(&numMsgs);

  if (numMsgs > 0 )   //if only srcFolder has messages..
    newMsgFolder->CopyMessages(srcFolder, msgSupportsArray, PR_FALSE, msgWindow, listener, PR_TRUE /* is folder*/, PR_FALSE /* allowUndo */);
  else
  {
    nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(newMsgFolder);
    if (localFolder)
    {
      // normally these would get called from ::EndCopy when the last message
      // was finished copying. But since there are no messages, we have to call
      // them explicitly.
      nsCOMPtr<nsISupports> srcSupports = do_QueryInterface(newMsgFolder);
      localFolder->CopyAllSubFolders(srcFolder, msgWindow, listener);
      return localFolder->OnCopyCompleted(srcSupports, PR_TRUE);
    }
  }	    
  return NS_OK;  // otherwise the front-end will say Exception::CopyFolder
}

nsresult    //copy the sub folders
nsMsgLocalMailFolder::CopyAllSubFolders(nsIMsgFolder *srcFolder, 
                                      nsIMsgWindow *msgWindow, 
                                      nsIMsgCopyServiceListener *listener )
{
  nsresult rv;
  nsCOMPtr<nsIEnumerator> aEnumerator;
  srcFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  nsCOMPtr<nsIMsgFolder>folder;
  nsCOMPtr<nsISupports> aSupports;
  rv = aEnumerator->First();
  while (NS_SUCCEEDED(rv))
  {
    rv = aEnumerator->CurrentItem(getter_AddRefs(aSupports));
    folder = do_QueryInterface(aSupports);
    rv = aEnumerator->Next();
    if (folder)
      CopyFolderAcrossServer(folder, msgWindow, listener);  
    
  }  
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CopyFolder( nsIMsgFolder* srcFolder, PRBool isMoveFolder,
                                   nsIMsgWindow *msgWindow,
                                   nsIMsgCopyServiceListener* listener)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(srcFolder);
  
  if (isMoveFolder)   // isMoveFolder == true when "this" and srcFolder are on same server
    rv = CopyFolderLocal(srcFolder, isMoveFolder, msgWindow, listener );
  else
    rv = CopyFolderAcrossServer(srcFolder, msgWindow, listener );
	  
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CopyFolderLocal(nsIMsgFolder *srcFolder, PRBool isMoveFolder,
                                      nsIMsgWindow *msgWindow,
                                      nsIMsgCopyServiceListener *listener )
{
  nsresult rv;
  mInitialized = PR_TRUE;
  nsCOMPtr<nsIMsgFolder> newMsgFolder;
  PRBool isChildOfTrash=PR_FALSE;
  rv = IsChildOfTrash(&isChildOfTrash);
  if (isChildOfTrash)  
  {
    if (isMoveFolder) //do it just for the parent folder (isMoveFolder is true for parent only) if we are deleting/moving a folder tree
    {
      PRBool okToDelete = PR_FALSE;
      ConfirmFolderDeletion(msgWindow, &okToDelete);
      if (!okToDelete)
        return NS_MSG_ERROR_COPY_FOLDER_ABORTED;
    }

    PRBool match = PR_FALSE;
    rv = srcFolder->MatchOrChangeFilterDestination(nsnull, PR_FALSE, &match);
    if (match)
    {
      PRBool confirmed = PR_FALSE;
      srcFolder->ConfirmFolderDeletionForFilter(msgWindow, &confirmed);
      if (!confirmed) 
        return NS_MSG_ERROR_COPY_FOLDER_ABORTED;
    }
  }
  
  nsXPIDLString folderName;
  srcFolder->GetName(getter_Copies(folderName));
  
  srcFolder->ForceDBClosed();	  
  
  nsCOMPtr<nsIFileSpec> oldPathSpec;
  rv = srcFolder->GetPath(getter_AddRefs(oldPathSpec));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsFileSpec oldPath;
  rv = oldPathSpec->GetFileSpec(&oldPath);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsLocalFolderSummarySpec	summarySpec(oldPath);
  
  nsCOMPtr<nsIFileSpec> newPathSpec;
  rv = GetPath(getter_AddRefs(newPathSpec));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsFileSpec newPath;
  rv = newPathSpec->GetFileSpec(&newPath);
  NS_ENSURE_SUCCESS(rv,rv);
  
  if (!newPath.IsDirectory())
  {
    AddDirectorySeparator(newPath);
    newPath.CreateDirectory();
  }
  
  rv = CheckIfFolderExists(folderName.get(), this, msgWindow);
  if(NS_FAILED(rv)) 
    return rv;
  
  nsFileSpec path = oldPath;
  
  rv = path.CopyToDir(newPath);   //copying necessary for aborting.... if failure return
  NS_ENSURE_SUCCESS(rv, rv);     //would fail if a file by that name exists

  rv = summarySpec.CopyToDir(newPath);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = AddSubfolder(folderName, getter_AddRefs(newMsgFolder));  
  NS_ENSURE_SUCCESS(rv, rv);

  newMsgFolder->SetPrettyName(folderName.get());
  
  PRUint32 flags;
  srcFolder->GetFlags(&flags);
  newMsgFolder->SetFlags(flags);
  PRBool changed = PR_FALSE;
  rv = srcFolder->MatchOrChangeFilterDestination(newMsgFolder, PR_TRUE, &changed);
  if (changed)
    srcFolder->AlertFilterChanged(msgWindow);
  
  nsCOMPtr<nsIEnumerator> aEnumerator;
  srcFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  nsCOMPtr<nsIMsgFolder>folder;
  nsCOMPtr<nsISupports> supports;
  rv = aEnumerator->First();
  nsresult copyStatus = NS_OK;
  while (NS_SUCCEEDED(rv) && NS_SUCCEEDED(copyStatus))
  {
    rv = aEnumerator->CurrentItem(getter_AddRefs(supports));
    folder = do_QueryInterface(supports);
    rv = aEnumerator->Next();
    if (folder)
    {
      nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(newMsgFolder);
      if (localFolder)
        copyStatus = localFolder->CopyFolderLocal(folder, PR_FALSE, msgWindow, listener);  // PR_FALSE needed to avoid un-necessary deletions
    }
  }  
  
  if (isMoveFolder && NS_SUCCEEDED(copyStatus))
  {
    //notifying the "folder" that was dragged and dropped has been created.
    //no need to do this for its subfolders - isMoveFolder will be true for "folder"
    NotifyItemAdded(newMsgFolder);
    
    nsCOMPtr<nsIMsgFolder> msgParent;
    srcFolder->GetParentMsgFolder(getter_AddRefs(msgParent));
    srcFolder->SetParent(nsnull);
    if (msgParent) 
    {
      msgParent->PropagateDelete(srcFolder, PR_FALSE, msgWindow);  // The files have already been moved, so delete storage PR_FALSE 
      oldPath.Delete(PR_FALSE);  //berkeley mailbox
      summarySpec.Delete(PR_FALSE); //msf file
      if (!oldPath.IsDirectory())   //folder path cannot be directory but still check it to be safe
      {
        AddDirectorySeparator(oldPath);
        if (oldPath.IsDirectory())
          oldPath.Delete(PR_TRUE);   //delete the .sbd directory and it's content. All subfolders have been moved
      }

      nsCOMPtr<nsIFileSpec> parentPathSpec;
      rv = msgParent->GetPath(getter_AddRefs(parentPathSpec));
      NS_ENSURE_SUCCESS(rv,rv);
  
      nsFileSpec parentPath;
      rv = parentPathSpec->GetFileSpec(&parentPath);
      NS_ENSURE_SUCCESS(rv,rv);

      AddDirectorySeparator(parentPath);
      nsDirectoryIterator i(parentPath, PR_FALSE);
      // i.Exists() checks if the directory is empty or not 
      if (parentPath.IsDirectory() && !i.Exists())
        parentPath.Delete(PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CopyFileMessage(nsIFileSpec* fileSpec, nsIMsgDBHdr*
                                      msgToReplace, PRBool isDraftOrTemplate,
                                      nsIMsgWindow *msgWindow,
                                      nsIMsgCopyServiceListener* listener)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIInputStream> inputStream;
  nsParseMailMessageState* parseMsgState = nsnull;
  PRUint32 fileSize = 0;
  nsCOMPtr<nsISupports> fileSupport(do_QueryInterface(fileSpec, &rv));

  nsCOMPtr<nsISupportsArray> messages;
  rv = NS_NewISupportsArray(getter_AddRefs(messages));
  
  if (msgToReplace)
  {
    nsCOMPtr<nsISupports> msgSupport(do_QueryInterface(msgToReplace, &rv));
    if (NS_SUCCEEDED(rv))
      messages->AppendElement(msgSupport);
  }

  rv = InitCopyState(fileSupport, messages, msgToReplace ? PR_TRUE:PR_FALSE,
                     listener, msgWindow, PR_FALSE, PR_FALSE);
  if (NS_FAILED(rv)) goto done;

  parseMsgState = new nsParseMailMessageState();
  if (parseMsgState)
  {
    nsCOMPtr<nsIMsgDatabase> msgDb;
    mCopyState->m_parseMsgState = do_QueryInterface(parseMsgState, &rv);
    GetDatabaseWOReparse(getter_AddRefs(msgDb));
    if (msgDb)
      parseMsgState->SetMailDB(msgDb);
  }

  rv = fileSpec->OpenStreamForReading();
  if (NS_FAILED(rv)) goto done;

  rv = fileSpec->GetInputStream(getter_AddRefs(inputStream));
  if (NS_FAILED(rv)) goto done;
  
  rv = NS_ERROR_NULL_POINTER;
  if (inputStream)
    rv = inputStream->Available(&fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = BeginCopy(nsnull);
  if (NS_FAILED(rv)) goto done;
  rv = CopyData(inputStream, (PRInt32) fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = EndCopy(PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  if (msgToReplace && mDatabase)  //mDatabase should have been initialized above - if we got msgDb
    rv = DeleteMessage(msgToReplace, msgWindow, PR_TRUE, PR_TRUE);

done:
  if(NS_FAILED(rv))
    (void) OnCopyCompleted(fileSupport, PR_FALSE);

  fileSpec->CloseStream();
  return rv;
}

nsresult nsMsgLocalMailFolder::DeleteMessage(nsISupports *message,
                                             nsIMsgWindow *msgWindow,
                                             PRBool deleteStorage, PRBool commit)
{
  nsresult rv = NS_OK;
  if (deleteStorage)
  {
    nsCOMPtr <nsIMsgDBHdr> msgDBHdr(do_QueryInterface(message, &rv));
    
    if(NS_SUCCEEDED(rv))
      rv = mDatabase->DeleteHeader(msgDBHdr, nsnull, commit, PR_TRUE);
  }
  return rv;
}

#include "nsIRssIncomingServer.h"

NS_IMETHODIMP nsMsgLocalMailFolder::GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server)); 
  if (NS_FAILED(rv)) return rv;
  if (!server) return NS_MSG_INVALID_OR_MISSING_SERVER;
  
  nsCOMPtr<nsILocalMailIncomingServer> localMailServer = do_QueryInterface(server);
  if (!localMailServer) 
      return NS_MSG_INVALID_OR_MISSING_SERVER;
  
  // XXX todo, move all this into nsILocalMailIncomingServer's GetNewMail
  // so that we don't have to have RSS foo here.
  nsCOMPtr<nsIRssIncomingServer> rssServer = do_QueryInterface(server);
  if (rssServer)
  {
      return localMailServer->GetNewMail(aWindow, aListener, this, nsnull); 
  }
  
  nsCOMPtr<nsIMsgFolder> inbox;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = server->GetRootMsgFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    PRUint32 numFolders;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
  }
  nsCOMPtr<nsIMsgLocalMailFolder> localInbox = do_QueryInterface(inbox, &rv);
  if (NS_SUCCEEDED(rv) && localInbox)
  {
    PRBool valid = PR_FALSE;
    nsCOMPtr <nsIMsgDatabase> db;
    rv = inbox->GetMsgDatabase(aWindow, getter_AddRefs(db));
    if (NS_SUCCEEDED(rv) && db)
    {
      rv = db->GetSummaryValid(&valid);
      if (valid)
        rv = localMailServer->GetNewMail(aWindow, aListener, inbox, nsnull); 
      else
        rv = localInbox->SetCheckForNewMessagesAfterParsing(PR_TRUE);
    }
  }
  return rv;
}

nsresult nsMsgLocalMailFolder::WriteStartOfNewMessage()
{
  mCopyState->m_curDstKey = mCopyState->m_fileStream->tell();
  
  // CopyFileMessage() and CopyMessages() from servers other than pop3
  if (mCopyState->m_parseMsgState)
  {
    mCopyState->m_parseMsgState->SetEnvelopePos(mCopyState->m_curDstKey);
    mCopyState->m_parseMsgState->SetState(nsIMsgParseMailMsgState::ParseHeadersState);
  }
  if (mCopyState->m_dummyEnvelopeNeeded)
  {
    nsCString result;
    char timeBuffer[128];
    PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
    PR_FormatTimeUSEnglish(timeBuffer, sizeof(timeBuffer),
                           "%a %b %d %H:%M:%S %Y",
                           &now);
    result.Append("From - ");
    result.Append(timeBuffer);
    result.Append(MSG_LINEBREAK);

    // *** jt - hard code status line for now; come back later

    nsresult rv;
    nsCOMPtr <nsIMsgDBHdr> curSourceMessage; 
    curSourceMessage = do_QueryElementAt(mCopyState->m_messages,
                                         mCopyState->m_curCopyIndex, &rv);

  	char statusStrBuf[50];
    if (curSourceMessage)
    {
			PRUint32 dbFlags = 0;
      curSourceMessage->GetFlags(&dbFlags);

      // write out x-mozilla-status, but make sure we don't write out MSG_FLAG_OFFLINE
			PR_snprintf(statusStrBuf, sizeof(statusStrBuf), X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, 
        dbFlags & ~(MSG_FLAG_RUNTIME_ONLY | MSG_FLAG_OFFLINE) & 0x0000FFFF);
      // need to carry the new flag over to the new header.
      if (dbFlags & MSG_FLAG_NEW)
      {
      }
    }
    else
    {
      strcpy(statusStrBuf, "X-Mozilla-Status: 0001" MSG_LINEBREAK);
    }
    *(mCopyState->m_fileStream) << result.get();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.get(), result.Length());
    *(mCopyState->m_fileStream) << statusStrBuf;
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
        statusStrBuf, strlen(statusStrBuf));
    result = "X-Mozilla-Status2: 00000000" MSG_LINEBREAK;
    *(mCopyState->m_fileStream) << result.get();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.get(), result.Length());
    mCopyState->m_fromLineSeen = PR_TRUE;
  }
  else 
  {
    mCopyState->m_fromLineSeen = PR_FALSE;
  }

  mCopyState->m_curCopyIndex++;

  return NS_OK;
}

//nsICopyMessageListener
NS_IMETHODIMP nsMsgLocalMailFolder::BeginCopy(nsIMsgDBHdr *message)
{
  if (!mCopyState) 
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
 
  PRInt32 messageIndex = (mCopyState->m_copyingMultipleMessages) ? mCopyState->m_curCopyIndex - 1 : mCopyState->m_curCopyIndex;
  NS_ASSERTION(!mCopyState->m_copyingMultipleMessages || mCopyState->m_curCopyIndex >= 0, "mCopyState->m_curCopyIndex invalid");
  // by the time we get here, m_curCopyIndex is 1 relative because WriteStartOfNewMessage increments it
  mCopyState->m_messages->QueryElementAt(messageIndex, NS_GET_IID(nsIMsgDBHdr),
                                  (void **)getter_AddRefs(mCopyState->m_message));

  DisplayMoveCopyStatusMsg();
  // if we're copying more than one message, StartMessage will handle this.
  if (!mCopyState->m_copyingMultipleMessages)
	rv = WriteStartOfNewMessage();
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::CopyData(nsIInputStream *aIStream, PRInt32 aLength)
{
  //check to make sure we have control of the write.
  PRBool haveSemaphore;
  nsresult rv = NS_OK;
  
  rv = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this),
    &haveSemaphore);
  if(NS_FAILED(rv))
    return rv;
  if(!haveSemaphore)
    return NS_MSG_FOLDER_BUSY;
  
  if (!mCopyState) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  PRUint32 readCount;
  if ( aLength + mCopyState->m_leftOver > mCopyState->m_dataBufferSize )
  {
    mCopyState->m_dataBuffer = (char *) PR_REALLOC(mCopyState->m_dataBuffer, aLength + mCopyState->m_leftOver+ 1);
    if (!mCopyState->m_dataBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
    mCopyState->m_dataBufferSize = aLength + mCopyState->m_leftOver;
  }
  
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
  char *start, *end;
  PRUint32 linebreak_len = 0;
  
  rv = aIStream->Read(mCopyState->m_dataBuffer + mCopyState->m_leftOver,
    aLength, &readCount);
  NS_ENSURE_SUCCESS(rv, rv);
  mCopyState->m_leftOver += readCount;
  mCopyState->m_dataBuffer[mCopyState->m_leftOver] ='\0';
  start = mCopyState->m_dataBuffer;
  end = (char *) memchr(start, '\r', mCopyState->m_leftOver);
  if (!end)
   	end = (char *) memchr(start, '\n', mCopyState->m_leftOver);
  else if (*(end+1) == nsCRT::LF && linebreak_len == 0)
    linebreak_len = 2;
  
  if (linebreak_len == 0) // not set yet
    linebreak_len = 1;
  
  nsCString line;
  char tmpChar = 0;
  PRInt32 lineLength, bytesWritten;
  
  while (start && end)
  {
    if (mCopyState->m_fromLineSeen)
    {
      if (strncmp(start, "From ", 5) == 0)
      {
        line = ">";
        
        tmpChar = *end;
        *end = 0;
        line += start;
        *end = tmpChar;
        line += MSG_LINEBREAK;
        
        lineLength = line.Length();
        bytesWritten = mCopyState->m_fileStream->write(line.get(), lineLength); 
        if (bytesWritten != lineLength)
        {
          ThrowAlertMsg("copyMsgWriteFailed", mCopyState->m_msgWindow);
          mCopyState->m_writeFailed = PR_TRUE;
          return NS_MSG_ERROR_WRITING_MAIL_FOLDER;
        }
        
        if (mCopyState->m_parseMsgState)
          mCopyState->m_parseMsgState->ParseAFolderLine(line.get(),
          line.Length());
        goto keepGoing;
      }
    }
    else
    {
      mCopyState->m_fromLineSeen = PR_TRUE;
      NS_ASSERTION(strncmp(start, "From ", 5) == 0, 
        "Fatal ... bad message format\n");
    }
    
    lineLength = end-start+linebreak_len;
    bytesWritten = mCopyState->m_fileStream->write(start, lineLength);
    if (bytesWritten != lineLength)
    {
      ThrowAlertMsg("copyMsgWriteFailed", mCopyState->m_msgWindow);
      mCopyState->m_writeFailed = PR_TRUE;
      return NS_MSG_ERROR_WRITING_MAIL_FOLDER;
    }
    
    if (mCopyState->m_parseMsgState)
      mCopyState->m_parseMsgState->ParseAFolderLine(start,
      end-start+linebreak_len);
keepGoing:
    start = end+linebreak_len;
    if (start >=
      &mCopyState->m_dataBuffer[mCopyState->m_leftOver])
    {
      mCopyState->m_leftOver = 0;
      break;
    }
    char *endBuffer = mCopyState->m_dataBuffer + mCopyState->m_leftOver;
    end = (char *) memchr(start, '\r', endBuffer - start + 1);
    if (end)
    {
      if (*(end+1) == nsCRT::LF)  //need to set the linebreak_len each time
        linebreak_len = 2;  //CRLF
      else
        linebreak_len = 1;  //only CR
    }
    if (!end)
    {
      end = (char *) memchr(start, '\n', endBuffer - start + 1);
      if (end)
        linebreak_len = 1;   //LF
      else
        linebreak_len =0;  //no LF
    }
    if (start && !end)
    {
      mCopyState->m_leftOver -= (start - mCopyState->m_dataBuffer);
      memcpy (mCopyState->m_dataBuffer, start,
        mCopyState->m_leftOver+1);
    }
  }
  return rv;
}

void nsMsgLocalMailFolder::CopyPropertiesToMsgHdr(nsIMsgDBHdr *destHdr, nsIMsgDBHdr *srcHdr)
{
  nsXPIDLCString sourceJunkScore;
  srcHdr->GetStringProperty("junkscore", getter_Copies(sourceJunkScore));
  destHdr->SetStringProperty("junkscore", sourceJunkScore);
  srcHdr->GetStringProperty("junkscoreorigin", getter_Copies(sourceJunkScore));
  destHdr->SetStringProperty("junkscoreorigin", sourceJunkScore);
}


NS_IMETHODIMP nsMsgLocalMailFolder::EndCopy(PRBool copySucceeded)
{
  // we are the destination folder for a move/copy
  nsresult rv = copySucceeded ? NS_OK : NS_ERROR_FAILURE;
  if (!mCopyState) return NS_OK;
  if (!copySucceeded || mCopyState->m_writeFailed)
  {
    if (mCopyState->m_fileStream)
      mCopyState->m_fileStream->close();
    
    nsCOMPtr <nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    
    if (NS_SUCCEEDED(rv) && pathSpec)
      pathSpec->Truncate(mCopyState->m_curDstKey);
    
    if (!mCopyState->m_isMove)
    {
    /*passing PR_TRUE because the messages that have been successfully copied have their corresponding
    hdrs in place. The message that has failed has been truncated so the msf file and berkeley mailbox
      are in sync*/
      
      (void) OnCopyCompleted(mCopyState->m_srcSupport, PR_TRUE);
      
      // enable the dest folder
      EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_FALSE /*dbBatching*/); //dest folder doesn't need db batching
    }
    return NS_OK;
  }
  
  nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;
  PRBool multipleCopiesFinished = (mCopyState->m_curCopyIndex >= mCopyState->m_totalMsgCount);
  
  if (mCopyState->m_undoMsgTxn)
    localUndoTxn = do_QueryInterface(mCopyState->m_undoMsgTxn);
  
  if (mCopyState)
  {
    NS_ASSERTION(mCopyState->m_leftOver == 0, "whoops, something wrong with previous copy");
    mCopyState->m_leftOver = 0; // reset to 0.
    // need to reset this in case we're move/copying multiple msgs.
    mCopyState->m_fromLineSeen = PR_FALSE;
    // flush the copied message.
    if (mCopyState->m_fileStream)
      mCopyState->m_fileStream->seek(PR_SEEK_CUR, 0); // seeking causes a flush, w/o syncing
  }
  //Copy the header to the new database
  if (copySucceeded && mCopyState->m_message)
  { //  CopyMessages() goes here; CopyFileMessage() never gets in here because
    //  the mCopyState->m_message will be always null for file message
    
    nsCOMPtr<nsIMsgDBHdr> newHdr;
    
    if(!mCopyState->m_parseMsgState)
    {
      if(mDatabase)
      {
        rv = mDatabase->CopyHdrFromExistingHdr(mCopyState->m_curDstKey,
          mCopyState->m_message, PR_TRUE,
          getter_AddRefs(newHdr));
        PRUint32 newHdrFlags;
        // turn off offline flag - it's not valid for local mail folders.
        if (newHdr)
          newHdr->AndFlags(~MSG_FLAG_OFFLINE, &newHdrFlags);
      }
      // we can do undo with the dest folder db, see bug #198909
      //else
      //  mCopyState->m_undoMsgTxn = nsnull; //null out the transaction because we can't undo w/o the msg db
    }
    
    // if we plan on allowing undo, (if we have a mCopyState->m_parseMsgState or not)
    // we need to save the source and dest keys on the undo txn.
    // see bug #179856 for details
    PRBool isImap;
    if (NS_SUCCEEDED(rv) && localUndoTxn) {
      localUndoTxn->GetSrcIsImap(&isImap);
      if (!isImap || !mCopyState->m_copyingMultipleMessages)
      {
        nsMsgKey aKey;
        PRUint32 statusOffset;
        mCopyState->m_message->GetMessageKey(&aKey);
        mCopyState->m_message->GetStatusOffset(&statusOffset);
        localUndoTxn->AddSrcKey(aKey);
        localUndoTxn->AddSrcStatusOffset(statusOffset);
        localUndoTxn->AddDstKey(mCopyState->m_curDstKey);
      }
    }
  }
  if (mCopyState->m_dummyEnvelopeNeeded)
  {
    mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
    *(mCopyState->m_fileStream) << MSG_LINEBREAK;
    if (mCopyState->m_parseMsgState)
      mCopyState->m_parseMsgState->ParseAFolderLine(CRLF, MSG_LINEBREAK_LEN);
  }
  
  // CopyFileMessage() and CopyMessages() from servers other than mailbox
  if (mCopyState->m_parseMsgState)
  {
    nsCOMPtr<nsIMsgDatabase> msgDb;
    nsCOMPtr<nsIMsgDBHdr> newHdr;
    
    mCopyState->m_parseMsgState->FinishHeader();
    
    GetDatabaseWOReparse(getter_AddRefs(msgDb));
    if (msgDb)
    {
      nsresult result = mCopyState->m_parseMsgState->GetNewMsgHdr(getter_AddRefs(newHdr));
      if (NS_SUCCEEDED(result) && newHdr)
      {
        // need to copy junk score from mCopyState->m_message to newHdr.
        if (mCopyState->m_message)
          CopyPropertiesToMsgHdr(newHdr, mCopyState->m_message);
        msgDb->AddNewHdrToDB(newHdr, PR_TRUE);
        if (localUndoTxn)
        { 
          // ** jt - recording the message size for possible undo use; the
          // message size is different for pop3 and imap4 messages
          PRUint32 msgSize;
          newHdr->GetMessageSize(&msgSize);
          localUndoTxn->AddDstMsgSize(msgSize);
        }
      }
      //	    msgDb->SetSummaryValid(PR_TRUE);
      //	    msgDb->Commit(nsMsgDBCommitType::kLargeCommit);
    }
    else
      mCopyState->m_undoMsgTxn = nsnull; //null out the transaction because we can't undo w/o the msg db
    
    mCopyState->m_parseMsgState->Clear();
    
    if (mCopyState->m_listener) // CopyFileMessage() only
      mCopyState->m_listener->SetMessageKey((PRUint32) mCopyState->m_curDstKey);
  }
  
  if (!multipleCopiesFinished && !mCopyState->m_copyingMultipleMessages)
  { // CopyMessages() goes here; CopyFileMessage() never gets in here because
    // curCopyIndex will always be less than the mCopyState->m_totalMsgCount
    nsCOMPtr<nsISupports> aSupport =
      getter_AddRefs(mCopyState->m_messages->ElementAt
      (mCopyState->m_curCopyIndex));
    rv = CopyMessageTo(aSupport, this, mCopyState->m_msgWindow, mCopyState->m_isMove);
  }
  else
  { // both CopyMessages() & CopyFileMessage() go here if they have
    // done copying operation; notify completion to copy service
    if(!mCopyState->m_isMove)
    {
      if (multipleCopiesFinished)
      {
        nsCOMPtr<nsIMsgFolder> srcFolder;
        srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
        if (mCopyState->m_isFolder)
          CopyAllSubFolders(srcFolder, nsnull, nsnull);  //Copy all subfolders then notify completion
        
        if (mCopyState->m_msgWindow && mCopyState->m_undoMsgTxn)
        {
          nsCOMPtr<nsITransactionManager> txnMgr;
          mCopyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
          if (txnMgr)
            txnMgr->DoTransaction(mCopyState->m_undoMsgTxn);
        }
        if (srcFolder && !mCopyState->m_isFolder)
          srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);	 
        
        (void) OnCopyCompleted(mCopyState->m_srcSupport, PR_TRUE);
        // enable the dest folder
        EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_FALSE /*dbBatching*/); //dest folder doesn't need db batching
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndMove(PRBool moveSucceeded)
{
  nsresult result = NS_OK;

  if (!mCopyState)
    return NS_OK;
  if (!moveSucceeded || mCopyState->m_writeFailed)
  {
    //Notify that a completion finished.
    nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
    srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);

    /*passing PR_TRUE because the messages that have been successfully copied have their corressponding
    hdrs in place. The message that has failed has been truncated so the msf file and berkeley mailbox
    are in sync*/

    (void) OnCopyCompleted(mCopyState->m_srcSupport, PR_TRUE);

    // enable the dest folder
    EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_FALSE /*dbBatching*/ );  //dest folder doesn't need db batching
   
    return NS_OK;
  }
  
  if (mCopyState && mCopyState->m_curCopyIndex >= mCopyState->m_totalMsgCount)
  {
    
    //Notify that a completion finished.
    nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
    if(srcFolder)
    {
      // lets delete these all at once - much faster that way
      result = srcFolder->DeleteMessages(mCopyState->m_messages, mCopyState->m_msgWindow, PR_TRUE, PR_TRUE, nsnull, mCopyState->m_allowUndo);
      srcFolder->NotifyFolderEvent(NS_SUCCEEDED(result) ? mDeleteOrMoveMsgCompletedAtom : mDeleteOrMoveMsgFailedAtom);
    }
      
    // enable the dest folder
    EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_FALSE /*dbBatching*/); //dest folder doesn't need db batching
      
    if (NS_SUCCEEDED(result) && mCopyState->m_msgWindow && mCopyState->m_undoMsgTxn)
    {
      nsCOMPtr<nsITransactionManager> txnMgr;
      mCopyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
      if (txnMgr)
        txnMgr->DoTransaction(mCopyState->m_undoMsgTxn);
    }
    (void) OnCopyCompleted(mCopyState->m_srcSupport, NS_SUCCEEDED(result) ? PR_TRUE : PR_FALSE);  //clear the copy state so that the next message from a different folder can be move
  }
  
  return NS_OK;

}

// this is the beginning of the next message copied
NS_IMETHODIMP nsMsgLocalMailFolder::StartMessage()
{
  return WriteStartOfNewMessage();
}

// just finished the current message.
NS_IMETHODIMP nsMsgLocalMailFolder::EndMessage(nsMsgKey key)
{
  nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;
  nsCOMPtr<nsIMsgWindow> msgWindow;
  nsresult rv;
  
  if (mCopyState->m_undoMsgTxn)
  {
    localUndoTxn = do_QueryInterface(mCopyState->m_undoMsgTxn, &rv);
    if (NS_SUCCEEDED(rv))
      localUndoTxn->GetMsgWindow(getter_AddRefs(msgWindow));
  }
  
  if (localUndoTxn)
  {
    localUndoTxn->AddSrcKey(key);
    localUndoTxn->AddDstKey(mCopyState->m_curDstKey);
  }
  
  // I think this is always true for online to offline copy
  mCopyState->m_dummyEnvelopeNeeded = PR_TRUE;
  
  if (mCopyState->m_dummyEnvelopeNeeded)
  {
    mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
    *(mCopyState->m_fileStream) << MSG_LINEBREAK;
    if (mCopyState->m_parseMsgState)
      mCopyState->m_parseMsgState->ParseAFolderLine(CRLF, MSG_LINEBREAK_LEN);
  }
  
  // CopyFileMessage() and CopyMessages() from servers other than mailbox
  if (mCopyState->m_parseMsgState)
  {
    nsresult result;
    nsCOMPtr<nsIMsgDatabase> msgDb;
    nsCOMPtr<nsIMsgDBHdr> newHdr;
    
    mCopyState->m_parseMsgState->FinishHeader();
    
    result = mCopyState->m_parseMsgState->GetNewMsgHdr(getter_AddRefs(newHdr));
    if (NS_SUCCEEDED(result) && newHdr)
    {
      nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
      nsCOMPtr<nsIMsgDatabase> srcDB;
      if (srcFolder)
      {
        srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
        if (srcDB)
        {
          nsCOMPtr <nsIMsgDBHdr> srcMsgHdr;
          srcDB->GetMsgHdrForKey(key, getter_AddRefs(srcMsgHdr));
          if (srcMsgHdr)
            CopyPropertiesToMsgHdr(newHdr, srcMsgHdr);
        }
      }
      
      result = GetDatabaseWOReparse(getter_AddRefs(msgDb));
      if (NS_SUCCEEDED(result) && msgDb)
      {
        msgDb->AddNewHdrToDB(newHdr, PR_TRUE);
        if (localUndoTxn)
        { // ** jt - recording the message size for possible undo use; the
          // message size is different for pop3 and imap4 messages
          PRUint32 msgSize;
          newHdr->GetMessageSize(&msgSize);
          localUndoTxn->AddDstMsgSize(msgSize);
        }
      }
      else
        mCopyState->m_undoMsgTxn = nsnull; //null out the transaction because we can't undo w/o the msg db
    }
    mCopyState->m_parseMsgState->Clear();
    
    if (mCopyState->m_listener) // CopyFileMessage() only
      mCopyState->m_listener->SetMessageKey((PRUint32) mCopyState->m_curDstKey);
  }
  
  if (mCopyState->m_fileStream)
    mCopyState->m_fileStream->flush();
  return NS_OK;
}


nsresult nsMsgLocalMailFolder::CopyMessagesTo(nsISupportsArray *messages, 
                                             nsIMsgWindow *aMsgWindow, nsIMsgFolder *dstFolder,
                                             PRBool isMove)
{
  if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;
   
  nsresult rv;

  nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener = do_CreateInstance(NS_COPYMESSAGESTREAMLISTENER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsICopyMessageListener> copyListener(do_QueryInterface(dstFolder));
  if(!copyListener)
    return NS_ERROR_NO_INTERFACE;
  
  nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryInterface(mCopyState->m_srcSupport));
  if(!srcFolder)
    return NS_ERROR_NO_INTERFACE;
  
  rv = copyStreamListener->Init(srcFolder, copyListener, nsnull);
  if(NS_FAILED(rv))
    return rv;
  
  if (!mCopyState->m_messageService)
  {
    nsXPIDLCString uri;
    srcFolder->GetURI(getter_Copies(uri));
    rv = GetMessageServiceFromURI(uri, getter_AddRefs(mCopyState->m_messageService));
  }
  
  if (NS_SUCCEEDED(rv) && mCopyState->m_messageService)
  {
    nsMsgKeyArray keyArray;
    PRUint32 numMessages = 0;
    messages->Count(&numMessages);
    for (PRUint32 i = 0; i < numMessages; i++)
    {
      nsCOMPtr<nsIMsgDBHdr> aMessage = do_QueryElementAt(messages, i, &rv);
      if(NS_SUCCEEDED(rv) && aMessage)
      {
        nsMsgKey key;
        aMessage->GetMessageKey(&key);
        keyArray.Add(key);
      }
    }
    keyArray.QuickSort();
    rv = SortMessagesBasedOnKey(messages, &keyArray, srcFolder);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIStreamListener> streamListener(do_QueryInterface(copyStreamListener));
    if(!streamListener)
      return NS_ERROR_NO_INTERFACE;
    mCopyState->m_curCopyIndex = 0; 
    // we need to kick off the first message - subsequent messages
    // are kicked off by nsMailboxProtocol when it finishes a message
    // before starting the next message. Only do this if the source folder
    // is a local folder, however. IMAP will handle calling StartMessage for
    // each message that gets downloaded, and news doesn't go through here
    // because news only downloads one message at a time, and this routine
    // is for multiple message copy.
    nsCOMPtr <nsIMsgLocalMailFolder> srcLocalFolder = do_QueryInterface(srcFolder);
    if (srcLocalFolder)
      StartMessage();
    mCopyState->m_messageService->CopyMessages(&keyArray, srcFolder, streamListener, isMove,
      nsnull, aMsgWindow, nsnull);
  }
  
  return rv;
}

nsresult nsMsgLocalMailFolder::CopyMessageTo(nsISupports *message, 
                                             nsIMsgFolder *dstFolder /* dst same as "this" */,
                                             nsIMsgWindow *aMsgWindow,
                                             PRBool isMove)
{
  if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv;
  nsCOMPtr<nsIMsgDBHdr> msgHdr(do_QueryInterface(message));
  if(!msgHdr)
    return NS_ERROR_FAILURE;
  
  mCopyState->m_message = do_QueryInterface(msgHdr, &rv);
  
  nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryInterface(mCopyState->m_srcSupport));
  if(!srcFolder)
    return NS_ERROR_NO_INTERFACE;
  nsXPIDLCString uri;
  srcFolder->GetUriForMsg(msgHdr, getter_Copies(uri));
  
  nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener = do_CreateInstance(NS_COPYMESSAGESTREAMLISTENER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsICopyMessageListener> copyListener(do_QueryInterface(dstFolder));
  if(!copyListener)
    return NS_ERROR_NO_INTERFACE;
  
  rv = copyStreamListener->Init(srcFolder, copyListener, nsnull);
  if(NS_FAILED(rv))
    return rv;
  
  if (!mCopyState->m_messageService)
  {
    rv = GetMessageServiceFromURI(uri, getter_AddRefs(mCopyState->m_messageService));
  }
  
  if (NS_SUCCEEDED(rv) && mCopyState->m_messageService)
  {
    nsCOMPtr<nsIStreamListener>
      streamListener(do_QueryInterface(copyStreamListener));
    if(!streamListener)
      return NS_ERROR_NO_INTERFACE;
    mCopyState->m_messageService->CopyMessage(uri, streamListener, isMove,
      nsnull, aMsgWindow, nsnull);
  }
  
  return rv;
}

// A message is being deleted from a POP3 mail file, so check and see if we have the message
// being deleted in the server. If so, then we need to remove the message from the server as well.
// We have saved the UIDL of the message in the popstate.dat file and we must match this uidl, so
// read the message headers and see if we have it, then mark the message for deletion from the server.
// The next time we look at mail the message will be deleted from the server.

NS_IMETHODIMP
nsMsgLocalMailFolder::MarkMsgsOnPop3Server(nsISupportsArray *aMessages, PRInt32 aMark)
{
  nsLocalFolderScanState folderScanState;
  nsCOMPtr<nsIPop3IncomingServer> curFolderPop3MailServer;
  nsCOMPtr<nsIFileSpec> mailboxSpec;
  nsFileSpec realSpec;
  nsCOMArray<nsIPop3IncomingServer> pop3Servers; // servers with msgs deleted...

  nsCOMPtr<nsIMsgIncomingServer> incomingServer;
  nsresult rv = GetServer(getter_AddRefs(incomingServer));
  if (!incomingServer) 
    return NS_MSG_INVALID_OR_MISSING_SERVER;

  nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // I wonder if we should run through the pop3 accounts and see if any of them have
  // leave on server set. If not, we could short-circuit some of this.

  curFolderPop3MailServer = do_QueryInterface(incomingServer, &rv);
  rv = GetPath(getter_AddRefs(mailboxSpec));
  NS_ENSURE_SUCCESS(rv, rv);

  mailboxSpec->GetFileSpec(&realSpec);
  folderScanState.m_fileSpec = &realSpec;
  rv = GetFolderScanState(&folderScanState);
  NS_ENSURE_SUCCESS(rv,rv);
  
  PRUint32 srcCount;
  aMessages->Count(&srcCount);
  
  // Filter delete requests are always honored, others are subject
  // to the deleteMailLeftOnServer preference.
  PRInt32 mark;
  mark = (aMark == POP3_FORCE_DEL) ? POP3_DELETE : aMark;

  for (PRUint32 i = 0; i < srcCount; i++)
  {
    /* get uidl for this message */
    nsCOMPtr<nsIMsgDBHdr> msgDBHdr (do_QueryElementAt(aMessages, i, &rv));
    
    PRUint32 flags = 0;

    if (msgDBHdr)
    {
      msgDBHdr->GetFlags(&flags);
      nsCOMPtr <nsIPop3IncomingServer> msgPop3Server = curFolderPop3MailServer;
      PRBool leaveOnServer = PR_FALSE;
      PRBool deleteMailLeftOnServer = PR_FALSE;
      // set up defaults, in case there's no x-mozilla-account header
      if (curFolderPop3MailServer)
      {
        curFolderPop3MailServer->GetDeleteMailLeftOnServer(&deleteMailLeftOnServer);
        curFolderPop3MailServer->GetLeaveMessagesOnServer(&leaveOnServer);
      }

      rv = GetUidlFromFolder(&folderScanState, msgDBHdr);
      if (!NS_SUCCEEDED(rv))
        continue;

      if (folderScanState.m_uidl)
      {
        nsCOMPtr <nsIMsgAccount> account;
        rv = accountManager->GetAccount(folderScanState.m_accountKey.get(), getter_AddRefs(account));
        if (NS_SUCCEEDED(rv) && account)
        {
          account->GetIncomingServer(getter_AddRefs(incomingServer));
          nsCOMPtr<nsIPop3IncomingServer> curMsgPop3MailServer = do_QueryInterface(incomingServer);
          if (curMsgPop3MailServer)
          {
            msgPop3Server = curMsgPop3MailServer;
            msgPop3Server->GetDeleteMailLeftOnServer(&deleteMailLeftOnServer);
            msgPop3Server->GetLeaveMessagesOnServer(&leaveOnServer);
          }
        }
      }
      // ignore this header if not partial and leaveOnServer not set...
      if (! (flags & MSG_FLAG_PARTIAL) && !leaveOnServer)
        continue;
      // if marking deleted, ignore header if we're not deleting from
      // server when deleting locally.
      if (aMark == POP3_DELETE && leaveOnServer && !deleteMailLeftOnServer)
        continue;
      if (folderScanState.m_uidl)
      {
        msgPop3Server->AddUidlToMark(folderScanState.m_uidl, mark);
        // remember this pop server in list of servers with msgs deleted
        if (pop3Servers.IndexOfObject(msgPop3Server) == kNotFound)
          pop3Servers.AppendObject(msgPop3Server);
      }
    }
  }

  // need to do this for all pop3 mail servers that had messages deleted.
  PRUint32 serverCount = pop3Servers.Count();
  for (PRUint32 index = 0; index < serverCount; index++)
    pop3Servers[index]->MarkMessages();

  mailboxSpec->CloseStream();
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::DeleteDownloadMsg(nsIMsgDBHdr *aMsgHdr,
  PRBool *aDoSelect)
{
  PRUint32 numMsgs;
  char *newMsgId;

  // This method is only invoked thru DownloadMessagesForOffline()
  if (mDownloadState != DOWNLOAD_STATE_NONE)
  {
    // We only remember the first key, no matter how many
    // messages were originally selected.
    if (mDownloadState == DOWNLOAD_STATE_INITED)
    {
      aMsgHdr->GetMessageKey(&mDownloadSelectKey);
      mDownloadState = DOWNLOAD_STATE_GOTMSG;
    }
  
    aMsgHdr->GetMessageId(&newMsgId);
  
    // Walk through all the selected headers, looking for a matching
    // Message-ID.
    mDownloadMessages->Count(&numMsgs);
    for (PRUint32 i = 0; i < numMsgs; i++)
    {
      nsresult rv;
      nsCOMPtr<nsIMsgDBHdr> msgDBHdr (do_QueryElementAt(mDownloadMessages, i, &rv));
      char *oldMsgId = nsnull;
      msgDBHdr->GetMessageId(&oldMsgId);
  
      // Delete the first match and remove it from the array
      if (!PL_strcmp(newMsgId, oldMsgId))
      {
#if DOWNLOAD_NOTIFY_STYLE == DOWNLOAD_NOTIFY_LAST
        msgDBHdr->GetMessageKey(&mDownloadOldKey);
        msgDBHdr->GetThreadParent(&mDownloadOldParent);
        msgDBHdr->GetFlags(&mDownloadOldFlags);
        mDatabase->DeleteHeader(msgDBHdr, nsnull, PR_FALSE, PR_FALSE);
        // Tell caller we want to select this message
        if (aDoSelect)
          *aDoSelect = PR_TRUE;
#else
        mDatabase->DeleteHeader(msgDBHdr, nsnull, PR_FALSE, PR_TRUE);
        // Tell caller we want to select this message
        if (aDoSelect && mDownloadState == DOWNLOAD_STATE_GOTMSG)
          *aDoSelect = PR_TRUE;
#endif
        mDownloadMessages->DeleteElementAt(i);
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::SelectDownloadMsg()
{

#if DOWNLOAD_NOTIFY_STYLE == DOWNLOAD_NOTIFY_LAST
  if (mDownloadState >= DOWNLOAD_STATE_GOTMSG)
    mDatabase->NotifyKeyDeletedAll(mDownloadOldKey, mDownloadOldParent, mDownloadOldFlags, nsnull);
#endif

  if (mDownloadState == DOWNLOAD_STATE_GOTMSG && mDownloadWindow)
  {
    nsCAutoString newuri;
    nsBuildLocalMessageURI(mBaseMessageURI, mDownloadSelectKey, newuri);
    mDownloadWindow->SelectMessage(newuri.get());
    mDownloadState = DOWNLOAD_STATE_DIDSEL;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::DownloadMessagesForOffline(
	nsISupportsArray *aMessages, nsIMsgWindow *aWindow)
{
  if (mDownloadState != DOWNLOAD_STATE_NONE) 
    return NS_ERROR_FAILURE; // already has a download in progress

  // We're starting a download...
  mDownloadState = DOWNLOAD_STATE_INITED;

  MarkMsgsOnPop3Server(aMessages, POP3_FETCH_BODY);

  // Pull out all the PARTIAL messages into a new array
  PRUint32 srcCount;
  aMessages->Count(&srcCount);

  NS_NewISupportsArray(getter_AddRefs(mDownloadMessages));
  for (PRUint32 i = 0; i < srcCount; i++)
  {
    nsresult rv;
    nsCOMPtr<nsIMsgDBHdr> msgDBHdr (do_QueryElementAt(aMessages, i, &rv));
    if (msgDBHdr)
    {
      PRUint32 flags = 0;
      msgDBHdr->GetFlags(&flags);
      if (flags & MSG_FLAG_PARTIAL)
      {
        mDownloadMessages->AppendElement(msgDBHdr);
      }
    }
  }
  mDownloadWindow = aWindow;

  return GetNewMessages(aWindow, this);
}

// TODO:  once we move certain code into the IncomingServer (search for TODO)
// this method will go away.
// sometimes this gets called when we don't have the server yet, so
// that's why we're not calling GetServer()
const char *
nsMsgLocalMailFolder::GetIncomingServerType()
{
  nsresult rv;

  if (mType) return mType;

  // this is totally hacky - we have to re-parse the URI, then
  // guess at "none" or "pop3"
  // if anyone has any better ideas mail me! -alecf@netscape.com
  nsCOMPtr<nsIURL> url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return "";

  rv = url->SetSpec(nsDependentCString(mURI));
  if (NS_FAILED(rv)) return "";

  nsCAutoString userPass;
  rv = url->GetUserPass(userPass);
  if (NS_FAILED(rv)) return "";
  if (!userPass.IsEmpty())
    nsUnescape(userPass.BeginWriting());

  nsCAutoString hostName;
  rv = url->GetAsciiHost(hostName);
  if (NS_FAILED(rv)) return "";
  if (!hostName.IsEmpty())
	nsUnescape(hostName.BeginWriting());

  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return "";

  nsCOMPtr<nsIMsgIncomingServer> server;

  // try "none" first
  rv = accountManager->FindServer(userPass.get(),
                                  hostName.get(),
                                  "none",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) 
  {
    mType = "none";
    return mType;
  }

  // next try "pop3"
  rv = accountManager->FindServer(userPass.get(),
                                  hostName.get(),
                                  "pop3",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) 
  {
    mType = "pop3";
    return mType;
  }

#ifdef HAVE_MOVEMAIL
  // next try "movemail"
  rv = accountManager->FindServer(userPass.get(),
                                  hostName.get(),
                                  "movemail",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) 
  {
    mType = "movemail";
    return mType;
  }
#endif /* HAVE_MOVEMAIL */

  return "";
}

nsresult nsMsgLocalMailFolder::CreateBaseMessageURI(const char *aURI)
{
  return nsCreateLocalBaseMessageURI(aURI, &mBaseMessageURI);
}

NS_IMETHODIMP
nsMsgLocalMailFolder::OnStartRunningUrl(nsIURI * aUrl)
{
  nsresult rv;
  nsCOMPtr<nsIPop3URL> popurl = do_QueryInterface(aUrl, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString aSpec;
    aUrl->GetSpec(aSpec);
    if (strstr(aSpec.get(), "uidl="))
    {
      nsCOMPtr<nsIPop3Sink> popsink;
      rv = popurl->GetPop3Sink(getter_AddRefs(popsink));
      if (NS_SUCCEEDED(rv))
        popsink->SetBaseMessageUri(mBaseMessageURI);
    }
  }
  return nsMsgDBFolder::OnStartRunningUrl(aUrl);
}

NS_IMETHODIMP
nsMsgLocalMailFolder::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
  // If we just finished a DownloadMessages call, reset...
  if (mDownloadState != DOWNLOAD_STATE_NONE)
  {
    mDownloadState = DOWNLOAD_STATE_NONE;
    mDownloadMessages = nsnull;
    mDownloadWindow = nsnull;
    return nsMsgDBFolder::OnStopRunningUrl(aUrl, aExitCode);
  }
  if (NS_SUCCEEDED(aExitCode))
  {
    nsresult rv;
    nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;	
    nsCOMPtr<nsIMsgWindow> msgWindow;
    rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
    nsCAutoString aSpec;
    aUrl->GetSpec(aSpec);
    
    if (strstr(aSpec.get(), "uidl="))
    {
      nsCOMPtr<nsIPop3URL> popurl = do_QueryInterface(aUrl, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsXPIDLCString messageuri;
        rv = popurl->GetMessageUri(getter_Copies(messageuri));
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIRDFService> rdfService = 
                   do_GetService("@mozilla.org/rdf/rdf-service;1", &rv); 
          if(NS_SUCCEEDED(rv))
          {
            nsCOMPtr <nsIMsgDBHdr> msgDBHdr;
            rv = GetMsgDBHdrFromURI(messageuri, getter_AddRefs(msgDBHdr));
            if(NS_SUCCEEDED(rv))
                rv = mDatabase->DeleteHeader(msgDBHdr, nsnull, PR_TRUE,
                                             PR_TRUE);
            nsCOMPtr<nsIPop3Sink> pop3sink;
            nsXPIDLCString newMessageUri;
            rv = popurl->GetPop3Sink(getter_AddRefs(pop3sink));
            if (NS_SUCCEEDED(rv))
            {
              pop3sink->GetMessageUri(getter_Copies(newMessageUri));
              if(msgWindow)
              {
                msgWindow->SelectMessage(newMessageUri);
              }
            }
          }
        }
      }
    }
    
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
    {
      // if we are the inbox and running pop url 
      nsCOMPtr<nsIPop3URL> popurl = do_QueryInterface(aUrl, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr<nsIMsgIncomingServer> server;
        GetServer(getter_AddRefs(server));
        if (server)
          server->SetPerformingBiff(PR_FALSE);  //biff is over
      }
      if (mDatabase)
      {
        if (mCheckForNewMessagesAfterParsing)
        {
          PRBool valid;
          mDatabase->GetSummaryValid(&valid);
          if (valid)
          {
            if (msgWindow)
             rv = GetNewMessages(msgWindow, nsnull);
          }
          mCheckForNewMessagesAfterParsing = PR_FALSE;
        }
      }
    }
  }

  m_parsingFolder = PR_FALSE;
  return nsMsgDBFolder::OnStopRunningUrl(aUrl, aExitCode);
}

nsresult nsMsgLocalMailFolder::DisplayMoveCopyStatusMsg()
{
  nsresult rv = NS_OK;
  if (mCopyState)
  {
    if (!mCopyState->m_statusFeedback)
    {
      // get msgWindow from undo txn
      nsCOMPtr<nsIMsgWindow> msgWindow;
	    nsresult rv;

      if (mCopyState->m_undoMsgTxn)
      {
	      nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;
		    localUndoTxn = do_QueryInterface(mCopyState->m_undoMsgTxn, &rv);
        if (NS_SUCCEEDED(rv))
          localUndoTxn->GetMsgWindow(getter_AddRefs(msgWindow));
      NS_ASSERTION(msgWindow, "no msg window");
      }
      if (!msgWindow)
        return NS_OK; // not a fatal error.
      msgWindow->GetStatusFeedback(getter_AddRefs(mCopyState->m_statusFeedback));
    }

    if (!mCopyState->m_stringBundle)
    {
      nsCOMPtr <nsIMsgStringService> stringService = do_GetService(NS_MSG_MAILBOXSTRINGSERVICE_CONTRACTID);

      rv = stringService->GetBundle(getter_AddRefs(mCopyState->m_stringBundle));

      NS_ASSERTION(NS_SUCCEEDED(rv), "GetBundle failed");
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (mCopyState->m_statusFeedback && mCopyState->m_stringBundle)
    {
      nsXPIDLString finalString;
      nsXPIDLString folderName;
      GetName(getter_Copies(folderName));
      PRInt32 statusMsgId = (mCopyState->m_isMove) ? MOVING_MSGS_STATUS : COPYING_MSGS_STATUS;
      nsAutoString numMsgSoFarString;
      numMsgSoFarString.AppendInt((mCopyState->m_copyingMultipleMessages) ? mCopyState->m_curCopyIndex : 1);

      nsAutoString totalMessagesString;
      totalMessagesString.AppendInt(mCopyState->m_totalMsgCount);

      const PRUnichar * stringArray[] = { numMsgSoFarString.get(), totalMessagesString.get(), folderName.get() };
      rv = mCopyState->m_stringBundle->FormatStringFromID(statusMsgId, stringArray, 3,
                                               getter_Copies(finalString));
      PRInt64 minIntervalBetweenProgress;
      PRInt64 nowMS = LL_ZERO;

      // only update status/progress every half second
      LL_I2L(minIntervalBetweenProgress, 500);
      PRInt64 diffSinceLastProgress;
      LL_I2L(nowMS, PR_IntervalToMilliseconds(PR_IntervalNow()));
      LL_SUB(diffSinceLastProgress, nowMS, mCopyState->m_lastProgressTime); // r = a - b
      LL_SUB(diffSinceLastProgress, diffSinceLastProgress, minIntervalBetweenProgress); // r = a - b
      if (!LL_GE_ZERO(diffSinceLastProgress) && mCopyState->m_curCopyIndex < mCopyState->m_totalMsgCount)
        return NS_OK;

      mCopyState->m_lastProgressTime = nowMS;
      mCopyState->m_statusFeedback->ShowStatusString(finalString);
      mCopyState->m_statusFeedback->ShowProgress(mCopyState->m_curCopyIndex * 100 / mCopyState->m_totalMsgCount);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::SetFlagsOnDefaultMailboxes(PRUint32 flags)
{
  if (flags & MSG_FOLDER_FLAG_INBOX)
    setSubfolderFlag(NS_LITERAL_STRING("Inbox").get(), MSG_FOLDER_FLAG_INBOX);

  if (flags & MSG_FOLDER_FLAG_SENTMAIL)
    setSubfolderFlag(NS_LITERAL_STRING("Sent").get(), MSG_FOLDER_FLAG_SENTMAIL);
  
  if (flags & MSG_FOLDER_FLAG_DRAFTS)
    setSubfolderFlag(NS_LITERAL_STRING("Drafts").get(), MSG_FOLDER_FLAG_DRAFTS);

  if (flags & MSG_FOLDER_FLAG_TEMPLATES)
    setSubfolderFlag(NS_LITERAL_STRING("Templates").get(), MSG_FOLDER_FLAG_TEMPLATES);
  
  if (flags & MSG_FOLDER_FLAG_TRASH)
    setSubfolderFlag(NS_LITERAL_STRING("Trash").get(), MSG_FOLDER_FLAG_TRASH);

  if (flags & MSG_FOLDER_FLAG_QUEUE)
    setSubfolderFlag(NS_LITERAL_STRING("Unsent Messages").get(), MSG_FOLDER_FLAG_QUEUE);
	
  if (flags & MSG_FOLDER_FLAG_JUNK)
    setSubfolderFlag(NS_LITERAL_STRING("Junk").get(), MSG_FOLDER_FLAG_JUNK);

  return NS_OK;
}

nsresult
nsMsgLocalMailFolder::setSubfolderFlag(const PRUnichar *aFolderName,
                                       PRUint32 flags)
{
  // FindSubFolder() expects the folder name to be escaped
  // see bug #192043
  nsCAutoString escapedFolderName;
  nsresult rv = NS_MsgEscapeEncodeURLPath(nsDependentString(aFolderName), escapedFolderName);
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr<nsIMsgFolder> msgFolder;
  rv = FindSubFolder(escapedFolderName, getter_AddRefs(msgFolder));
  
  if (NS_FAILED(rv)) 
    return rv;
  if (!msgFolder) 
    return NS_ERROR_FAILURE;
  
  rv = msgFolder->SetFlag(flags);
  if (NS_FAILED(rv)) 
    return rv;
  
  msgFolder->SetPrettyName(aFolderName);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetCheckForNewMessagesAfterParsing(PRBool *aCheckForNewMessagesAfterParsing)
{
  NS_ENSURE_ARG(aCheckForNewMessagesAfterParsing);
  *aCheckForNewMessagesAfterParsing = mCheckForNewMessagesAfterParsing;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::SetCheckForNewMessagesAfterParsing(PRBool aCheckForNewMessagesAfterParsing)
{
  mCheckForNewMessagesAfterParsing = aCheckForNewMessagesAfterParsing;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::NotifyCompactCompleted()
{
  (void) RefreshSizeOnDisk();
  (void) CloseDBIfFolderNotOpen();
  nsCOMPtr <nsIAtom> compactCompletedAtom;
  compactCompletedAtom = do_GetAtom("CompactCompleted");
  NotifyFolderEvent(compactCompletedAtom);
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Shutdown(PRBool shutdownChildren)
{
  mInitialized = PR_FALSE;
  return nsMsgDBFolder::Shutdown(shutdownChildren);
}

nsresult
nsMsgLocalMailFolder::SpamFilterClassifyMessage(const char *aURI, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  ++mNumFilterClassifyRequests;
  return aJunkMailPlugin->ClassifyMessage(aURI, aMsgWindow, this);   
}

nsresult
nsMsgLocalMailFolder::SpamFilterClassifyMessages(const char **aURIArray, PRUint32 aURICount, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  mNumFilterClassifyRequests += aURICount;
  return aJunkMailPlugin->ClassifyMessages(aURICount, aURIArray, aMsgWindow, this);   
}


NS_IMETHODIMP
nsMsgLocalMailFolder::OnMessageClassified(const char *aMsgURI, nsMsgJunkStatus aClassification)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = GetMsgDBHdrFromURI(aMsgURI, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsMsgKey msgKey;
  rv = msgHdr->GetMessageKey(&msgKey);
  NS_ENSURE_SUCCESS(rv, rv);

  mDatabase->SetStringProperty(msgKey, "junkscore", (aClassification == nsIJunkMailPlugin::JUNK) ? "100" : "0");
  mDatabase->SetStringProperty(msgKey, "junkscoreorigin", "plugin");

  nsCOMPtr<nsISpamSettings> spamSettings;
  PRBool moveOnSpam = PR_FALSE;
  
  rv = GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv); 
  
  rv = server->GetSpamSettings(getter_AddRefs(spamSettings));
  NS_ENSURE_SUCCESS(rv, rv); 

  if (aClassification == nsIJunkMailPlugin::JUNK)
  {
    PRBool markAsReadOnSpam;
    (void)spamSettings->GetMarkAsReadOnSpam(&markAsReadOnSpam);
    if (markAsReadOnSpam)
    {
      rv = mDatabase->MarkRead(msgKey, true, this);
        if (!NS_SUCCEEDED(rv))
          NS_WARNING("failed marking spam message as read");
    }

    PRBool willMoveMessage = PR_FALSE;

    // don't do the move when we are opening up 
    // the junk mail folder or the trash folder
    // or when manually classifying messages in those folders
    if (!(mFlags & MSG_FOLDER_FLAG_JUNK || mFlags & MSG_FOLDER_FLAG_TRASH))
    {
      rv = spamSettings->GetMoveOnSpam(&moveOnSpam);
      NS_ENSURE_SUCCESS(rv,rv);
      if (moveOnSpam)
      {
        nsXPIDLCString uriStr;
        rv = spamSettings->GetSpamFolderURI(getter_Copies(uriStr));
        NS_ENSURE_SUCCESS(rv,rv);
        mSpamFolderURI = uriStr;

        nsCOMPtr<nsIMsgFolder> folder;
        rv = GetExistingFolder(mSpamFolderURI.get(), getter_AddRefs(folder));
        if (NS_SUCCEEDED(rv) && folder) 
        {
          rv = folder->SetFlag(MSG_FOLDER_FLAG_JUNK);
          NS_ENSURE_SUCCESS(rv,rv);
          mSpamKeysToMove.Add(msgKey);
          willMoveMessage = PR_TRUE;
        }
        else
        {
          // XXX TODO
          // JUNK MAIL RELATED
          // the listener should do
          // rv = folder->SetFlag(MSG_FOLDER_FLAG_JUNK);
          // NS_ENSURE_SUCCESS(rv,rv);
          // mSpamKeysToMove.Add(msgKey);
          // willMoveMessage = PR_TRUE;
          rv = GetOrCreateFolder(mSpamFolderURI, nsnull /* aListener */);
          NS_ASSERTION(NS_SUCCEEDED(rv), "GetOrCreateFolder failed");
        }
      }
    }
    rv = spamSettings->LogJunkHit(msgHdr, willMoveMessage);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  if (--mNumFilterClassifyRequests == 0)
  {
    if (mSpamKeysToMove.GetSize() > 0)
    {
      if (!mSpamFolderURI.IsEmpty())
      {
        nsCOMPtr<nsIMsgFolder> folder;
        rv = GetExistingFolder(mSpamFolderURI.get(), getter_AddRefs(folder));
        if (NS_SUCCEEDED(rv) && folder) {      
          nsCOMPtr<nsISupportsArray> messages;
          NS_NewISupportsArray(getter_AddRefs(messages));
          for (PRUint32 keyIndex = 0; keyIndex < mSpamKeysToMove.GetSize(); keyIndex++)
          {
            nsCOMPtr<nsIMsgDBHdr> mailHdr = nsnull;
            rv = GetMessageHeader(mSpamKeysToMove.ElementAt(keyIndex), getter_AddRefs(mailHdr));
            if (NS_SUCCEEDED(rv) && mailHdr)
            {
              nsCOMPtr<nsISupports> iSupports = do_QueryInterface(mailHdr);
              messages->AppendElement(iSupports);
            }
          }
        
          nsCOMPtr<nsIMsgCopyService> copySvc = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
          NS_ENSURE_SUCCESS(rv,rv);
        
          rv = copySvc->CopyMessages(this, messages, folder, PR_TRUE,
            /*nsIMsgCopyServiceListener* listener*/ nsnull, nsnull, PR_FALSE /*allowUndo*/);
          NS_ASSERTION(NS_SUCCEEDED(rv), "CopyMessages failed");
        }
      }
    }
    PRInt32 numNewMessages;
    GetNumNewMessages(PR_FALSE, &numNewMessages);
    SetNumNewMessages(numNewMessages - mSpamKeysToMove.GetSize());
    mSpamKeysToMove.RemoveAll();
    // check if this is the inbox first...
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
      PerformBiffNotifications();

  }

  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetFolderScanState(nsLocalFolderScanState *aState)
{
  nsresult rv;

  NS_FileSpecToIFile(aState->m_fileSpec, getter_AddRefs(aState->m_localFile));
  aState->m_fileStream = do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aState->m_fileStream->Init(aState->m_localFile, PR_RDONLY, 0664, PR_FALSE);
  if (NS_SUCCEEDED(rv))
  {
    aState->m_inputStream = do_QueryInterface(aState->m_fileStream);
    aState->m_seekableStream = do_QueryInterface(aState->m_inputStream);
    aState->m_fileLineStream = do_QueryInterface(aState->m_inputStream);
    aState->m_uidl = nsnull;
  }

  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetUidlFromFolder(nsLocalFolderScanState *aState,
       nsIMsgDBHdr *aMsgDBHdr)
{
  nsresult rv;
  PRUint32 messageOffset;
  PRBool more = PR_FALSE;
  PRUint32 size = 0, len = 0;
  const char *accountKey = nsnull;

  aMsgDBHdr->GetMessageOffset(&messageOffset);
  rv = aState->m_seekableStream->Seek(PR_SEEK_SET, messageOffset);
  NS_ENSURE_SUCCESS(rv,rv);

  aState->m_uidl = nsnull;

  aMsgDBHdr->GetMessageSize(&len);
  while (len > 0)
  {
    rv = aState->m_fileLineStream->ReadLine(aState->m_header, &more);
    if (NS_SUCCEEDED(rv))
    {
      size = aState->m_header.Length();
      if (!size)
        break;
      len -= size;
      // account key header will always be before X_UIDL header
      if (!accountKey)
      {
        accountKey = strstr(aState->m_header.get(), HEADER_X_MOZILLA_ACCOUNT_KEY);
        if (accountKey)
	{
          accountKey += strlen(HEADER_X_MOZILLA_ACCOUNT_KEY) + 2;
	  aState->m_accountKey = accountKey;
	}
      } else
      {
        aState->m_uidl = strstr(aState->m_header.get(), X_UIDL);
        if (aState->m_uidl)
        {
          aState->m_uidl += X_UIDL_LEN + 2; // skip UIDL: header
          break;
        }
      }
    }
  }
  return rv;
}

  // this adds a message to the end of the folder, parsing it as it goes, and
  // applying filters, if applicable.
NS_IMETHODIMP
nsMsgLocalMailFolder::AddMessage(const char *aMessage)
{
  nsCOMPtr<nsIFileSpec> pathSpec;
  nsresult rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;
  
  nsFileSpec fileSpec;
  rv = pathSpec->GetFileSpec(&fileSpec);
  if (NS_FAILED(rv)) return rv;
  
  nsIOFileStream outFileStream(fileSpec);
  outFileStream.seek(fileSpec.GetFileSize());
      
  // create a new mail parser
  nsRefPtr<nsParseNewMailState> newMailParser = new nsParseNewMailState;
  if (newMailParser == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isLocked;
  
  GetLocked(&isLocked);
  if(!isLocked)
    AcquireSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
  else
    return NS_MSG_FOLDER_BUSY;


  rv = newMailParser->Init(rootFolder, this, 
                           fileSpec, &outFileStream, nsnull);
  if (NS_SUCCEEDED(rv))
  {

//  in_server->SetServerBusy(PR_TRUE);

    outFileStream << aMessage;
    newMailParser->BufferInput(aMessage, strlen(aMessage));

    outFileStream.flush();
    newMailParser->OnStopRequest(nsnull, nsnull, NS_OK);
    newMailParser->SetDBFolderStream(nsnull); // stream is going away
    if (outFileStream.is_open())
        outFileStream.close();
  }
  ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
  return rv;
}

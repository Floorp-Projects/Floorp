/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Lorenzo Colitti <lorenzo@colitti.com>
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

#include "msgCore.h"

#include "nsMsgImapCID.h"
#include "nsImapMailFolder.h"
#include "nsIEnumerator.h"
#include "nsILocalFile.h"
#include "nsIFolderListener.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsMsgFolderFlags.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsImapFlagAndUidState.h"
#include "nsIEventQueueService.h"
#include "nsIImapUrl.h"
#include "nsImapUtils.h"
#include "nsMsgUtils.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsImapUndoTxn.h"
#include "nsIIMAPHostSessionList.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"
#include "nsImapStringBundle.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsTextFormatter.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsMsgI18N.h"
#include "nsICacheSession.h"
#include "nsEscape.h"
#include "nsIDOMWindowInternal.h"
#include "nsIMsgFilter.h"
#include "nsImapMoveCoalescer.h"
#include "nsIPrompt.h"
#include "nsIPromptService.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsSpecialSystemDirectory.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIImapFlagAndUidState.h"
#include "nsIImapHeaderXferInfo.h"
#include "nsIMessenger.h"
#include "nsIMsgSearchAdapter.h"
#include "nsIImapMockChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsIMsgOfflineImapOperation.h"
#include "nsImapOfflineSync.h"
#include "nsIMsgAccountManager.h"
#include "nsQuickSort.h"
#include "nsIImapMockChannel.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsIMAPNamespace.h"
#include "nsHashtable.h"
#include "nsIMsgFolderCompactor.h"
#include "nsMsgMessageFlags.h"
#include "nsIMimeHeaders.h"
#include "nsIMsgMdnGenerator.h"
#include "nsISpamSettings.h"
#include "nsInt64.h"
#include <time.h>

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kParseMailMsgStateCID, NS_PARSEMAILMSGSTATE_CID);
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);

nsIAtom* nsImapMailFolder::mImapHdrDownloadedAtom=nsnull;

#define FOUR_K 4096
#define MAILNEWS_CUSTOM_HEADERS "mailnews.customHeaders"

/*
    Copies the contents of srcDir into destDir.
    destDir will be created if it doesn't exist.
*/

static
nsresult RecursiveCopy(nsIFile* srcDir, nsIFile* destDir)
{
  nsresult rv;
  PRBool isDir;
  
  rv = srcDir->IsDirectory(&isDir);
  if (NS_FAILED(rv)) return rv;
  if (!isDir) return NS_ERROR_INVALID_ARG;
  
  PRBool exists;
  rv = destDir->Exists(&exists);
  if (NS_SUCCEEDED(rv) && !exists)
    rv = destDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
  if (NS_FAILED(rv)) return rv;
  
  PRBool hasMore = PR_FALSE;
  nsCOMPtr<nsISimpleEnumerator> dirIterator;
  rv = srcDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
  if (NS_FAILED(rv)) return rv;
  
  rv = dirIterator->HasMoreElements(&hasMore);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIFile> dirEntry;
  
  while (hasMore)
  {
    rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
    if (NS_SUCCEEDED(rv))
    {
      rv = dirEntry->IsDirectory(&isDir);
      if (NS_SUCCEEDED(rv))
      {
        if (isDir)
        {
          nsCOMPtr<nsIFile> destClone;
          rv = destDir->Clone(getter_AddRefs(destClone));
          if (NS_SUCCEEDED(rv))
          {
            nsCOMPtr<nsILocalFile> newChild(do_QueryInterface(destClone));
            nsAutoString leafName;
            dirEntry->GetLeafName(leafName);
            newChild->AppendRelativePath(leafName);
            rv = newChild->Exists(&exists);
            if (NS_SUCCEEDED(rv) && !exists)
              rv = newChild->Create(nsIFile::DIRECTORY_TYPE, 0775);
            rv = RecursiveCopy(dirEntry, newChild);
          }
        }
        else
          rv = dirEntry->CopyTo(destDir, EmptyString());
      }
      
    }
    rv = dirIterator->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;
  }
  
  return rv;
}

nsImapMailFolder::nsImapMailFolder() :
    m_initialized(PR_FALSE),m_haveDiscoveredAllFolders(PR_FALSE),
    m_haveReadNameFromDB(PR_FALSE), 
    m_curMsgUid(0), m_nextMessageByteLength(0),
    m_urlRunning(PR_FALSE),
  m_verifiedAsOnlineFolder(PR_FALSE),
  m_explicitlyVerify(PR_FALSE),
  m_folderIsNamespace(PR_FALSE),
    m_folderNeedsSubscribing(PR_FALSE),
    m_folderNeedsAdded(PR_FALSE),
    m_folderNeedsACLListed(PR_TRUE),
    m_performingBiff(PR_FALSE),
    m_folderQuotaCommandIssued(PR_FALSE),
    m_folderQuotaDataIsValid(PR_FALSE),
    m_downloadMessageForOfflineUse(PR_FALSE),
    m_downloadingFolderForOfflineUse(PR_FALSE),
    m_folderQuotaUsedKB(0),
    m_folderQuotaMaxKB(0)
{
  MOZ_COUNT_CTOR(nsImapMailFolder); // double count these for now.

  if (mImapHdrDownloadedAtom == nsnull)
    mImapHdrDownloadedAtom = NS_NewAtom("ImapHdrDownloaded");
  m_appendMsgMonitor = nsnull;  // since we're not using this (yet?) make it null.
  // if we do start using it, it should be created lazily
  
  nsresult rv;
  
  // Get current thread envent queue
  nsCOMPtr<nsIEventQueueService> pEventQService = 
    do_GetService(kEventQueueServiceCID, &rv); 
  if (NS_SUCCEEDED(rv) && pEventQService)
    pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
    getter_AddRefs(m_eventQueue));
  m_moveCoalescer = nsnull;
  m_boxFlags = 0;
  m_uidValidity = kUidUnknown;
  m_numStatusRecentMessages = 0;
  m_numStatusUnseenMessages = 0;
  m_hierarchyDelimiter = kOnlineHierarchySeparatorUnknown;
  m_pathName = nsnull;
  m_folderACL = nsnull;
  m_aclFlags = 0;
  m_supportedUserFlags = 0;
  m_namespace = nsnull;
  m_numFilterClassifyRequests = 0; 
}

nsImapMailFolder::~nsImapMailFolder()
{
  MOZ_COUNT_DTOR(nsImapMailFolder);
    if (m_appendMsgMonitor)
        PR_DestroyMonitor(m_appendMsgMonitor);

  // I think our destructor gets called before the base class...
  if (mInstanceCount == 1)
    NS_IF_RELEASE(mImapHdrDownloadedAtom);
  NS_IF_RELEASE(m_moveCoalescer);
  delete m_pathName;
  delete m_folderACL;
}

NS_IMPL_ADDREF_INHERITED(nsImapMailFolder, nsMsgDBFolder)
NS_IMPL_RELEASE_INHERITED(nsImapMailFolder, nsMsgDBFolder)
NS_IMPL_QUERY_HEAD(nsImapMailFolder)
    NS_IMPL_QUERY_BODY(nsIMsgImapMailFolder)
    NS_IMPL_QUERY_BODY(nsICopyMessageListener)
    NS_IMPL_QUERY_BODY(nsIImapMailFolderSink)
    NS_IMPL_QUERY_BODY(nsIImapMessageSink)
    NS_IMPL_QUERY_BODY(nsIImapExtensionSink)
    NS_IMPL_QUERY_BODY(nsIImapMiscellaneousSink)
    NS_IMPL_QUERY_BODY(nsIUrlListener)
    NS_IMPL_QUERY_BODY(nsIMsgFilterHitNotify)
    NS_IMPL_QUERY_BODY(nsIJunkMailClassificationListener)
NS_IMPL_QUERY_TAIL_INHERITING(nsMsgDBFolder)


NS_IMETHODIMP nsImapMailFolder::Enumerate(nsIEnumerator* *result)
{
#if 0
    nsresult rv = NS_OK;
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

nsresult nsImapMailFolder::AddDirectorySeparator(nsFileSpec &path)
{
  nsresult rv = NS_OK;
  if (mURI.Equals(kImapRootURI)) 
  {
      // don't concat the full separator with .sbd
  }
  else 
  {
      nsAutoString sep;
      rv = nsGetMailFolderSeparator(sep);
      if (NS_FAILED(rv)) return rv;

      // see if there's a dir with the same name ending with .sbd
      // unfortunately we can't just say:
      //          path += sep;
      // here because of the way nsFileSpec concatenates
      nsAutoString str; str.AssignWithConversion(nsFilePath(path));
      str += sep;
      path = nsFilePath(str);
    }

  return rv;
}

static PRBool
nsShouldIgnoreFile(nsString& name)
{
    PRInt32 len = name.Length();
    if (len > 4 && name.RFind(".msf", PR_TRUE) == len -4)
    {
        name.SetLength(len-4); // truncate the string
        return PR_FALSE;
    }
    return PR_TRUE;
}

// this is only called for virtual folders, currently.
NS_IMETHODIMP nsImapMailFolder::AddSubfolder(const nsAString& aName,
                                   nsIMsgFolder** aChild)
{
  NS_ENSURE_ARG_POINTER(aChild);
  
  PRInt32 flags = 0;
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCAutoString uri(mURI);
  uri.Append('/');
  
  // convert name to imap modified utf7, like an imap server would
  nsCAutoString utfFolderName;
  rv = CopyUTF16toMUTF7(PromiseFlatString(aName), utfFolderName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  uri += utfFolderName.get();
  
  nsCOMPtr <nsIMsgFolder> msgFolder;
  rv = GetChildWithURI(uri.get(), PR_FALSE/*deep*/, PR_TRUE /*case Insensitive*/, getter_AddRefs(msgFolder));  
  if (NS_SUCCEEDED(rv) && msgFolder)
    return NS_MSG_FOLDER_EXISTS;
  
  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uri, getter_AddRefs(res));
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;
  
  folder->GetFlags((PRUint32 *)&flags);
  
  flags |= MSG_FOLDER_FLAG_MAIL;
  
  folder->SetParent(this);
  
  folder->SetFlags(flags);
  
  nsCOMPtr<nsISupports> supports = do_QueryInterface(folder);
  if(folder)
    mSubFolders->AppendElement(supports);
  NS_ADDREF(*aChild = folder);
  
  nsCOMPtr <nsIMsgImapMailFolder> imapChild = do_QueryInterface(*aChild);
  if (imapChild)
  {
    NS_LossyConvertUTF16toASCII folderCName(aName);
    imapChild->SetOnlineName(folderCName.get());
    imapChild->SetHierarchyDelimiter(m_hierarchyDelimiter);
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::AddSubfolderWithPath(nsAString& name, nsIFileSpec *dbPath, 
                                             nsIMsgFolder **child)
{
  if(!child)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv)); 

  if(NS_FAILED(rv))
    return rv;

  PRInt32 flags = 0;

  nsCAutoString uri = mURI + NS_LITERAL_CSTRING("/");
  AppendUTF16toUTF8(name, uri);

  //will make sure mSubFolders does not have duplicates because of bogus msf files.

  nsCOMPtr <nsIMsgFolder> msgFolder;
  rv = GetChildWithURI(uri.get(), PR_FALSE/*deep*/, PR_FALSE /*case Insensitive*/, getter_AddRefs(msgFolder));  
  if (NS_SUCCEEDED(rv) && msgFolder)
  {
    return NS_MSG_FOLDER_EXISTS;
  }
  
  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uri, getter_AddRefs(res));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;        

  folder->SetPath(dbPath);
    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder);

    folder->GetFlags((PRUint32 *)&flags);

    folder->SetParent(this);

  flags |= MSG_FOLDER_FLAG_MAIL;

   PRBool isServer;
   rv = GetIsServer(&isServer);

   PRInt32 pFlags;
   GetFlags ((PRUint32 *) &pFlags);
   PRBool isParentInbox = pFlags & MSG_FOLDER_FLAG_INBOX;
 
   //Only set these if these are top level children or parent is inbox

  if(NS_SUCCEEDED(rv))
  {
    if(isServer &&
       name.LowerCaseEqualsLiteral("inbox"))
      flags |= MSG_FOLDER_FLAG_INBOX;
    else if(isServer || isParentInbox) 
    {
      nsAutoString trashName;
      GetTrashFolderName(trashName);
      if (name.Equals(trashName))
        flags |= MSG_FOLDER_FLAG_TRASH;
    }
#if 0
    else if(name.LowerCaseEqualsLiteral("sent"))
      folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
    else if(name.LowerCaseEqualsLiteral("drafts"))
      folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
    else if (name.LowerCaseEqualsLiteral("templates"))
      folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
#endif
  }

    folder->SetFlags(flags);
  //at this point we must be ok and we don't want to return failure in case GetIsServer failed.
  rv = NS_OK;

  nsCOMPtr <nsISupports> supports = do_QueryInterface(folder);
  NS_ASSERTION(supports, "couldn't get isupports from imap folder");
  if (supports)
    mSubFolders->AppendElement(supports);
  *child = folder;
  NS_IF_ADDREF(*child);
  return rv;
}

nsresult nsImapMailFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;
  nsAutoString currentFolderNameStr;    // online name
  nsAutoString currentFolderDBNameStr;  // possibly munged name
  nsCOMPtr<nsIMsgFolder> child;
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsIImapIncomingServer> imapServer;

  if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
    imapServer = do_QueryInterface(server);

  PRBool isServer;
  rv = GetIsServer(&isServer);

  char *folderName;
  for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++) 
  {
    nsFileSpec currentFolderPath = dir.Spec();
    folderName = currentFolderPath.GetLeafName();
    currentFolderNameStr.AssignWithConversion(folderName);
    if (isServer && imapServer)
    {
      PRBool isPFC;
      imapServer->GetIsPFC(folderName, &isPFC);
      if (isPFC)
      {
        nsCOMPtr <nsIMsgFolder> pfcFolder;
        imapServer->GetPFC(PR_TRUE, getter_AddRefs(pfcFolder));
        continue;
      }
      // should check if this is the PFC
    }
    if (nsShouldIgnoreFile(currentFolderNameStr))
    {
      PL_strfree(folderName);
      continue;
    }

    // OK, here we need to get the online name from the folder cache if we can.
    // If we can, use that to create the sub-folder

    nsCOMPtr <nsIMsgFolderCacheElement> cacheElement;
    nsCOMPtr <nsIFileSpec> curFolder;
    nsCOMPtr <nsIFileSpec> dbFile;

    NS_NewFileSpecWithSpec(currentFolderPath, getter_AddRefs(dbFile));
    // don't strip off the .msf in currentFolderPath.
    currentFolderPath.SetLeafName(folderName);
    rv = NS_NewFileSpecWithSpec(currentFolderPath, getter_AddRefs(curFolder));

    currentFolderDBNameStr = currentFolderNameStr;
    nsAutoString utf7LeafName = currentFolderNameStr;

    if (NS_SUCCEEDED(rv) && curFolder)
    {
      rv = GetFolderCacheElemFromFileSpec(dbFile, getter_AddRefs(cacheElement));

      if (NS_SUCCEEDED(rv) && cacheElement)
      {
        nsXPIDLString unicodeName;
        nsXPIDLCString onlineFullUtf7Name;

        PRUint32 folderFlags;
        rv = cacheElement->GetInt32Property("flags", (PRInt32 *) &folderFlags);
        if (NS_SUCCEEDED(rv) && folderFlags & MSG_FOLDER_FLAG_VIRTUAL) //ignore virtual folders
          continue;
        PRInt32 hierarchyDelimiter;
        rv = cacheElement->GetInt32Property("hierDelim", &hierarchyDelimiter);
        if (NS_SUCCEEDED(rv) && hierarchyDelimiter == kOnlineHierarchySeparatorUnknown)
          continue; // ignore .msf files for folders with unknown delimiter.
        rv = cacheElement->GetStringProperty("onlineName", getter_Copies(onlineFullUtf7Name));
        if (NS_SUCCEEDED(rv) && onlineFullUtf7Name.get() && strlen(onlineFullUtf7Name.get()))
        {
          // Call ConvertFolderName() and HideFolderName() to do special folder name
          // mapping and hiding, if configured to do so. For example, need to hide AOL's
          // 'RECYCLE_OUT' & convert a few AOL folder names. Regular imap accounts
          // will do no-op in the calls
          if (imapServer)
          {
            PRBool hideFolder;
            rv = imapServer->HideFolderName(onlineFullUtf7Name.get(), &hideFolder);
            if (hideFolder)
              continue;	// skip this folder
            else
            {
              rv = imapServer->ConvertFolderName(onlineFullUtf7Name.get(), getter_Copies(unicodeName));
              if (NS_FAILED(rv)) {
                rv = CopyMUTF7toUTF16(onlineFullUtf7Name, unicodeName);
                if (NS_FAILED(rv))  // XXX Does this make sense? 
                  CopyASCIItoUTF16(onlineFullUtf7Name, unicodeName);
              }
            }
          }

          currentFolderNameStr.Assign(unicodeName);

          PRUnichar delimiter = 0;
          GetHierarchyDelimiter(&delimiter);
          PRInt32 leafPos = currentFolderNameStr.RFindChar(delimiter);
          if (leafPos > 0)
            currentFolderNameStr.Cut(0, leafPos + 1);

          // take the utf7 full online name, and determine the utf7 leaf name
          CopyASCIItoUTF16(onlineFullUtf7Name, utf7LeafName);
          leafPos = utf7LeafName.RFindChar(delimiter);
          if (leafPos > 0)
            utf7LeafName.Cut(0, leafPos + 1);
        }
      }
    }
      // make the imap folder remember the file spec it was created with.
    nsCAutoString leafName; leafName.AssignWithConversion(currentFolderDBNameStr);
    nsCOMPtr <nsIFileSpec> msfFileSpec;
    rv = NS_NewFileSpecWithSpec(currentFolderPath, getter_AddRefs(msfFileSpec));
    if (NS_SUCCEEDED(rv) && msfFileSpec)
    {
      // leaf name is the db name w/o .msf (nsShouldIgnoreFile strips it off)
      // so this trims the .msf off the file spec.
      msfFileSpec->SetLeafName(leafName.get());
    }
    // use the utf7 name as the uri for the folder.
    AddSubfolderWithPath(utf7LeafName, msfFileSpec, getter_AddRefs(child));
    if (child)
    {
      // use the unicode name as the "pretty" name. Set it so it won't be
      // automatically computed from the URI, which is in utf7 form.
      if (!currentFolderNameStr.IsEmpty())
        child->SetPrettyName(currentFolderNameStr.get());

    }
    PL_strfree(folderName);
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetSubFolders(nsIEnumerator* *result)
{
  PRBool isServer;
  nsresult rv = GetIsServer(&isServer);

  if (!m_initialized)
  {
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;

    nsFileSpec path;
    rv = pathSpec->GetFileSpec(&path);
    if (NS_FAILED(rv)) return rv;

    // host directory does not need .sbd tacked on
    if (NS_SUCCEEDED(rv) && !isServer)
      rv = AddDirectorySeparator(path);

    if(NS_FAILED(rv)) return rv;
    
    m_initialized = PR_TRUE;      // need to set this here to avoid infinite recursion from CreateSubfolders.
    // we have to treat the root folder specially, because it's name
    // doesn't end with .sbd

    PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
    if (path.IsDirectory()) 
    {
        newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
        if (!mIsServer)
          SetFlag(newFlags);
        rv = CreateSubFolders(path);
    }
    if (isServer)
    {
      PRUint32 numFolders = 0;
      nsCOMPtr <nsIMsgFolder> inboxFolder;

      rv = GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inboxFolder));
      if (NS_FAILED(rv) || numFolders == 0 || !inboxFolder)
      {
        // create an inbox if we don't have one.
        CreateClientSubfolderInfo("INBOX", kOnlineHierarchySeparatorUnknown,0, PR_TRUE);
      }
    }

    UpdateSummaryTotals(PR_FALSE);

    if (NS_FAILED(rv)) return rv;
  }
  rv = mSubFolders->Enumerate(result);
  return rv;
}

//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsImapMailFolder::GetDatabase(nsIMsgWindow *aMsgWindow)
{
  nsresult folderOpen = NS_OK;
  if (!mDatabase)
  {
    nsCOMPtr<nsIFileSpec> pathSpec;
    nsresult rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
    if (msgDBService)
      folderOpen = msgDBService->OpenFolderDB(this, PR_TRUE, PR_FALSE, getter_AddRefs(mDatabase));

    if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
      folderOpen = NS_OK;

    if(mDatabase)
    {
      if(mAddListener)
        mDatabase->AddListener(this);
      UpdateSummaryTotals(PR_TRUE);
    }
  }
  return folderOpen;
}

NS_IMETHODIMP
nsImapMailFolder::UpdateFolder(nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  PRBool selectFolder = PR_FALSE;
  
  if (mFlags & MSG_FOLDER_FLAG_INBOX && !m_filterList) 
      rv = GetFilterList(msgWindow, getter_AddRefs(m_filterList));
  
  if (m_filterList) 
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get server");
    
    PRBool canFileMessagesOnServer = PR_TRUE;
    if (server) 
    {
      rv = server->GetCanFileMessagesOnServer(&canFileMessagesOnServer);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to determine if we could file messages on this server");
    }
    
    // the mdn filter is for filing return receipts into the sent folder
    // some servers (like AOL mail servers)
    // can't file to the sent folder, so we don't add the filter for those servers
    if (canFileMessagesOnServer) 
    {
      rv = server->ConfigureTemporaryFilters(m_filterList);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to add temp filters");
    }
  }
  
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv); 
  if (NS_FAILED(rv)) return rv;
  
  selectFolder = PR_TRUE;
  
  PRBool isServer;
  rv = GetIsServer(&isServer);
  if (NS_SUCCEEDED(rv) && isServer)
  {
    if (!m_haveDiscoveredAllFolders)
    {
      PRBool hasSubFolders = PR_FALSE;
      GetHasSubFolders(&hasSubFolders);
      if (!hasSubFolders)
      {
        rv = CreateClientSubfolderInfo("Inbox", kOnlineHierarchySeparatorUnknown,0, PR_FALSE);
        if (NS_FAILED(rv)) 
          return rv;
      }
      m_haveDiscoveredAllFolders = PR_TRUE;
    }
    selectFolder = PR_FALSE;
  }
  rv = GetDatabase(msgWindow);
  
  PRBool canOpenThisFolder = PR_TRUE;
  GetCanIOpenThisFolder(&canOpenThisFolder);
  
  PRBool hasOfflineEvents = PR_FALSE;
  GetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS, &hasOfflineEvents);
  
  if (!WeAreOffline())
  {
    if (hasOfflineEvents)
    {
      nsImapOfflineSync *goOnline = new nsImapOfflineSync(msgWindow, this, this);
      if (goOnline)
        return goOnline->ProcessNextOperation();
    }
  }
  else // we're offline - check if we're password protecting the offline store
  {
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
      if (NS_SUCCEEDED(rv))
      {
        PRBool passwordMatches = PR_FALSE;
        rv = PromptForCachePassword(server, msgWindow, passwordMatches);
        if (!passwordMatches)
          return NS_ERROR_FAILURE;
      }
    }
  }
  if (!canOpenThisFolder) 
    selectFolder = PR_FALSE;
  // don't run select if we're already running a url/select...
  if (NS_SUCCEEDED(rv) && !m_urlRunning && selectFolder)
  {
    nsCOMPtr <nsIEventQueue> eventQ;
    nsCOMPtr<nsIEventQueueService> pEventQService = 
      do_GetService(kEventQueueServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pEventQService)
      pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
      getter_AddRefs(eventQ));
    nsCOMPtr <nsIURI> url;
    rv = imapService->SelectFolder(eventQ, this, m_urlListener, msgWindow, getter_AddRefs(url));
    if (NS_SUCCEEDED(rv))
      m_urlRunning = PR_TRUE;
    if (url)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(url);
      if (mailnewsUrl)
      {
        mailnewsUrl->RegisterListener(this);
        m_urlListener = nsnull;
      }
    }
    switch (rv)
    {
      case NS_MSG_ERROR_OFFLINE:
        if (msgWindow)
          AutoCompact(msgWindow);  
        // note fall through to next case.
      case NS_BINDING_ABORTED:
        rv = NS_OK;
        NotifyFolderEvent(mFolderLoadedAtom);
        break;
      default:
        break;
    }
  }
  else if (NS_SUCCEEDED(rv))  // tell the front end that the folder is loaded if we're not going to 
  {                           // actually run a url.
    if (!m_urlRunning)        // if we're already running a url, we'll let that one send the folder loaded
      NotifyFolderEvent(mFolderLoadedAtom);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  if (result)
    *result = nsnull;
  if (!mDatabase)
    GetDatabase(nsnull);
  if (mDatabase)
    return mDatabase->EnumerateMessages(result);
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsImapMailFolder::CreateSubfolder(const PRUnichar* folderName, nsIMsgWindow *msgWindow )
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!folderName) 
      return rv;

    nsAutoString trashName;
    GetTrashFolderName(trashName);
    if ( nsDependentString(folderName).Equals(trashName) )   // Trash , a special folder
    {
        ThrowAlertMsg("folderExists", msgWindow);
        return NS_MSG_FOLDER_EXISTS;
    }
    else if (mIsServer && nsDependentString(folderName).LowerCaseEqualsLiteral("inbox") )  // Inbox, a special folder
    {
        ThrowAlertMsg("folderExists", msgWindow);
        return NS_MSG_FOLDER_EXISTS;
    }

    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = imapService->CreateFolder(m_eventQueue, this, 
                                       folderName, this, nsnull);
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::CreateClientSubfolderInfo(const char *folderName, PRUnichar hierarchyDelimiter, PRInt32 flags, PRBool suppressNotification)
{
  nsresult rv = NS_OK;
    
  //Get a directory based on our current path.
  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;

  nsFileSpec path;
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;

//  if (!path.Exists())
//  {
//    path.CreateDir();
//  }

  rv = CreateDirectoryForFolder(path);
  if(NS_FAILED(rv))
    return rv;

    NS_ConvertASCIItoUTF16 leafName(folderName);
    nsAutoString folderNameStr;
    nsAutoString parentName = leafName;
    PRInt32 folderStart = leafName.FindChar('/');
    if (folderStart > 0)
    {
        nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIRDFResource> res;
        nsCOMPtr<nsIMsgImapMailFolder> parentFolder;
        nsCAutoString uri (mURI);
        parentName.Right(leafName, leafName.Length() - folderStart - 1);
        parentName.Truncate(folderStart);

	// the parentName might be too long or have some illegal chars
    // so we make it safe.
    // XXX Here it's assumed that IMAP folder names are stored locally 
    // in modified UTF-7 (ASCII-only) as is stored remotely.  If we ever change
    // this, we have to work with nsString instead of nsCString 
    // (ref. bug 264071)
        nsCAutoString safeParentName;
        safeParentName.AssignWithConversion(parentName);
        NS_MsgHashIfNecessary(safeParentName);
        path += safeParentName.get();

        rv = CreateDirectoryForFolder(path);
        if (NS_FAILED(rv)) return rv;
        uri.Append('/');
        uri.AppendWithConversion(parentName);
        
        rv = rdf->GetResource(uri,
                              getter_AddRefs(res));
        if (NS_FAILED(rv)) return rv;
        parentFolder = do_QueryInterface(res, &rv);
        if (NS_FAILED(rv)) return rv;
        nsCAutoString leafnameC;
        leafnameC.AssignWithConversion(leafName);
        return parentFolder->CreateClientSubfolderInfo(leafnameC.get(), hierarchyDelimiter,flags, suppressNotification);
    }
    
  // if we get here, it's really a leaf, and "this" is the parent.
    folderNameStr = leafName;
    
//    path += folderNameStr;

  // Create an empty database for this mail folder, set its name from the user  
  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
  nsCOMPtr<nsIMsgFolder> child;

  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  if (msgDBService)
  {
    nsCOMPtr<nsIMsgDatabase> unusedDB;
    nsCOMPtr <nsIFileSpec> dbFileSpec;

    // warning, path will be changed
    rv = CreateFileSpecForDB(folderName, path, getter_AddRefs(dbFileSpec));
    NS_ENSURE_SUCCESS(rv,rv);

    //Now let's create the actual new folder
    rv = AddSubfolderWithPath(folderNameStr, dbFileSpec, getter_AddRefs(child));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = msgDBService->OpenMailDBFromFileSpec(dbFileSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) getter_AddRefs(unusedDB));
   if (rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
     rv = NS_OK;

    if (NS_SUCCEEDED(rv) && unusedDB)
    {
    //need to set the folder name
      nsCOMPtr <nsIDBFolderInfo> folderInfo;
      rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
//      if(NS_SUCCEEDED(rv))
//      {
        // ### DMB used to be leafNameFromUser?
//        folderInfo->SetMailboxName(&folderNameStr);
//      }

      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(child);
      if (imapFolder)
      {
        nsCAutoString onlineName(m_onlineFolderName); 
        if (!onlineName.IsEmpty())
          onlineName.Append(char(hierarchyDelimiter));
        onlineName.AppendWithConversion(folderNameStr);
        imapFolder->SetVerifiedAsOnlineFolder(PR_TRUE);
        imapFolder->SetOnlineName(onlineName.get());
        imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
        imapFolder->SetBoxFlags(flags);
   
        child->SetFlag(MSG_FOLDER_FLAG_ELIDED);
        nsXPIDLString unicodeName;
        rv = CopyMUTF7toUTF16(nsDependentCString(folderName), unicodeName);
        if (NS_SUCCEEDED(rv))
          child->SetPrettyName(unicodeName);
 
        // store the online name as the mailbox name in the db folder info
        // I don't think anyone uses the mailbox name, so we'll use it
        // to restore the online name when blowing away an imap db.
        if (folderInfo)
        {
          nsAutoString unicodeOnlineName; unicodeOnlineName.AssignWithConversion(onlineName.get());
          folderInfo->SetMailboxName(unicodeOnlineName);
        }
      }

      unusedDB->SetSummaryValid(PR_TRUE);
      unusedDB->Commit(nsMsgDBCommitType::kLargeCommit);
      unusedDB->Close(PR_TRUE);
    }
  }
  if (!suppressNotification)
  {
    nsCOMPtr <nsIAtom> folderCreateAtom;
    if(NS_SUCCEEDED(rv) && child)
    {
      NotifyItemAdded(child);
      folderCreateAtom = do_GetAtom("FolderCreateCompleted");
      child->NotifyFolderEvent(folderCreateAtom);
    }
    else
    {
      folderCreateAtom = do_GetAtom("FolderCreateFailed");
      NotifyFolderEvent(folderCreateAtom);
    }
  }
  return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::List()
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = imapService->ListFolder(m_eventQueue, this, this, nsnull);
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::RemoveSubFolder (nsIMsgFolder *which)
{
    nsCOMPtr<nsISupportsArray> folders;
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(folders));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsISupports> folderSupport = do_QueryInterface(which, &rv);
    if (NS_FAILED(rv)) return rv;
    folders->AppendElement(folderSupport);
    which->Delete();
    return nsMsgDBFolder::DeleteSubFolders(folders, nsnull);
}

NS_IMETHODIMP nsImapMailFolder::CreateStorageIfMissing(nsIUrlListener* urlListener)
{
   nsresult status = NS_OK;
  nsCOMPtr <nsIMsgFolder> msgParent;
  GetParentMsgFolder(getter_AddRefs(msgParent));

  // parent is probably not set because *this* was probably created by rdf
  // and not by folder discovery. So, we have to compute the parent.
  if (!msgParent)
  {
    nsCAutoString folderName(mURI);
      
    PRInt32 leafPos = folderName.RFindChar('/');
    nsCAutoString parentName(folderName);

    if (leafPos > 0)
    {
      // If there is a hierarchy, there is a parent.
      // Don't strip off slash if it's the first character
      parentName.Truncate(leafPos);
      // get the corresponding RDF resource
      // RDF will create the folder resource if it doesn't already exist
      nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &status));
      if (NS_FAILED(status)) return status;
      nsCOMPtr<nsIRDFResource> resource;
      status = rdf->GetResource(parentName, getter_AddRefs(resource));
      if (NS_FAILED(status)) return status;

      msgParent = do_QueryInterface(resource, &status);
    }
  }
  if (msgParent)
  {
    nsXPIDLString folderName;
    GetName(getter_Copies(folderName));
    nsresult rv;
    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv); 
    if (NS_SUCCEEDED(rv) && imapService)
    {
      nsCOMPtr <nsIURI> uri;
      imapService->EnsureFolderExists(m_eventQueue, msgParent, folderName.get(), urlListener, getter_AddRefs(uri));
    }
  }

  return status;
}


NS_IMETHODIMP nsImapMailFolder::GetVerifiedAsOnlineFolder(PRBool *aVerifiedAsOnlineFolder)
{

  if (!aVerifiedAsOnlineFolder)
    return NS_ERROR_NULL_POINTER;

  *aVerifiedAsOnlineFolder = m_verifiedAsOnlineFolder;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetVerifiedAsOnlineFolder(PRBool aVerifiedAsOnlineFolder)
{
  m_verifiedAsOnlineFolder = aVerifiedAsOnlineFolder;
  // mark ancestors as verified as well
  if (aVerifiedAsOnlineFolder)
  {
    nsCOMPtr<nsIMsgFolder> parent;
    do
    {
      GetParent(getter_AddRefs(parent));
      if (parent)
      {
        nsCOMPtr<nsIMsgImapMailFolder> imapParent = do_QueryInterface(parent);
        if (imapParent)
        {
          PRBool verifiedOnline;
          imapParent->GetVerifiedAsOnlineFolder(&verifiedOnline);
          if (verifiedOnline)
            break;
          imapParent->SetVerifiedAsOnlineFolder(PR_TRUE);
        }
      }
    }
    while (parent);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetOnlineDelimiter(char** onlineDelimiter)
{
  if (onlineDelimiter)
  {
    nsresult rv;
    PRUnichar delimiter = 0;
    rv = GetHierarchyDelimiter(&delimiter);
    nsAutoString aString(delimiter);
    *onlineDelimiter = ToNewCString(aString);
    return rv;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsImapMailFolder::SetHierarchyDelimiter(PRUnichar aHierarchyDelimiter)
{
  m_hierarchyDelimiter = aHierarchyDelimiter;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetHierarchyDelimiter(PRUnichar *aHierarchyDelimiter)
{
  if (!aHierarchyDelimiter)
    return NS_ERROR_NULL_POINTER;
   ReadDBFolderInfo(PR_FALSE); // update cache first.
  *aHierarchyDelimiter = m_hierarchyDelimiter;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetBoxFlags(PRInt32 aBoxFlags)
{
  ReadDBFolderInfo(PR_FALSE);

  m_boxFlags = aBoxFlags;
  PRUint32 newFlags = mFlags;

  newFlags |= MSG_FOLDER_FLAG_IMAPBOX;

  if (m_boxFlags & kNoinferiors)
    newFlags |= MSG_FOLDER_FLAG_IMAP_NOINFERIORS;
  else
    newFlags &= ~MSG_FOLDER_FLAG_IMAP_NOINFERIORS;
    if (m_boxFlags & kNoselect)
        newFlags |= MSG_FOLDER_FLAG_IMAP_NOSELECT;
    else
        newFlags &= ~MSG_FOLDER_FLAG_IMAP_NOSELECT;
    if (m_boxFlags & kPublicMailbox)
        newFlags |= MSG_FOLDER_FLAG_IMAP_PUBLIC;
    else
        newFlags &= ~MSG_FOLDER_FLAG_IMAP_PUBLIC;
    if (m_boxFlags & kOtherUsersMailbox)
        newFlags |= MSG_FOLDER_FLAG_IMAP_OTHER_USER;
    else
        newFlags &= ~MSG_FOLDER_FLAG_IMAP_OTHER_USER;
    if (m_boxFlags & kPersonalMailbox)
        newFlags |= MSG_FOLDER_FLAG_IMAP_PERSONAL;
    else
        newFlags &= ~MSG_FOLDER_FLAG_IMAP_PERSONAL;

    SetFlags(newFlags);
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetBoxFlags(PRInt32 *aBoxFlags)
{
  if (!aBoxFlags)
    return NS_ERROR_NULL_POINTER;
  *aBoxFlags = m_boxFlags;
  return NS_OK;
}


NS_IMETHODIMP nsImapMailFolder::GetExplicitlyVerify(PRBool *aExplicitlyVerify)
{
  if (!aExplicitlyVerify)
    return NS_ERROR_NULL_POINTER;
  *aExplicitlyVerify = m_explicitlyVerify;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetExplicitlyVerify(PRBool aExplicitlyVerify)
{
  m_explicitlyVerify = aExplicitlyVerify;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetNoSelect(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  return GetFlag(MSG_FOLDER_FLAG_IMAP_NOSELECT, aResult);
}

NS_IMETHODIMP nsImapMailFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  // compact offline store, if folder configured for offline use.
  // for now, check aMsgWindow because not having aMsgWindow means
  // we're doing a compact at shut-down. TEMPORARY HACK
 if (aMsgWindow && mFlags & MSG_FOLDER_FLAG_OFFLINE)
    CompactOfflineStore(aMsgWindow);

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
 
  return  imapService->Expunge(m_eventQueue, this, aListener, nsnull);
}

NS_IMETHODIMP nsImapMailFolder::CompactAll(nsIUrlListener *aListener,  nsIMsgWindow *aMsgWindow, nsISupportsArray *aFolderArray, PRBool aCompactOfflineAlso, nsISupportsArray *aOfflineFolderArray)
{
  NS_ASSERTION(!aOfflineFolderArray, "compacting automatically compacts offline stores");
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
      for (PRUint32 i=0; i< cnt;i++)
      {
        nsCOMPtr<nsISupports> supports = getter_AddRefs(allDescendents->ElementAt(i));
        nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
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

NS_IMETHODIMP nsImapMailFolder::UpdateStatus(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIURI> uri;

  rv = imapService->UpdateFolderStatus(m_eventQueue, this, aListener, getter_AddRefs(uri));
  if (uri)
  {
    nsCOMPtr <nsIMsgMailNewsUrl> mailNewsUrl = do_QueryInterface(uri);
    // if no msg window, we won't put up error messages (this is almost certainly a biff-inspired status)
    if (!aMsgWindow)
      mailNewsUrl->SetSuppressErrorMsgs(PR_TRUE);
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::EmptyTrash(nsIMsgWindow *aMsgWindow,
                                           nsIUrlListener *aListener)
{
    nsCOMPtr<nsIMsgFolder> trashFolder;
    nsresult rv = GetTrashFolder(getter_AddRefs(trashFolder));
    if (NS_SUCCEEDED(rv))
    {
       nsCOMPtr<nsIMsgAccountManager> accountManager = 
                do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
       if (accountManager)
       {
         // if we are emptying trash on exit and we are an aol server then don't perform
         // this operation because it's causing a hang that we haven't been able to figure out yet
         // this is an rtm fix and we'll look for the right solution post rtm. 

         PRBool empytingOnExit = PR_FALSE;
         accountManager->GetEmptyTrashInProgress(&empytingOnExit);
         if (empytingOnExit)
         {
            nsCOMPtr<nsIImapIncomingServer> imapServer;
            rv = GetImapIncomingServer(getter_AddRefs(imapServer));

            if (NS_SUCCEEDED(rv) && imapServer) 
            {
              PRBool isAOLServer = PR_FALSE;
              imapServer->GetIsAOLServer(&isAOLServer);
              if (isAOLServer)
                return NS_ERROR_FAILURE;  // we will not be performing an empty trash....
            } // if we fetched an imap server
         } // if emptying trash on exit which is done through the account manager.
       }

        nsCOMPtr<nsIMsgDatabase> trashDB;

        if (WeAreOffline())
        {
          nsCOMPtr <nsIMsgDatabase> trashDB;
          rv = trashFolder->GetMsgDatabase(nsnull, getter_AddRefs(trashDB));
          if (NS_SUCCEEDED(rv) && trashDB)
          {
            nsMsgKey fakeKey;
            trashDB->GetNextFakeOfflineMsgKey(&fakeKey);
    
            nsCOMPtr <nsIMsgOfflineImapOperation> op;
            rv = trashDB->GetOfflineOpForKey(fakeKey, PR_TRUE, getter_AddRefs(op));
            trashFolder->SetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
            op->SetOperation(nsIMsgOfflineImapOperation::kDeleteAllMsgs);
          }
          return rv;
        }
        nsCOMPtr <nsIDBFolderInfo> transferInfo;
        rv = trashFolder->GetDBTransferInfo(getter_AddRefs(transferInfo));
        rv = trashFolder->Delete(); // delete summary spec
        trashFolder->SetDBTransferInfo(transferInfo);

        trashFolder->SetSizeOnDisk(0);
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
        PRBool hasSubfolders = PR_FALSE;
        rv = trashFolder->GetHasSubFolders(&hasSubfolders);
        if (hasSubfolders)
        {
            nsCOMPtr<nsIEnumerator> aEnumerator;
            nsCOMPtr<nsISupports> aSupport;
            nsCOMPtr<nsIMsgFolder> aFolder;
            nsCOMPtr<nsISupportsArray> aSupportsArray;
            rv = NS_NewISupportsArray(getter_AddRefs(aSupportsArray));
            if (NS_FAILED(rv)) return rv;
            rv = trashFolder->GetSubFolders(getter_AddRefs(aEnumerator));
            PRBool confirmDeletion;
            nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
            if (NS_SUCCEEDED(rv))
              prefBranch->GetBoolPref("mail.imap.confirm_emptyTrashFolderDeletion", &confirmDeletion);

            nsXPIDLString confirmationStr;
            nsCOMPtr<nsIStringBundle> bundle;
            nsCOMPtr<nsIDOMWindowInternal> parentWindow;
            nsCOMPtr<nsIPromptService> promptService;
            if (confirmDeletion)
            {
              IMAPGetStringByID(IMAP_EMPTY_TRASH_CONFIRM, getter_Copies(confirmationStr));
              promptService = do_GetService("@mozilla.org/embedcomp/prompt-service;1");
              nsCOMPtr<nsIDocShell> docShell;
              if (aMsgWindow)
              {
	              (void) aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
   	            parentWindow = do_QueryInterface(docShell);
              }
              rv = IMAPGetStringBundle(getter_AddRefs(bundle));
              NS_ENSURE_SUCCESS(rv, rv);
            rv = aEnumerator->First();
            while(NS_SUCCEEDED(rv))
            {
                PRInt32 dlgResult  = -1;
                rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
                if (confirmDeletion)
                {
                  nsXPIDLString statusString, confirmText;
                  nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(aSupport);
                  nsXPIDLString folderName;
                  folder->GetName(getter_Copies(folderName));
                  const PRUnichar *formatStrings[1] = { folderName.get() };

                  rv = bundle->FormatStringFromID(IMAP_EMPTY_TRASH_CONFIRM,
                                                    formatStrings, 1,
                                                    getter_Copies(confirmText));
                  // Got the text, now show dialog.
                  rv = promptService->ConfirmEx(parentWindow, nsnull, confirmText,
                                              (nsIPromptService::BUTTON_TITLE_OK * nsIPromptService::BUTTON_POS_0) +
                                              (nsIPromptService::BUTTON_TITLE_CANCEL * nsIPromptService::BUTTON_POS_1), 
                                                nsnull, nsnull, nsnull, nsnull, nsnull, &dlgResult);
                }
              if ( NS_SUCCEEDED( rv ) ) 
              {
                  if (dlgResult == 1)
                  return NS_BINDING_ABORTED;
                rv = aEnumerator->Next();
              }
            }
          }
        }
        if (aListener)
        {
            rv = imapService->DeleteAllMessages(m_eventQueue, trashFolder,
                                                aListener, nsnull);
        }
        else
        {
            nsCOMPtr<nsIUrlListener> urlListener = 
                do_QueryInterface(trashFolder);
            rv = imapService->DeleteAllMessages(m_eventQueue, trashFolder,
                                                urlListener, nsnull);
        }
        // return an error if this failed. We want the empty trash on exit code
        // to know if this fails so that it doesn't block waiting for empty trash to finish.
        if (NS_FAILED(rv))
          return rv;
        if (hasSubfolders)
        {
            nsCOMPtr<nsIEnumerator> aEnumerator;
            nsCOMPtr<nsISupports> aSupport;
            nsCOMPtr<nsIMsgFolder> aFolder;
            nsCOMPtr<nsISupportsArray> aSupportsArray;
            rv = NS_NewISupportsArray(getter_AddRefs(aSupportsArray));
            if (NS_FAILED(rv)) return rv;
            rv = trashFolder->GetSubFolders(getter_AddRefs(aEnumerator));

            rv = aEnumerator->First();
            while(NS_SUCCEEDED(rv))
            {
                rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
                      aSupportsArray->AppendElement(aSupport);
                  rv = aEnumerator->Next();
              }
              PRUint32 cnt = 0;
              aSupportsArray->Count(&cnt);
              for (PRInt32 i = cnt-1; i >= 0; i--)
              {
                  aFolder = do_QueryElementAt(aSupportsArray, i);
                  aSupportsArray->RemoveElementAt(i);
                  if (aFolder)
                      trashFolder->PropagateDelete(aFolder, PR_TRUE, aMsgWindow);
              }
            }
        }

        return NS_OK;
    }

    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Delete ()
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mDatabase)
    {
        mDatabase->ForceClosed();
        mDatabase = nsnull;
    }

    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_SUCCEEDED(rv))
    {
        nsFileSpec fileSpec;
        rv = pathSpec->GetFileSpec(&fileSpec);
        if (NS_SUCCEEDED(rv))
        {
            nsLocalFolderSummarySpec summarySpec(fileSpec);
            if (summarySpec.Exists())
                summarySpec.Delete(PR_FALSE);
        }
    }
    if (mPath)
    {
      nsFileSpec fileSpec;
      if (NS_SUCCEEDED(mPath->GetFileSpec(&fileSpec)) && fileSpec.Exists())
        fileSpec.Delete(PR_FALSE);
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Rename (const PRUnichar *newName, nsIMsgWindow *msgWindow )
{
    if (mFlags & MSG_FOLDER_FLAG_VIRTUAL)
      return nsMsgDBFolder::Rename(newName, msgWindow);
    nsresult rv = NS_ERROR_FAILURE;
    nsAutoString newNameStr(newName);
    if (newNameStr.FindChar(m_hierarchyDelimiter,0) != -1)
    {
      nsCOMPtr<nsIDocShell> docShell;
      if (msgWindow)
        msgWindow->GetRootDocShell(getter_AddRefs(docShell));
      if (docShell)
      {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = IMAPGetStringBundle(getter_AddRefs(bundle));
        if (NS_SUCCEEDED(rv) && bundle)
        {
          const PRUnichar *formatStrings[] =
          {
             (const PRUnichar*) m_hierarchyDelimiter
          };
          nsXPIDLString alertString;
          rv = bundle->FormatStringFromID(IMAP_SPECIAL_CHAR,
                                        formatStrings, 1,
                                        getter_Copies(alertString));
          nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
          if (dialog && alertString)
            dialog->Alert(nsnull, alertString);
        }
      }
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr <nsIImapIncomingServer> incomingImapServer;

    GetImapIncomingServer(getter_AddRefs(incomingImapServer));
    if (incomingImapServer)
      RecursiveCloseActiveConnections(incomingImapServer);

    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    return imapService->RenameLeaf(m_eventQueue, this, newName, this, msgWindow,
                                     nsnull);
}

NS_IMETHODIMP nsImapMailFolder::RecursiveCloseActiveConnections(nsIImapIncomingServer *incomingImapServer)
{
  NS_ENSURE_ARG(incomingImapServer);
  PRUint32 cnt = 0, i;
  nsresult rv;
  if (mSubFolders)
  {
      nsCOMPtr<nsIMsgImapMailFolder> folder;
      mSubFolders->Count(&cnt);
      if (cnt > 0)
      {
          for (i = 0; i < cnt; i++)
          {
              folder = do_QueryElementAt(mSubFolders, i);
              if (folder)
                  folder->RecursiveCloseActiveConnections(incomingImapServer);
              nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(folder, &rv);
              if (NS_SUCCEEDED(rv) && msgFolder)
                incomingImapServer->CloseConnectionForFolder(msgFolder);
          }
      }
  }
  return NS_OK;  
}

// this is called *after* we've done the rename on the server.
NS_IMETHODIMP nsImapMailFolder::PrepareToRename()
{
    PRUint32 cnt = 0, i;
    if (mSubFolders)
    {
        nsCOMPtr<nsIMsgImapMailFolder> folder;
        mSubFolders->Count(&cnt);
        if (cnt > 0)
        {
            for (i = 0; i < cnt; i++)
            {
                folder = do_QueryElementAt(mSubFolders, i);
                if (folder)
                    folder->PrepareToRename();
            }
        }
    }
    SetOnlineName("");
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::RenameLocal(const char *newName, nsIMsgFolder *parent)
{
    // XXX Here it's assumed that IMAP folder names are stored locally 
    // in modified UTF-7 (ASCII-only) as is stored remotely.  If we ever change
    // this, we have to work with nsString instead of nsCString 
    // (ref. bug 264071)
    nsCAutoString leafname(newName);
    nsCAutoString parentName;
    // newName always in the canonical form "greatparent/parentname/leafname"
    PRInt32 leafpos = leafname.RFindChar('/');
    if (leafpos >0) 
        leafname.Cut(0, leafpos+1);
    m_msgParser = nsnull;
    PrepareToRename();
    ForceDBClosed();

    nsresult rv = NS_OK;
    nsCOMPtr<nsIFileSpec> oldPathSpec;
    rv = GetPath(getter_AddRefs(oldPathSpec));
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIFileSpec> parentPathSpec;
	rv = parent->GetPath(getter_AddRefs(parentPathSpec));
	NS_ENSURE_SUCCESS(rv,rv);

	nsFileSpec parentPath;
	rv = parentPathSpec->GetFileSpec(&parentPath);
	NS_ENSURE_SUCCESS(rv,rv);

    if (!parentPath.IsDirectory())
	  AddDirectorySeparator(parentPath);
    
    PRUint32 cnt = 0;
    nsFileSpec dirSpec;

    if (mSubFolders)
        mSubFolders->Count(&cnt);
    if (cnt > 0)
    {
        oldPathSpec->GetFileSpec(&dirSpec);
        rv = CreateDirectoryForFolder(dirSpec);
    }
    nsFileSpec fileSpec;
    oldPathSpec->GetFileSpec(&fileSpec);
    nsLocalFolderSummarySpec oldSummarySpec(fileSpec);
    nsCAutoString newNameStr;
    oldSummarySpec.Delete(PR_FALSE);
    if (cnt > 0)
    {
       newNameStr = leafname;
       NS_MsgHashIfNecessary(newNameStr);
       newNameStr += ".sbd";
       char *leafName = dirSpec.GetLeafName();
       if (nsCRT::strcmp(leafName, newNameStr.get()) != 0 )
       {
         dirSpec.Rename(newNameStr.get());      // in case of rename operation leaf names will differ
         nsCRT::free(leafName);
         return rv;
       }
       nsCRT::free(leafName);
                                           
       parentPath += newNameStr.get();    //only for move we need to progress further in case the parent differs

       if (!parentPath.IsDirectory())
         parentPath.CreateDirectory();
       else
         NS_ASSERTION(0,"Directory already exists.");
       
       nsCOMPtr<nsILocalFile> srcDir = (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
       NS_ENSURE_SUCCESS(rv,rv);

       nsCOMPtr<nsILocalFile> destDir = (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
       NS_ENSURE_SUCCESS(rv,rv);
	  
       srcDir->InitWithNativePath(nsDependentCString(dirSpec.GetNativePathCString()));
	   
       destDir->InitWithNativePath(nsDependentCString(parentPath.GetNativePathCString()));
       
       rv = RecursiveCopy(srcDir, destDir);
   
       NS_ENSURE_SUCCESS(rv,rv);

       dirSpec.Delete(PR_TRUE);                         // moving folders
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetPrettyName(PRUnichar ** prettyName)
{
  return GetName(prettyName);
}
    
NS_IMETHODIMP nsImapMailFolder::UpdateSummaryTotals(PRBool force) 
{
  if (!mNotifyCountChanges || mIsServer)
    return NS_OK;

  // could we move this into nsMsgDBFolder, or do we need to deal
  // with the pending imap counts?
  nsresult rv = NS_OK;

  PRInt32 oldUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
  PRInt32 oldTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;
  //We need to read this info from the database
  ReadDBFolderInfo(force);

  PRInt32 newUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
  PRInt32 newTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;

  //Need to notify listeners that total count changed.
  if(oldTotalMessages != newTotalMessages)
  {
    NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, newTotalMessages);
  }

  if(oldUnreadMessages != newUnreadMessages)
  {
    NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, newUnreadMessages);
  }

  FlushToFolderCache();
  return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::GetDeletable (PRBool *deletable)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::GetSizeOnDisk(PRUint32 * size)
{
  NS_ENSURE_ARG_POINTER(size);
  *size = mFolderSize;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::GetCanCreateSubfolders(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = !(mFlags & (MSG_FOLDER_FLAG_IMAP_NOINFERIORS | MSG_FOLDER_FLAG_VIRTUAL));

  PRBool isServer = PR_FALSE;
  GetIsServer(&isServer);
  if (!isServer)
  {
      nsCOMPtr<nsIImapIncomingServer> imapServer;
      nsresult rv = GetImapIncomingServer(getter_AddRefs(imapServer));
      PRBool dualUseFolders = PR_TRUE;
      if (NS_SUCCEEDED(rv) && imapServer)
          imapServer->GetDualUseFolders(&dualUseFolders);
      if (!dualUseFolders && *aResult)
          *aResult = (mFlags & MSG_FOLDER_FLAG_IMAP_NOSELECT);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::GetCanSubscribe(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_FALSE;

  PRBool isImapServer = PR_FALSE;
  nsresult rv = GetIsServer(&isImapServer);
  if (NS_FAILED(rv)) return rv;
 
  // you can only subscribe to imap servers, not imap folders
  *aResult = isImapServer;
  return NS_OK;
}

nsresult nsImapMailFolder::GetServerKey(char **serverKey)
{
  // look for matching imap folders, then pop folders
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
    return server->GetKey(serverKey);
  return rv;
}

nsresult nsImapMailFolder::GetImapIncomingServer(nsIImapIncomingServer **aImapIncomingServer)
{
  NS_ENSURE_ARG(aImapIncomingServer);

  *aImapIncomingServer = nsnull;

  nsCOMPtr<nsIMsgIncomingServer> server;

  if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
  {
    nsCOMPtr <nsIImapIncomingServer> incomingServer = do_QueryInterface(server);
    *aImapIncomingServer = incomingServer;
    NS_IF_ADDREF(*aImapIncomingServer);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsImapMailFolder::AddMessageDispositionState(nsIMsgDBHdr *aMessage, nsMsgDispositionState aDispositionFlag)
{
  nsMsgDBFolder::AddMessageDispositionState(aMessage, aDispositionFlag);

  // set the mark message answered flag on the server for this message...
  if (aMessage)
  {
    nsMsgKeyArray messageIDs;
    nsMsgKey msgKey;
    aMessage->GetMessageKey(&msgKey);
    messageIDs.Add(msgKey);

    if (aDispositionFlag == nsIMsgFolder::nsMsgDispositionState_Replied)
      StoreImapFlags(kImapMsgAnsweredFlag, PR_TRUE, messageIDs.GetArray(), messageIDs.GetSize());
    else if (aDispositionFlag == nsIMsgFolder::nsMsgDispositionState_Forwarded)
      StoreImapFlags(kImapMsgForwardedFlag, PR_TRUE, messageIDs.GetArray(), messageIDs.GetSize());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::MarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
  // tell the folder to do it, which will mark them read in the db.
  nsresult rv = nsMsgDBFolder::MarkMessagesRead(messages, markRead);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString messageIds;
    nsMsgKeyArray keysToMarkRead;
    rv = BuildIdsAndKeyArray(messages, messageIds, keysToMarkRead);
    if (NS_FAILED(rv)) return rv;

    StoreImapFlags(kImapMsgSeenFlag, markRead,  keysToMarkRead.GetArray(), keysToMarkRead.GetSize());
    rv = GetDatabase(nsnull);
    if (NS_SUCCEEDED(rv))
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::SetLabelForMessages(nsISupportsArray *aMessages, nsMsgLabelValue aLabel)
{
  NS_ENSURE_ARG(aMessages);

  nsCAutoString messageIds;
  nsMsgKeyArray keysToLabel;
  nsresult rv = BuildIdsAndKeyArray(aMessages, messageIds, keysToLabel);
  NS_ENSURE_SUCCESS(rv, rv);
  return StoreImapFlags((aLabel << 9), PR_TRUE, keysToLabel.GetArray(), keysToLabel.GetSize());
}

NS_IMETHODIMP
nsImapMailFolder::MarkAllMessagesRead(void)
{
  nsresult rv = GetDatabase(nsnull);
  
  if(NS_SUCCEEDED(rv))
  {
    nsMsgKeyArray thoseMarked;
    EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_TRUE /*dbBatching*/);
    rv = mDatabase->MarkAllRead(&thoseMarked);
    EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE /*dbBatching*/);
    if (NS_SUCCEEDED(rv))
    {
      rv = StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, thoseMarked.GetArray(), thoseMarked.GetSize());
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
    }
  }

  return rv;
}

NS_IMETHODIMP nsImapMailFolder::MarkThreadRead(nsIMsgThread *thread)
{

  nsresult rv = GetDatabase(nsnull);
  if(NS_SUCCEEDED(rv))
  {
    nsMsgKeyArray thoseMarked;
		rv = mDatabase->MarkThreadRead(thread, nsnull, &thoseMarked);
    if (NS_SUCCEEDED(rv))
    {
      rv = StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, thoseMarked.GetArray(), thoseMarked.GetSize());
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
    }
  }
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::ReadFromFolderCacheElem(nsIMsgFolderCacheElement *element)
{
  nsresult rv = nsMsgDBFolder::ReadFromFolderCacheElem(element);
  PRInt32 hierarchyDelimiter = kOnlineHierarchySeparatorUnknown;
  nsXPIDLCString onlineName;

  element->GetInt32Property("boxFlags", &m_boxFlags);
  if (NS_SUCCEEDED(element->GetInt32Property("hierDelim", &hierarchyDelimiter)) 
      && hierarchyDelimiter != kOnlineHierarchySeparatorUnknown)
    m_hierarchyDelimiter = (PRUnichar) hierarchyDelimiter;
  rv = element->GetStringProperty("onlineName", getter_Copies(onlineName));
  if (NS_SUCCEEDED(rv) && (const char *) onlineName && strlen((const char *) onlineName))
    m_onlineFolderName.Assign(onlineName);

  m_aclFlags = -1; // init to invalid value.
  element->GetInt32Property("aclFlags", (PRInt32 *) &m_aclFlags);
#ifdef DEBUG_bienvenu
  if (!nsCRT::strcasecmp((const char *) onlineName, "Sent"))
    printf("loading folder cache elem for %s flags = %lx", (const char *) onlineName, mFlags);
  else if (!nsCRT::strcasecmp((const char *) onlineName, "INBOX"))
    printf("loading folder cache elem for %s flags = %lx", (const char *) onlineName, mFlags);
#endif
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::WriteToFolderCacheElem(nsIMsgFolderCacheElement *element)
{
  nsresult rv = nsMsgDBFolder::WriteToFolderCacheElem(element);
  element->SetInt32Property("boxFlags", m_boxFlags);
  element->SetInt32Property("hierDelim", (PRInt32) m_hierarchyDelimiter);
  element->SetStringProperty("onlineName", m_onlineFolderName.get());
  element->SetInt32Property("aclFlags", (PRInt32) m_aclFlags);
  return rv;
}



NS_IMETHODIMP
nsImapMailFolder::MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged)
{
  nsresult rv;

  // tell the folder to do it, which will mark them read in the db.
  rv = nsMsgDBFolder::MarkMessagesFlagged(messages, markFlagged);
  if (NS_SUCCEEDED(rv))
  {
    nsCAutoString messageIds;
        nsMsgKeyArray keysToMarkFlagged;
    rv = BuildIdsAndKeyArray(messages, messageIds, keysToMarkFlagged);
    if (NS_FAILED(rv)) return rv;

    rv = StoreImapFlags(kImapMsgFlaggedFlag, markFlagged,  keysToMarkFlagged.GetArray(), keysToMarkFlagged.GetSize());
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::SetOnlineName(const char * aOnlineFolderName)
{
  nsresult rv;
  nsCOMPtr<nsIMsgDatabase> db; 
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  // do this after GetDBFolderInfoAndDB, because it crunches m_onlineFolderName (not sure why)
  m_onlineFolderName = aOnlineFolderName;
  if(NS_SUCCEEDED(rv) && folderInfo)
  {
    nsAutoString onlineName; onlineName.AssignWithConversion(aOnlineFolderName);
    rv = folderInfo->SetProperty("onlineName", onlineName);
    rv = folderInfo->SetMailboxName(onlineName);
    // so, when are we going to commit this? Definitely not every time!
    // We could check if the online name has changed.
    db->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  folderInfo = nsnull;
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::GetOnlineName(char ** aOnlineFolderName)
{
  if (!aOnlineFolderName)
    return NS_ERROR_NULL_POINTER;
  ReadDBFolderInfo(PR_FALSE); // update cache first.
  *aOnlineFolderName = ToNewCString(m_onlineFolderName);
  return (*aOnlineFolderName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;

  // ### do we want to read from folder cache first, or has that been done?
}


NS_IMETHODIMP
nsImapMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
  nsresult openErr=NS_ERROR_UNEXPECTED;
  if(!db || !folderInfo)
    return NS_ERROR_NULL_POINTER; //ducarroz: should we use NS_ERROR_INVALID_ARG?
  nsresult rv;

  openErr = GetDatabase(nsnull);

  *db = mDatabase;
  NS_IF_ADDREF(*db);
  if (NS_SUCCEEDED(openErr)&& *db)
  {
    openErr = (*db)->GetDBFolderInfo(folderInfo);
    if (NS_SUCCEEDED(openErr) && folderInfo)
    {
      nsXPIDLCString onlineName;
      if (NS_SUCCEEDED((*folderInfo)->GetCharPtrProperty("onlineName", getter_Copies(onlineName))))
      {
        if (!onlineName.IsEmpty())
          m_onlineFolderName.Assign(onlineName);
        else
        {
          nsAutoString autoOnlineName; 
          // autoOnlineName.AssignWithConversion(name);
          (*folderInfo)->GetMailboxName(autoOnlineName);
          if (autoOnlineName.IsEmpty())
          {
            nsXPIDLCString uri;
            rv = GetURI(getter_Copies(uri));
            if (NS_FAILED(rv)) return rv;
            nsXPIDLCString hostname;
            rv = GetHostname(getter_Copies(hostname));
            if (NS_FAILED(rv)) return rv;
            nsXPIDLCString name;
            rv = nsImapURI2FullName(kImapRootURI, hostname, uri, getter_Copies(name));
            nsCAutoString onlineCName(name);
            if (m_hierarchyDelimiter != '/')
              onlineCName.ReplaceChar('/',  char(m_hierarchyDelimiter));
            m_onlineFolderName.Assign(onlineCName); 
            autoOnlineName.AssignWithConversion(onlineCName.get());
          }
          rv = (*folderInfo)->SetProperty("onlineName", autoOnlineName);
        }
      }
    }
  }
  return openErr;
}

nsresult
nsImapMailFolder::BuildIdsAndKeyArray(nsISupportsArray* messages,
                                      nsCString& msgIds,
                                      nsMsgKeyArray& keyArray)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    PRUint32 count = 0;
    PRUint32 i;

    if (!messages) return rv;

    rv = messages->Count(&count);
    if (NS_FAILED(rv)) return rv;

    // build up message keys.
    for (i = 0; i < count; i++)
    {
      nsMsgKey key;
      nsCOMPtr <nsIMsgDBHdr> msgDBHdr = do_QueryElementAt(messages, i, &rv);
      if (msgDBHdr)
        rv = msgDBHdr->GetMessageKey(&key);
      if (NS_SUCCEEDED(rv))
        keyArray.Add(key);
    }
    
  return AllocateUidStringFromKeys(keyArray.GetArray(), keyArray.GetSize(), msgIds);
}

static int PR_CALLBACK CompareKey (const void *v1, const void *v2, void *)
{
	// QuickSort callback to compare array values
	nsMsgKey i1 = *(nsMsgKey *)v1;
	nsMsgKey i2 = *(nsMsgKey *)v2;
	return i1 - i2;
}

/* static */nsresult
nsImapMailFolder::AllocateUidStringFromKeys(nsMsgKey *keys, PRUint32 numKeys, nsCString &msgIds)
{
  if (!numKeys)
    return NS_ERROR_INVALID_ARG;
  nsresult rv = NS_OK;
  PRUint32 startSequence;
  startSequence = keys[0];
  PRUint32 curSequenceEnd = startSequence;
  PRUint32 total = numKeys;
  // sort keys and then generate ranges instead of singletons!
  NS_QuickSort(keys, numKeys, sizeof(nsMsgKey), CompareKey, nsnull);
  for (PRUint32 keyIndex = 0; keyIndex < total; keyIndex++)
  {
    PRUint32 curKey = keys[keyIndex];
    PRUint32 nextKey = (keyIndex + 1 < total) ? keys[keyIndex + 1] : 0xFFFFFFFF;
    PRBool lastKey = (nextKey == 0xFFFFFFFF);

    if (lastKey)
      curSequenceEnd = curKey;
    if (nextKey == (PRUint32) curSequenceEnd + 1 && !lastKey)
    {
      curSequenceEnd = nextKey;
      continue;
    }
    else if (curSequenceEnd > startSequence)
    {
      msgIds.AppendInt(startSequence);
      msgIds += ':';
      msgIds.AppendInt(curSequenceEnd);
      if (!lastKey)
        msgIds += ',';
      startSequence = nextKey;
      curSequenceEnd = startSequence;
    }
    else
    {
      startSequence = nextKey;
      curSequenceEnd = startSequence;
      msgIds.AppendInt(keys[keyIndex]);
      if (!lastKey)
        msgIds += ',';
    }
  }
  return rv;
}

nsresult nsImapMailFolder::MarkMessagesImapDeleted(nsMsgKeyArray *keyArray, PRBool deleted, nsIMsgDatabase *db)
{
  for (PRUint32 kindex = 0; kindex < keyArray->GetSize(); kindex++)
  {
    nsMsgKey key = keyArray->ElementAt(kindex);
    db->MarkImapDeleted(key, deleted, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::DeleteMessages(nsISupportsArray *messages,
                                               nsIMsgWindow *msgWindow,
                                               PRBool deleteStorage, PRBool isMove,
                                               nsIMsgCopyServiceListener* listener,
                                               PRBool allowUndo)
{
  // *** jt - assuming delete is move to the trash folder for now
  nsCOMPtr<nsIEnumerator> aEnumerator;
  nsCOMPtr<nsIRDFResource> res;
  nsCAutoString uri;
  PRBool deleteImmediatelyNoTrash = PR_FALSE;
  nsCAutoString messageIds;
  nsMsgKeyArray srcKeyArray;
  PRBool deleteMsgs = PR_TRUE;  //used for toggling delete status - default is true
  nsMsgImapDeleteModel deleteModel = nsMsgImapDeleteModels::MoveToTrash;
  imapMessageFlagsType messageFlags = kImapMsgDeletedFlag;

  nsCOMPtr<nsIImapIncomingServer> imapServer;
  nsresult rv = GetFlag(MSG_FOLDER_FLAG_TRASH, &deleteImmediatelyNoTrash);
  rv = GetImapIncomingServer(getter_AddRefs(imapServer));
  
  if (NS_SUCCEEDED(rv) && imapServer)
  {
    imapServer->GetDeleteModel(&deleteModel);
    if (deleteModel != nsMsgImapDeleteModels::MoveToTrash || deleteStorage)
      deleteImmediatelyNoTrash = PR_TRUE;
    // if we're deleting a message, we should pseudo-interrupt the msg
    //load of the current message.
    PRBool interrupted = PR_FALSE;
    imapServer->PseudoInterruptMsgLoad(this, msgWindow, &interrupted);
  }
  
  rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);
  if (NS_FAILED(rv)) return rv;
  
  
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsCOMPtr<nsIMsgFolder> trashFolder;
  
  if (!deleteImmediatelyNoTrash)
  {
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_SUCCEEDED(rv) && rootFolder)
    {
      PRUint32 numFolders = 0;
      rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH,
        1, &numFolders,
        getter_AddRefs(trashFolder));

      NS_ASSERTION(NS_SUCCEEDED(rv) && trashFolder != 0, "couldn't find trash");
      
      // if we can't find the trash, we'll just have to do an imap delete and pretend this is the trash
      if (NS_FAILED(rv) || !trashFolder)
        deleteImmediatelyNoTrash = PR_TRUE;
    }
  }
  
  if ((NS_SUCCEEDED(rv) && deleteImmediatelyNoTrash) || deleteModel == nsMsgImapDeleteModels::IMAPDelete )
  {
    if (allowUndo)
    {
      //need to take care of these two delete models
      nsImapMoveCopyMsgTxn* undoMsgTxn = new nsImapMoveCopyMsgTxn(
        this, &srcKeyArray, messageIds.get(), nsnull,
        PR_TRUE, isMove, m_eventQueue, nsnull);
      if (!undoMsgTxn) return NS_ERROR_OUT_OF_MEMORY;
      undoMsgTxn->SetTransactionType(nsIMessenger::eDeleteMsg);
      // we're adding this undo action before the delete is successful. This is evil,
      // but 4.5 did it as well.
      nsCOMPtr <nsITransactionManager> txnMgr;
      if (msgWindow)
        msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
      if (txnMgr)
        txnMgr->DoTransaction(undoMsgTxn);
    }
    
    if (deleteModel == nsMsgImapDeleteModels::IMAPDelete && !deleteStorage)
    {
      PRUint32 cnt, flags;
      rv = messages->Count(&cnt);
      NS_ENSURE_SUCCESS(rv, rv);
      deleteMsgs = PR_FALSE;
      for (PRUint32 i=0; i <cnt; i++)
      {
        nsCOMPtr <nsIMsgDBHdr> msgHdr = do_QueryElementAt(messages, i);
        if (msgHdr)
        {
          msgHdr->GetFlags(&flags);
          if (!(flags & MSG_FLAG_IMAP_DELETED))
          {
            deleteMsgs = PR_TRUE;
            break;
          }
        }
      }
    }
    if (deleteMsgs)
      messageFlags |= kImapMsgSeenFlag;
    rv = StoreImapFlags(messageFlags, deleteMsgs, srcKeyArray.GetArray(), srcKeyArray.GetSize());
    
    if (NS_SUCCEEDED(rv))
    {
      if (mDatabase)
      {
        if (deleteModel == nsMsgImapDeleteModels::IMAPDelete)
        {

          MarkMessagesImapDeleted(&srcKeyArray, deleteMsgs, mDatabase);
        }
        else
        {
          EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_TRUE /*dbBatching*/);  //"remove it immediately" model
          mDatabase->DeleteMessages(&srcKeyArray,nsnull);
          EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE /*dbBatching*/);
          NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);            
        }    
      }   
    }
    return rv;
  }
  else  // have to move the messages to the trash
  {
    if(trashFolder)
    {
      nsCOMPtr<nsIMsgFolder> srcFolder;
      nsCOMPtr<nsISupports>srcSupport;
      PRUint32 count = 0;
      rv = messages->Count(&count);
      
      rv = QueryInterface(NS_GET_IID(nsIMsgFolder),
        getter_AddRefs(srcFolder));
      
      nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = copyService->CopyMessages(srcFolder, messages, trashFolder, PR_TRUE, listener, msgWindow, allowUndo);
    }
  }
  return rv;
}

// check if folder is the trash, or a descendent of the trash
// so we can tell if the folders we're deleting from it should
// be *really* deleted.
PRBool
nsImapMailFolder::TrashOrDescendentOfTrash(nsIMsgFolder* folder)
{
    nsCOMPtr<nsIMsgFolder> parent;
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsresult rv;
    PRUint32 flags = 0;

    if (!folder) return PR_FALSE;
    curFolder = do_QueryInterface(folder, &rv);
    if (NS_FAILED(rv)) return PR_FALSE;

    do 
    {
        rv = curFolder->GetFlags(&flags);
        if (NS_FAILED(rv)) return PR_FALSE;
        if (flags & MSG_FOLDER_FLAG_TRASH)
            return PR_TRUE;
        rv = curFolder->GetParentMsgFolder(getter_AddRefs(parent));
        if (NS_FAILED(rv)) return PR_FALSE;
        curFolder = do_QueryInterface(parent, &rv);
    } while (NS_SUCCEEDED(rv) && curFolder);

    return PR_FALSE;
}
NS_IMETHODIMP
nsImapMailFolder::DeleteSubFolders(nsISupportsArray* folders, nsIMsgWindow *msgWindow)
{
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsCOMPtr<nsIUrlListener> urlListener;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    PRInt32 i;
    PRUint32 folderCount = 0;
    nsresult rv;
    // "this" is the folder we're deleting from
    PRBool deleteNoTrash = TrashOrDescendentOfTrash(this) || !DeleteIsMoveToTrash();
    PRBool confirmed = PR_FALSE;
    PRBool confirmDeletion = PR_TRUE;


    (void) folders->Count(&folderCount);

    for (i = folderCount - 1; i >= 0; i--)
    {
      curFolder = do_QueryElementAt(folders, i, &rv);
      if (NS_SUCCEEDED(rv))
      {
        PRUint32 folderFlags;
        curFolder->GetFlags(&folderFlags);
        if (folderFlags & MSG_FOLDER_FLAG_VIRTUAL)
        {
          RemoveSubFolder(curFolder);
          folders->RemoveElementAt(i);
          // since the folder pane only allows single selection, we can do this
          deleteNoTrash = confirmed = PR_TRUE;
          confirmDeletion = PR_FALSE;
        }
      }
    }

    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      if (!deleteNoTrash)
      {
        rv = GetTrashFolder(getter_AddRefs(trashFolder));

        //If we can't find the trash folder and we are supposed to move it to the trash
        //return failure.
        if(NS_FAILED(rv) || !trashFolder)
          return NS_ERROR_FAILURE;

         PRBool canHaveSubFoldersOfTrash = PR_TRUE;
         trashFolder->GetCanCreateSubfolders(&canHaveSubFoldersOfTrash);
         if (canHaveSubFoldersOfTrash) // UW server doesn't set NOINFERIORS - check dual use pref
         {
           nsCOMPtr<nsIImapIncomingServer> imapServer;
           rv = GetImapIncomingServer(getter_AddRefs(imapServer));

           if (NS_SUCCEEDED(rv) && imapServer) 
           {
             PRBool serverSupportsDualUseFolders;
             imapServer->GetDualUseFolders(&serverSupportsDualUseFolders);
             if (!serverSupportsDualUseFolders)
               canHaveSubFoldersOfTrash = PR_FALSE;
           }
         }
         if (!canHaveSubFoldersOfTrash)
           deleteNoTrash = PR_TRUE;

         nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
         if (NS_SUCCEEDED(rv))
           prefBranch->GetBoolPref("mailnews.confirm.moveFoldersToTrash", &confirmDeletion);
      }
      if (!confirmed && (confirmDeletion || deleteNoTrash)) //let us alert the user if we are deleting folder immediately
      {
        nsXPIDLString confirmationStr;
        IMAPGetStringByID(((!deleteNoTrash) ? IMAP_MOVE_FOLDER_TO_TRASH : IMAP_DELETE_NO_TRASH),
        getter_Copies(confirmationStr));

        if (!msgWindow) 
          return NS_ERROR_NULL_POINTER;
        nsCOMPtr<nsIDocShell> docShell;
        msgWindow->GetRootDocShell(getter_AddRefs(docShell));

        nsCOMPtr<nsIPrompt> dialog;
        if (docShell) 
          dialog = do_GetInterface(docShell);

        if (dialog && confirmationStr)
          dialog->Confirm(nsnull, confirmationStr, &confirmed);
      }
      else
        confirmed = PR_TRUE;

      if (confirmed)
      {
        for (i = 0; i < folderCount; i++)
        {
          curFolder = do_QueryElementAt(folders, i, &rv);
          if (NS_SUCCEEDED(rv))
          {
            urlListener = do_QueryInterface(curFolder);
            if (deleteNoTrash)
              rv = imapService->DeleteFolder(m_eventQueue,
                                             curFolder,
                                             urlListener,
                                             nsnull);
            else
            {
              PRBool confirm = PR_FALSE;
              PRBool match = PR_FALSE;
              rv = curFolder->MatchOrChangeFilterDestination(nsnull, PR_FALSE, &match);
              if (match)
              {
                curFolder->ConfirmFolderDeletionForFilter(msgWindow, &confirm);
                if (!confirm) 
                  return NS_OK;
              }
              rv = imapService->MoveFolder(m_eventQueue,
                                           curFolder,
                                           trashFolder,
                                           urlListener,
                                           msgWindow,
                                           nsnull);
            }
          }
        }
      }
    }
    
    if (confirmed && deleteNoTrash)   //delete subfolders only if you are  deleting things from trash
        return nsMsgDBFolder::DeleteSubFolders(folders, msgWindow);
    else
        return rv;
}

// Called by Biff, or when user presses GetMsg button.
NS_IMETHODIMP nsImapMailFolder::GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener)
{
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));

  if(NS_SUCCEEDED(rv) && rootFolder) 
  {

    nsCOMPtr<nsIImapIncomingServer> imapServer;
    GetImapIncomingServer(getter_AddRefs(imapServer));
 
    PRBool performingBiff = PR_FALSE;

    if (imapServer)
    {
      nsCOMPtr<nsIMsgIncomingServer> incomingServer = do_QueryInterface(imapServer, &rv);
      if (incomingServer)
        incomingServer->GetPerformingBiff(&performingBiff);
    }

    // Check preferences to see if we should check all folders for new 
    // messages, or just the inbox and marked ones
    PRBool checkAllFolders = PR_FALSE;

    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && prefBranch) 
      // This pref might not exist, which is OK. We'll only check inbox and marked ones
      rv = prefBranch->GetBoolPref("mail.check_all_imap_folders_for_new", &checkAllFolders); 

    m_urlListener = aListener;                                                  

    // Get new messages for inbox
    PRUint32 numFolders;
    nsCOMPtr<nsIMsgFolder> inbox;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
    if (inbox)
    {
      nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(inbox, &rv);
      if (imapFolder)
        imapFolder->SetPerformingBiff(performingBiff);

      inbox->SetGettingNewMessages(PR_TRUE);
      rv = inbox->UpdateFolder(aWindow);
    }

    // Get new messages for other folders if marked, or all of them if the pref is set
    if (imapServer)
      rv = imapServer->GetNewMessagesForNonInboxFolders(rootFolder, aWindow, checkAllFolders, performingBiff);
  }

  return rv;
}

NS_IMETHODIMP nsImapMailFolder::Shutdown(PRBool shutdownChildren)
{
  m_filterList = nsnull;
  m_initialized = PR_FALSE;
  // m_pathName is used to decide if folder pathname needs to be reconstructed in GetPath().
  delete m_pathName;
  m_pathName = nsnull; 
  NS_IF_RELEASE(m_moveCoalescer);
  return nsMsgDBFolder::Shutdown(shutdownChildren);
}

nsresult nsImapMailFolder::GetBodysToDownload(nsMsgKeyArray *keysOfMessagesToDownload)
{
  NS_ENSURE_ARG(keysOfMessagesToDownload);

  nsresult rv = NS_ERROR_NULL_POINTER; // if mDatabase not set

  if (mDatabase)
  {
    nsCOMPtr <nsISimpleEnumerator> enumerator;
    rv = mDatabase->EnumerateMessages(getter_AddRefs(enumerator));
    if (NS_SUCCEEDED(rv) && enumerator)
    {
      PRBool hasMore;

      while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE)) 
      {
        nsCOMPtr <nsIMsgDBHdr> pHeader;
        rv = enumerator->GetNext(getter_AddRefs(pHeader));
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
        if (pHeader && NS_SUCCEEDED(rv))
        {
          PRBool shouldStoreMsgOffline = PR_FALSE;
          nsMsgKey msgKey;
          pHeader->GetMessageKey(&msgKey);
          // MsgFitsDownloadCriteria ignores MSG_FOLDER_FLAG_OFFLINE, which we want
          if (m_downloadingFolderForOfflineUse)
            MsgFitsDownloadCriteria(msgKey, &shouldStoreMsgOffline);
          else
            ShouldStoreMsgOffline(msgKey, &shouldStoreMsgOffline);
          if (shouldStoreMsgOffline)
            keysOfMessagesToDownload->Add(msgKey);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::OnNewIdleMessages()
{
  PRBool checkAllFolders = PR_FALSE;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) 
    // This pref might not exist, which is OK. 
    (void) prefBranch->GetBoolPref("mail.check_all_imap_folders_for_new", &checkAllFolders); 

  // only trigger biff if we're checking all new folders for new messages, or this particular folder,
  // but excluding trash,junk, sent, and no select folders, by default.
  if ((checkAllFolders && 
    !(mFlags & (MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_JUNK | MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_IMAP_NOSELECT)))
    || (mFlags & (MSG_FOLDER_FLAG_CHECK_NEW|MSG_FOLDER_FLAG_INBOX)))
    SetPerformingBiff(PR_TRUE);

  return UpdateFolder(nsnull);
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxInfo(
  nsIImapProtocol* aProtocol, nsIMailboxSpec* aSpec)
{
  nsresult rv = NS_ERROR_FAILURE;
  ChangeNumPendingTotalMessages(-GetNumPendingTotalMessages());
  ChangeNumPendingUnread(-GetNumPendingUnread());
  m_numStatusRecentMessages = 0; // clear this since we selected the folder.
  m_numStatusUnseenMessages = 0; // clear this since we selected the folder.

  
  if (!mDatabase)
    GetDatabase(nsnull);
  
  PRBool folderSelected;
  rv = aSpec->GetFolderSelected(&folderSelected);
  if (NS_SUCCEEDED(rv) && folderSelected)
  {
    nsMsgKeyArray existingKeys;
    nsMsgKeyArray keysToDelete;
    nsMsgKeyArray keysToFetch;
    nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
    PRInt32 imapUIDValidity = 0;
    
    rv = NS_ERROR_UNEXPECTED;
    if (mDatabase)
      rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
    
    if (NS_SUCCEEDED(rv) && dbFolderInfo)
      dbFolderInfo->GetImapUidValidity(&imapUIDValidity);
    
    if (mDatabase) 
    {
      mDatabase->ListAllKeys(existingKeys);
      if (mDatabase->ListAllOfflineDeletes(&existingKeys) > 0)
        existingKeys.QuickSort();
    }
    PRInt32 folderValidity;
    aSpec->GetFolder_UIDVALIDITY(&folderValidity);
    
    nsCOMPtr <nsIImapFlagAndUidState> flagState;
    
    aSpec->GetFlagState(getter_AddRefs(flagState));
    
    // remember what the supported user flags are.
    PRUint32 supportedUserFlags;
    aSpec->GetSupportedUserFlags(&supportedUserFlags);
    SetSupportedUserFlags(supportedUserFlags);

    m_uidValidity = folderValidity;

    if ((imapUIDValidity != folderValidity) /* && // if UIDVALIDITY Changed 
      !NET_IsOffline() */)
    {
      nsCOMPtr<nsIFileSpec> pathSpec;
      rv = GetPath(getter_AddRefs(pathSpec));
      if (NS_FAILED(rv)) return rv;
      
      nsFileSpec dbName;
      rv = pathSpec->GetFileSpec(&dbName);
      if (NS_FAILED(rv)) return rv;
      
      nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      
      nsCOMPtr <nsIDBFolderInfo> transferInfo;
      if (dbFolderInfo)
        dbFolderInfo->GetTransferInfo(getter_AddRefs(transferInfo));
      if (mDatabase)
      {
        dbFolderInfo = nsnull;
        mDatabase->ForceClosed();
      }
      mDatabase = nsnull;
      
      nsLocalFolderSummarySpec  summarySpec(dbName);
      // Remove summary file.
      summarySpec.Delete(PR_FALSE);
      
      // Create a new summary file, update the folder message counts, and
      // Close the summary file db.
      rv = msgDBService->OpenFolderDB(this, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase));
      
      // ********** Important *************
      // David, help me here I don't know this is right or wrong
      if (rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
        rv = NS_OK;
      
      if (NS_FAILED(rv) && mDatabase)
      {
        mDatabase->ForceClosed();
        mDatabase = nsnull;
      }
      else if (NS_SUCCEEDED(rv) && mDatabase)
      {
        if (transferInfo)
          SetDBTransferInfo(transferInfo);
        
        SummaryChanged();
        rv = NS_ERROR_UNEXPECTED;
        if (mDatabase) 
        {
          if(mAddListener)
            mDatabase->AddListener(this);
          rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
        }
      }
      // store the new UIDVALIDITY value
      
      if (NS_SUCCEEDED(rv) && dbFolderInfo)
        dbFolderInfo->SetImapUidValidity(folderValidity);
      // delete all my msgs, the keys are bogus now
      // add every message in this folder
      existingKeys.RemoveAll();
      //      keysToDelete.CopyArray(&existingKeys);
      
      if (flagState)
      {
        nsMsgKeyArray no_existingKeys;
        FindKeysToAdd(no_existingKeys, keysToFetch, flagState);
      }
      if (NS_FAILED(rv))
        dbName.Delete(PR_FALSE);
      
    }   
    else if (!flagState /*&& !NET_IsOffline() */) // if there are no messages on the server
    {
      keysToDelete.CopyArray(&existingKeys);
    }
    else /* if ( !NET_IsOffline()) */
    {
      FindKeysToDelete(existingKeys, keysToDelete, flagState);
      
      PRUint32 boxFlags;
      
      aSpec->GetBox_flags(&boxFlags);
      // if this is the result of an expunge then don't grab headers
      if (!(boxFlags & kJustExpunged))
        FindKeysToAdd(existingKeys, keysToFetch, flagState);
    }
    
    
    if (keysToDelete.GetSize())
    {
      PRUint32 total;
      
      // It would be nice to notify RDF or whoever of a mass delete here.
      if (mDatabase) 
      {
        mDatabase->DeleteMessages(&keysToDelete, nsnull);
        total = keysToDelete.GetSize();
      }
    }
    // If we are performing biff for this folder, tell the
    // stand-alone biff about the new high water mark
    if (m_performingBiff)
    {
      if (keysToFetch.GetSize() > 0)
      {
        // We must ensure that the server knows that we are performing biff.
        // Otherwise the stand-alone biff won't fire.
        nsCOMPtr<nsIMsgIncomingServer> server;
        if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
          server->SetPerformingBiff(PR_TRUE);
        
        SetNumNewMessages(keysToFetch.GetSize());
      }
    }
    SyncFlags(flagState);
    PRInt32 numUnreadFromServer;
    aSpec->GetNumUnseenMessages(&numUnreadFromServer);
    if (mDatabase&&
        mNumUnreadMessages + keysToFetch.GetSize() > numUnreadFromServer)
      mDatabase->SyncCounts();

    if (keysToFetch.GetSize())
    {     
      PrepareToAddHeadersToMailDB(aProtocol, keysToFetch, aSpec);
    }
    else 
    {
      // let the imap libnet module know that we don't need headers
      if (aProtocol)
        aProtocol->NotifyHdrsToDownload(nsnull, 0);
      PRBool gettingNewMessages;
      GetGettingNewMessages(&gettingNewMessages);
      if (gettingNewMessages)
        ProgressStatus(aProtocol,IMAP_NO_NEW_MESSAGES, nsnull);
    }
  }
  
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxStatus(
  nsIImapProtocol* aProtocol, nsIMailboxSpec* aSpec)
{
  NS_ENSURE_ARG_POINTER(aSpec);
  PRInt32 numRecent, numUnread;
  aSpec->GetNumRecentMessages(&numRecent);
  aSpec->GetNumUnseenMessages(&numUnread);
  // If m_numStatusUnseenMessages is 0, it means
  // this is the first time we've done a Status.
  // In that case, we count all the previous pending unread messages we know about
  // as unread messages. 
  // We may want to do similar things with total messages, but the total messages
  // include deleted messages if the folder hasn't been expunged.
  PRInt32 previousUnreadMessages = (m_numStatusUnseenMessages) 
    ? m_numStatusUnseenMessages : GetNumPendingUnread() + mNumUnreadMessages;
  if (numUnread != previousUnreadMessages)
  {
    // we're going to assume that recent messages are unread.
    ChangeNumPendingUnread(numUnread - previousUnreadMessages);
    ChangeNumPendingTotalMessages(numUnread - previousUnreadMessages);
    if (numUnread > previousUnreadMessages)
    {
      SetHasNewMessages(PR_TRUE);
      SetNumNewMessages(numUnread - previousUnreadMessages);
      SetBiffState(nsMsgBiffState_NewMail);
    }
    SummaryChanged();
  }
  SetPerformingBiff(PR_FALSE);
  m_numStatusUnseenMessages = numUnread;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::ParseMsgHdrs(nsIImapProtocol *aProtocol, nsIImapHeaderXferInfo *aHdrXferInfo)
{
  PRInt32 numHdrs;
  nsCOMPtr <nsIImapHeaderInfo> headerInfo;

  if (!mDatabase)
    GetDatabase(nsnull);
  
  nsresult rv = aHdrXferInfo->GetNumHeaders(&numHdrs);
  for (PRUint32 i = 0; NS_SUCCEEDED(rv) && i < numHdrs; i++)
  {

    rv = aHdrXferInfo->GetHeader(i, getter_AddRefs(headerInfo));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!headerInfo)
      break;
    PRInt32 msgSize;
    nsMsgKey msgKey;
    PRBool containsKey;
    const char *msgHdrs;
    headerInfo->GetMsgSize(&msgSize);
    headerInfo->GetMsgUid(&msgKey);
    if (msgKey == nsMsgKey_None) // not a valid uid.
      continue;
    if (mDatabase && NS_SUCCEEDED(mDatabase->ContainsKey(msgKey, &containsKey)) && containsKey)
    {
      NS_ASSERTION(PR_FALSE, "downloading hdrs for hdr we already have");
      continue;
    }
    nsresult rv = SetupHeaderParseStream(msgSize, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    headerInfo->GetMsgHdrs(&msgHdrs);
    rv = ParseAdoptedHeaderLine(msgHdrs, msgKey);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = NormalEndHeaderParseStream(aProtocol);
  }

  return rv;
}

nsresult nsImapMailFolder::SetupHeaderParseStream(PRUint32 aSize, 
                                                  const char *content_type, nsIMailboxSpec *boxSpec)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if (!mDatabase)
    GetDatabase(nsnull);
  
  m_nextMessageByteLength = aSize;
  if (!m_msgParser)
  {
    rv = nsComponentManager::CreateInstance(kParseMailMsgStateCID, nsnull, 
      NS_GET_IID(nsIMsgParseMailMsgState), (void **) getter_AddRefs(m_msgParser));
  }
  else
    m_msgParser->Clear();
  
  if (m_msgParser)
  {
    m_msgParser->SetMailDB(mDatabase);
    return
      m_msgParser->SetState(nsIMsgParseMailMsgState::ParseHeadersState);
  }
  else
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsresult nsImapMailFolder::ParseAdoptedHeaderLine(const char *aMessageLine, PRUint32 aMsgKey)
{
  // we can get blocks that contain more than one line, 
  // but they never contain partial lines
  const char *str = aMessageLine;
  m_curMsgUid = aMsgKey;
  m_msgParser->SetEnvelopePos(m_curMsgUid);
  // m_envelope_pos, for local folders, 
  // is the msg key. Setting this will set the msg key for the new header.
  
  PRInt32 len = strlen(str);
  char *currentEOL  = PL_strstr(str, MSG_LINEBREAK);
  const char *currentLine = str;
  while (currentLine < (str + len))
  {
    if (currentEOL)
    {
      m_msgParser->ParseAFolderLine(currentLine, 
        (currentEOL + MSG_LINEBREAK_LEN) -
        currentLine);
      currentLine = currentEOL + MSG_LINEBREAK_LEN;
      currentEOL  = PL_strstr(currentLine, MSG_LINEBREAK);
    }
    else
    {
      m_msgParser->ParseAFolderLine(currentLine, PL_strlen(currentLine));
      currentLine = str + len + 1;
    }
  }
  return NS_OK;
}
    
nsresult nsImapMailFolder::NormalEndHeaderParseStream(nsIImapProtocol*
                                                           aProtocol)
{
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;
  nsresult rv = NS_OK;

  if (m_msgParser)
  {
    nsMailboxParseState parseState;
    m_msgParser->GetState(&parseState);
    if (parseState == nsIMsgParseMailMsgState::ParseHeadersState)
    m_msgParser->ParseAFolderLine(CRLF, 2);
    m_msgParser->GetNewMsgHdr(getter_AddRefs(newMsgHdr));
  }
  if (NS_SUCCEEDED(rv) && newMsgHdr)
  {
    char *headers;
    PRInt32 headersSize;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv)) // don't use NS_ENSURE_SUCCESS here; it's not a fatal error
    {
      nsXPIDLCString redirectorType;
      server->GetRedirectorType(getter_Copies(redirectorType));

      // only notify redirected type servers of new hdrs for performance
      if (!redirectorType.IsEmpty())
        NotifyFolderEvent(mImapHdrDownloadedAtom);
    }
    newMsgHdr->SetMessageKey(m_curMsgUid);
    TweakHeaderFlags(aProtocol, newMsgHdr);
    PRUint32 messageSize;
    if (NS_SUCCEEDED(newMsgHdr->GetMessageSize(&messageSize)))
      mFolderSize += messageSize;
    m_msgMovedByFilter = PR_FALSE;
    // If this is the inbox, try to apply filters.
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
    {
      PRUint32 msgFlags;

      newMsgHdr->GetFlags(&msgFlags);
      if (!(msgFlags & (MSG_FLAG_READ | MSG_FLAG_IMAP_DELETED))) // only fire on unread msgs that haven't been deleted
      {
        rv = m_msgParser->GetAllHeaders(&headers, &headersSize);

        if (NS_SUCCEEDED(rv) && headers)
        {
          if (m_filterList)
          {
            nsCOMPtr <nsIMsgWindow> msgWindow;
            if (aProtocol)
            {
              nsCOMPtr <nsIImapUrl> aImapUrl;
              nsCOMPtr <nsIMsgMailNewsUrl> msgUrl;
              rv = aProtocol->GetRunningImapURL(getter_AddRefs(aImapUrl));
              if (NS_SUCCEEDED(rv) && aImapUrl)
              {
                msgUrl = do_QueryInterface(aImapUrl);
                if (msgUrl)
                  msgUrl->GetMsgWindow(getter_AddRefs(msgWindow));
              }

            }
            GetMoveCoalescer();  // not sure why we're doing this here.
            m_filterList->ApplyFiltersToHdr(nsMsgFilterType::InboxRule, newMsgHdr, this, mDatabase, 
                                            headers, headersSize, this, msgWindow);
          }
        }
      }
    }
    // here we need to tweak flags from uid state..
    if (mDatabase && (!m_msgMovedByFilter || ShowDeletedMessages()))
      mDatabase->AddNewHdrToDB(newMsgHdr, PR_TRUE);
    m_msgParser->Clear(); // clear out parser, because it holds onto a msg hdr.
    m_msgParser->SetMailDB(nsnull); // tell it to let go of the db too.
    // I don't think we want to do this - it does bad things like set the size incorrectly.
//    m_msgParser->FinishHeader();
  }
    return NS_OK;
}
    
NS_IMETHODIMP nsImapMailFolder::AbortHeaderParseStream(nsIImapProtocol*
                                                       aProtocol)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::BeginCopy(nsIMsgDBHdr *message)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!m_copyState) 
    return rv;
  if (m_copyState->m_tmpFileSpec) // leftover file spec nuke it
  {
    PRBool isOpen = PR_FALSE;
    rv = m_copyState->m_tmpFileSpec->IsStreamOpen(&isOpen);
    if (isOpen)
      m_copyState->m_tmpFileSpec->CloseStream();
    nsFileSpec fileSpec;
    m_copyState->m_tmpFileSpec->GetFileSpec(&fileSpec);
    if (fileSpec.Valid())
      fileSpec.Delete(PR_FALSE);
    m_copyState->m_tmpFileSpec = nsnull;
  }
  if (message)
    m_copyState->m_message = do_QueryInterface(message, &rv);

  nsSpecialSystemDirectory tmpFileSpec(nsSpecialSystemDirectory::OS_TemporaryDirectory);
  
  tmpFileSpec += "nscpmsg.txt";
  tmpFileSpec.MakeUnique();
  rv = NS_NewFileSpecWithSpec(tmpFileSpec,
                                getter_AddRefs(m_copyState->m_tmpFileSpec));
  if (NS_SUCCEEDED(rv) && m_copyState->m_tmpFileSpec)
    rv = m_copyState->m_tmpFileSpec->OpenStreamForWriting();

  if (!m_copyState->m_dataBuffer)
    m_copyState->m_dataBuffer = (char*) PR_CALLOC(COPY_BUFFER_SIZE+1);
  if (!m_copyState->m_dataBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
  m_copyState->m_dataBufferSize = COPY_BUFFER_SIZE;

  return rv;
}

NS_IMETHODIMP nsImapMailFolder::CopyData(nsIInputStream *aIStream,
                     PRInt32 aLength)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  NS_ASSERTION(m_copyState && m_copyState->m_tmpFileSpec 
                  && m_copyState->m_dataBuffer, "Fatal copy operation error\n");
  if (!m_copyState || !m_copyState->m_tmpFileSpec || !m_copyState->m_dataBuffer) 
    return rv;

  PRUint32 readCount;
  PRInt32 writeCount;

  if ( aLength + m_copyState->m_leftOver > m_copyState->m_dataBufferSize )
  {
    m_copyState->m_dataBuffer = (char *) PR_REALLOC(m_copyState->m_dataBuffer, aLength + m_copyState->m_leftOver+ 1);
    if (!m_copyState->m_dataBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
    m_copyState->m_dataBufferSize = aLength + m_copyState->m_leftOver;
  }

  char *start, *end;
  PRUint32 linebreak_len = 1;

  rv = aIStream->Read(m_copyState->m_dataBuffer+m_copyState->m_leftOver, aLength, &readCount);
  if (NS_FAILED(rv)) 
    return rv;

  m_copyState->m_leftOver += readCount;
  m_copyState->m_dataBuffer[m_copyState->m_leftOver] = '\0';

  start = m_copyState->m_dataBuffer;
  if (m_copyState->m_eatLF)
  {
    if (*start == nsCRT::LF)
      start++;
    m_copyState->m_eatLF = PR_FALSE;
  }
  end = PL_strchr(start, '\r');
  if (!end)
    end = PL_strchr(start, '\n');
  else if (*(end+1) == nsCRT::LF)
    linebreak_len = 2;

  while (start && end)
  {
    if (PL_strncasecmp(start, "X-Mozilla-Status:", 17) &&
        PL_strncasecmp(start, "X-Mozilla-Status2:", 18) &&
        PL_strncmp(start, "From - ", 7))
    {
      rv = m_copyState->m_tmpFileSpec->Write(start,
                                             end-start,
                                             &writeCount);
      rv = m_copyState->m_tmpFileSpec->Write(CRLF, 2, &writeCount);
    }
    start = end+linebreak_len;
    if (start >=
        m_copyState->m_dataBuffer+m_copyState->m_leftOver)
    {
       m_copyState->m_leftOver = 0;
       break;
    }
    linebreak_len = 1;

    end = PL_strchr(start, '\r');
    if (!end)
      end = PL_strchr(start, '\n');
    else if (*(end+1) == nsCRT::LF)
      linebreak_len = 2;
    else if (! *(end+1)) // block might have split CRLF so remember if
      m_copyState->m_eatLF = PR_TRUE; // we should eat LF

    if (start && !end)
    {
      m_copyState->m_leftOver -= (start - m_copyState->m_dataBuffer);
      memcpy(m_copyState->m_dataBuffer, start, m_copyState->m_leftOver+1); // including null
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::EndCopy(PRBool copySucceeded)
{
  nsresult rv = copySucceeded ? NS_OK : NS_ERROR_FAILURE;
    if (copySucceeded && m_copyState && m_copyState->m_tmpFileSpec)
    {
        nsCOMPtr<nsIUrlListener> urlListener;
        m_copyState->m_tmpFileSpec->Flush();
        m_copyState->m_tmpFileSpec->CloseStream();
        
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        rv = QueryInterface(NS_GET_IID(nsIUrlListener),
                            getter_AddRefs(urlListener));
        nsCOMPtr<nsISupports> copySupport;
        if (m_copyState)
            copySupport = do_QueryInterface(m_copyState);
        rv = imapService->AppendMessageFromFile(m_eventQueue,
                                                m_copyState->m_tmpFileSpec,
                                                this, "", PR_TRUE,
                                                m_copyState->m_selectedState,
                                                urlListener, nsnull,
                                                copySupport,
                                                m_copyState->m_msgWindow);
		
    }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::EndMove(PRBool moveSucceeded)
{
  return NS_OK;
}
// this is the beginning of the next message copied
NS_IMETHODIMP nsImapMailFolder::StartMessage()
{
  return NS_OK;
}

// just finished the current message.
NS_IMETHODIMP nsImapMailFolder::EndMessage(nsMsgKey key)
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::ApplyFilterHit(nsIMsgFilter *filter, nsIMsgWindow *msgWindow, PRBool *applyMore)
{  
  NS_ENSURE_ARG_POINTER(applyMore);

  nsMsgRuleActionType actionType;
  nsXPIDLCString actionTargetFolderUri;
  PRUint32  newFlags;
  nsresult rv = NS_OK;

  // look at action - currently handle move
  #ifdef DEBUG_bienvenu
    printf("got a rule hit!\n");
  #endif

  nsCOMPtr<nsIMsgDBHdr> msgHdr;

  if (m_msgParser)
    m_msgParser->GetNewMsgHdr(getter_AddRefs(msgHdr));

  if (!msgHdr)
    return NS_ERROR_NULL_POINTER; //fatal error, cannot apply filters

  PRBool deleteToTrash = DeleteIsMoveToTrash();

  nsCOMPtr<nsISupportsArray> filterActionList;
  rv = NS_NewISupportsArray(getter_AddRefs(filterActionList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = filter->GetSortedActionList(filterActionList);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numActions;
  rv = filterActionList->Count(&numActions);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool loggingEnabled = PR_FALSE;
  if (m_filterList && numActions)
    (void)m_filterList->GetLoggingEnabled(&loggingEnabled);

  PRBool msgIsNew = PR_TRUE;

  for (PRUint32 actionIndex = 0; actionIndex < numActions && *applyMore; actionIndex++)
  {
    nsCOMPtr<nsIMsgRuleAction> filterAction;
    filterActionList->QueryElementAt(actionIndex, NS_GET_IID(nsIMsgRuleAction), getter_AddRefs(filterAction));
    if (!filterAction)
      continue;
    if (NS_SUCCEEDED(filterAction->GetType(&actionType)))
    {
      if (actionType == nsMsgFilterAction::MoveToFolder)
      {
        filterAction->GetTargetFolderUri(getter_Copies(actionTargetFolderUri));
        if (actionTargetFolderUri.IsEmpty())
        {
          NS_ASSERTION(PR_FALSE, "actionTargetFolderUri is empty");
          continue;
        }
      }

      PRUint32 msgFlags;
      nsMsgKey    msgKey;
      nsCAutoString trashNameVal;

      msgHdr->GetFlags(&msgFlags);
      msgHdr->GetMessageKey(&msgKey);
      PRBool isRead = (msgFlags & MSG_FLAG_READ);
      switch (actionType)
      {
        case nsMsgFilterAction::Delete:
        {
          if (deleteToTrash)
          {
            // set value to trash folder
            nsCOMPtr <nsIMsgFolder> mailTrash;
            rv = GetTrashFolder(getter_AddRefs(mailTrash));
            if (NS_SUCCEEDED(rv) && mailTrash)
              rv = mailTrash->GetURI(getter_Copies(actionTargetFolderUri));

            // msgHdr->OrFlags(MSG_FLAG_READ, &newFlags);  // mark read in trash.
          }
          else  // (!deleteToTrash)
          {
            msgHdr->OrFlags(MSG_FLAG_READ | MSG_FLAG_IMAP_DELETED, &newFlags);
            nsMsgKeyArray keysToFlag;

            keysToFlag.Add(msgKey);
            StoreImapFlags(kImapMsgSeenFlag | kImapMsgDeletedFlag, PR_TRUE, keysToFlag.GetArray(), keysToFlag.GetSize());
            m_msgMovedByFilter = PR_TRUE; // this will prevent us from adding the header to the db.
          }
          msgIsNew = PR_FALSE;
        }
        // note that delete falls through to move.
        case nsMsgFilterAction::MoveToFolder:
        {
          // if moving to a different file, do it.
          nsXPIDLCString uri;
          rv = GetURI(getter_Copies(uri));

          if ((const char*)actionTargetFolderUri &&
            nsCRT::strcmp(uri, actionTargetFolderUri))
          {
            msgHdr->GetFlags(&msgFlags);

            if (msgFlags & MSG_FLAG_MDN_REPORT_NEEDED && !isRead)
            {
               msgHdr->SetFlags(msgFlags & ~MSG_FLAG_MDN_REPORT_NEEDED);
               msgHdr->OrFlags(MSG_FLAG_MDN_REPORT_SENT, &newFlags);
            }
            nsresult err = MoveIncorporatedMessage(msgHdr, mDatabase, actionTargetFolderUri, filter, msgWindow);
            if (NS_SUCCEEDED(err))
              m_msgMovedByFilter = PR_TRUE;
          }
          // don't apply any more filters, even if it was a move to the same folder
          *applyMore = PR_FALSE; 
        }
        break;
        case nsMsgFilterAction::MarkRead:
        {
          nsMsgKeyArray keysToFlag;
          keysToFlag.Add(msgKey);
          StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, keysToFlag.GetArray(), keysToFlag.GetSize());
          msgIsNew = PR_FALSE;
        }
        break;
        case nsMsgFilterAction::MarkFlagged:
        {
          nsMsgKeyArray keysToFlag;
          keysToFlag.Add(msgKey);
          StoreImapFlags(kImapMsgFlaggedFlag, PR_TRUE, keysToFlag.GetArray(), keysToFlag.GetSize());
        }
        break;
        case nsMsgFilterAction::KillThread:
          // The db will check for this flag when a hdr gets added to the db, and set the flag appropriately on the thread object
          msgHdr->OrFlags(MSG_FLAG_IGNORED, &newFlags);
          break;
        case nsMsgFilterAction::WatchThread:
          msgHdr->OrFlags(MSG_FLAG_WATCHED, &newFlags);
        break;
        case nsMsgFilterAction::ChangePriority:
        {
          nsMsgPriorityValue filterPriority;
          filterAction->GetPriority(&filterPriority);
          msgHdr->SetPriority(filterPriority);
        }
        break;
        case nsMsgFilterAction::Label:
        {
          nsMsgLabelValue filterLabel;
          filterAction->GetLabel(&filterLabel);
          msgHdr->SetLabel(filterLabel);
          nsMsgKeyArray keysToFlag;

          keysToFlag.Add(msgKey);
          StoreImapFlags((filterLabel << 9), PR_TRUE, keysToFlag.GetArray(), keysToFlag.GetSize());
        }
        break;
        case nsMsgFilterAction::JunkScore:
        {
          nsCAutoString junkScoreStr;
          PRInt32 junkScore;
          filterAction->GetJunkScore(&junkScore);
          junkScoreStr.AppendInt(junkScore);
          mDatabase->SetStringProperty(msgKey, "junkscore", junkScoreStr.get());
          mDatabase->SetStringProperty(msgKey, "junkscoreorigin", /* ### should this be plugin? */"plugin");
          if (junkScore == 100 || !junkScore) // if score is 0 or 100, set up to store junk status on server.
          {
            nsMsgKeyArray *keysToClassify = m_moveCoalescer->GetKeyBucket((junkScore == 100) ? 0 : 1);
            NS_ASSERTION(keysToClassify, "error getting key bucket");
            if (keysToClassify)
              keysToClassify->Add(msgKey);
          }
        }
        break;
        default:
          break;
      }
      if (loggingEnabled)
      {
        // only log if successful move, or non-move action
        if (m_msgMovedByFilter || (actionType != nsMsgFilterAction::MoveToFolder &&
             (actionType != nsMsgFilterAction::Delete || !deleteToTrash)))
          (void) filter->LogRuleHit(filterAction, msgHdr);
      }
    }
  }
  if (!msgIsNew)
  {
    PRInt32 numNewMessages;
    GetNumNewMessages(PR_FALSE, &numNewMessages);
    SetNumNewMessages(numNewMessages - 1);
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetImapFlags(const char *uids, PRInt32 flags, nsIURI **url)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return imapService->SetMessageFlags(m_eventQueue, this, this,
                                 url, uids, flags, PR_TRUE);
}

// "this" is the parent folder
NS_IMETHODIMP nsImapMailFolder::PlaybackOfflineFolderCreate(const PRUnichar *aFolderName, nsIMsgWindow *aWindow, nsIURI **url)
{
    NS_ENSURE_ARG_POINTER(aFolderName);

    nsresult rv;
    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    return imapService->CreateFolder(m_eventQueue, this, 
                                       aFolderName, this, url);
}

NS_IMETHODIMP nsImapMailFolder::ReplayOfflineMoveCopy(nsMsgKey *msgKeys, PRUint32 numKeys, PRBool isMove, nsIMsgFolder *aDstFolder,
                         nsIUrlListener *aUrlListener, nsIMsgWindow *aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr <nsIURI> resultUrl;
    nsCAutoString uids;
    AllocateUidStringFromKeys(msgKeys, numKeys, uids);
    rv = imapService->OnlineMessageCopy(m_eventQueue, 
                         this,
                         uids.get(),
                         aDstFolder,
                         PR_TRUE,
                         isMove,
                         aUrlListener,
                         getter_AddRefs(resultUrl), nsnull, aWindow);
    if (resultUrl)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(resultUrl);
      if (mailnewsUrl)
      {
        nsCOMPtr <nsIUrlListener> folderListener = do_QueryInterface(aDstFolder);
        if (folderListener)
          mailnewsUrl->RegisterListener(folderListener);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::StoreImapFlags(PRInt32 flags, PRBool addFlags, nsMsgKey *keys, PRUint32 numKeys)
{
  nsresult rv = NS_OK;
  if (!WeAreOffline())
  {
    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCAutoString msgIds;
      AllocateUidStringFromKeys(keys, numKeys, msgIds);

      if (addFlags)
      {
        imapService->AddMessageFlags(m_eventQueue, this, this,
                                     nsnull, msgIds.get(), flags, PR_TRUE);
      }
      else
      {
        imapService->SubtractMessageFlags(m_eventQueue, this, this,
                                          nsnull, msgIds.get(), flags,
                                          PR_TRUE);
      }
    }
  }
  else
  {
    GetDatabase(nsnull);
    if (mDatabase)
    {
      PRUint32 total = numKeys;

      for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
      {
        nsCOMPtr <nsIMsgOfflineImapOperation> op;
        rv = mDatabase->GetOfflineOpForKey(keys[keyIndex], PR_TRUE, getter_AddRefs(op));
        SetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
        if (NS_SUCCEEDED(rv) && op)
        {
          imapMessageFlagsType newFlags;
          op->GetNewFlags(&newFlags);

          if (addFlags)
            op->SetFlagOperation(newFlags | flags);
          else
            op->SetFlagOperation(newFlags & ~flags);
        }
      }
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit); // flush offline flags
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::LiteSelect(nsIUrlListener *aUrlListener)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return imapService->LiteSelectFolder(m_eventQueue, this, aUrlListener, nsnull);
}

nsresult nsImapMailFolder::GetFolderOwnerUserName(char **userName)
{  
  if ((mFlags & MSG_FOLDER_FLAG_IMAP_PERSONAL) ||
    !(mFlags & (MSG_FOLDER_FLAG_IMAP_PUBLIC | MSG_FOLDER_FLAG_IMAP_OTHER_USER)))
  {
    // this is one of our personal mail folders
    // return our username on this host
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
    return server->GetUsername(userName);
  else
    return rv;
  }
  
  // the only other type of owner is if it's in the other users' namespace
  if (!(mFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER))
    return NS_OK;
  
  if (m_ownerUserName.IsEmpty())
  {
    nsXPIDLCString onlineName;
    GetOnlineName(getter_Copies(onlineName));
    m_ownerUserName = nsIMAPNamespaceList::GetFolderOwnerNameFromPath(GetNamespaceForFolder(), onlineName.get());
  }
  *userName = !m_ownerUserName.IsEmpty() ? ToNewCString(m_ownerUserName) : nsnull;
  return NS_OK;
}

// returns the online folder name, with the other users' namespace and his username
// stripped out
nsresult nsImapMailFolder::GetOwnersOnlineFolderName(char **retName)
{
  nsXPIDLCString onlineName;

  GetOnlineName(getter_Copies(onlineName));
  if (mFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
  {
    nsXPIDLCString user;
    GetFolderOwnerUserName(getter_Copies(user));
    if (!onlineName.IsEmpty() && !user.IsEmpty())
    {
      const char *where = PL_strstr(onlineName.get(), user.get());
      NS_ASSERTION(where, "user name not in online name");
      if (where)
      {
        const char *relativeFolder = where + strlen(user) + 1;
        if (!relativeFolder)	// root of this user's personal namespace
        {
          *retName = PL_strdup("");
          return NS_OK;
        }
        else
        {
          *retName = PL_strdup(relativeFolder);
          return NS_OK;
        }
      }
    }

    *retName = PL_strdup(onlineName.get());
    return NS_OK;
  }
  else if (!(mFlags & MSG_FOLDER_FLAG_IMAP_PUBLIC))
  {
    // We own this folder.
    *retName = nsIMAPNamespaceList::GetFolderNameWithoutNamespace(GetNamespaceForFolder(), onlineName);
    
  }
  else
    *retName = PL_strdup(onlineName.get());
  return NS_OK;
}

nsIMAPNamespace *nsImapMailFolder::GetNamespaceForFolder()
{
  if (!m_namespace)
  {
#ifdef DEBUG_bienvenu
    // Make sure this isn't causing us to open the database
    NS_ASSERTION(m_hierarchyDelimiter != kOnlineHierarchySeparatorUnknown, "haven't set hierarchy delimiter");
#endif
    nsXPIDLCString serverKey;
    nsXPIDLCString onlineName;
    GetServerKey(getter_Copies(serverKey));
    GetOnlineName(getter_Copies(onlineName));
    PRUnichar hierarchyDelimiter;
    GetHierarchyDelimiter(&hierarchyDelimiter);

    m_namespace = nsIMAPNamespaceList::GetNamespaceForFolder(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter);
    NS_ASSERTION(m_namespace, "didn't get namespace for folder");
    if (m_namespace)
    {
      nsIMAPNamespaceList::SuggestHierarchySeparatorForNamespace(m_namespace, (char) hierarchyDelimiter);
      m_folderIsNamespace = nsIMAPNamespaceList::GetFolderIsNamespace(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter, m_namespace);
    }
  }
  return m_namespace;
}

void nsImapMailFolder::SetNamespaceForFolder(nsIMAPNamespace *ns)
{
#ifdef DEBUG_bienvenu
  NS_ASSERTION(ns, "null namespace");
#endif
  m_namespace = ns;
}

nsresult nsImapMailFolder::GetServerAdminUrl(char **aAdminUrl)
{
  nsCOMPtr<nsIImapIncomingServer> imapServer;
  nsresult rv = GetImapIncomingServer(getter_AddRefs(imapServer));

  if (NS_SUCCEEDED(rv) && imapServer) 
    rv = imapServer->GetManageMailAccountUrl(aAdminUrl);
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::FolderPrivileges(nsIMsgWindow *window)
{
  nsresult rv = NS_ERROR_NULL_POINTER;  // if no window...
  if (window) 
  {
    if (!m_adminUrl.IsEmpty())
    {
      nsCOMPtr <nsIDocShell> docShell;
      rv = window->GetRootDocShell(getter_AddRefs(docShell));
      if (NS_SUCCEEDED(rv) && docShell)
      {
        nsCOMPtr<nsIURI> uri;
        if (NS_FAILED(rv = NS_NewURI(getter_AddRefs(uri), m_adminUrl.get())))
          return rv;
        rv = docShell->LoadURI(uri, nsnull, nsIWebNavigation::LOAD_FLAGS_IS_LINK, PR_FALSE);

      }
    }
    else
    {
    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = imapService->GetFolderAdminUrl(m_eventQueue, this, window, this, nsnull);
    if (NS_SUCCEEDED(rv))
      m_urlRunning = PR_TRUE;
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetHasAdminUrl(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  nsXPIDLCString manageMailAccountUrl;
  nsresult rv = GetServerAdminUrl(getter_Copies(manageMailAccountUrl));
  *aBool = (NS_SUCCEEDED(rv) && !manageMailAccountUrl.IsEmpty());
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetAdminUrl(char **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = ToNewCString(m_adminUrl);
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetAdminUrl(const char *adminUrl)
{
  m_adminUrl = adminUrl;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetHdrParser(nsIMsgParseMailMsgState **aHdrParser)
{
  NS_ENSURE_ARG_POINTER(aHdrParser);
  NS_IF_ADDREF(*aHdrParser = m_msgParser);
  return NS_OK;
}

  // this is used to issue an arbitrary imap command on the passed in msgs.
  // It assumes the command needs to be run in the selected state.
NS_IMETHODIMP nsImapMailFolder::IssueCommandOnMsgs(const char *command, const char *uids, nsIMsgWindow *aWindow, nsIURI **url)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return imapService->IssueCommandOnMsgs(m_eventQueue, this, aWindow, command, uids, url);
}

NS_IMETHODIMP nsImapMailFolder::FetchCustomMsgAttribute(const char *attribute, const char *uids, nsIMsgWindow *aWindow, nsIURI **url)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  return imapService->FetchCustomMsgAttribute(m_eventQueue, this, aWindow, attribute, uids, url);
}

nsresult nsImapMailFolder::MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
                                                   nsIMsgDatabase *sourceDB, 
                                                   const nsACString& destFolderUri,
                                                   nsIMsgFilter *filter,
                                                   nsIMsgWindow *msgWindow)
{
  nsresult err = NS_OK;
  
  if (m_moveCoalescer)
  {

    nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &err)); 
    nsCOMPtr<nsIRDFResource> res;
    err = rdf->GetResource(destFolderUri, getter_AddRefs(res));
    if (NS_FAILED(err))
      return err;

    nsCOMPtr<nsIMsgFolder> destIFolder(do_QueryInterface(res, &err));
    if (NS_FAILED(err))
      return err;        

    if (destIFolder)
    {
      // check if the destination is a real folder (by checking for null parent)
      // and if it can file messages (e.g., servers or news folders can't file messages).
      // Or read only imap folders...
      PRBool canFileMessages = PR_TRUE;
      nsCOMPtr<nsIMsgFolder> parentFolder;
      destIFolder->GetParent(getter_AddRefs(parentFolder));
      if (parentFolder)
        destIFolder->GetCanFileMessages(&canFileMessages);
      if (!parentFolder || !canFileMessages)
      {
        filter->SetEnabled(PR_FALSE);
        destIFolder->ThrowAlertMsg("filterDisabled",msgWindow);
        return NS_MSG_NOT_A_MAIL_FOLDER;
      }
      // put the header into the source db, since it needs to be there when we copy it
      // and we need a valid header to pass to StartAsyncCopyMessagesInto
      nsMsgKey keyToFilter;
      mailHdr->GetMessageKey(&keyToFilter);

      if (sourceDB && destIFolder)
      {
        PRBool imapDeleteIsMoveToTrash = DeleteIsMoveToTrash();

        m_moveCoalescer->AddMove (destIFolder, keyToFilter);
        // For each folder, we need to keep track of the ids we want to move to that
        // folder - we used to store them in the MSG_FolderInfo and then when we'd finished
        // downloading headers, we'd iterate through all the folders looking for the ones
        // that needed messages moved into them - perhaps instead we could
        // keep track of nsIMsgFolder, nsMsgKeyArray pairs here in the imap code.
//        nsMsgKeyArray *idsToMoveFromInbox = msgFolder->GetImapIdsToMoveFromInbox();
//        idsToMoveFromInbox->Add(keyToFilter);
        if (imapDeleteIsMoveToTrash)
        {
        }
         
        destIFolder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
        
        if (imapDeleteIsMoveToTrash)  
          err = 0;
      }
    }
  }
  
  
  // we have to return an error because we do not actually move the message
  // it is done async and that can fail
  return err;
}


// both of these algorithms assume that key arrays and flag states are sorted by increasing key.
void nsImapMailFolder::FindKeysToDelete(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToDelete, nsIImapFlagAndUidState *flagState)
{
  PRBool showDeletedMessages = ShowDeletedMessages();
  PRUint32 total = existingKeys.GetSize();
  PRInt32 messageIndex;

  int onlineIndex=0; // current index into flagState
  for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
  {
    PRUint32 uidOfMessage;

    flagState->GetNumberOfMessages(&messageIndex);
    while ((onlineIndex < messageIndex) && 
         (flagState->GetUidOfMessage(onlineIndex, &uidOfMessage), (existingKeys[keyIndex] > uidOfMessage) ))
    {
      onlineIndex++;
    }
    
    imapMessageFlagsType flags;
    flagState->GetUidOfMessage(onlineIndex, &uidOfMessage);
    flagState->GetMessageFlags(onlineIndex, &flags);
    // delete this key if it is not there or marked deleted
    if ( (onlineIndex >= messageIndex ) ||
       (existingKeys[keyIndex] != uidOfMessage) ||
       ((flags & kImapMsgDeletedFlag) && !showDeletedMessages) )
    {
      nsMsgKey doomedKey = existingKeys[keyIndex];
      if ((PRInt32) doomedKey < 0 && doomedKey != nsMsgKey_None)
        continue;
      else
        keysToDelete.Add(existingKeys[keyIndex]);
    }
    
    flagState->GetUidOfMessage(onlineIndex, &uidOfMessage);
    if (existingKeys[keyIndex] == uidOfMessage) 
      onlineIndex++;
  }
}

void nsImapMailFolder::FindKeysToAdd(const nsMsgKeyArray &existingKeys, nsMsgKeyArray &keysToFetch, nsIImapFlagAndUidState *flagState)
{
  PRBool showDeletedMessages = ShowDeletedMessages();
  int dbIndex=0; // current index into existingKeys
  PRInt32 existTotal, numberOfKnownKeys;
  PRInt32 messageIndex;
  
  existTotal = numberOfKnownKeys = existingKeys.GetSize();
  flagState->GetNumberOfMessages(&messageIndex);
  for (PRInt32 flagIndex=0; flagIndex < messageIndex; flagIndex++)
  {
    PRUint32 uidOfMessage;
    flagState->GetUidOfMessage(flagIndex, &uidOfMessage);
    while ( (flagIndex < numberOfKnownKeys) && (dbIndex < existTotal) &&
        existingKeys[dbIndex] < uidOfMessage) 
      dbIndex++;
    
    if ( (flagIndex >= numberOfKnownKeys)  || 
       (dbIndex >= existTotal) ||
       (existingKeys[dbIndex] != uidOfMessage ) )
    {
      numberOfKnownKeys++;

      imapMessageFlagsType flags;
      flagState->GetMessageFlags(flagIndex, &flags);
      NS_ASSERTION(uidOfMessage != nsMsgKey_None, "got invalid msg key");
      if (uidOfMessage != nsMsgKey_None && (showDeletedMessages || ! (flags & kImapMsgDeletedFlag)))
      {
        if (mDatabase)
        {
          PRBool dbContainsKey;
          if (NS_SUCCEEDED(mDatabase->ContainsKey(uidOfMessage, &dbContainsKey)) && dbContainsKey)
          {
            NS_ASSERTION(PR_FALSE, "db has key - flagState messed up?");
            continue;
          }
        }
        keysToFetch.Add(uidOfMessage);
      }
    }
  }
}

void nsImapMailFolder::PrepareToAddHeadersToMailDB(nsIImapProtocol* aProtocol, const nsMsgKeyArray &keysToFetch,
                                                nsIMailboxSpec *boxSpec)
{
  PRUint32 *theKeys = (PRUint32 *) PR_Malloc( keysToFetch.GetSize() * sizeof(PRUint32) );
  if (theKeys)
  {
    PRUint32 total = keysToFetch.GetSize();
    
    for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
      theKeys[keyIndex] = keysToFetch[keyIndex];
    
    // tell the imap thread which hdrs to download
    if (aProtocol)
    {
      aProtocol->NotifyHdrsToDownload(theKeys, total /*keysToFetch.GetSize() */);
      // now, tell it we don't need any bodies.
      aProtocol->NotifyBodysToDownload(nsnull, 0);
    }
  }
  else
  {
    if (aProtocol)
      aProtocol->NotifyHdrsToDownload(nsnull, 0);
  }
}


void nsImapMailFolder::TweakHeaderFlags(nsIImapProtocol* aProtocol, nsIMsgDBHdr *tweakMe)
{
  if (mDatabase && aProtocol && tweakMe)
  {
    tweakMe->SetMessageKey(m_curMsgUid);
    tweakMe->SetMessageSize(m_nextMessageByteLength);
    
    PRBool foundIt = PR_FALSE;
    imapMessageFlagsType imap_flags;

    nsXPIDLCString customFlags;
    nsresult res = aProtocol->GetFlagsForUID(m_curMsgUid, &foundIt, &imap_flags, getter_Copies(customFlags));
    if (NS_SUCCEEDED(res) && foundIt)
    {
      // make a mask and clear these message flags
      PRUint32 mask = MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_MARKED | MSG_FLAG_IMAP_DELETED | MSG_FLAG_LABELS;
      PRUint32 dbHdrFlags;

      tweakMe->GetFlags(&dbHdrFlags);
      tweakMe->AndFlags(~mask, &dbHdrFlags);
      
      // set the new value for these flags
      PRUint32 newFlags = 0;
      if (imap_flags & kImapMsgSeenFlag)
        newFlags |= MSG_FLAG_READ;
      else // if (imap_flags & kImapMsgRecentFlag)
        newFlags |= MSG_FLAG_NEW;

      // Okay here is the MDN needed logic (if DNT header seen):
      /* if server support user defined flag:
          MDNSent flag set => clear kMDNNeeded flag
          MDNSent flag not set => do nothing, leave kMDNNeeded on
         else if 
          not MSG_FLAG_NEW => clear kMDNNeeded flag
          MSG_FLAG_NEW => do nothing, leave kMDNNeeded on
       */
      PRUint16 userFlags;
      res = aProtocol->GetSupportedUserFlags(&userFlags);
      if (NS_SUCCEEDED(res) && (userFlags & (kImapMsgSupportUserFlag |
                            kImapMsgSupportMDNSentFlag)))
      {
        if (imap_flags & kImapMsgMDNSentFlag)
        {
          newFlags |= MSG_FLAG_MDN_REPORT_SENT;
          if (dbHdrFlags & MSG_FLAG_MDN_REPORT_NEEDED)
            tweakMe->AndFlags(~MSG_FLAG_MDN_REPORT_NEEDED, &dbHdrFlags);
        }
      }

      if (imap_flags & kImapMsgAnsweredFlag)
        newFlags |= MSG_FLAG_REPLIED;
      if (imap_flags & kImapMsgFlaggedFlag)
        newFlags |= MSG_FLAG_MARKED;
      if (imap_flags & kImapMsgDeletedFlag)
        newFlags |= MSG_FLAG_IMAP_DELETED;
      if (imap_flags & kImapMsgForwardedFlag)
        newFlags |= MSG_FLAG_FORWARDED;

      // db label flags are 0x0E000000 and imap label flags are 0x0E00
      // so we need to shift 16 bits to the left to convert them.
      if (imap_flags & kImapMsgLabelFlags)
      {
        // we need to set label attribute on header because the dbview code 
        // does msgHdr->GetLabel when asked to paint a row
        tweakMe->SetLabel((imap_flags & kImapMsgLabelFlags) >> 9);
        newFlags |= (imap_flags & kImapMsgLabelFlags) << 16;
      }

      if (newFlags)
        tweakMe->OrFlags(newFlags, &dbHdrFlags);
      if (!customFlags.IsEmpty())
        (void) HandleCustomFlags(m_curMsgUid, tweakMe, customFlags);
    }
  }
}    

NS_IMETHODIMP
//nsImapMailFolder::SetupMsgWriteStream(nsIFileSpec * aFileSpec, PRBool addDummyEnvelope)
nsImapMailFolder::SetupMsgWriteStream(const char * aNativeString, PRBool addDummyEnvelope)
{
    nsresult rv = NS_ERROR_FAILURE;
//    if (!aFileSpec)
//        return NS_ERROR_NULL_POINTER;
    nsFileSpec fileSpec (aNativeString);
//    aFileSpec->GetFileSpec(&fileSpec);
  fileSpec.Delete(PR_FALSE);
  nsCOMPtr<nsISupports>  supports;
  rv = NS_NewIOFileStream(getter_AddRefs(supports), fileSpec, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 00700);
  m_tempMessageStream = do_QueryInterface(supports);
    if (m_tempMessageStream && addDummyEnvelope)
    {
        nsCAutoString result;
        char *ct;
        PRUint32 writeCount;
        time_t now = time ((time_t*) 0);
        ct = ctime(&now);
        ct[24] = 0;
        result = "From - ";
        result += ct;
        result += MSG_LINEBREAK;
        
        m_tempMessageStream->Write(result.get(), result.Length(),
                                   &writeCount);
        result = "X-Mozilla-Status: 0001";
        result += MSG_LINEBREAK;
        m_tempMessageStream->Write(result.get(), result.Length(),
                                   &writeCount);
        result =  "X-Mozilla-Status2: 00000000";
        result += MSG_LINEBREAK;
        m_tempMessageStream->Write(result.get(), result.Length(),
                                   &writeCount);
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::DownloadMessagesForOffline(nsISupportsArray *messages, nsIMsgWindow *window)
{
  nsCAutoString messageIds;
  nsMsgKeyArray srcKeyArray;
  nsresult rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);
  if (NS_FAILED(rv) || messageIds.IsEmpty()) return rv;

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  SetNotifyDownloadedLines(PR_TRUE); 
  rv = AcquireSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this));
  if (NS_FAILED(rv))
  {
    ThrowAlertMsg("operationFailedFolderBusy", window);
    return rv;
  }

  return imapService->DownloadMessagesForOffline(messageIds.get(), this, this, window);
}

NS_IMETHODIMP nsImapMailFolder::DownloadAllForOffline(nsIUrlListener *listener, nsIMsgWindow *msgWindow)
{
  nsresult rv = NS_OK;
  nsCOMPtr <nsIURI> runningURI;
  PRBool noSelect;
  GetFlag(MSG_FOLDER_FLAG_IMAP_NOSELECT, &noSelect);

  if (!noSelect)
  {
    nsCAutoString messageIdsToDownload;
    nsMsgKeyArray msgsToDownload;

    GetDatabase(msgWindow);
    m_downloadingFolderForOfflineUse = PR_TRUE;

    rv = AcquireSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this));
    if (NS_FAILED(rv))
    {
      ThrowAlertMsg("operationFailedFolderBusy", msgWindow);
      return rv;
    }
    SetNotifyDownloadedLines(PR_TRUE); 
    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    // selecting the folder with m_downloadingFolderForOfflineUse true will cause
    // us to fetch any message bodies we don't have.
    rv = imapService->SelectFolder(m_eventQueue, this, listener, msgWindow, nsnull);
    if (NS_SUCCEEDED(rv))
      m_urlRunning = PR_TRUE;
  }
  else
    return NS_MSG_FOLDER_UNREADABLE;
  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::GetNotifyDownloadedLines(PRBool *notifyDownloadedLines)
{
  NS_ENSURE_ARG(notifyDownloadedLines);
  *notifyDownloadedLines = m_downloadMessageForOfflineUse;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetNotifyDownloadedLines(PRBool notifyDownloadedLines)
{
  m_downloadMessageForOfflineUse = notifyDownloadedLines;
  return NS_OK;
}

NS_IMETHODIMP 
nsImapMailFolder::ParseAdoptedMsgLine(const char *adoptedMessageLine, nsMsgKey uidOfMessage)
{
  PRUint32 count = 0;
  nsresult rv = NS_OK;
  // remember the uid of the message we're downloading.
  m_curMsgUid = uidOfMessage;
  if (m_downloadMessageForOfflineUse && !m_offlineHeader)
  {
    GetMessageHeader(uidOfMessage, getter_AddRefs(m_offlineHeader));
    rv = StartNewOfflineMessage();
  }
  // adoptedMessageLine is actually a string with a lot of message lines, separated by native line terminators
  // we need to count the number of MSG_LINEBREAK's to determine how much to increment m_numOfflineMsgLines by.
  if (m_downloadMessageForOfflineUse)
  {
    const char *nextLine = adoptedMessageLine;
    do
    {
      m_numOfflineMsgLines++;
      nextLine = PL_strstr(nextLine, MSG_LINEBREAK);
      if (nextLine)
        nextLine += MSG_LINEBREAK_LEN;
    }
    while (nextLine && *nextLine);
  }
  if (m_tempMessageStream)
  {
      nsCOMPtr <nsISeekableStream> seekable;

      seekable = do_QueryInterface(m_tempMessageStream);

      if (seekable)
        seekable->Seek(PR_SEEK_END, 0);
     rv = m_tempMessageStream->Write(adoptedMessageLine, 
                  PL_strlen(adoptedMessageLine), &count);
     NS_ASSERTION(NS_SUCCEEDED(rv), "failed to write to stream");
  }
                                                                                
  return rv;
}

void nsImapMailFolder::EndOfflineDownload()
{
  if (m_tempMessageStream)
  {
    m_tempMessageStream->Close();
    m_tempMessageStream = nsnull;
    if (mDatabase)
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
}

NS_IMETHODIMP
nsImapMailFolder::NormalEndMsgWriteStream(nsMsgKey uidOfMessage, 
                                          PRBool markRead,
                                          nsIImapUrl *imapUrl)
{
  nsresult res = NS_OK;
  PRBool commit = PR_FALSE;
  if (m_offlineHeader)
    EndNewOfflineMessage();

  m_curMsgUid = uidOfMessage;
  
  if (commit && mDatabase)
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);

  return res;
}

NS_IMETHODIMP
nsImapMailFolder::AbortMsgWriteStream()
{
    return NS_ERROR_FAILURE;
}

    // message move/copy related methods
NS_IMETHODIMP 
nsImapMailFolder::OnlineCopyCompleted(nsIImapProtocol *aProtocol, ImapOnlineCopyState aCopyState)
{
  NS_ENSURE_ARG_POINTER(aProtocol);

  nsresult rv;
  if (aCopyState == ImapOnlineCopyStateType::kSuccessfulCopy)
  {
      
    nsCOMPtr <nsIImapUrl> imapUrl;
    rv = aProtocol->GetRunningImapURL(getter_AddRefs(imapUrl));
        if (NS_FAILED(rv) || !imapUrl) return NS_ERROR_FAILURE;
    nsImapAction action;
    rv = imapUrl->GetImapAction(&action);
    if (NS_FAILED(rv)) return rv;
    
    if (action == nsIImapUrl::nsImapOnlineToOfflineMove)
    {
        nsXPIDLCString messageIds;
        rv = imapUrl->CreateListOfMessageIdsString(getter_Copies(messageIds));

        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIEventQueue> queue;  
        // get the Event Queue for this thread...
        nsCOMPtr<nsIEventQueueService> pEventQService = 
                 do_GetService(kEventQueueServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                 getter_AddRefs(queue));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
   
        rv = imapService->AddMessageFlags(queue, this, nsnull, nsnull,
                                          messageIds,
                                          kImapMsgDeletedFlag,
                                          PR_TRUE);
      if (NS_SUCCEEDED(rv))
      {
        nsMsgKeyArray affectedMessages;
        char *keyTokenString = nsCRT::strdup(messageIds);
        ParseUidString(keyTokenString, affectedMessages);
        if (mDatabase) 
          mDatabase->DeleteMessages(&affectedMessages,nsnull);
        nsCRT::free(keyTokenString);
        return rv;
      }
    }
    /* unhandled action */
    else return NS_ERROR_FAILURE;
  }

  /* unhandled copystate */
  else 
  {
    // whoops, this is the wrong folder - should use the source folder
    if (m_copyState)
    {
       nsCOMPtr<nsIMsgFolder> srcFolder;
       srcFolder = do_QueryInterface(m_copyState->m_srcSupport, &rv);
       if (srcFolder)
        srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
    }
    return NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::PrepareToReleaseObject(nsISupports * aSupports)
{
  NS_ASSERTION(!mSupportsToRelease, "can't prepare to release w/o releasing prev object");
  mSupportsToRelease = aSupports;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::ReleaseObject()
{
  mSupportsToRelease = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::CloseMockChannel(nsIImapMockChannel * aChannel)
{
  aChannel->Close();
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::ReleaseUrlCacheEntry(nsIMsgMailNewsUrl *aUrl)
{
  if (aUrl)
    aUrl->SetMemCacheEntry(nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::BeginMessageUpload()
{
    return NS_ERROR_FAILURE;
}

nsresult nsImapMailFolder::HandleCustomFlags(nsMsgKey uidOfMessage, nsIMsgDBHdr *dbHdr, nsXPIDLCString &keywords)
{
  PRBool messageClassified = PR_TRUE;
  nsXPIDLCString::const_iterator b, e;
  if (FindInReadable(NS_LITERAL_CSTRING("NonJunk"), keywords.BeginReading(b), keywords.EndReading(e)))
    mDatabase->SetStringProperty(uidOfMessage, "junkscore", "0");
  // Mac Mail uses "NotJunk"
  else if (FindInReadable(NS_LITERAL_CSTRING("NotJunk"), keywords.BeginReading(b), keywords.EndReading(e)))
    mDatabase->SetStringProperty(uidOfMessage, "junkscore", "0");
  // ### TODO: we really should parse the keywords into space delimited keywords before checking
  else if (FindInReadable(NS_LITERAL_CSTRING("Junk"), keywords.BeginReading(b), keywords.EndReading(e)))
    mDatabase->SetStringProperty(uidOfMessage, "junkscore", "100");
  else
    messageClassified = PR_FALSE;
  if (messageClassified)
  {
    // only set the junkscore origin if it wasn't set before. We assume plugin since we
    // think that's the more common scenario.
    nsXPIDLCString existingProperty;
    dbHdr->GetStringProperty("junkscoreorigin", getter_Copies(existingProperty));
    if (existingProperty.IsEmpty())
      dbHdr->SetStringProperty("junkscoreorigin", "plugin");
  }
  return dbHdr->SetStringProperty("keywords", keywords);
}

// synchronize the message flags in the database with the server flags
nsresult nsImapMailFolder::SyncFlags(nsIImapFlagAndUidState *flagState)
{
  nsresult rv = GetDatabase(nsnull); // we need a database for this
  NS_ENSURE_SUCCESS(rv, rv);
    // update all of the database flags
  PRInt32 messageIndex;
  PRUint32 messageSize;
  PRUint32 oldFolderSize = mFolderSize;
  // take this opportunity to recalculate the folder size:
  mFolderSize = 0;
  flagState->GetNumberOfMessages(&messageIndex);

  for (PRInt32 flagIndex=0; flagIndex < messageIndex; flagIndex++)
  {
    PRUint32 uidOfMessage;
    flagState->GetUidOfMessage(flagIndex, &uidOfMessage);
    imapMessageFlagsType flags;
    flagState->GetMessageFlags(flagIndex, &flags);
    nsCOMPtr<nsIMsgDBHdr> dbHdr;
    PRBool containsKey;
    rv = mDatabase->ContainsKey(uidOfMessage , &containsKey);
    // if we don't have the header, don't diddle the flags.
    // GetMsgHdrForKey will create the header if it doesn't exist.
    if (NS_FAILED(rv) || !containsKey)
      continue;

    rv = mDatabase->GetMsgHdrForKey(uidOfMessage, getter_AddRefs(dbHdr));
    if (NS_SUCCEEDED(dbHdr->GetMessageSize(&messageSize)))
      mFolderSize += messageSize;

    if (flags & kImapMsgCustomKeywordFlag)
    {
      nsXPIDLCString keywords;
      if (NS_SUCCEEDED(flagState->GetCustomFlags(uidOfMessage, getter_Copies(keywords))))
      {
        if (!keywords.IsEmpty() && dbHdr && NS_SUCCEEDED(rv))
        {
          HandleCustomFlags(uidOfMessage, dbHdr, keywords);
        }
      }
    }
    NotifyMessageFlagsFromHdr(dbHdr, uidOfMessage, flags);
  }
  if (oldFolderSize != mFolderSize)
    NotifyIntPropertyChanged(kFolderSizeAtom, oldFolderSize, mFolderSize);

  return NS_OK;
}

// helper routine to sync the flags on a given header
nsresult nsImapMailFolder::NotifyMessageFlagsFromHdr(nsIMsgDBHdr *dbHdr, nsMsgKey msgKey, PRUint32 flags)
{
    mDatabase->MarkHdrRead(dbHdr, (flags & kImapMsgSeenFlag) != 0, nsnull);
    mDatabase->MarkHdrReplied(dbHdr, (flags & kImapMsgAnsweredFlag) != 0, nsnull);
    mDatabase->MarkHdrMarked(dbHdr, (flags & kImapMsgFlaggedFlag) != 0, nsnull);
    mDatabase->MarkImapDeleted(msgKey, (flags & kImapMsgDeletedFlag) != 0, nsnull);
    // this turns on labels, but it doesn't handle the case where the user
    // unlabels a message on one machine, and expects it to be unlabeled
    // on their other machines. If I turn that on, I'll be removing all the labels
    // that were assigned before we started storing them on the server, which will
    // make some people very unhappy.
    if (flags & kImapMsgLabelFlags)
      mDatabase->SetLabel(msgKey, (flags & kImapMsgLabelFlags) >> 9);
    if (flags & kImapMsgMDNSentFlag)
      mDatabase->MarkMDNSent(msgKey, PR_TRUE, nsnull);
  
    return NS_OK;
}

// message flags operation - this is called (rarely) from the imap protocol,
// proxied over from the imap thread to the ui thread
NS_IMETHODIMP
nsImapMailFolder::NotifyMessageFlags(PRUint32 flags, nsMsgKey msgKey)
{
  if (NS_SUCCEEDED(GetDatabase(nsnull)) && mDatabase)
  {
    nsCOMPtr<nsIMsgDBHdr> dbHdr;
    nsresult rv;
    PRBool containsKey;
    
    rv = mDatabase->ContainsKey(msgKey , &containsKey);
    // if we don't have the header, don't diddle the flags.
    // GetMsgHdrForKey will create the header if it doesn't exist.
    if (NS_FAILED(rv) || !containsKey)
      return rv;
    
    rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(dbHdr));
    
    if(NS_SUCCEEDED(rv) && dbHdr)
    {
      NotifyMessageFlagsFromHdr(dbHdr, msgKey, flags);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::NotifyMessageDeleted(const char *onlineFolderName,PRBool deleteAllMsgs, const char *msgIdString)
{
  const char *doomedKeyString = msgIdString;

  if (deleteAllMsgs)
    return NS_OK;

  char *keyTokenString = PL_strdup(doomedKeyString);
  nsMsgKeyArray affectedMessages;
  ParseUidString(keyTokenString, affectedMessages);

  if (doomedKeyString && !ShowDeletedMessages())
  {
    if (affectedMessages.GetSize() > 0) // perhaps Search deleted these messages
    {
      GetDatabase(nsnull);
      if (mDatabase)
        mDatabase->DeleteMessages(&affectedMessages, nsnull);
    }
    
  }
  else if (doomedKeyString) // && !imapDeleteIsMoveToTrash
  {
    GetDatabase(nsnull);
    if (mDatabase)
      SetIMAPDeletedFlag(mDatabase, affectedMessages, nsnull);
  }
  PR_Free(keyTokenString);
  return NS_OK;
}

PRBool nsImapMailFolder::ShowDeletedMessages()
{
  nsresult err;
  nsCOMPtr<nsIImapHostSessionList> hostSession = 
      do_GetService(kCImapHostSessionList, &err);
  PRBool showDeleted = PR_FALSE;

  if (NS_SUCCEEDED(err) && hostSession)
  {
    nsXPIDLCString serverKey;
    GetServerKey(getter_Copies(serverKey));
    err = hostSession->GetShowDeletedMessagesForHost(serverKey, showDeleted);
  }
  // check for special folders that need to show deleted messages
  if (!showDeleted)
  {
    nsCOMPtr<nsIImapIncomingServer> imapServer;
    nsresult rv = GetImapIncomingServer(getter_AddRefs(imapServer));

    if (NS_SUCCEEDED(rv) && imapServer) 
    {
      // See if the redirector type has a different trash folder name (ie, not 'TRASH').
      // If so then convert it to the beautified name (if configured) and compare it 
      // against the current folder name.
      nsXPIDLCString specialTrashName;
      rv = imapServer->GetTrashFolderByRedirectorType(getter_Copies(specialTrashName));
      if (NS_SUCCEEDED(rv))
      {
        nsXPIDLString convertedName;
        rv = imapServer->ConvertFolderName(specialTrashName.get(), getter_Copies(convertedName));
        if (NS_SUCCEEDED(rv))
        {
          nsXPIDLString folderName;
          GetName(getter_Copies(folderName));
          if (StringBeginsWith(folderName, convertedName, nsCaseInsensitiveStringComparator()))
            showDeleted = PR_TRUE;
        }
      }
    }
  }
  return showDeleted;
}


PRBool nsImapMailFolder::DeleteIsMoveToTrash()
{
  nsresult err;
  nsCOMPtr<nsIImapHostSessionList> hostSession = 
             do_GetService(kCImapHostSessionList, &err);
  PRBool rv = PR_TRUE;

    if (NS_SUCCEEDED(err) && hostSession)
  {
    nsXPIDLCString serverKey;
    GetServerKey(getter_Copies(serverKey));
    err = hostSession->GetDeleteIsMoveToTrashForHost(serverKey.get(), rv);
  }
  return rv;
}

nsresult nsImapMailFolder::GetTrashFolder(nsIMsgFolder **pTrashFolder)
{
  if (!pTrashFolder)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv))
  {
    PRUint32 numFolders;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 1, &numFolders, pTrashFolder);
    if (numFolders != 1)
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}


// store MSG_FLAG_IMAP_DELETED in the specified mailhdr records
void nsImapMailFolder::SetIMAPDeletedFlag(nsIMsgDatabase *mailDB, const nsMsgKeyArray &msgids, PRBool markDeleted)
{
  nsresult markStatus = 0;
  PRUint32 total = msgids.GetSize();

  for (PRUint32 msgIndex=0; !markStatus && (msgIndex < total); msgIndex++)
  {
    markStatus = mailDB->MarkImapDeleted(msgids[msgIndex], markDeleted, nsnull);
  }
}


NS_IMETHODIMP
nsImapMailFolder::GetMessageSizeFromDB(const char *id, PRBool idIsUid, PRUint32 *size)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ENSURE_ARG(size);
  *size = 0;
  (void) GetDatabase(nsnull);
  if (id && mDatabase)
  {
    PRUint32 key = atoi(id);
    nsCOMPtr<nsIMsgDBHdr> mailHdr;
    NS_ASSERTION(idIsUid, "ids must be uids to get message size");
    if (idIsUid)
      rv = mDatabase->GetMsgHdrForKey(key, getter_AddRefs(mailHdr));
    if (NS_SUCCEEDED(rv) && mailHdr)
      rv = mailHdr->GetMessageSize(size);
  }
  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::SetContentModified(nsIImapUrl *aImapUrl, nsImapContentModifiedType modified)
{
  return aImapUrl->SetContentModified(modified);
}

NS_IMETHODIMP
nsImapMailFolder::SetImageCacheSessionForUrl(nsIMsgMailNewsUrl *mailurl)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsICacheSession> cacheSession;
  rv = imapService->GetCacheSession(getter_AddRefs(cacheSession));
  if (NS_SUCCEEDED(rv) && cacheSession)
    rv = mailurl->SetImageCacheSession(cacheSession);

  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetCurMoveCopyMessageFlags(nsIImapUrl *runningUrl, PRUint32 *aResult)
{
  nsCOMPtr <nsISupports> copyState;
  runningUrl->GetCopyState(getter_AddRefs(copyState));
  if (copyState)
  {
    nsCOMPtr<nsImapMailCopyState> mailCopyState = do_QueryInterface(copyState);
    if (mailCopyState && mailCopyState->m_message)
    {
      nsMsgLabelValue label;
      mailCopyState->m_message->GetFlags(aResult);
      mailCopyState->m_message->GetLabel(&label);
      if (label != 0)
        *aResult |= label << 25;
    }
    // if the message is being added to the Sent or Templates folders,
    // add the seen flag so the message gets marked read
    else if (mFlags & (MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_TEMPLATES))
      *aResult = MSG_FLAG_READ;
  }
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::OnStartRunningUrl(nsIURI *aUrl)
{
  NS_PRECONDITION(aUrl, "sanity check - need to be be running non-null url");
  m_urlRunning = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::OnStopRunningUrl(nsIURI *aUrl, nsresult aExitCode)
{
  nsresult rv = NS_OK;
  PRBool endedOfflineDownload = PR_FALSE;
  m_urlRunning = PR_FALSE;
  if (m_downloadingFolderForOfflineUse)
  {
    ReleaseSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this));
    m_downloadingFolderForOfflineUse = PR_FALSE;
    endedOfflineDownload = PR_TRUE;
    EndOfflineDownload();
  }
  nsCOMPtr<nsIMsgMailSession> session = 
           do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  if (aUrl)
  {
    nsCOMPtr<nsIMsgWindow> msgWindow;
    nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
    nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aUrl);
    PRBool folderOpen = PR_FALSE;
    if (mailUrl)
      mailUrl->GetMsgWindow(getter_AddRefs(msgWindow));
    if (session)
      session->IsFolderOpenInWindow(this, &folderOpen);
#ifdef DEBUG_bienvenu1
    nsXPIDLCString urlSpec;
    aUrl->GetSpec(getter_Copies(urlSpec));
   printf("stop running url %s\n", (const char *) urlSpec);
#endif

   if (imapUrl)
   {
        nsImapAction imapAction = nsIImapUrl::nsImapTest;
        imapUrl->GetImapAction(&imapAction);
        if (imapAction == nsIImapUrl::nsImapMsgFetch || imapAction == nsIImapUrl::nsImapMsgDownloadForOffline)
        {
          ReleaseSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this));
          SetNotifyDownloadedLines(PR_FALSE);
          if (!endedOfflineDownload)
            EndOfflineDownload();
        }

        switch(imapAction)
        {
        case nsIImapUrl::nsImapDeleteMsg:
        case nsIImapUrl::nsImapOnlineMove:
        case nsIImapUrl::nsImapOnlineCopy:
          if (NS_SUCCEEDED(aExitCode))
          {
              if (folderOpen)
                UpdateFolder(msgWindow);
              else
                UpdatePendingCounts();
          }
          if (m_copyState)
          {
            nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(m_copyState->m_srcSupport, &rv);
            if (m_copyState->m_isMove && !m_copyState->m_isCrossServerOp)
            {
              if (NS_SUCCEEDED(aExitCode))
              {
                nsCOMPtr<nsIMsgDatabase> srcDB;
                if (srcFolder)
                    rv = srcFolder->GetMsgDatabase(msgWindow,
                        getter_AddRefs(srcDB));
                if (NS_SUCCEEDED(rv) && srcDB)
                {
                  nsCOMPtr<nsImapMoveCopyMsgTxn> msgTxn;
                  nsMsgKeyArray srcKeyArray;
                  if (m_copyState->m_allowUndo)
                  {
                     msgTxn = do_QueryInterface(m_copyState->m_undoMsgTxn); 
                     if (msgTxn)
                       msgTxn->GetSrcKeyArray(srcKeyArray);
                  }
                  else
                  {
                     nsCAutoString messageIds;
                     rv = BuildIdsAndKeyArray(m_copyState->m_messages, messageIds, srcKeyArray);
                     NS_ENSURE_SUCCESS(rv,rv);
                  }
                 
                  if (!ShowDeletedMessages())
                    srcDB->DeleteMessages(&srcKeyArray, nsnull);  
                  else
                    MarkMessagesImapDeleted(&srcKeyArray, PR_TRUE, srcDB);
                }
                srcFolder->EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE/* dbBatching*/);
                // even if we're showing deleted messages, 
                // we still need to notify FE so it will show the imap deleted flag
                srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);      
              }
              else
              {
                srcFolder->EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE/* dbBatching*/);
                srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);  
              }
                
            }
            if (m_copyState->m_msgWindow && NS_SUCCEEDED(aExitCode)) //we should do this only if move/copy succeeds
            {
              nsCOMPtr<nsITransactionManager> txnMgr;
              m_copyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
              if (txnMgr)
                txnMgr->DoTransaction(m_copyState->m_undoMsgTxn);
            }
             (void) OnCopyCompleted(m_copyState->m_srcSupport, aExitCode);
          }
          // we're the dest folder of a move/copy - if we're not open in the ui,
          // then we should clear our nsMsgDatabase pointer. Otherwise, the db would
          // be open until the user selected it and then selected another folder.
          // but don't do this for the trash or inbox - we'll leave them open
          if (!folderOpen && ! (mFlags & (MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_INBOX)))
            SetMsgDatabase(nsnull);
          break;
        case nsIImapUrl::nsImapSubtractMsgFlags:
          {
          // this isn't really right - we'd like to know we were 
          // deleting a message to start with, but it probably
          // won't do any harm.
            imapMessageFlagsType flags = 0;
            imapUrl->GetMsgFlags(&flags);
            //we need to subtract the delete flag in db only in case when we show deleted msgs
            if (flags & kImapMsgDeletedFlag && ShowDeletedMessages())
            {
              nsCOMPtr<nsIMsgDatabase> db;
              rv = GetMsgDatabase(nsnull, getter_AddRefs(db));
              if (NS_SUCCEEDED(rv) && db)
              {
                nsMsgKeyArray keyArray;
                char *keyString;
                imapUrl->CreateListOfMessageIdsString(&keyString);
                if (keyString)
                {
                  ParseUidString(keyString, keyArray);
                  MarkMessagesImapDeleted(&keyArray, PR_FALSE, db);
                  db->Commit(nsMsgDBCommitType::kLargeCommit);
                  nsCRT::free(keyString);
                }
              }
            }
          }
          break;
        case nsIImapUrl::nsImapAddMsgFlags:
          {
            imapMessageFlagsType flags = 0;
            imapUrl->GetMsgFlags(&flags);
            if (flags & kImapMsgDeletedFlag)
            {
              // we need to delete headers from db only when we don't show deleted msgs
              if (!ShowDeletedMessages()) 
              {
                nsCOMPtr<nsIMsgDatabase> db;
                rv = GetMsgDatabase(nsnull, getter_AddRefs(db));
                if (NS_SUCCEEDED(rv) && db)
                {
                  nsMsgKeyArray keyArray;
                  char *keyString = nsnull;
                  imapUrl->CreateListOfMessageIdsString(&keyString);
                  if (keyString)
                  {
                    ParseUidString(keyString, keyArray);
                    db->DeleteMessages(&keyArray, nsnull);
                    db->SetSummaryValid(PR_TRUE);
                    db->Commit(nsMsgDBCommitType::kLargeCommit);
                    nsCRT::free(keyString);
                  }
                }
              }
              // see bug #188051
              // only send the folder event only if we are deleting
              // (and not for other flag changes)
              NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
            }
          }
          break;
        case nsIImapUrl::nsImapAppendMsgFromFile:
        case nsIImapUrl::nsImapAppendDraftFromFile:
            if (m_copyState)
            {
              if (NS_SUCCEEDED(aExitCode))
              {
                UpdatePendingCounts();

                m_copyState->m_curIndex++;
                if (m_copyState->m_curIndex >= m_copyState->m_totalCount)
                {
                  if (folderOpen)
                    UpdateFolder(msgWindow);
                  if (m_copyState->m_msgWindow && m_copyState->m_undoMsgTxn)
                  {
                    nsCOMPtr<nsITransactionManager> txnMgr;
                    m_copyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
                    if (txnMgr)
                      txnMgr->DoTransaction(m_copyState->m_undoMsgTxn);
                  }
                  (void) OnCopyCompleted(m_copyState->m_srcSupport, aExitCode);
                }
              }
              else
                //clear the copyState if copy has failed
                (void) OnCopyCompleted(m_copyState->m_srcSupport, aExitCode);              
            }
            break;
        case nsIImapUrl::nsImapRenameFolder:
          if (NS_FAILED(aExitCode))
          {
            nsCOMPtr <nsIAtom> folderRenameAtom;
            folderRenameAtom = do_GetAtom("RenameCompleted");
            NotifyFolderEvent(folderRenameAtom);
          }
          break;
        case nsIImapUrl::nsImapDeleteAllMsgs:
            if (NS_SUCCEEDED(aExitCode))
            {
              if (folderOpen)
                UpdateFolder(msgWindow);
              else
              {
                ChangeNumPendingTotalMessages(-mNumPendingTotalMessages);
                ChangeNumPendingUnread(-mNumPendingUnreadMessages);
                m_numStatusUnseenMessages = 0;
              }

            }
            break;
        case nsIImapUrl::nsImapListFolder:
            // check if folder is now verified - if not,
            // we should remove it?
            if (NS_SUCCEEDED(aExitCode) && !m_verifiedAsOnlineFolder)
            {
              nsCOMPtr<nsIMsgFolder> parent;
              rv = GetParent(getter_AddRefs(parent));

              if (NS_SUCCEEDED(rv) && parent)
              {
                nsCOMPtr<nsIMsgImapMailFolder> imapParent = do_QueryInterface(parent);
                if (imapParent)
                  imapParent->RemoveSubFolder(this);
              }
            }
          break;
        case nsIImapUrl::nsImapRefreshFolderUrls:
          // we finished getting an admin url for the folder.
            if (!m_adminUrl.IsEmpty())
              FolderPrivileges(msgWindow);
            break;
        case nsIImapUrl::nsImapCreateFolder:
          if (NS_FAILED(aExitCode))  //if success notification already done
          {
            nsCOMPtr <nsIAtom> folderCreateAtom;
            folderCreateAtom = do_GetAtom("FolderCreateFailed");
            NotifyFolderEvent(folderCreateAtom);
          }
          break;
        case nsIImapUrl::nsImapSubscribe:
          if (NS_SUCCEEDED(aExitCode) && msgWindow)
          {
            nsXPIDLCString canonicalFolderName;
            imapUrl->CreateCanonicalSourceFolderPathString(getter_Copies(canonicalFolderName));
            nsCOMPtr <nsIMsgFolder> rootFolder;
            nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
            if(NS_SUCCEEDED(rv) && rootFolder)
            {
              nsCOMPtr <nsIMsgImapMailFolder> imapRoot = do_QueryInterface(rootFolder);
              if (imapRoot)
              {
                nsCOMPtr <nsIMsgImapMailFolder> foundFolder;
                rv = imapRoot->FindOnlineSubFolder(canonicalFolderName, getter_AddRefs(foundFolder));
                if (NS_SUCCEEDED(rv) && foundFolder)
                {
                  nsXPIDLCString uri;
                  nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryInterface(foundFolder);
                  if (msgFolder)
                  {
                    msgFolder->GetURI(getter_Copies(uri));
                    msgWindow->SelectFolder(uri.get());
                  }
                }
              }
            }
          }
          break;
        default:
            break;
        }
    }
    // give base class a chance to send folder loaded notification...
    rv = nsMsgDBFolder::OnStopRunningUrl(aUrl, aExitCode);
    // query it for a mailnews interface for now....
    if (mailUrl)
      rv = mailUrl->UnRegisterListener(this);

  }
  SetGettingNewMessages(PR_FALSE); // if we're not running a url, we must not be getting new mail :-)

  if (m_urlListener)
  {
    m_urlListener->OnStopRunningUrl(aUrl, aExitCode);
    m_urlListener = nsnull;
  }
  return rv;
}

void nsImapMailFolder::UpdatePendingCounts()
{
  if (m_copyState)
  {
    if (!m_copyState->m_isCrossServerOp)
      ChangeNumPendingTotalMessages(m_copyState->m_totalCount);
    else
      ChangeNumPendingTotalMessages(1);

    // count the moves that were unread
    int numUnread = m_copyState->m_unreadCount;
  
    if (numUnread)
    {
      m_numStatusUnseenMessages += numUnread; // adjust last status count by this delta.
      ChangeNumPendingUnread(numUnread);
    }
    SummaryChanged();
  }
} 



    // nsIImapExtensionSink methods

NS_IMETHODIMP
nsImapMailFolder::ClearFolderRights(nsIImapProtocol* aProtocol,
                                    nsIMAPACLRightsInfo* aclRights)
{
  SetFolderNeedsACLListed(PR_FALSE);
  delete m_folderACL;
  m_folderACL = new nsMsgIMAPFolderACL(this);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::AddFolderRights(const char *userName, const char *rights)
{
  SetFolderNeedsACLListed(PR_FALSE);
  GetFolderACL()->SetFolderRightsForUser(userName, rights);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::RefreshFolderRights()
{
  if (GetFolderACL()->GetIsFolderShared())
    SetFlag(MSG_FOLDER_FLAG_PERSONAL_SHARED);
  else
    ClearFlag(MSG_FOLDER_FLAG_PERSONAL_SHARED);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetCopyResponseUid(nsIImapProtocol* aProtocol,
                                     nsMsgKeyArray* aKeyArray,
                                     const char* msgIdString,
                                     nsIImapUrl * aUrl)
{   // CopyMessages() only
  nsresult rv = NS_OK;
  nsCOMPtr<nsImapMoveCopyMsgTxn> msgTxn;
  nsCOMPtr<nsISupports> copyState;
  
  if (aUrl)
    aUrl->GetCopyState(getter_AddRefs(copyState));

  if (copyState)
  {
      nsCOMPtr<nsImapMailCopyState> mailCopyState =
          do_QueryInterface(copyState, &rv);
      if (NS_FAILED(rv)) return rv;
      if (mailCopyState->m_undoMsgTxn)
          msgTxn = do_QueryInterface(mailCopyState->m_undoMsgTxn, &rv);
  }
  if (msgTxn)
      msgTxn->SetCopyResponseUid(aKeyArray, msgIdString);
  
  return NS_OK;
}    

NS_IMETHODIMP
nsImapMailFolder::StartMessage(nsIMsgMailNewsUrl * aUrl)
{
  nsCOMPtr<nsIImapUrl> imapUrl (do_QueryInterface(aUrl));
  nsCOMPtr<nsISupports> copyState;
  NS_ENSURE_TRUE(imapUrl, NS_ERROR_FAILURE);
  
  imapUrl->GetCopyState(getter_AddRefs(copyState));
  if (copyState)
  {
    nsCOMPtr <nsICopyMessageStreamListener> listener = do_QueryInterface(copyState);
    if (listener)
      listener->StartMessage();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::EndMessage(nsIMsgMailNewsUrl * aUrl, nsMsgKey uidOfMessage)
{
  nsCOMPtr<nsIImapUrl> imapUrl (do_QueryInterface(aUrl));
  nsCOMPtr<nsISupports> copyState;
  NS_ENSURE_TRUE(imapUrl, NS_ERROR_FAILURE);
  
  imapUrl->GetCopyState(getter_AddRefs(copyState));
  if (copyState)
  {
    nsCOMPtr <nsICopyMessageStreamListener> listener = do_QueryInterface(copyState);
    if (listener)
      listener->EndMessage(uidOfMessage);
  }

  return NS_OK;
}

#define WHITESPACE " \015\012"     // token delimiter

NS_IMETHODIMP
nsImapMailFolder::NotifySearchHit(nsIMsgMailNewsUrl * aUrl, 
                                  const char* searchHitLine)
{
  nsresult rv = GetDatabase(nsnull /* don't need msg window, that's more for local mbox parsing */);
  if (!mDatabase || NS_FAILED(rv))
    return rv;
  // expect search results in the form of "* SEARCH <hit> <hit> ..."
                // expect search results in the form of "* SEARCH <hit> <hit> ..."
  char *tokenString = nsCRT::strdup(searchHitLine);
  if (tokenString)
  {
      char *currentPosition = PL_strcasestr(tokenString, "SEARCH");
      if (currentPosition)
      {
        currentPosition += strlen("SEARCH");
        char *newStr;
          
        PRBool shownUpdateAlert = PR_FALSE;
        char *hitUidToken = nsCRT::strtok(currentPosition, WHITESPACE, &newStr);
        while (hitUidToken)
        {
          long naturalLong; // %l is 64 bits on OSF1
          sscanf(hitUidToken, "%ld", &naturalLong);
          nsMsgKey hitUid = (nsMsgKey) naturalLong;
      
          nsCOMPtr <nsIMsgDBHdr> hitHeader;
          rv = mDatabase->GetMsgHdrForKey(hitUid, getter_AddRefs(hitHeader));
          if (NS_SUCCEEDED(rv) && hitHeader)
          {
            nsCOMPtr <nsIMsgSearchSession> searchSession;
            nsCOMPtr <nsIMsgSearchAdapter> searchAdapter;
            aUrl->GetSearchSession(getter_AddRefs(searchSession));
            if (searchSession)
            {
              searchSession->GetRunningAdapter(getter_AddRefs(searchAdapter));
              if (searchAdapter)
                searchAdapter->AddResultElement(hitHeader);
            }
          }
          else if (!shownUpdateAlert)
          {
          }
        
          hitUidToken = nsCRT::strtok(newStr, WHITESPACE, &newStr);
        }
    }

    nsCRT::free(tokenString);
  }
  else
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP
nsImapMailFolder::SetAppendMsgUid(nsIImapProtocol* aProtocol,
                                  nsMsgKey aKey,
                                  nsIImapUrl * aUrl)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> copyState;

  if (aUrl)
    aUrl->GetCopyState(getter_AddRefs(copyState));
  if (copyState)
  {
    nsCOMPtr<nsImapMailCopyState> mailCopyState = do_QueryInterface(copyState, &rv);
    if (NS_FAILED(rv)) return rv;

    if (mailCopyState->m_undoMsgTxn) // CopyMessages()
    {
        nsCOMPtr<nsImapMoveCopyMsgTxn> msgTxn;
        msgTxn = do_QueryInterface(mailCopyState->m_undoMsgTxn, &rv);
        if (NS_SUCCEEDED(rv))
            msgTxn->AddDstKey(aKey);
    }
    else if (mailCopyState->m_listener) // CopyFileMessage();
                                        // Draft/Template goes here
       mailCopyState->m_listener->SetMessageKey(aKey);
  }
  return NS_OK;
}    

NS_IMETHODIMP
nsImapMailFolder::GetMessageId(nsIImapProtocol* aProtocl,
                               nsCString* messageId,
                               nsIImapUrl * aUrl)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> copyState;

  if (aUrl)
    aUrl->GetCopyState(getter_AddRefs(copyState));
  if (copyState)
  {
    nsCOMPtr<nsImapMailCopyState> mailCopyState =
        do_QueryInterface(copyState, &rv);
    if (NS_FAILED(rv)) return rv;
    if (mailCopyState->m_listener)
        rv = mailCopyState->m_listener->GetMessageId(messageId);
  }
  if (NS_SUCCEEDED(rv) && messageId->Length() > 0)
  {
      if (messageId->First() == '<')
          messageId->Cut(0, 1);
      if (messageId->Last() == '>')
          messageId->SetLength(messageId->Length() -1);
  }  
  return rv;
}
    // nsIImapMiscellaneousSink methods
NS_IMETHODIMP
nsImapMailFolder::AddSearchResult(nsIImapProtocol* aProtocol, 
                                  const char* searchHitLine)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
{
  nsCOMPtr <nsIMsgWindow> msgWindow; // we might need this for the filter plugins.
  if (mDatabase)
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  SetSizeOnDisk(mFolderSize); 
  PRInt32 numNewBiffMsgs = 0;
  if (m_performingBiff)
    GetNumNewMessages(PR_FALSE, &numNewBiffMsgs);

  PlaybackCoalescedOperations();
  if (aProtocol)
  {
    // check if we should download message bodies because it's the inbox and 
    // the server is specified as one where where we download msg bodies automatically.
    PRBool autoDownloadNewHeaders = PR_FALSE;
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
    {
      nsCOMPtr<nsIImapIncomingServer> imapServer;
      nsresult rv = GetImapIncomingServer(getter_AddRefs(imapServer));

      if (NS_SUCCEEDED(rv) && imapServer)
        imapServer->GetDownloadBodiesOnGetNewMail(&autoDownloadNewHeaders);
      // this isn't quite right - we only want to download bodies for new headers
      // but we don't know what the new headers are. We could query the inbox db
      // for new messages, if the above filter playback actually moves the filtered
      // messages before we get to this code.
      if (autoDownloadNewHeaders)
      {
          // acquire semaphore for offline store. If it fails, we won't download for offline use.
        if (NS_SUCCEEDED(AcquireSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this))))
          m_downloadingFolderForOfflineUse = PR_TRUE;
      }
    }

    if (m_downloadingFolderForOfflineUse)
    {
      nsMsgKeyArray keysToDownload;
      GetBodysToDownload(&keysToDownload);
      if (keysToDownload.GetSize() > 0)
        SetNotifyDownloadedLines(PR_TRUE);

      aProtocol->NotifyBodysToDownload(keysToDownload.GetArray(), keysToDownload.GetSize());
    }
    else
      aProtocol->NotifyBodysToDownload(nsnull, 0/*keysToFetch.GetSize() */);

    nsCOMPtr <nsIURI> runningUri;
    aProtocol->GetRunningUrl(getter_AddRefs(runningUri));
    if (runningUri)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(runningUri);
      if (mailnewsUrl)
        mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
    }
  }

  PRBool filtersRun;
  CallFilterPlugins(msgWindow, &filtersRun);
  if (!filtersRun && m_performingBiff && mDatabase && numNewBiffMsgs > 0)
  {
    // If we are performing biff for this folder, tell the
    // stand-alone biff about the new high water mark
    // We must ensure that the server knows that we are performing biff.
    // Otherwise the stand-alone biff won't fire.
    nsCOMPtr<nsIMsgIncomingServer> server;
    if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
      server->SetPerformingBiff(PR_TRUE);
    
    SetBiffState(nsIMsgFolder::nsMsgBiffState_NewMail);
    if (server)
      server->SetPerformingBiff(PR_FALSE);
    m_performingBiff = PR_FALSE;
  }

  if (m_filterList)
    (void)m_filterList->FlushLogIfNecessary();
 
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                        nsMsgBiffState biffState)
{
  SetBiffState(biffState);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::GetUidValidity(PRInt32 *uidValidity)
{
  NS_ENSURE_ARG(uidValidity);
  if (m_uidValidity == kUidUnknown)
  {
    nsCOMPtr<nsIMsgDatabase> db; 
    nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
    (void) GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(db));
    if (db)
      db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
    
    if (dbFolderInfo)
      dbFolderInfo->GetImapUidValidity((PRInt32 *) &m_uidValidity);
  }
  *uidValidity = m_uidValidity;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetUidValidity(PRInt32 uidValidity)
{
  m_uidValidity = uidValidity;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::FillInFolderProps(nsIMsgImapFolderProps *aFolderProps)
{
  NS_ENSURE_ARG(aFolderProps);
  PRUint32 folderTypeStringID;
  PRUint32 folderTypeDescStringID = 0;
  PRUint32 folderQuotaStatusStringID;
  nsXPIDLString folderType;
  nsXPIDLString folderTypeDesc;
  nsXPIDLString folderQuotaStatusDesc;
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = IMAPGetStringBundle(getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  // get the host session list and get server capabilities.
  PRUint32 capability = kCapabilityUndefined;

  nsCOMPtr<nsIImapHostSessionList> hostSession = do_GetService(kCImapHostSessionList, &rv);
  // if for some bizarre reason this fails, we'll still fall through to the normal sharing code
  if (NS_SUCCEEDED(rv) && hostSession)
  {
    nsXPIDLCString serverKey;
    GetServerKey(getter_Copies(serverKey));
    hostSession->GetCapabilityForHost(serverKey, capability);

    // Figure out what to display in the Quota tab of the folder properties.
    // Does the server support quotas?
    if(capability & kQuotaCapability)
    {
      // Have we asked the server for quota information?
      if(m_folderQuotaCommandIssued)
      {
        // Has the server replied with storage quota info?
        if(m_folderQuotaDataIsValid)
        {
          // If so, set quota data
          folderQuotaStatusStringID = 0;
          aFolderProps->SetQuotaData(m_folderQuotaRoot, m_folderQuotaUsedKB, m_folderQuotaMaxKB);
        }
        else
        {
          // If not, there is no storage quota set on this folder
          folderQuotaStatusStringID = IMAP_QUOTA_STATUS_NOQUOTA;
        }
      }
      else
      {
        // The folder is not open, so no quota information is available
        folderQuotaStatusStringID = IMAP_QUOTA_STATUS_FOLDERNOTOPEN;
      }
    }
    else
    {
      // Either the server doesn't support quotas, or we don't know if it does
      // (e.g., because we don't have a connection yet). If the latter, we fall back
      // to saying that no information is available because the folder is not open.
      folderQuotaStatusStringID = (capability == kCapabilityUndefined) ?
        IMAP_QUOTA_STATUS_FOLDERNOTOPEN : IMAP_QUOTA_STATUS_NOTSUPPORTED;
    }

    if(folderQuotaStatusStringID == 0)
    {
      // Display quota data
      aFolderProps->ShowQuotaData(PR_TRUE);
    }
    else
    {
      // Hide quota data and show reason why it is not available
      aFolderProps->ShowQuotaData(PR_FALSE);

      rv = IMAPGetStringByID(folderQuotaStatusStringID, getter_Copies(folderQuotaStatusDesc));
      if (NS_SUCCEEDED(rv))
        aFolderProps->SetQuotaStatus(folderQuotaStatusDesc);
    }

    // See if the server supports ACL.
    // If not, just set the folder description to a string that says
    // the server doesn't support sharing, and return.
    if (! (capability & kACLCapability))
    {
      rv = IMAPGetStringByID(IMAP_SERVER_DOESNT_SUPPORT_ACL, getter_Copies(folderTypeDesc));
      if (NS_SUCCEEDED(rv))
        aFolderProps->SetFolderTypeDescription(folderTypeDesc);
      aFolderProps->ServerDoesntSupportACL();
      return NS_OK;
    }
  }
  if (mFlags & MSG_FOLDER_FLAG_IMAP_PUBLIC)
  {
    folderTypeStringID = IMAP_PUBLIC_FOLDER_TYPE_NAME;
    folderTypeDescStringID = IMAP_PUBLIC_FOLDER_TYPE_DESCRIPTION;
  }
  else if (mFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
  {
    folderTypeStringID = IMAP_OTHER_USERS_FOLDER_TYPE_NAME;
    nsXPIDLCString owner;
    nsXPIDLString uniOwner;
    GetFolderOwnerUserName(getter_Copies(owner));
    if (owner.IsEmpty())
    {
      rv = IMAPGetStringByID(folderTypeStringID, getter_Copies(uniOwner));
      // Another user's folder, for which we couldn't find an owner name
      NS_ASSERTION(PR_FALSE, "couldn't get owner name for other user's folder");
    }
    else
    {
      // is this right? It doesn't leak, does it?
      CopyASCIItoUTF16(owner, uniOwner);
    }
    const PRUnichar *params[] = { uniOwner.get() };
    rv = bundle->FormatStringFromID(IMAP_OTHER_USERS_FOLDER_TYPE_DESCRIPTION, params, 1, getter_Copies(folderTypeDesc));
  }

  else if (GetFolderACL()->GetIsFolderShared())
  {
    folderTypeStringID = IMAP_PERSONAL_SHARED_FOLDER_TYPE_NAME;
    folderTypeDescStringID = IMAP_PERSONAL_SHARED_FOLDER_TYPE_DESCRIPTION;
  }
  else
  {
    folderTypeStringID = IMAP_PERSONAL_SHARED_FOLDER_TYPE_NAME;
    folderTypeDescStringID = IMAP_PERSONAL_FOLDER_TYPE_DESCRIPTION;
  }

  rv = IMAPGetStringByID(folderTypeStringID, getter_Copies(folderType));
  if (NS_SUCCEEDED(rv))
    aFolderProps->SetFolderType(folderType);

  if (folderTypeDesc.IsEmpty() && folderTypeDescStringID != 0)
    rv = IMAPGetStringByID(folderTypeDescStringID, getter_Copies(folderTypeDesc));
  if (!folderTypeDesc.IsEmpty())
    aFolderProps->SetFolderTypeDescription(folderTypeDesc.get());

  nsXPIDLString rightsString;
  rv = CreateACLRightsStringForFolder(getter_Copies(rightsString));
  if (NS_SUCCEEDED(rv))
    aFolderProps->SetFolderPermissions(rightsString.get());

  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetAclFlags(PRUint32 aclFlags)
{
  nsresult rv = NS_OK;
  if (m_aclFlags != aclFlags)
  {
    nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
    PRBool dbWasOpen = (mDatabase != nsnull);
    rv = GetDatabase(nsnull);

    m_aclFlags = aclFlags;
    if (mDatabase)
    {
      rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
      if (NS_SUCCEEDED(rv) && dbFolderInfo)
        dbFolderInfo->SetUint32Property("aclFlags", aclFlags);
      // if setting the acl flags caused us to open the db, release the ref
      // because on startup, we might get acl on all folders,which will
      // leave a lot of db's open.
      if (!dbWasOpen)
      {
        mDatabase->Close(PR_TRUE /* commit changes */);
        mDatabase = nsnull;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetAclFlags(PRUint32 *aclFlags)
{
  NS_ENSURE_ARG_POINTER(aclFlags);

  nsresult rv = NS_OK;

  ReadDBFolderInfo(PR_FALSE); // update cache first.
  if (m_aclFlags == -1) // -1 means invalid value, so get it from db.
  {
    nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
    PRBool dbWasOpen = (mDatabase != nsnull);
    rv = GetDatabase(nsnull);

    if (mDatabase)
    {
      rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
      if (NS_SUCCEEDED(rv) && dbFolderInfo)
      {
        rv = dbFolderInfo->GetUint32Property("aclFlags", 0, aclFlags);
        m_aclFlags = *aclFlags;
      }
      // if getting the acl flags caused us to open the db, release the ref
      // because on startup, we might get acl on all folders,which will
      // leave a lot of db's open.
      if (!dbWasOpen)
      {
        mDatabase->Close(PR_TRUE /* commit changes */);
        mDatabase = nsnull;
      }
    }
  }
  else
    *aclFlags = m_aclFlags;
  return rv;
}


nsresult nsImapMailFolder::SetSupportedUserFlags(PRUint32 userFlags)
{
  nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
  nsresult rv = GetDatabase(nsnull);

  m_supportedUserFlags = userFlags;
  if (mDatabase)
  {
    rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
    if (NS_SUCCEEDED(rv) && dbFolderInfo)
      dbFolderInfo->SetUint32Property("imapFlags", userFlags);
  }


  return rv;
}

nsresult nsImapMailFolder::GetSupportedUserFlags(PRUint32 *userFlags)
{
  NS_ENSURE_ARG_POINTER(userFlags);

  nsresult rv = NS_OK;

  ReadDBFolderInfo(PR_FALSE); // update cache first.
  if (m_supportedUserFlags == 0) // 0 means invalid value, so get it from db.
  {
    nsCOMPtr<nsIDBFolderInfo> dbFolderInfo;
    rv = GetDatabase(nsnull);

    if (mDatabase)
    {
      rv = mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
      if (NS_SUCCEEDED(rv) && dbFolderInfo)
      {
        rv = dbFolderInfo->GetUint32Property("imapFlags", 0, userFlags);
        m_supportedUserFlags = *userFlags;
      }
    }
  }
  else
    *userFlags = m_supportedUserFlags;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetCanIOpenThisFolder(PRBool *aBool)
{
  NS_ENSURE_ARG_POINTER(aBool);
  PRBool noSelect;
  GetFlag(MSG_FOLDER_FLAG_IMAP_NOSELECT, &noSelect);
  *aBool = (noSelect) ? PR_FALSE : GetFolderACL()->GetCanIReadFolder();
  return NS_OK;
}

///////// nsMsgIMAPFolderACL class ///////////////////////////////

// This string is defined in the ACL RFC to be "anyone"
#define IMAP_ACL_ANYONE_STRING "anyone"

/* static */PRBool nsMsgIMAPFolderACL::FreeHashRights(nsHashKey *aKey, void *aData,
                                        void *closure)
{
  PR_FREEIF(aData);
  return PR_TRUE;
}

nsMsgIMAPFolderACL::nsMsgIMAPFolderACL(nsImapMailFolder *folder)
{
  NS_ASSERTION(folder, "need folder");
  m_folder = folder;
  m_rightsHash = new nsHashtable(24);
  m_aclCount = 0;
  BuildInitialACLFromCache();
}

nsMsgIMAPFolderACL::~nsMsgIMAPFolderACL()
{
  m_rightsHash->Reset(FreeHashRights, nsnull);
  delete m_rightsHash;
}

// We cache most of our own rights in the MSG_FOLDER_PREF_* flags
void nsMsgIMAPFolderACL::BuildInitialACLFromCache()
{
  nsCAutoString myrights;
  
  PRUint32 startingFlags;
  m_folder->GetAclFlags(&startingFlags);
  
  if (startingFlags & IMAP_ACL_READ_FLAG)
    myrights += "r";
  if (startingFlags & IMAP_ACL_STORE_SEEN_FLAG)
    myrights += "s";
  if (startingFlags & IMAP_ACL_WRITE_FLAG)
    myrights += "w";
  if (startingFlags & IMAP_ACL_INSERT_FLAG)
    myrights += "i";
  if (startingFlags & IMAP_ACL_POST_FLAG)
    myrights += "p";
  if (startingFlags & IMAP_ACL_CREATE_SUBFOLDER_FLAG)
    myrights +="c";
  if (startingFlags & IMAP_ACL_DELETE_FLAG)
    myrights += "d";
  if (startingFlags & IMAP_ACL_ADMINISTER_FLAG)
    myrights += "a";
  
  if (!myrights.IsEmpty())
    SetFolderRightsForUser(nsnull, myrights.get());
}

void nsMsgIMAPFolderACL::UpdateACLCache()
{
  PRUint32 startingFlags = 0;
  m_folder->GetAclFlags(&startingFlags);
  
  if (GetCanIReadFolder())
    startingFlags |= IMAP_ACL_READ_FLAG;
  else
    startingFlags &= ~IMAP_ACL_READ_FLAG;
  
  if (GetCanIStoreSeenInFolder())
    startingFlags |= IMAP_ACL_STORE_SEEN_FLAG;
  else
    startingFlags &= ~IMAP_ACL_STORE_SEEN_FLAG;
  
  if (GetCanIWriteFolder())
    startingFlags |= IMAP_ACL_WRITE_FLAG;
  else
    startingFlags &= ~IMAP_ACL_WRITE_FLAG;
  
  if (GetCanIInsertInFolder())
    startingFlags |= IMAP_ACL_INSERT_FLAG;
  else
    startingFlags &= ~IMAP_ACL_INSERT_FLAG;
  
  if (GetCanIPostToFolder())
    startingFlags |= IMAP_ACL_POST_FLAG;
  else
    startingFlags &= ~IMAP_ACL_POST_FLAG;
  
  if (GetCanICreateSubfolder())
    startingFlags |= IMAP_ACL_CREATE_SUBFOLDER_FLAG;
  else
    startingFlags &= ~IMAP_ACL_CREATE_SUBFOLDER_FLAG;
  
  if (GetCanIDeleteInFolder())
    startingFlags |= IMAP_ACL_DELETE_FLAG;
  else
    startingFlags &= ~IMAP_ACL_DELETE_FLAG;
  
  if (GetCanIAdministerFolder())
    startingFlags |= IMAP_ACL_ADMINISTER_FLAG;
  else
    startingFlags &= ~IMAP_ACL_ADMINISTER_FLAG;
  
  m_folder->SetAclFlags(startingFlags);
}

PRBool nsMsgIMAPFolderACL::SetFolderRightsForUser(const char *userName, const char *rights)
{
  PRBool ret = PR_FALSE;
  nsXPIDLCString myUserName;
  nsCOMPtr <nsIMsgIncomingServer> server;

  nsresult rv = m_folder->GetServer(getter_AddRefs(server));
  NS_ASSERTION(NS_SUCCEEDED(rv), "error getting server");
  if (NS_FAILED(rv)) 
    return PR_FALSE;
  // we need the real user name to match with what the imap server returns
  // in the acl response.
  server->GetRealUsername(getter_Copies(myUserName));

  nsCAutoString ourUserName;
  
  if (!userName)
    ourUserName.Assign(myUserName);
  else
    ourUserName.Assign(userName);

  ToLowerCase(ourUserName);
  char *rightsWeOwn = PL_strdup(rights);
  nsCStringKey hashKey(ourUserName);
  if (rightsWeOwn && !ourUserName.IsEmpty())
  {
    char *oldValue = (char *) m_rightsHash->Get(&hashKey);
    if (oldValue)
    {
      PR_Free(oldValue);
      m_rightsHash->Remove(&hashKey);
      m_aclCount--;
      NS_ASSERTION(m_aclCount >= 0, "acl count can't go negative");
    }
    m_aclCount++;
    ret = (m_rightsHash->Put(&hashKey, rightsWeOwn) == 0);
  }
  
  if (!ourUserName.IsEmpty() && 
    (myUserName.Equals(ourUserName) || ourUserName.Equals(IMAP_ACL_ANYONE_STRING)))
  {
    // if this is setting an ACL for me, cache it in the folder pref flags
    UpdateACLCache();
  }
  
  return ret;
}

const char *nsMsgIMAPFolderACL::GetRightsStringForUser(const char *inUserName)
{
  nsXPIDLCString userName;
  userName.Assign(inUserName);
  if (userName.IsEmpty())
  {
    nsCOMPtr <nsIMsgIncomingServer> server;

    nsresult rv = m_folder->GetServer(getter_AddRefs(server));
    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting server");
    if (NS_FAILED(rv)) return nsnull;
    // we need the real user name to match with what the imap server returns
    // in the acl response.
    server->GetRealUsername(getter_Copies(userName));
  }
  ToLowerCase(userName);
  nsCStringKey userKey(userName.get());
  
  return (const char *)m_rightsHash->Get(&userKey);
}

// First looks for individual user;  then looks for 'anyone' if the user isn't found.
// Returns defaultIfNotFound, if neither are found.
PRBool nsMsgIMAPFolderACL::GetFlagSetInRightsForUser(const char *userName, char flag, PRBool defaultIfNotFound)
{
  const char *flags = GetRightsStringForUser(userName);
  if (!flags)
  {
    const char *anyoneFlags = GetRightsStringForUser(IMAP_ACL_ANYONE_STRING);
    if (!anyoneFlags)
      return defaultIfNotFound;
    else
      return (strchr(anyoneFlags, flag) != nsnull);
  }
  else
    return (strchr(flags, flag) != nsnull);
}

PRBool nsMsgIMAPFolderACL::GetCanUserLookupFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'l', PR_FALSE);
}

PRBool nsMsgIMAPFolderACL::GetCanUserReadFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'r', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserStoreSeenInFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 's', PR_FALSE);
}

PRBool nsMsgIMAPFolderACL::GetCanUserWriteFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'w', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserInsertInFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'i', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserPostToFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'p', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserCreateSubfolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'c', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserDeleteInFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'd', PR_FALSE);
}

PRBool	nsMsgIMAPFolderACL::GetCanUserAdministerFolder(const char *userName)
{
  return GetFlagSetInRightsForUser(userName, 'a', PR_FALSE);
}

PRBool nsMsgIMAPFolderACL::GetCanILookupFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'l', PR_TRUE);
}

PRBool nsMsgIMAPFolderACL::GetCanIReadFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'r', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanIStoreSeenInFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 's', PR_TRUE);
}

PRBool nsMsgIMAPFolderACL::GetCanIWriteFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'w', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanIInsertInFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'i', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanIPostToFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'p', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanICreateSubfolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'c', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanIDeleteInFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'd', PR_TRUE);
}

PRBool	nsMsgIMAPFolderACL::GetCanIAdministerFolder()
{
  return GetFlagSetInRightsForUser(nsnull, 'a', PR_TRUE);
}

// We use this to see if the ACLs think a folder is shared or not.
// We will define "Shared" in 5.0 to mean:
// At least one user other than the currently authenticated user has at least one
// explicitly-listed ACL right on that folder.
PRBool	nsMsgIMAPFolderACL::GetIsFolderShared()
{
  // If we have more than one ACL count for this folder, which means that someone
  // other than ourself has rights on it, then it is "shared."
  if (m_aclCount > 1)
    return PR_TRUE;
  
  // Or, if "anyone" has rights to it, it is shared.
  nsCStringKey hashKey(IMAP_ACL_ANYONE_STRING);
  const char *anyonesRights = (const char *)m_rightsHash->Get(&hashKey);
  
  return (anyonesRights != nsnull);
}

PRBool nsMsgIMAPFolderACL::GetDoIHaveFullRightsForFolder()
{
  return (GetCanIReadFolder() &&
    GetCanIWriteFolder() &&
    GetCanIInsertInFolder() &&
    GetCanIAdministerFolder() &&
    GetCanICreateSubfolder() &&
    GetCanIDeleteInFolder() &&
    GetCanILookupFolder() &&
    GetCanIStoreSeenInFolder() &&
    GetCanIPostToFolder());
}

// Returns a newly allocated string describing these rights
nsresult nsMsgIMAPFolderACL::CreateACLRightsString(PRUnichar **rightsString)
{
  nsAutoString rights;
  nsXPIDLString curRight;
  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = IMAPGetStringBundle(getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);

  if (GetDoIHaveFullRightsForFolder())
  {
    return bundle->GetStringFromID(IMAP_ACL_FULL_RIGHTS, rightsString);
  }
  else
  {
    if (GetCanIReadFolder())
    {
      bundle->GetStringFromID(IMAP_ACL_READ_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIWriteFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_WRITE_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIInsertInFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_INSERT_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanILookupFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_LOOKUP_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIStoreSeenInFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_SEEN_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIDeleteInFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_DELETE_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanICreateSubfolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_CREATE_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIPostToFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_POST_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
    if (GetCanIAdministerFolder())
    {
      if (!rights.IsEmpty()) rights.AppendLiteral(", ");
      bundle->GetStringFromID(IMAP_ACL_ADMINISTER_RIGHT, getter_Copies(curRight));
      rights.Append(curRight);
    }
  }
  *rightsString = ToNewUnicode(rights);
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetPath(nsIFileSpec ** aPathName)
{
  nsresult rv;
  if (! m_pathName) 
  {
    m_pathName = new nsNativeFileSpec("");
    if (! m_pathName)
       return NS_ERROR_OUT_OF_MEMORY;

    rv = nsImapURI2Path(kImapRootURI, mURI.get(), *m_pathName);
    //    printf("constructing path %s\n", (const char *) *m_pathName);
    if (NS_FAILED(rv)) return rv;
  }
  rv = NS_NewFileSpecWithSpec(*m_pathName, aPathName);
  return NS_OK;
}


NS_IMETHODIMP nsImapMailFolder::SetPath(nsIFileSpec * aPathName)                
{                                                                               
  nsMsgDBFolder::SetPath(aPathName);   // call base class so mPath will get set too
  if (!aPathName)
     return NS_ERROR_NULL_POINTER;

  // not sure why imap has m_pathName and doesn't just use mPath.
  if (!m_pathName)
  {
    m_pathName = new nsFileSpec("");
    if (! m_pathName)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return aPathName->GetFileSpec(m_pathName);                                  
}                                                                               
                                                                                 

nsresult nsImapMailFolder::DisplayStatusMsg(nsIImapUrl *aImapUrl, const PRUnichar *msg)
{
  nsCOMPtr<nsIImapMockChannel> mockChannel;
  aImapUrl->GetMockChannel(getter_AddRefs(mockChannel));
  if (mockChannel)
  {
    nsCOMPtr<nsIProgressEventSink> progressSink;
    mockChannel->GetProgressEventSink(getter_AddRefs(progressSink));
    if (progressSink)
    {
        nsCOMPtr<nsIRequest> request = do_QueryInterface(mockChannel);
        if (!request) return NS_ERROR_FAILURE;
      progressSink->OnStatus(request, nsnull, NS_OK, msg);      // XXX i18n message
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::ProgressStatus(nsIImapProtocol* aProtocol,
                                 PRUint32 aMsgId, const PRUnichar *extraInfo)
{
  nsXPIDLString progressMsg;

  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
  {
    nsCOMPtr<nsIImapServerSink> serverSink = do_QueryInterface(server);
    if (serverSink)
      serverSink->GetImapStringByID(aMsgId, getter_Copies(progressMsg));
  }
  if (progressMsg.IsEmpty())
    IMAPGetStringByID(aMsgId, getter_Copies(progressMsg));

  if (aProtocol && !progressMsg.IsEmpty())
  {
    nsCOMPtr <nsIImapUrl> imapUrl;
    aProtocol->GetRunningImapURL(getter_AddRefs(imapUrl));
    if (imapUrl)
    {
      if (extraInfo)
      {
        PRUnichar *printfString = nsTextFormatter::smprintf(progressMsg, extraInfo);
        if (printfString)
          progressMsg.Adopt(printfString);  
      }
      DisplayStatusMsg(imapUrl, progressMsg);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::PercentProgress(nsIImapProtocol* aProtocol,
                                  ProgressInfo* aInfo)
{
  if (aProtocol)
  {
    nsCOMPtr <nsIImapUrl> imapUrl;
    aProtocol->GetRunningImapURL(getter_AddRefs(imapUrl));
    if (imapUrl)
    {
      nsCOMPtr<nsIImapMockChannel> mockChannel;
      imapUrl->GetMockChannel(getter_AddRefs(mockChannel));
      if (mockChannel)
      {
        nsCOMPtr<nsIProgressEventSink> progressSink;
        mockChannel->GetProgressEventSink(getter_AddRefs(progressSink));
        if (progressSink)
        {
            nsCOMPtr<nsIRequest> request = do_QueryInterface(mockChannel);
            if (!request) return NS_ERROR_FAILURE;

          progressSink->OnProgress(request, nsnull, aInfo->currentProgress, aInfo->maxProgress);
          if (aInfo->message)
            progressSink->OnStatus(request, nsnull, NS_OK, aInfo->message);      // XXX i18n message

        }

      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::CopyNextStreamMessage(PRBool copySucceeded, nsISupports *copyState)
{
    //if copy has failed it could be either user interrupted it or for some other reason
    //don't do any subsequent copies or delete src messages if it is move


    if (!copySucceeded)
      return NS_OK;

    nsresult rv;

    nsCOMPtr<nsImapMailCopyState> mailCopyState = do_QueryInterface(copyState,
                                                                    &rv);
    if (NS_FAILED(rv)) return rv;

    if (!mailCopyState->m_streamCopy) 
      return NS_OK;

    if (mailCopyState->m_curIndex < mailCopyState->m_totalCount)
    {
        mailCopyState->m_message = do_QueryElementAt(mailCopyState->m_messages,
                                                     mailCopyState->m_curIndex,
                                                     &rv);
        if (NS_SUCCEEDED(rv))
        {
            PRBool isRead;
            mailCopyState->m_message->GetIsRead(&isRead);
            mailCopyState->m_unreadCount = (isRead) ? 0 : 1;
            rv = CopyStreamMessage(mailCopyState->m_message,
                                   this, mailCopyState->m_msgWindow, mailCopyState->m_isMove);
        }
    }
    else if (mailCopyState->m_isMove)  
    {
        nsCOMPtr<nsIMsgFolder> srcFolder =
            do_QueryInterface(mailCopyState->m_srcSupport, &rv);
        if (NS_SUCCEEDED(rv) && srcFolder)
        {
          srcFolder->DeleteMessages(mailCopyState->m_messages, nsnull,
            PR_TRUE, PR_TRUE, nsnull, PR_FALSE);
          // we want to send this notification after the source messages have
          // been deleted.
          nsCOMPtr<nsIMsgLocalMailFolder> popFolder = do_QueryInterface(srcFolder); 
          if (popFolder)   //needed if move pop->imap to notify FE
            srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::SetUrlState(nsIImapProtocol* aProtocol,
                              nsIMsgMailNewsUrl* aUrl,
                              PRBool isRunning,
                              nsresult statusCode)
{
  if (!isRunning)
  {
    ProgressStatus(aProtocol, IMAP_DONE, nsnull);
    m_urlRunning = PR_FALSE;
    // if no protocol, then we're reading from the mem or disk cache
    // and we don't want to end the offline download just yet.
    if (aProtocol)
    {
      EndOfflineDownload();
      if (m_downloadingFolderForOfflineUse)
      {
        ReleaseSemaphore(NS_STATIC_CAST(nsIMsgImapMailFolder*, this));
        m_downloadingFolderForOfflineUse = PR_FALSE;
      }
    }
  }

    if (aUrl)
        return aUrl->SetUrlState(isRunning, statusCode);
    return statusCode;
}

// used when copying from local mail folder, or other imap server)
nsresult
nsImapMailFolder::CopyMessagesWithStream(nsIMsgFolder* srcFolder,
                                nsISupportsArray* messages,
                                PRBool isMove,
                                PRBool isCrossServerOp,
                                nsIMsgWindow *msgWindow,
                                nsIMsgCopyServiceListener* listener, 
                                PRBool allowUndo)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!srcFolder || !messages) return rv;

    nsCOMPtr<nsISupports> aSupport(do_QueryInterface(srcFolder, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = InitCopyState(aSupport, messages, isMove, PR_FALSE, isCrossServerOp, listener, msgWindow, allowUndo);
    if(NS_FAILED(rv)) 
      return rv;

    m_copyState->m_streamCopy = PR_TRUE;

    // ** jt - needs to create server to server move/copy undo msg txn
    if (m_copyState->m_allowUndo)
    {
       nsCAutoString messageIds;
       nsMsgKeyArray srcKeyArray;
       nsCOMPtr<nsIUrlListener> urlListener;

       rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));
       rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);

       nsImapMoveCopyMsgTxn* undoMsgTxn = new nsImapMoveCopyMsgTxn(
              srcFolder, &srcKeyArray, messageIds.get(), this,
              PR_TRUE, isMove, m_eventQueue, urlListener);

       if (!undoMsgTxn) return NS_ERROR_OUT_OF_MEMORY;
       if (isMove)
       {
          if (mFlags & MSG_FOLDER_FLAG_TRASH)
            undoMsgTxn->SetTransactionType(nsIMessenger::eDeleteMsg);
          else
            undoMsgTxn->SetTransactionType(nsIMessenger::eMoveMsg);
       }
       else
       {
          undoMsgTxn->SetTransactionType(nsIMessenger::eCopyMsg);
       }
    
       rv = undoMsgTxn->QueryInterface(
         NS_GET_IID(nsImapMoveCopyMsgTxn), 
         getter_AddRefs(m_copyState->m_undoMsgTxn) );
    }
    nsCOMPtr<nsIMsgDBHdr> aMessage;
    aMessage = do_QueryElementAt(messages, 0, &rv);
    if (NS_SUCCEEDED(rv))
        CopyStreamMessage(aMessage, this, msgWindow, isMove);

    return rv; //we are clearing copy state in CopyMessages on failure
}

nsresult nsImapMailFolder::GetClearedOriginalOp(nsIMsgOfflineImapOperation *op, nsIMsgOfflineImapOperation **originalOp, nsIMsgDatabase **originalDB)
{
	nsIMsgOfflineImapOperation *returnOp = nsnull;
  nsOfflineImapOperationType opType;
  op->GetOperation(&opType);
  NS_ASSERTION(opType & nsIMsgOfflineImapOperation::kMoveResult, "not an offline move op");
	
	nsXPIDLCString sourceFolderURI;
	op->GetSourceFolderURI(getter_Copies(sourceFolderURI));
	
  nsCOMPtr<nsIRDFResource> res;
  nsresult rv;

  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) 
    return rv; 
  rv = rdf->GetResource(sourceFolderURI, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> sourceFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && sourceFolder)
    {
	    if (sourceFolder)
	    {
        nsCOMPtr <nsIDBFolderInfo> folderInfo;
        sourceFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), originalDB);
		    if (*originalDB)
		    {
          nsMsgKey originalKey;
          op->GetMessageKey(&originalKey);
          rv = (*originalDB)->GetOfflineOpForKey(originalKey, PR_FALSE, &returnOp);
			    if (NS_SUCCEEDED(rv) && returnOp)
			    {
				    nsXPIDLCString moveDestination;
            nsXPIDLCString thisFolderURI;

            GetURI(getter_Copies(thisFolderURI));

				    returnOp->GetDestinationFolderURI(getter_Copies(moveDestination));
            if (!nsCRT::strcmp(moveDestination, thisFolderURI))
					    returnOp->ClearOperation(nsIMsgOfflineImapOperation::kMoveResult);
			    }
		    }
      }
    }
	}
  NS_IF_ADDREF(returnOp);
  *originalOp = returnOp;
  return rv;
}

nsresult nsImapMailFolder::GetOriginalOp(nsIMsgOfflineImapOperation *op, nsIMsgOfflineImapOperation **originalOp, nsIMsgDatabase **originalDB)
{
  nsIMsgOfflineImapOperation *returnOp = nsnull;
  
  nsXPIDLCString sourceFolderURI;
  op->GetSourceFolderURI(getter_Copies(sourceFolderURI));
	
  nsCOMPtr<nsIRDFResource> res;
  nsresult rv;

  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  if (NS_FAILED(rv)) 
    return rv; 
  rv = rdf->GetResource(sourceFolderURI, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIMsgFolder> sourceFolder(do_QueryInterface(res, &rv));
    if (NS_SUCCEEDED(rv) && sourceFolder)
    {
	    if (sourceFolder)
	    {
        nsCOMPtr <nsIDBFolderInfo> folderInfo;
        sourceFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), originalDB);
		    if (*originalDB)
		    {
          nsMsgKey originalKey;
          op->GetMessageKey(&originalKey);
          rv = (*originalDB)->GetOfflineOpForKey(originalKey, PR_FALSE, &returnOp);
		    }
      }
    }
	}
  NS_IF_ADDREF(returnOp);
  *originalOp = returnOp;
  return rv;
}

nsresult nsImapMailFolder::CopyOfflineMsgBody(nsIMsgFolder *srcFolder, nsIMsgDBHdr *destHdr, nsIMsgDBHdr *origHdr)
{
  nsCOMPtr<nsIOutputStream> outputStream;
  nsresult rv = GetOfflineStoreOutputStream(getter_AddRefs(outputStream));
  nsCOMPtr <nsISeekableStream> seekable;

  seekable = do_QueryInterface(outputStream);

  if (seekable)
  {

    nsMsgKey messageOffset;
    PRUint32 messageSize;
    origHdr->GetMessageOffset(&messageOffset);
    origHdr->GetOfflineMessageSize(&messageSize);

    PRInt64 tellPos;
    seekable->Tell(&tellPos);
    nsInt64 curStorePos = tellPos;
    destHdr->SetMessageOffset((PRUint32) curStorePos);
    nsCOMPtr <nsIInputStream> offlineStoreInputStream;
    rv = srcFolder->GetOfflineStoreInputStream(getter_AddRefs(offlineStoreInputStream));
    if (NS_SUCCEEDED(rv) && offlineStoreInputStream)
    {
      nsCOMPtr<nsISeekableStream> seekStream = do_QueryInterface(offlineStoreInputStream);
      NS_ASSERTION(seekStream, "non seekable stream - can't read from offline msg");
      if (seekStream)
      {
        rv = seekStream->Seek(nsISeekableStream::NS_SEEK_SET, messageOffset);
        if (NS_SUCCEEDED(rv))
        {
          // now, copy the dest folder offline store msg to the temp file
          PRInt32 inputBufferSize = 10240;
          char *inputBuffer = (char *) PR_Malloc(inputBufferSize);
          PRInt32 bytesLeft;
          PRUint32 bytesRead, bytesWritten;
          bytesLeft = messageSize;
          rv = (inputBuffer) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
          while (bytesLeft > 0 && NS_SUCCEEDED(rv))
          {
            rv = offlineStoreInputStream->Read(inputBuffer, inputBufferSize, &bytesRead);
            if (NS_SUCCEEDED(rv) && bytesRead > 0)
            {
              rv = outputStream->Write(inputBuffer, PR_MIN((PRInt32) bytesRead, bytesLeft), &bytesWritten);
              NS_ASSERTION((PRInt32) bytesWritten == PR_MIN((PRInt32) bytesRead, bytesLeft), "wrote out incorrect number of bytes");
            }
            else
              break;
            bytesLeft -= bytesRead;
          }
          PR_FREEIF(inputBuffer);
          outputStream->Flush();
        }
      }
    }
  }
  return rv;
}

// this imap folder is the destination of an offline move/copy.
// We are either offline, or doing a pseudo-offline delete (where we do an offline
// delete, load the next message, then playback the offline delete). 
nsresult nsImapMailFolder::CopyMessagesOffline(nsIMsgFolder* srcFolder,
                               nsISupportsArray* messages,
                               PRBool isMove,
                               nsIMsgWindow *msgWindow,
                               nsIMsgCopyServiceListener* listener)
{
  NS_ENSURE_ARG(messages);
  nsresult rv = NS_OK;
  nsresult stopit = 0;
  nsCOMPtr <nsIMsgDatabase> sourceMailDB;
  nsCOMPtr <nsIDBFolderInfo> srcDbFolderInfo;
  srcFolder->GetDBFolderInfoAndDB(getter_AddRefs(srcDbFolderInfo), getter_AddRefs(sourceMailDB));
  PRBool deleteToTrash = PR_FALSE;
  PRBool deleteImmediately = PR_FALSE;
  PRUint32 srcCount;
  messages->Count(&srcCount);
  nsCOMPtr<nsIImapIncomingServer> imapServer;
  rv = GetImapIncomingServer(getter_AddRefs(imapServer));

  if (NS_SUCCEEDED(rv) && imapServer)
  {
    nsMsgImapDeleteModel deleteModel;
    imapServer->GetDeleteModel(&deleteModel);
    deleteToTrash = (deleteModel == nsMsgImapDeleteModels::MoveToTrash);
    deleteImmediately = (deleteModel == nsMsgImapDeleteModels::DeleteNoTrash);
  }	
  if (sourceMailDB)
  {
    // save the future ops in the source DB, if this is not a imap->local copy/move
    
    nsCOMPtr <nsITransactionManager> txnMgr;
    if (msgWindow)
      msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
    if (txnMgr)
      txnMgr->BeginBatch();
    GetDatabase(nsnull);
    if (mDatabase) 
    {
      // get the highest key in the dest db, so we can make up our fake keys
      PRBool highWaterDeleted = PR_FALSE;
      nsMsgKey fakeBase = 1;
      nsCOMPtr <nsIDBFolderInfo> folderInfo;
      rv = mDatabase->GetDBFolderInfo(getter_AddRefs(folderInfo));
      NS_ENSURE_SUCCESS(rv, rv);
      nsMsgKey highWaterMark = nsMsgKey_None;
      folderInfo->GetHighWater(&highWaterMark);
      
      fakeBase += highWaterMark;
      
      // put fake message in destination db, delete source if move
      for (PRUint32 sourceKeyIndex=0; !stopit && (sourceKeyIndex < srcCount); sourceKeyIndex++)
      {
        PRBool	messageReturningHome = PR_FALSE;
        nsXPIDLCString sourceFolderURI;
        srcFolder->GetURI(getter_Copies(sourceFolderURI));
        nsXPIDLCString originalSrcFolderURI;
        if (sourceFolderURI.get())
          originalSrcFolderURI.Adopt(nsCRT::strdup(sourceFolderURI.get()));
        nsCOMPtr<nsIMsgDBHdr> message;
        message = do_QueryElementAt(messages, sourceKeyIndex);
        nsMsgKey originalKey;
        if (message)
        {
          rv = message->GetMessageKey(&originalKey);
        }
        else
        {
          NS_ASSERTION(PR_FALSE, "bad msg in src array");
          continue;
        }
        nsCOMPtr <nsIMsgOfflineImapOperation> sourceOp;
        rv = sourceMailDB->GetOfflineOpForKey(originalKey, PR_TRUE, getter_AddRefs(sourceOp));
        if (NS_SUCCEEDED(rv) && sourceOp)
        {
          srcFolder->SetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
          nsCOMPtr <nsIMsgDatabase> originalDB;
          nsOfflineImapOperationType opType;
          sourceOp->GetOperation(&opType);
          // if we already have an offline op for this key, then we need to see if it was
          // moved into the source folder while offline
          if (opType == nsIMsgOfflineImapOperation::kMoveResult) // offline move
          {
            // gracious me, we are moving something we already moved while offline!
            // find the original operation and clear it!
            nsCOMPtr <nsIMsgOfflineImapOperation> originalOp;
            rv = GetClearedOriginalOp(sourceOp, getter_AddRefs(originalOp), getter_AddRefs(originalDB));
            if (originalOp)
            {
              nsXPIDLCString originalString;
              nsXPIDLCString srcFolderURI;
              
              srcFolder->GetURI(getter_Copies(srcFolderURI));
              sourceOp->GetSourceFolderURI(getter_Copies(originalString));
              sourceOp->GetMessageKey(&originalKey);
              originalSrcFolderURI.Adopt(originalString.get() ? nsCRT::strdup(originalString.get()) : 0);
              
              if (isMove)
                sourceMailDB->RemoveOfflineOp(sourceOp);
              
              sourceOp = originalOp;
              if (!nsCRT::strcmp(originalSrcFolderURI, srcFolderURI))
              {
                messageReturningHome = PR_TRUE;
                originalDB->RemoveOfflineOp(originalOp);
              }
            }
          }
          
          if (!messageReturningHome)
          {
            nsXPIDLCString folderURI;
            GetURI(getter_Copies(folderURI));
            
            if (isMove)
            {
              sourceOp->SetDestinationFolderURI(folderURI); // offline move
              sourceOp->SetOperation(nsIMsgOfflineImapOperation::kMsgMoved);
            }
            else
              sourceOp->AddMessageCopyOperation(folderURI); // offline copy

           nsMsgKeyArray srcKeyArray;
           nsCOMPtr<nsIUrlListener> urlListener;

            sourceOp->GetOperation(&opType);
           srcKeyArray.Add(originalKey);
           rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));
           nsImapOfflineTxn *undoMsgTxn = new 
              nsImapOfflineTxn(srcFolder, &srcKeyArray, this, isMove, opType, message,
                m_eventQueue, urlListener);

            if (undoMsgTxn)
            {
              if (isMove)
                undoMsgTxn->SetTransactionType(nsIMessenger::eMoveMsg);
              else
                undoMsgTxn->SetTransactionType(nsIMessenger::eCopyMsg);
              // we're adding this undo action before the delete is successful. This is evil,
              // but 4.5 did it as well.
              if (txnMgr)
                txnMgr->DoTransaction(undoMsgTxn);
            }
          }
          PRBool hasMsgOffline = PR_FALSE;
          srcFolder->HasMsgOffline(originalKey, &hasMsgOffline);
//          if (hasMsgOffline)
//            CopyOfflineMsgBody(srcFolder, originalKey);

        }
        else
          stopit = NS_ERROR_FAILURE;
        
        
        nsCOMPtr <nsIMsgDBHdr> mailHdr;
        rv = sourceMailDB->GetMsgHdrForKey(originalKey, getter_AddRefs(mailHdr));
        if (NS_SUCCEEDED(rv) && mailHdr)
        {
          PRBool successfulCopy = PR_FALSE;
          nsMsgKey srcDBhighWaterMark;
          srcDbFolderInfo->GetHighWater(&srcDBhighWaterMark);
          highWaterDeleted = !highWaterDeleted && isMove && deleteToTrash &&
            (originalKey == srcDBhighWaterMark);
          
          nsCOMPtr <nsIMsgDBHdr> newMailHdr;
          rv = mDatabase->CopyHdrFromExistingHdr(fakeBase + sourceKeyIndex, mailHdr,
            PR_TRUE, getter_AddRefs(newMailHdr));
          if (!newMailHdr || NS_FAILED(rv))
          {
            NS_ASSERTION(PR_FALSE, "failed to copy hdr");
            stopit = rv;
          }
          
          if (NS_SUCCEEDED(stopit))
          {
            PRBool hasMsgOffline = PR_FALSE;
            srcFolder->HasMsgOffline(originalKey, &hasMsgOffline);
            if (hasMsgOffline)
              CopyOfflineMsgBody(srcFolder, newMailHdr, mailHdr);

            nsCOMPtr <nsIMsgOfflineImapOperation> destOp;
            mDatabase->GetOfflineOpForKey(fakeBase + sourceKeyIndex, PR_TRUE, getter_AddRefs(destOp));
            if (destOp)
            {
              // check if this is a move back to the original mailbox, in which case
              // we just delete the offline operation.
              if (messageReturningHome)
              {
                mDatabase->RemoveOfflineOp(destOp);
              }
              else
              {
                SetFlag(MSG_FOLDER_FLAG_OFFLINEEVENTS);
                destOp->SetSourceFolderURI(originalSrcFolderURI);
                destOp->SetMessageKey(originalKey);
                {
                  nsCOMPtr<nsIUrlListener> urlListener;

                  QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));
                  nsMsgKeyArray keyArray;
                  keyArray.Add(fakeBase + sourceKeyIndex);
                  nsImapOfflineTxn *undoMsgTxn = new 
                    nsImapOfflineTxn(this, &keyArray, this, isMove, nsIMsgOfflineImapOperation::kAddedHeader,
                      newMailHdr, m_eventQueue, urlListener);
                 if (undoMsgTxn)
                 {
                   if (txnMgr)
                     txnMgr->DoTransaction(undoMsgTxn);
                 }
                }
              }
            }
            else
              stopit = NS_ERROR_FAILURE;
          }
          successfulCopy = NS_SUCCEEDED(stopit);
          
          
          nsMsgKey msgKey;
          mailHdr->GetMessageKey(&msgKey);
          if (isMove && successfulCopy)	
          {
           nsMsgKeyArray srcKeyArray;
           nsCOMPtr<nsIUrlListener> urlListener;

           srcKeyArray.Add(msgKey);
           rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));

            nsOfflineImapOperationType opType = nsIMsgOfflineImapOperation::kDeletedMsg;
            if (!deleteToTrash)
              opType = nsIMsgOfflineImapOperation::kMsgMarkedDeleted;
            srcKeyArray.Add(msgKey);
            nsImapOfflineTxn *undoMsgTxn = new 
              nsImapOfflineTxn(srcFolder, &srcKeyArray, this, isMove, opType, mailHdr,
                m_eventQueue, urlListener);
             if (undoMsgTxn)
             {
               if (isMove)
               {
                 if (mFlags & MSG_FOLDER_FLAG_TRASH)
                   undoMsgTxn->SetTransactionType(nsIMessenger::eDeleteMsg);
                 else
                   undoMsgTxn->SetTransactionType(nsIMessenger::eMoveMsg);
               }
               else
               {
                  undoMsgTxn->SetTransactionType(nsIMessenger::eCopyMsg);
               }
               if (txnMgr)
                 txnMgr->DoTransaction(undoMsgTxn);
             }
            if (deleteToTrash || deleteImmediately)
              sourceMailDB->DeleteMessage(msgKey, nsnull, PR_FALSE);
            else
              sourceMailDB->MarkImapDeleted(msgKey,PR_TRUE,nsnull); // offline delete
          }
          
          
          if (!successfulCopy)
            highWaterDeleted = PR_FALSE;
        }
      }
            
      if (isMove)
        sourceMailDB->Commit(nsMsgDBCommitType::kLargeCommit);
      mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
      SummaryChanged();
      srcFolder->SummaryChanged();
    }
    if (txnMgr)
      txnMgr->EndBatch();
  }

  nsCOMPtr<nsISupports> srcSupport = do_QueryInterface(srcFolder);
  OnCopyCompleted(srcSupport, rv);

  if (NS_SUCCEEDED(rv) && isMove)
    srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);

  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::CopyMessages(nsIMsgFolder* srcFolder,
                               nsISupportsArray* messages,
                               PRBool isMove,
                               nsIMsgWindow *msgWindow,
                               nsIMsgCopyServiceListener* listener,
                               PRBool isFolder, //isFolder for future use when we do cross-server folder move/copy
                               PRBool allowUndo)  
{
  nsresult rv = NS_OK;
  nsCAutoString messageIds;
  nsMsgKeyArray srcKeyArray;
  nsCOMPtr<nsIUrlListener> urlListener;
  nsCOMPtr<nsISupports> srcSupport;
  nsCOMPtr<nsISupports> copySupport;

  if (WeAreOffline())
    return CopyMessagesOffline(srcFolder, messages, isMove, msgWindow, listener);

  nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  srcSupport = do_QueryInterface(srcFolder);

  nsCOMPtr <nsIMsgIncomingServer> srcServer;
  nsCOMPtr <nsIMsgIncomingServer> dstServer;

  rv = srcFolder->GetServer(getter_AddRefs(srcServer));
  if(NS_FAILED(rv)) goto done;

  rv = GetServer(getter_AddRefs(dstServer));
  if(NS_FAILED(rv)) goto done;

  NS_ENSURE_TRUE(dstServer, NS_ERROR_NULL_POINTER);
  PRBool sameServer;
  rv = dstServer->Equals(srcServer, &sameServer);
  if (NS_FAILED(rv)) goto done;

   PRUint32 supportedUserFlags;
   GetSupportedUserFlags(&supportedUserFlags);

   if (! (supportedUserFlags & kImapMsgSupportUserFlag))
   {
      PRUint32 count = 0;
      PRUint32 i;

      rv = messages->Count(&count);
      if (NS_FAILED(rv)) return rv;

      // check if any msg hdr has special flags or properties set
      // that we need to set on the dest hdr
      for (i = 0; i < count; i++)
      {
        nsCOMPtr <nsIMsgDBHdr> msgDBHdr = do_QueryElementAt(messages, i, &rv);
        if (mDatabase && msgDBHdr)
        {
          nsMsgLabelValue label;
          nsXPIDLCString junkScore, junkScoreOrigin;
          msgDBHdr->GetStringProperty("junkscore", getter_Copies(junkScore));
          msgDBHdr->GetStringProperty("junkscoreorigin", getter_Copies(junkScoreOrigin));
          if (!junkScore.IsEmpty()) // ignore already scored messages.
            mDatabase->SetAttributesOnPendingHdr(msgDBHdr, "junkscore", junkScore.get(), 0);
          if (!junkScoreOrigin.IsEmpty())
            mDatabase->SetAttributesOnPendingHdr(msgDBHdr, "junkscoreorigin", junkScore.get(), 0);
          msgDBHdr->GetLabel(&label);
          if (label != 0)
          {
            nsCAutoString labelStr;
            labelStr.AppendInt(label);
            mDatabase->SetAttributesOnPendingHdr(msgDBHdr, "label", labelStr.get(), 0);
          }
        }
      }
   }
  // if the folders aren't on the same server, do a stream base copy
  if (!sameServer) 
  {
    rv = CopyMessagesWithStream(srcFolder, messages, isMove, PR_TRUE, msgWindow, listener, allowUndo);
    goto done;
  }

  rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);
  if(NS_FAILED(rv)) goto done;

  rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));

  rv = InitCopyState(srcSupport, messages, isMove, PR_TRUE, PR_FALSE, listener, msgWindow, allowUndo);
  if (NS_FAILED(rv)) goto done;

  m_copyState->m_curIndex = m_copyState->m_totalCount;

  if (isMove)
    srcFolder->EnableNotifications(allMessageCountNotifications, PR_FALSE, PR_TRUE/* dbBatching*/);  //disable message count notification 

  copySupport = do_QueryInterface(m_copyState);
  if (imapService)
  rv = imapService->OnlineMessageCopy(m_eventQueue,
                                            srcFolder, messageIds.get(),
                                            this, PR_TRUE, isMove,
                                            urlListener, nsnull,
                                            copySupport, msgWindow);
  if (m_copyState->m_allowUndo)
    if (NS_SUCCEEDED(rv))
    {
       nsImapMoveCopyMsgTxn* undoMsgTxn = new nsImapMoveCopyMsgTxn(
       srcFolder, &srcKeyArray, messageIds.get(), this,
       PR_TRUE, isMove, m_eventQueue, urlListener);
       if (!undoMsgTxn) return NS_ERROR_OUT_OF_MEMORY;
       if (isMove)
       {
         if (mFlags & MSG_FOLDER_FLAG_TRASH)
           undoMsgTxn->SetTransactionType(nsIMessenger::eDeleteMsg);
         else
           undoMsgTxn->SetTransactionType(nsIMessenger::eMoveMsg);
       }
       else
       {
          undoMsgTxn->SetTransactionType(nsIMessenger::eCopyMsg);
       }
       rv = undoMsgTxn->QueryInterface(
         NS_GET_IID(nsImapMoveCopyMsgTxn), 
         getter_AddRefs(m_copyState->m_undoMsgTxn) );
    }
    else 
      NS_ASSERTION(PR_FALSE, "online copy failed");

done:
    if (NS_FAILED(rv))
    {
      (void) OnCopyCompleted(srcSupport, PR_FALSE);
      if (isMove)
      {
        srcFolder->EnableNotifications(allMessageCountNotifications, PR_TRUE, PR_TRUE/* dbBatching*/);  //enable message count notification 
        NotifyFolderEvent(mDeleteOrMoveMsgFailedAtom);
      }
    }
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::CopyFolder(nsIMsgFolder* srcFolder,
                               PRBool isMoveFolder,
                               nsIMsgWindow *msgWindow,
                               nsIMsgCopyServiceListener* listener)
{

  NS_ENSURE_ARG_POINTER(srcFolder);
  
  nsresult rv = NS_OK;

  if (isMoveFolder)   //move folder permitted when dstFolder and the srcFolder are on same server
  {
    nsCOMPtr <nsIImapService> imapService = do_GetService (NS_IMAPSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr <nsIUrlListener> urlListener = do_QueryInterface(srcFolder);
      PRBool match = PR_FALSE;
      PRBool confirmed = PR_FALSE;
      if (mFlags & MSG_FOLDER_FLAG_TRASH)
      {
        rv = srcFolder->MatchOrChangeFilterDestination(nsnull, PR_FALSE, &match);
        if (match)
        {
          srcFolder->ConfirmFolderDeletionForFilter(msgWindow, &confirmed);
          if (!confirmed) return NS_OK;
        }
      }
      rv = imapService->MoveFolder(m_eventQueue,
                                   srcFolder,
                                   this,
                                   urlListener,
                                   msgWindow,
                                   nsnull);
    }

  }
  else
	  NS_ASSERTION(0,"isMoveFolder is false. Trying to copy to a different server.");

  return rv;
  
}

NS_IMETHODIMP
nsImapMailFolder::CopyFileMessage(nsIFileSpec* fileSpec,
                                  nsIMsgDBHdr* msgToReplace,
                                  PRBool isDraftOrTemplate,
                                  nsIMsgWindow *msgWindow,
                                  nsIMsgCopyServiceListener* listener)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsMsgKey key = 0xffffffff;
    nsCAutoString messageId;
    nsCOMPtr<nsIUrlListener> urlListener;
    nsCOMPtr<nsISupportsArray> messages;
    nsCOMPtr<nsISupports> srcSupport = do_QueryInterface(fileSpec, &rv);

    rv = NS_NewISupportsArray(getter_AddRefs(messages));
    if (NS_FAILED(rv)) 
      return OnCopyCompleted(srcSupport, rv);

    nsCOMPtr<nsIImapService> imapService = do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
      return OnCopyCompleted(srcSupport, rv);

    rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));

    if (msgToReplace)
    {
        rv = msgToReplace->GetMessageKey(&key);
        if (NS_SUCCEEDED(rv))
            messageId.AppendInt((PRInt32) key);
    }

    rv = InitCopyState(srcSupport, messages, PR_FALSE, isDraftOrTemplate,
                       PR_FALSE, listener, msgWindow, PR_FALSE);
    if (NS_FAILED(rv)) 
      return OnCopyCompleted(srcSupport, rv);

    nsCOMPtr<nsISupports> copySupport;
    if( m_copyState ) 
        copySupport = do_QueryInterface(m_copyState);
    if (!isDraftOrTemplate)
      m_copyState->m_totalCount = 1;
    rv = imapService->AppendMessageFromFile(m_eventQueue, fileSpec, this,
                                            messageId.get(),
                                            PR_TRUE, isDraftOrTemplate,
                                            urlListener, nsnull,
                                            copySupport,
                                            msgWindow);
    if (NS_FAILED(rv))
      return OnCopyCompleted(srcSupport, rv);

    return rv;
}

nsresult 
nsImapMailFolder::CopyStreamMessage(nsIMsgDBHdr* message,
                                    nsIMsgFolder* dstFolder, // should be this
                                    nsIMsgWindow *aMsgWindow,
                                    PRBool isMove)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!m_copyState) return rv;

    nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener = do_CreateInstance(NS_COPYMESSAGESTREAMLISTENER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsICopyMessageListener>
        copyListener(do_QueryInterface(dstFolder, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgFolder>
        srcFolder(do_QueryInterface(m_copyState->m_srcSupport, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = copyStreamListener->Init(srcFolder, copyListener, nsnull);
    if (NS_FAILED(rv)) return rv;
       
    nsCOMPtr<nsIMsgDBHdr> msgHdr(do_QueryInterface(message));
    if (!msgHdr) return NS_ERROR_FAILURE;

    nsXPIDLCString uri;
    srcFolder->GetUriForMsg(msgHdr, getter_Copies(uri));

    if (!m_copyState->m_msgService)
    {
        rv = GetMessageServiceFromURI(uri, getter_AddRefs(m_copyState->m_msgService));
    }

    if (NS_SUCCEEDED(rv) && m_copyState->m_msgService)
    {
    nsCOMPtr<nsIStreamListener>
            streamListener(do_QueryInterface(copyStreamListener, &rv));
    if(NS_FAILED(rv) || !streamListener)
      return NS_ERROR_NO_INTERFACE;

        rv = m_copyState->m_msgService->CopyMessage(uri, streamListener,
                                                     isMove && !m_copyState->m_isCrossServerOp, nsnull, aMsgWindow, nsnull);
  }
    return rv;
}

nsImapMailCopyState::nsImapMailCopyState() :
    m_isMove(PR_FALSE), m_selectedState(PR_FALSE),
    m_isCrossServerOp(PR_FALSE), m_curIndex(0),
    m_totalCount(0), m_streamCopy(PR_FALSE), m_dataBuffer(nsnull),
    m_dataBufferSize(0), m_leftOver(0), m_allowUndo(PR_FALSE), 
    m_eatLF(PR_FALSE)
{
}

nsImapMailCopyState::~nsImapMailCopyState()
{
    PR_Free(m_dataBuffer);
    if (m_msgService && m_message)
    {
      nsCOMPtr <nsIMsgFolder> srcFolder = do_QueryInterface(m_srcSupport);
      if (srcFolder)
      {
        nsXPIDLCString uri;
        srcFolder->GetUriForMsg(m_message, getter_Copies(uri));
      }
    }
    if (m_tmpFileSpec)
    {
        PRBool isOpen = PR_FALSE;
        nsFileSpec  fileSpec;
        if (isOpen)
            m_tmpFileSpec->CloseStream();
        m_tmpFileSpec->GetFileSpec(&fileSpec);
        if (fileSpec.Valid())
            fileSpec.Delete(PR_FALSE);
    }
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsImapMailCopyState, nsImapMailCopyState)

nsresult
nsImapMailFolder::InitCopyState(nsISupports* srcSupport,
                                nsISupportsArray* messages,
                                PRBool isMove,
                                PRBool selectedState,
                                PRBool acrossServers,
                                nsIMsgCopyServiceListener* listener,
                                nsIMsgWindow *msgWindow,
                                PRBool allowUndo)
{
    nsresult rv = NS_OK;

    if (!srcSupport || !messages) return NS_ERROR_NULL_POINTER;
    NS_ASSERTION(!m_copyState, "move/copy already in progress");
    if (m_copyState) return NS_ERROR_FAILURE;

    nsImapMailCopyState* copyState = new nsImapMailCopyState();
    m_copyState = do_QueryInterface(copyState);

    if (!m_copyState) 
      return NS_ERROR_OUT_OF_MEMORY;

    m_copyState->m_isCrossServerOp = acrossServers;
    if (srcSupport)
        m_copyState->m_srcSupport = do_QueryInterface(srcSupport, &rv);

    if (NS_SUCCEEDED(rv))
    {
      m_copyState->m_messages = do_QueryInterface(messages, &rv);
      rv = messages->Count(&m_copyState->m_totalCount);
      if (!m_copyState->m_isCrossServerOp)
      {
        if (NS_SUCCEEDED(rv))
        {
            PRUint32 numUnread = 0;
            for (PRUint32 keyIndex=0; keyIndex < m_copyState->m_totalCount; keyIndex++)
            {
              nsCOMPtr<nsIMsgDBHdr> message =
                  do_QueryElementAt(m_copyState->m_messages, keyIndex, &rv);
              // if the key is not there, then assume what the caller tells us to.
              PRBool isRead = PR_FALSE;
              PRUint32 flags;
              if (message )
              {
                message->GetFlags(&flags);
                isRead = flags & MSG_FLAG_READ;
              }

              if (!isRead)
                numUnread++;
            }
            m_copyState->m_unreadCount = numUnread;
        }
      }
      else
      {
        nsCOMPtr<nsIMsgDBHdr> message =
            do_QueryElementAt(m_copyState->m_messages,
                              m_copyState->m_curIndex, &rv);
          // if the key is not there, then assume what the caller tells us to.
        PRBool isRead = PR_FALSE;
        PRUint32 flags;
        if (message )
        {
          message->GetFlags(&flags);
          isRead = flags & MSG_FLAG_READ;
        }

        m_copyState->m_unreadCount = (isRead) ? 0 : 1;
      }
    }
    m_copyState->m_isMove = isMove;
    m_copyState->m_allowUndo = allowUndo;
    m_copyState->m_selectedState = selectedState;
    m_copyState->m_msgWindow = msgWindow;
    if (listener)
        m_copyState->m_listener = do_QueryInterface(listener, &rv);
        
    return rv;
}

nsresult
nsImapMailFolder::OnCopyCompleted(nsISupports *srcSupport, nsresult rv)
{
  m_copyState = nsnull;
  nsresult result;
  nsCOMPtr<nsIMsgCopyService> copyService = 
    do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &result);
  if (NS_SUCCEEDED(result))
    copyService->NotifyCompletion(srcSupport, this, rv);    
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::MatchName(nsString *name, PRBool *matches)
{
  if (!matches)
    return NS_ERROR_NULL_POINTER;
    PRBool isInbox = mName.LowerCaseEqualsLiteral("inbox");
    if (isInbox)
        *matches = mName.Equals(*name, nsCaseInsensitiveStringComparator());
    else
        *matches = mName.Equals(*name);
        
  return NS_OK;
}

nsresult nsImapMailFolder::CreateBaseMessageURI(const char *aURI)
{
  nsresult rv;

  rv = nsCreateImapBaseMessageURI(aURI, &mBaseMessageURI);
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderURL(char **aFolderURL)
{
  NS_ENSURE_ARG_POINTER(aFolderURL);
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLCString rootURI;
  rootFolder->GetURI(getter_Copies(rootURI));

  NS_ASSERTION(mURI.Length() > rootURI.Length(), "Should match with a folder name!");
  nsAdoptingCString escapedName(nsEscape(mURI.get() + rootURI.Length(),
                                         url_Path));
  if (escapedName.IsEmpty()) {
      return NS_ERROR_OUT_OF_MEMORY;
  }

  *aFolderURL = ToNewCString(rootURI + escapedName);
  if (!*aFolderURL)
      return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP nsImapMailFolder::GetFolderNeedsSubscribing(PRBool *bVal)
{
    if (!bVal)
        return NS_ERROR_NULL_POINTER;
    *bVal = m_folderNeedsSubscribing;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderNeedsSubscribing(PRBool bVal)
{
    m_folderNeedsSubscribing = bVal;
    return NS_OK;
}

nsMsgIMAPFolderACL * nsImapMailFolder::GetFolderACL()
{
  if (!m_folderACL)
    m_folderACL = new nsMsgIMAPFolderACL(this);
  return m_folderACL;
}

nsresult nsImapMailFolder::CreateACLRightsStringForFolder(PRUnichar **rightsString)
{
  NS_ENSURE_ARG_POINTER(rightsString);
  GetFolderACL();	// lazy create
  if (m_folderACL)
  {
    return m_folderACL->CreateACLRightsString(rightsString);
  }
  return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsImapMailFolder::GetFolderNeedsACLListed(PRBool *bVal)
{
  NS_ENSURE_ARG_POINTER(bVal);
  PRBool dontNeedACLListed = !m_folderNeedsACLListed;
  // if we haven't acl listed, and it's not a no select folder or the inbox,
  //  then we'll list the acl if it's not a namespace.
  if (m_folderNeedsACLListed && !(mFlags & MSG_FOLDER_FLAG_IMAP_NOSELECT | MSG_FOLDER_FLAG_INBOX))
    GetIsNamespace(&dontNeedACLListed);

  *bVal = !dontNeedACLListed;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderNeedsACLListed(PRBool bVal)
{
    m_folderNeedsACLListed = bVal;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetIsNamespace(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv = NS_OK;
  if (!m_namespace)
  {
#ifdef DEBUG_bienvenu
    // Make sure this isn't causing us to open the database
    NS_ASSERTION(m_hierarchyDelimiter != kOnlineHierarchySeparatorUnknown, "hierarchy delimiter not set");
#endif

    nsXPIDLCString onlineName;
    nsXPIDLCString serverKey;
    GetServerKey(getter_Copies(serverKey));
    GetOnlineName(getter_Copies(onlineName));
    PRUnichar hierarchyDelimiter;
    GetHierarchyDelimiter(&hierarchyDelimiter);

    nsCOMPtr<nsIImapHostSessionList> hostSession = 
             do_GetService(kCImapHostSessionList, &rv);

    if (NS_SUCCEEDED(rv) && hostSession)
    {
      m_namespace = nsIMAPNamespaceList::GetNamespaceForFolder(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter);
      if (m_namespace == nsnull)
      {
        if (mFlags & MSG_FOLDER_FLAG_IMAP_OTHER_USER)
        {
           rv = hostSession->GetDefaultNamespaceOfTypeForHost(serverKey.get(), kOtherUsersNamespace, m_namespace);
        }
        else if (mFlags & MSG_FOLDER_FLAG_IMAP_PUBLIC)
        {
          rv = hostSession->GetDefaultNamespaceOfTypeForHost(serverKey.get(), kPublicNamespace, m_namespace);
        }
        else 
        {
          rv = hostSession->GetDefaultNamespaceOfTypeForHost(serverKey.get(), kPersonalNamespace, m_namespace);
        }
      }
      NS_ASSERTION(m_namespace, "failed to get namespace");
      if (m_namespace)
      {
        nsIMAPNamespaceList::SuggestHierarchySeparatorForNamespace(m_namespace, (char) hierarchyDelimiter);
        m_folderIsNamespace = nsIMAPNamespaceList::GetFolderIsNamespace(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter, m_namespace);
      }
    }
  } 
  *aResult = m_folderIsNamespace;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::SetIsNamespace(PRBool isNamespace)
{
  m_folderIsNamespace = isNamespace;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::ResetNamespaceReferences()
{
  nsXPIDLCString serverKey;
  nsXPIDLCString onlineName;
  GetServerKey(getter_Copies(serverKey));
  GetOnlineName(getter_Copies(onlineName));
  PRUnichar hierarchyDelimiter;
  GetHierarchyDelimiter(&hierarchyDelimiter);
  m_namespace = nsIMAPNamespaceList::GetNamespaceForFolder(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter);
//  NS_ASSERTION(m_namespace, "resetting null namespace");
  if (m_namespace)
    m_folderIsNamespace = nsIMAPNamespaceList::GetFolderIsNamespace(serverKey.get(), onlineName.get(), (char) hierarchyDelimiter, m_namespace);
  else
    m_folderIsNamespace = PR_FALSE;
  
  nsCOMPtr<nsIEnumerator> aEnumerator;
  GetSubFolders(getter_AddRefs(aEnumerator));
  if (!aEnumerator)
    return NS_OK;
  nsCOMPtr<nsISupports> aSupport;
  nsresult rv = aEnumerator->First();
  while (NS_SUCCEEDED(rv))
  {
     rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
			
     nsCOMPtr<nsIMsgImapMailFolder> folder = do_QueryInterface(aSupport, &rv);
     if (NS_FAILED(rv)) return rv;
     folder->ResetNamespaceReferences();
     rv = aEnumerator->Next();
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::FindOnlineSubFolder(const char *targetOnlineName, nsIMsgImapMailFolder **aResultFolder)
{
  nsresult rv = NS_OK;

  nsXPIDLCString onlineName;
  GetOnlineName(getter_Copies(onlineName));

  if (onlineName.Equals(targetOnlineName))
  {
    return QueryInterface(NS_GET_IID(nsIMsgImapMailFolder), (void **) aResultFolder);
  }
  nsCOMPtr<nsIEnumerator> aEnumerator;
  GetSubFolders(getter_AddRefs(aEnumerator));
  if (!aEnumerator)
    return NS_OK;
  nsCOMPtr<nsISupports> aSupport;
  rv = aEnumerator->First();
  while (NS_SUCCEEDED(rv))
  {
     rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
			
     nsCOMPtr<nsIMsgImapMailFolder> folder = do_QueryInterface(aSupport, &rv);
     if (NS_FAILED(rv)) return rv;
     rv = folder->FindOnlineSubFolder(targetOnlineName, aResultFolder);
     if (*aResultFolder)
       return rv;
     rv = aEnumerator->Next();
  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderNeedsAdded(PRBool *bVal)
{
    if (!bVal)
        return NS_ERROR_NULL_POINTER;
    *bVal = m_folderNeedsAdded;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderNeedsAdded(PRBool bVal)
{
    m_folderNeedsAdded = bVal;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderQuotaCommandIssued(PRBool *aCmdIssued)
{
    NS_ENSURE_ARG_POINTER(aCmdIssued);
    *aCmdIssued = m_folderQuotaCommandIssued;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderQuotaCommandIssued(PRBool aCmdIssued)
{
    m_folderQuotaCommandIssued = aCmdIssued;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderQuotaDataIsValid(PRBool *aIsValid)
{
    NS_ENSURE_ARG_POINTER(aIsValid);
    *aIsValid = m_folderQuotaDataIsValid;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderQuotaDataIsValid(PRBool aIsValid)
{
    m_folderQuotaDataIsValid = aIsValid;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderQuotaRoot(nsACString &aQuotaRoot)
{
    aQuotaRoot = m_folderQuotaRoot;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderQuotaRoot(const nsACString &aQuotaRoot)
{
    m_folderQuotaRoot = aQuotaRoot;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderQuotaUsedKB(PRUint32 *aUsedKB)
{
    NS_ENSURE_ARG_POINTER(aUsedKB);
    *aUsedKB = m_folderQuotaUsedKB;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderQuotaUsedKB(PRUint32 aUsedKB)
{
    m_folderQuotaUsedKB = aUsedKB;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetFolderQuotaMaxKB(PRUint32 *aMaxKB)
{
    NS_ENSURE_ARG_POINTER(aMaxKB);
    *aMaxKB = m_folderQuotaMaxKB;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderQuotaMaxKB(PRUint32 aMaxKB)
{
    m_folderQuotaMaxKB = aMaxKB;
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::PerformExpand(nsIMsgWindow *aMsgWindow)
{
    nsresult rv;
    PRBool usingSubscription = PR_FALSE;
    nsCOMPtr<nsIImapIncomingServer> imapServer;
    rv = GetImapIncomingServer(getter_AddRefs(imapServer));

    if (NS_FAILED(rv) || !imapServer) return NS_ERROR_FAILURE;
    rv = imapServer->GetUsingSubscription(&usingSubscription);
    if (NS_SUCCEEDED(rv) && !usingSubscription)
    {
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
            rv = imapService->DiscoverChildren(m_eventQueue, this, this,
                                               m_onlineFolderName.get(),
                                               nsnull);
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::RenameClient(nsIMsgWindow *msgWindow, nsIMsgFolder *msgFolder, const char* oldName, const char* newName )
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;

    nsFileSpec path;
    rv = pathSpec->GetFileSpec(&path);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgImapMailFolder> oldImapFolder = do_QueryInterface(msgFolder, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUnichar hierarchyDelimiter = '/';
    oldImapFolder->GetHierarchyDelimiter(&hierarchyDelimiter);
    PRInt32 boxflags=0;
    oldImapFolder->GetBoxFlags(&boxflags);

    nsAutoString newLeafName;
	nsAutoString newNameString;
    newNameString.AssignWithConversion(newName);
	newLeafName = newNameString;
	nsAutoString parentName; 
    nsAutoString folderNameStr;
    PRInt32 folderStart = newLeafName.RFindChar('/');  //internal use of hierarchyDelimiter is always '/'
    if (folderStart > 0)
    {
        newNameString.Right(newLeafName, newLeafName.Length() - folderStart - 1);
		CreateDirectoryForFolder(path);    //needed when we move a folder to a folder with no subfolders.
	}	

    // if we get here, it's really a leaf, and "this" is the parent.
    folderNameStr = newLeafName;
    
    // Create an empty database for this mail folder, set its name from the user  
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    nsCOMPtr<nsIMsgFolder> child;
    nsCOMPtr <nsIMsgImapMailFolder> imapFolder;

    nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID);
    if (msgDBService)
    {
      nsCOMPtr<nsIMsgDatabase> unusedDB;
      nsCOMPtr <nsIFileSpec> dbFileSpec;

      nsCAutoString proposedDBName;
      proposedDBName.AssignWithConversion(newLeafName);

      // warning, path will be changed
      rv = CreateFileSpecForDB(proposedDBName.get(), path, getter_AddRefs(dbFileSpec));
      NS_ENSURE_SUCCESS(rv,rv);

      // it's OK to use openMailDBFromFileSpec and not OpenFolderDB here, since we don't use the DB.
      rv = msgDBService->OpenMailDBFromFileSpec(dbFileSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) getter_AddRefs(unusedDB));

      if (NS_SUCCEEDED(rv) && unusedDB)
      {
        //need to set the folder name
        nsCOMPtr <nsIDBFolderInfo> folderInfo;
        rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));

        //Now let's create the actual new folder
        rv = AddSubfolderWithPath(folderNameStr, dbFileSpec, getter_AddRefs(child));
        if (!child || NS_FAILED(rv)) return rv;
        nsXPIDLString unicodeName;
        rv = CopyMUTF7toUTF16(proposedDBName, unicodeName);
        if (NS_SUCCEEDED(rv))
          child->SetName(unicodeName);
        imapFolder = do_QueryInterface(child);

        if (imapFolder)
        {
          nsCAutoString onlineName(m_onlineFolderName); 
          if (!onlineName.IsEmpty())
          onlineName.Append(char(hierarchyDelimiter));
          onlineName.AppendWithConversion(folderNameStr);
          imapFolder->SetVerifiedAsOnlineFolder(PR_TRUE);
          imapFolder->SetOnlineName(onlineName.get());
          imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
          imapFolder->SetBoxFlags(boxflags);

        // store the online name as the mailbox name in the db folder info
        // I don't think anyone uses the mailbox name, so we'll use it
        // to restore the online name when blowing away an imap db.
           if (folderInfo)
           {
             nsAutoString unicodeOnlineName; unicodeOnlineName.AssignWithConversion(onlineName.get());
             folderInfo->SetMailboxName(unicodeOnlineName);
           }
           PRBool changed = PR_FALSE;
           msgFolder->MatchOrChangeFilterDestination(child, PR_FALSE /*caseInsensitive*/, &changed);
           if (changed)
             msgFolder->AlertFilterChanged(msgWindow);
        }
        unusedDB->SetSummaryValid(PR_TRUE);
        unusedDB->Commit(nsMsgDBCommitType::kLargeCommit);
        unusedDB->Close(PR_TRUE);

        child->RenameSubFolders(msgWindow, msgFolder);
       
        nsCOMPtr<nsIMsgFolder> msgParent;
        msgFolder->GetParentMsgFolder(getter_AddRefs(msgParent));
        msgFolder->SetParent(nsnull);
        msgParent->PropagateDelete(msgFolder,PR_FALSE, nsnull);

        // Reset online status now that the folder is renamed.
        nsCOMPtr <nsIMsgImapMailFolder> oldImapFolder = do_QueryInterface(msgFolder);
        if (oldImapFolder)
          oldImapFolder->SetVerifiedAsOnlineFolder(PR_FALSE);

        NotifyItemAdded(child);
      }
    }     
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::RenameSubFolders(nsIMsgWindow *msgWindow, nsIMsgFolder *oldFolder)
{
  nsresult rv = NS_OK;
  m_initialized = PR_TRUE;
  nsCOMPtr<nsIEnumerator> aEnumerator;
  oldFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  nsCOMPtr<nsISupports> aSupport;
  rv = aEnumerator->First();
  while (NS_SUCCEEDED(rv))
  {
     rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
			
     nsCOMPtr<nsIMsgFolder>msgFolder = do_QueryInterface(aSupport);
     nsCOMPtr<nsIMsgImapMailFolder> folder = do_QueryInterface(msgFolder, &rv);
     if (NS_FAILED(rv)) return rv;

     PRUnichar hierarchyDelimiter = '/';
     folder->GetHierarchyDelimiter(&hierarchyDelimiter);
				
     PRInt32 boxflags;
     folder->GetBoxFlags(&boxflags);

     PRBool verified;
     folder->GetVerifiedAsOnlineFolder(&verified);
				
     nsCOMPtr<nsIFileSpec> oldPathSpec;
     rv = msgFolder->GetPath(getter_AddRefs(oldPathSpec));
     if (NS_FAILED(rv)) return rv;

     nsFileSpec oldPath;
     rv = oldPathSpec->GetFileSpec(&oldPath);
     if (NS_FAILED(rv)) return rv;

     nsCOMPtr<nsIFileSpec> newParentPathSpec;
     rv = GetPath(getter_AddRefs(newParentPathSpec));
     if (NS_FAILED(rv)) return rv;

     nsFileSpec newParentPath;
     rv = newParentPathSpec->GetFileSpec(&newParentPath);
     if (NS_FAILED(rv)) return rv;

     rv = AddDirectorySeparator(newParentPath);
     newParentPath += oldPath.GetLeafName();
     nsCString newPathStr(newParentPath.GetNativePathCString());
     nsCOMPtr<nsIFileSpec> newPathSpec;
     rv = NS_NewFileSpec(getter_AddRefs(newPathSpec));
     if (NS_FAILED(rv)) return rv;
     rv = newPathSpec->SetNativePath(newPathStr.get());
	 	  
     nsFileSpec newPath;
     rv = newPathSpec->GetFileSpec(&newPath);
     if (NS_FAILED(rv)) return rv;

     nsCOMPtr<nsIFileSpec> dbFileSpec;
     NS_NewFileSpecWithSpec(newPath, getter_AddRefs(dbFileSpec));

     nsCOMPtr<nsIMsgFolder> child;
				
     nsXPIDLString folderName;
     rv = msgFolder->GetName(getter_Copies(folderName));
     if (folderName.IsEmpty() || NS_FAILED(rv)) return rv;

     nsXPIDLCString utf7LeafName;
     rv = CopyUTF16toMUTF7(folderName, utf7LeafName);
     NS_ENSURE_SUCCESS(rv, rv);

     // XXX : Fix this non-sense by fixing AddSubfolderWithPath
     nsAutoString unicodeLeafName;
     CopyASCIItoUTF16(utf7LeafName, unicodeLeafName);

     rv = AddSubfolderWithPath(unicodeLeafName, dbFileSpec, getter_AddRefs(child));
     
     if (!child || NS_FAILED(rv)) return rv;

     child->SetName(folderName);
     nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(child);
     nsXPIDLCString onlineName;
     GetOnlineName(getter_Copies(onlineName));
     nsCAutoString onlineCName(onlineName);
     onlineCName.Append(char(hierarchyDelimiter));
     onlineCName.Append(utf7LeafName.get());
     if (imapFolder)
     {
       imapFolder->SetVerifiedAsOnlineFolder(verified);
       imapFolder->SetOnlineName(onlineCName.get());
       imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
       imapFolder->SetBoxFlags(boxflags);

       PRBool changed = PR_FALSE;
       msgFolder->MatchOrChangeFilterDestination(child, PR_FALSE /*caseInsensitive*/, &changed);
       if (changed)
         msgFolder->AlertFilterChanged(msgWindow);
	
       child->RenameSubFolders(msgWindow, msgFolder);
     }
     rv = aEnumerator->Next();

  }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::IsCommandEnabled(const char *command, PRBool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  NS_ENSURE_ARG_POINTER(command);

  *result = PR_TRUE;

  if(WeAreOffline() &&
     ((nsCRT::strcmp(command, "cmd_renameFolder") == 0) ||
      (nsCRT::strcmp(command, "cmd_compactFolder") == 0) ||
      (nsCRT::strcmp(command, "cmd_delete") == 0) ||
      (nsCRT::strcmp(command, "button_delete") == 0)))
     *result = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsImapMailFolder::GetCanFileMessages(PRBool *aCanFileMessages) 
{
  nsresult rv;
  *aCanFileMessages = PR_TRUE;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
    rv = server->GetCanFileMessagesOnServer(aCanFileMessages);

  if (*aCanFileMessages)
    rv = nsMsgDBFolder::GetCanFileMessages(aCanFileMessages);

  if (*aCanFileMessages)
  {
    PRBool noSelect;
    GetFlag(MSG_FOLDER_FLAG_IMAP_NOSELECT, &noSelect);
    *aCanFileMessages = (noSelect) ? PR_FALSE : GetFolderACL()->GetCanIInsertInFolder();
    return NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::GetCanDeleteMessages(PRBool *aCanDeleteMessages)
{
  NS_ENSURE_ARG_POINTER(aCanDeleteMessages);
  *aCanDeleteMessages = GetFolderACL()->GetCanIDeleteInFolder();
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::GetPerformingBiff(PRBool *aPerformingBiff)
{
  if (!aPerformingBiff)
    return NS_ERROR_NULL_POINTER;
  
  *aPerformingBiff = m_performingBiff;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetPerformingBiff(PRBool aPerformingBiff)
{
  m_performingBiff = aPerformingBiff;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::SetFilterList(nsIMsgFilterList *aMsgFilterList)
{
  m_filterList = aMsgFilterList;
  return nsMsgDBFolder::SetFilterList(aMsgFilterList);
}

nsresult nsImapMailFolder::GetMoveCoalescer()
{
   if (!m_moveCoalescer)
   {
      m_moveCoalescer = new nsImapMoveCoalescer(this, nsnull /* msgWindow */);
      NS_ENSURE_TRUE (m_moveCoalescer, NS_ERROR_OUT_OF_MEMORY);
      m_moveCoalescer->AddRef();
   }
   return NS_OK;
}

nsresult
nsImapMailFolder::SpamFilterClassifyMessage(const char *aURI, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  ++m_numFilterClassifyRequests;
  return aJunkMailPlugin->ClassifyMessage(aURI, aMsgWindow, this);   
}


nsresult
nsImapMailFolder::SpamFilterClassifyMessages(const char **aURIArray, PRUint32 aURICount, nsIMsgWindow *aMsgWindow, nsIJunkMailPlugin *aJunkMailPlugin)
{
  m_numFilterClassifyRequests += aURICount;
  return aJunkMailPlugin->ClassifyMessages(aURICount, aURIArray, aMsgWindow, this);   
}

NS_IMETHODIMP
nsImapMailFolder::StoreCustomKeywords(nsIMsgWindow *aMsgWindow, const char *aFlagsToAdd, 
                                      const char *aFlagsToSubtract, nsMsgKey *aKeysToStore, PRUint32 aNumKeys, nsIURI **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIImapService> imapService(do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv)); 
  NS_ENSURE_SUCCESS(rv, rv); 
  nsCAutoString msgIds;
  AllocateUidStringFromKeys(aKeysToStore, aNumKeys, msgIds);
  return imapService->StoreCustomKeywords(m_eventQueue, this, aMsgWindow, aFlagsToAdd, aFlagsToSubtract, msgIds.get(), _retval);
}

nsresult
nsImapMailFolder::PlaybackCoalescedOperations()
{
  if (m_moveCoalescer)
  {
    nsMsgKeyArray *junkKeysToClassify = m_moveCoalescer->GetKeyBucket(0);
    nsMsgKeyArray *nonJunkKeysToClassify = m_moveCoalescer->GetKeyBucket(1);

    if (junkKeysToClassify && junkKeysToClassify->GetSize() > 0)
      StoreCustomKeywords(m_moveCoalescer->GetMsgWindow(), "Junk", "", junkKeysToClassify->GetArray(), junkKeysToClassify->GetSize(), nsnull);
    if (nonJunkKeysToClassify && nonJunkKeysToClassify->GetSize() > 0)
      StoreCustomKeywords(m_moveCoalescer->GetMsgWindow(), "NonJunk", "", nonJunkKeysToClassify->GetArray(), nonJunkKeysToClassify->GetSize(), nsnull);
    junkKeysToClassify->RemoveAll();
    nonJunkKeysToClassify->RemoveAll();
    return m_moveCoalescer->PlaybackMoves();
  }
  return NS_OK; // must not be any coalesced operations
}

NS_IMETHODIMP
nsImapMailFolder::OnMessageClassified(const char *aMsgURI, nsMsgJunkStatus aClassification)
{
  nsXPIDLCString spamFolderURI;
  nsCOMPtr <nsIMsgIncomingServer> server;
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

  GetMoveCoalescer();
  if (m_moveCoalescer)
  {
    nsMsgKeyArray *keysToClassify = m_moveCoalescer->GetKeyBucket((aClassification == nsIJunkMailPlugin::JUNK) ? 0 : 1);
    NS_ASSERTION(keysToClassify, "error getting key bucket");
    if (keysToClassify)
      keysToClassify->Add(msgKey);
  }
  if (aClassification == nsIJunkMailPlugin::JUNK)
  {
    nsCOMPtr<nsISpamSettings> spamSettings;
    rv = GetServer(getter_AddRefs(server));
    NS_ENSURE_SUCCESS(rv, rv); 
    rv = server->GetSpamSettings(getter_AddRefs(spamSettings));
    NS_ENSURE_SUCCESS(rv, rv); 
    
    PRBool markAsReadOnSpam;
    (void)spamSettings->GetMarkAsReadOnSpam(&markAsReadOnSpam);
    if (markAsReadOnSpam)
    {
      if (!m_junkMessagesToMarkAsRead)
        NS_NewISupportsArray(getter_AddRefs(m_junkMessagesToMarkAsRead));
      m_junkMessagesToMarkAsRead->AppendElement(msgHdr);
    }
 
    PRBool willMoveMessage = PR_FALSE;
  
    // don't do the move when we are opening up 
    // the junk mail folder or the trash folder
    // or when manually classifying messages in those folders
    if (!(mFlags & MSG_FOLDER_FLAG_JUNK || mFlags & MSG_FOLDER_FLAG_TRASH))
    {
      PRBool moveOnSpam;
      (void)spamSettings->GetMoveOnSpam(&moveOnSpam);
      if (moveOnSpam)
      {
        rv = spamSettings->GetSpamFolderURI(getter_Copies(spamFolderURI));
        NS_ENSURE_SUCCESS(rv,rv);

        if (!spamFolderURI.IsEmpty())
        {          
          nsCOMPtr<nsIMsgFolder> folder;
          rv = GetExistingFolder(spamFolderURI, getter_AddRefs(folder));
          if (NS_SUCCEEDED(rv) && folder) 
          {
            rv = folder->SetFlag(MSG_FOLDER_FLAG_JUNK);
            NS_ENSURE_SUCCESS(rv,rv);
            if (NS_SUCCEEDED(GetMoveCoalescer())) 
            {
              m_moveCoalescer->AddMove(folder, msgKey);
              willMoveMessage = PR_TRUE;
            }
          }
          else 
          {
            // XXX TODO
            // JUNK MAIL RELATED
            // the listener should do
            // rv = folder->SetFlag(MSG_FOLDER_FLAG_JUNK);
            // NS_ENSURE_SUCCESS(rv,rv);
            // if (NS_SUCCEEDED(GetMoveCoalescer())) {
            //   m_moveCoalescer->AddMove(folder, msgKey);
            //   willMoveMessage = PR_TRUE;
            // }
            rv = GetOrCreateFolder(spamFolderURI, nsnull /* aListener */);
            NS_ASSERTION(NS_SUCCEEDED(rv), "GetOrCreateFolder failed");
          }
        }
      }
    }
    rv = spamSettings->LogJunkHit(msgHdr, willMoveMessage);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  if (--m_numFilterClassifyRequests == 0)
  {
    if (m_junkMessagesToMarkAsRead)
    {
      PRUint32 count;
      m_junkMessagesToMarkAsRead->Count(&count);
      if (count > 0)
      {
        rv = MarkMessagesRead(m_junkMessagesToMarkAsRead, true);
        NS_ENSURE_SUCCESS(rv,rv);
        m_junkMessagesToMarkAsRead->SizeTo(0);
      }
    }
    PlaybackCoalescedOperations();
    // If we are performing biff for this folder, tell the server object
    if (m_performingBiff)
    {
      // we don't need to adjust the num new messages in this folder because
      // the playback moves code already did that.
      (void) PerformBiffNotifications();
      nsCOMPtr<nsIMsgIncomingServer> server;
      if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
        server->SetPerformingBiff(PR_FALSE);
      m_performingBiff = PR_FALSE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsImapMailFolder::GetShouldDownloadAllHeaders(PRBool *aResult)
{
  *aResult = PR_FALSE;
  //for just the inbox, we check if the filter list has arbitary headers.
  //for all folders, check if we have a spam plugin that requires all headers
  if (mFlags & MSG_FOLDER_FLAG_INBOX) 
  {
    nsCOMPtr <nsIMsgFilterList> filterList;  
    nsresult rv = GetFilterList(nsnull, getter_AddRefs(filterList));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = filterList->GetShouldDownloadAllHeaders(aResult);
    if (*aResult)
      return rv;
  }
  nsCOMPtr <nsIMsgFilterPlugin> filterPlugin;
  nsCOMPtr<nsIMsgIncomingServer> server;

  if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))))
    server->GetSpamFilterPlugin(getter_AddRefs(filterPlugin));

  return (filterPlugin) ? filterPlugin->GetShouldDownloadAllHeaders(aResult) : NS_OK;
}


void nsImapMailFolder::GetTrashFolderName(nsAString &aFolderName)
{
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsIImapIncomingServer> imapServer;
    
  if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
    imapServer = do_QueryInterface(server);
    
  if (imapServer)
  {
    nsXPIDLString trashFolderName;
    if (NS_SUCCEEDED(imapServer->GetTrashFolderName(getter_Copies(trashFolderName))))
    {
      aFolderName = trashFolderName;
    }
  }
}

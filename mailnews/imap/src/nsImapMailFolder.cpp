/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsMsgImapCID.h"
#include "nsIMessage.h"
#include "nsImapMailFolder.h"
#include "nsIEnumerator.h"
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
#include "nsImapMessage.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsImapUndoTxn.h"
#include "nsIIMAPHostSessionList.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"
#include "nsImapStringBundle.h"
#include "nsIMsgFolderCacheElement.h"
#include "nsIMsgStatusFeedback.h"
#include "nsTextFormatter.h"

#include "nsIMsgFilter.h"
#include "nsIMsgFilterService.h"
#include "nsImapMoveCoalescer.h"
#include "nsIPrompt.h"
#include "nsINetSupportDialogService.h"
#include "nsSpecialSystemDirectory.h"
#include "nsXPIDLString.h"
#include "nsIImapFlagAndUidState.h"
#include "nsIMessenger.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kCImapDB, NS_IMAPDB_CID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kParseMailMsgStateCID, NS_PARSEMAILMSGSTATE_CID);
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,    NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);

#define FOUR_K 4096

nsImapMailFolder::nsImapMailFolder() :
    m_initialized(PR_FALSE),m_haveDiscoveredAllFolders(PR_FALSE),
    m_haveReadNameFromDB(PR_FALSE), 
    m_curMsgUid(0), m_nextMessageByteLength(0),
    m_urlRunning(PR_FALSE),
  m_verifiedAsOnlineFolder(PR_FALSE),
  m_explicitlyVerify(PR_FALSE),
    m_folderNeedsSubscribing(PR_FALSE),
    m_folderNeedsAdded(PR_FALSE),
    m_folderNeedsACLListed(PR_TRUE)
{
    m_appendMsgMonitor = nsnull;  // since we're not using this (yet?) make it null.
                // if we do start using it, it should be created lazily

  nsresult rv;

    // Get current thread envent queue

  NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pEventQService)
        pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                            getter_AddRefs(m_eventQueue));
  m_moveCoalescer = nsnull;
  m_boxFlags = 0;
  m_hierarchyDelimiter = kOnlineHierarchySeparatorUnknown;
  m_pathName = nsnull;
}

nsImapMailFolder::~nsImapMailFolder()
{
    if (m_appendMsgMonitor)
        PR_DestroyMonitor(m_appendMsgMonitor);

  if (m_moveCoalescer)
    delete m_moveCoalescer;
}

NS_IMPL_ADDREF_INHERITED(nsImapMailFolder, nsMsgDBFolder)
NS_IMPL_RELEASE_INHERITED(nsImapMailFolder, nsMsgDBFolder)

NS_IMETHODIMP nsImapMailFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  *aInstancePtr = nsnull;

    if (aIID.Equals(NS_GET_IID(nsIMsgImapMailFolder)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIMsgImapMailFolder*, this);
  }              
  else if(aIID.Equals(NS_GET_IID(nsICopyMessageListener)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsICopyMessageListener*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIImapMailFolderSink)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIImapMailFolderSink*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIImapMessageSink)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIImapMessageSink*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIImapExtensionSink)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIImapExtensionSink*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIImapMiscellaneousSink)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIImapMiscellaneousSink*, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIUrlListener)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIUrlListener *, this);
  }
  else if (aIID.Equals(NS_GET_IID(nsIMsgFilterHitNotify)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsIMsgFilterHitNotify *, this);
  }
  if(*aInstancePtr)
  {
    AddRef();
    return NS_OK;
  }

  return nsMsgDBFolder::QueryInterface(aIID, aInstancePtr);
}



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
  if (nsCRT::strcmp(mURI, kImapRootURI) == 0) 
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
      nsAutoString str((nsFilePath)path);
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

NS_IMETHODIMP nsImapMailFolder::AddSubfolderWithPath(nsAutoString *name, nsIFileSpec *dbPath, 
                                             nsIMsgFolder **child)
{
  if(!child)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 

  if(NS_FAILED(rv))
    return rv;

    PRInt32 flags = 0;
  nsAutoString uri;
  uri.Append(mURI);
  uri.Append('/');

  uri.Append(*name);
  char* uriStr = uri.ToNewCString();
  if (uriStr == nsnull) 
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIRDFResource> res;
  rv = rdf->GetResource(uriStr, getter_AddRefs(res));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;        

  folder->SetPath(dbPath);
    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(folder);

    folder->GetFlags((PRUint32 *)&flags);

    folder->SetParent(this);
  nsAllocator::Free(uriStr);

  flags |= MSG_FOLDER_FLAG_MAIL;

  PRBool isServer;
    rv = GetIsServer(&isServer);

  //Only set these is these are top level children.
  if(NS_SUCCEEDED(rv) && isServer)
  {

    if(name->Compare("Inbox", PR_TRUE) == 0)
      flags |= MSG_FOLDER_FLAG_INBOX;
    else if(name->Compare("Trash", PR_TRUE) == 0)
      flags |= MSG_FOLDER_FLAG_TRASH;
#if 0
    else if(name->Compare("Sent", PR_TRUE) == 0)
      folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
    else if(name->Compare("Drafts", PR_TRUE) == 0)
      folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
    else if (name->Compare("Templates", PR_TRUE) == 0)
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

  char *folderName;
  for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++) 
  {
    nsFileSpec currentFolderPath = (nsFileSpec&)dir;
    folderName = currentFolderPath.GetLeafName();
    currentFolderNameStr = folderName;
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
    currentFolderPath.SetLeafName(currentFolderNameStr);
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

        rv = cacheElement->GetStringProperty("onlineName", getter_Copies(onlineFullUtf7Name));
        if (NS_SUCCEEDED(rv) && (const char *) onlineFullUtf7Name && nsCRT::strlen((const char *) onlineFullUtf7Name))
        {
          if (imapServer)

            imapServer->CreatePRUnicharStringFromUTF7(onlineFullUtf7Name, getter_Copies(unicodeName));


          // take the full unicode folder name and find the unicode leaf name.
          currentFolderNameStr = unicodeName;
          PRInt32 leafPos = currentFolderNameStr.RFindChar('/');
          if (leafPos > 0)
            currentFolderNameStr.Cut(0, leafPos + 1);

          // take the utf7 full online name, and determine the utf7 leaf name
          utf7LeafName = onlineFullUtf7Name;
          leafPos = utf7LeafName.RFindChar('/');
          if (leafPos > 0)
            utf7LeafName.Cut(0, leafPos + 1);
        }
      }
    }
      // make the imap folder remember the file spec it was created with.
    nsCAutoString leafName (currentFolderDBNameStr);
    nsCOMPtr <nsIFileSpec> msfFileSpec;
    rv = NS_NewFileSpecWithSpec(currentFolderPath, getter_AddRefs(msfFileSpec));
    if (NS_SUCCEEDED(rv) && msfFileSpec)
    {
      // leaf name is the db name w/o .msf (nsShouldIgnoreFile strips it off)
      // so this trims the .msf off the file spec.
      msfFileSpec->SetLeafName(leafName);
    }
    // use the utf7 name as the uri for the folder.
    AddSubfolderWithPath(&utf7LeafName, msfFileSpec, getter_AddRefs(child));
    if (child)
    {
      // use the unicode name as the "pretty" name. Set it so it won't be
      // automatically computed from the URI, which is in utf7 form.
      if (currentFolderNameStr.Length() > 0)
        child->SetName(currentFolderNameStr.GetUnicode());

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
        
        // we have to treat the root folder specially, because it's name
        // doesn't end with .sbd

        PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
        if (path.IsDirectory()) {
            newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
            SetFlag(newFlags);
            rv = CreateSubFolders(path);
        }
    PRUint32 count;
    if (isServer && NS_SUCCEEDED(mSubFolders->Count(&count)) && count == 0)
    {
      // create an inbox...
      CreateClientSubfolderInfo("INBOX", kOnlineHierarchySeparatorUnknown);
    }
        UpdateSummaryTotals(PR_FALSE);

        if (NS_FAILED(rv)) return rv;
        m_initialized = PR_TRUE;      // XXX do this on failure too?
    }
    rv = mSubFolders->Enumerate(result);
    if (isServer)
        // *** Set identity pref default folder flags
        SetPrefFlag();
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::AddUnique(nsISupports* element)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::ReplaceElement(nsISupports* element,
                                               nsISupports* newElement)
{
    nsresult rv = NS_ERROR_FAILURE;
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

    nsCOMPtr<nsIMsgDatabase> mailDBFactory;

    rv = nsComponentManager::CreateInstance(kCImapDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDBFactory));
    if (NS_SUCCEEDED(rv) && mailDBFactory)
      folderOpen = mailDBFactory->Open(pathSpec, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase));

    if(mDatabase)
    {
      if(mAddListener)
        mDatabase->AddListener(this);

      // if we have to regenerate the folder, run the parser url.
      if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
      {
      }
      else
      {
        //Otherwise we have a valid database so lets extract necessary info.
        UpdateSummaryTotals(PR_TRUE);
      }
    }
    else
      folderOpen = rv;
  }
  return folderOpen;
}

NS_IMETHODIMP
nsImapMailFolder::UpdateFolder(nsIMsgWindow *msgWindow)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
  PRBool selectFolder = PR_FALSE;

  NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv); 

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
                rv = CreateClientSubfolderInfo("Inbox", kOnlineHierarchySeparatorUnknown);
                if (NS_FAILED(rv)) 
                    return rv;
            }
            m_haveDiscoveredAllFolders = PR_TRUE;
        }
        selectFolder = PR_FALSE;
    }
    rv = GetDatabase(msgWindow);

  // don't run select if we're already running a url/select...
  if (NS_SUCCEEDED(rv) && !m_urlRunning && selectFolder)
  {
    nsCOMPtr <nsIEventQueue> eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pEventQService)
      pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                        getter_AddRefs(eventQ));
    rv = imapService->SelectFolder(eventQ, this, this, msgWindow, nsnull);
    m_urlRunning = PR_TRUE;
  }
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
  if (result)
    *result = nsnull;
  nsresult rv;
  nsCOMPtr<nsISimpleEnumerator> msgHdrEnumerator;
  nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
  rv = NS_ERROR_UNEXPECTED;
  if (mDatabase)
      rv = mDatabase->EnumerateMessages(getter_AddRefs(msgHdrEnumerator));
  if(NS_SUCCEEDED(rv))
      rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator,
                                             this,
                                             &messageEnumerator);
  *result = messageEnumerator;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::CreateSubfolder(const PRUnichar* folderName)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!folderName) return rv;
    NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
    if (NS_SUCCEEDED(rv))
        rv = imapService->CreateFolder(m_eventQueue, this, 
                                       folderName, this, nsnull);
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::CreateClientSubfolderInfo(const char *folderName, PRUnichar hierarchyDelimiter)
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

    nsAutoString leafName (folderName);
    nsAutoString folderNameStr;
    nsAutoString parentName = leafName;
    PRInt32 folderStart = leafName.FindChar('/');
    if (folderStart > 0)
    {
        NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
        nsCOMPtr<nsIRDFResource> res;
        nsCOMPtr<nsIMsgImapMailFolder> parentFolder;
        nsCAutoString uri (mURI);
        parentName.Right(leafName, leafName.Length() - folderStart - 1);
        parentName.Truncate(folderStart);
        path += parentName;
        rv = CreateDirectoryForFolder(path);
        if (NS_FAILED(rv)) return rv;
        uri.Append('/');
        uri.Append(parentName);
        rv = rdf->GetResource(uri,
                              getter_AddRefs(res));
        if (NS_FAILED(rv)) return rv;
        parentFolder = do_QueryInterface(res, &rv);
        if (NS_FAILED(rv)) return rv;
      return parentFolder->CreateClientSubfolderInfo(nsCAutoString(leafName), hierarchyDelimiter);
    }
    
  // if we get here, it's really a leaf, and "this" is the parent.
    folderNameStr = leafName;
    
//    path += folderNameStr;

  // Create an empty database for this mail folder, set its name from the user  
  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    nsCOMPtr<nsIMsgFolder> child;

  rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDBFactory));
  if (NS_SUCCEEDED(rv) && mailDBFactory)
  {
        nsCOMPtr<nsIMsgDatabase> unusedDB;
    nsCOMPtr <nsIFileSpec> dbFileSpec;

    nsXPIDLCString uniqueLeafName;

    rv = CreatePlatformLeafNameForDisk(folderName, path, getter_Copies(uniqueLeafName));
    path.SetLeafName(uniqueLeafName);

    NS_NewFileSpecWithSpec(path, getter_AddRefs(dbFileSpec));
    rv = mailDBFactory->Open(dbFileSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) getter_AddRefs(unusedDB));

        if (NS_SUCCEEDED(rv) && unusedDB)
        {
      //need to set the folder name
      nsCOMPtr <nsIDBFolderInfo> folderInfo;
      rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
      if(NS_SUCCEEDED(rv))
      {
        // ### DMB used to be leafNameFromUser?
        folderInfo->SetMailboxName(&folderNameStr);
      }

      //Now let's create the actual new folder
      rv = AddSubfolderWithPath(&folderNameStr, dbFileSpec, getter_AddRefs(child));
//      if (NS_SUCCEEDED(rv) && child)
//        child->SetPath(dbFileSpec);

      nsCOMPtr <nsIMsgImapMailFolder> imapFolder = do_QueryInterface(child);
      if (imapFolder)
      {
        nsCAutoString onlineName = m_onlineFolderName; 
        if (onlineName.Length() > 0)
          onlineName += hierarchyDelimiter;
        onlineName += folderNameStr;
        imapFolder->SetVerifiedAsOnlineFolder(PR_TRUE);
        imapFolder->SetOnlineName(onlineName.GetBuffer());
                imapFolder->SetHierarchyDelimiter(hierarchyDelimiter);
      }

            unusedDB->SetSummaryValid(PR_TRUE);
      unusedDB->Commit(nsMsgDBCommitType::kLargeCommit);
            unusedDB->Close(PR_TRUE);
        }
  }
  if(NS_SUCCEEDED(rv) && child)
  {
    nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
    nsCOMPtr<nsISupports> folderSupports;
    rv = QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(folderSupports));
    if(childSupports && NS_SUCCEEDED(rv))
    {

      NotifyItemAdded(folderSupports, childSupports, "folderView");
    }
  }
  return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::List()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  rv = imapService->ListFolder(m_eventQueue, this, nsnull, nsnull);

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
    return nsMsgFolder::DeleteSubFolders(folders);
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
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::GetOnlineDelimiter(char** onlineDelimiter)
{
    if (onlineDelimiter)
    {
        nsresult rv;
        PRUnichar delimiter = 0;
        rv = GetHierarchyDelimiter(&delimiter);
        nsAutoString aString = delimiter;
        *onlineDelimiter = aString.ToNewCString();
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
  *aHierarchyDelimiter = m_hierarchyDelimiter;
  return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetBoxFlags(PRInt32 aBoxFlags)
{
  m_boxFlags = aBoxFlags;
  PRUint32 newFlags = mFlags;

    newFlags |= MSG_FOLDER_FLAG_IMAPBOX;

  if (m_boxFlags & kNoinferiors)
    newFlags |= MSG_FOLDER_FLAG_IMAP_NOINFERIORS;
  else
    newFlags &= ~MSG_FOLDER_FLAG_IMAP_NOINFERIORS;
    if (m_boxFlags & kImapTrash)
        newFlags |= MSG_FOLDER_FLAG_TRASH;
    else
        newFlags &= ~MSG_FOLDER_FLAG_TRASH;
    if (m_boxFlags & kImapSent)
        newFlags |= MSG_FOLDER_FLAG_SENTMAIL;
    else
        newFlags &= ~MSG_FOLDER_FLAG_SENTMAIL;
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

NS_IMETHODIMP nsImapMailFolder::Compact()
{
    nsresult rv;
    NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
    if (NS_SUCCEEDED(rv) && imapService)
    {
        rv = imapService->Expunge(m_eventQueue, this, nsnull, nsnull);
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::EmptyTrash(nsIMsgWindow *msgWindow)
{
    nsresult rv;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    rv = GetTrashFolder(getter_AddRefs(trashFolder));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIMsgDatabase> trashDB;

        rv = trashFolder->Delete(); // delete summary spec
        rv = trashFolder->GetMsgDatabase(msgWindow, getter_AddRefs(trashDB));

        nsCOMPtr<nsIUrlListener> urlListener =
            do_QueryInterface(trashFolder);

        NS_WITH_SERVICE (nsIImapService, imapService, kCImapService, &rv);
        if (NS_SUCCEEDED(rv))
            rv = imapService->DeleteAllMessages(m_eventQueue, trashFolder,
                                                urlListener, nsnull);
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
            rv = aEnumerator->First();
            while(NS_SUCCEEDED(rv))
            {
                rv = aEnumerator->CurrentItem(getter_AddRefs(aSupport));
                if (NS_SUCCEEDED(rv))
                {
                    aSupportsArray->AppendElement(aSupport);
                    rv = aEnumerator->Next();
                }
            }
            PRUint32 cnt = 0;
            aSupportsArray->Count(&cnt);
            for (PRInt32 i = cnt-1; i >= 0; i--)
            {
                aSupport = aSupportsArray->ElementAt(i);
                aSupportsArray->RemoveElementAt(i);
                aFolder = do_QueryInterface(aSupport);
                if (aFolder)
                    trashFolder->PropagateDelete(aFolder, PR_TRUE);
            }
        }
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Delete ()
{
    nsresult rv = NS_ERROR_FAILURE;
    if (mDatabase)
    {
        mDatabase->ForceClosed();
        mDatabase = null_nsCOMPtr();
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
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::Rename (const PRUnichar *newName)
{
    nsresult rv = NS_ERROR_FAILURE;
    NS_WITH_SERVICE (nsIImapService, imapService, kCImapService, &rv);
    if (NS_SUCCEEDED(rv))
        rv = imapService->RenameLeaf(m_eventQueue, this, newName, this,
                                     nsnull);
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::PrepareToRename()
{
    PRUint32 cnt = 0, i;
    if (mSubFolders)
    {
        nsCOMPtr<nsISupports> aSupport;
        nsCOMPtr<nsIMsgImapMailFolder> folder;
        mSubFolders->Count(&cnt);
        if (cnt > 0)
        {
            for (i = 0; i < cnt; i++)
            {
                aSupport = getter_AddRefs(mSubFolders->ElementAt(i));
                folder = do_QueryInterface(aSupport);
                if (folder)
                    folder->PrepareToRename();
            }
        }
    }
    SetOnlineName("");
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::RenameLocal(const char *newName)
{
    nsCAutoString leafname = newName;
    // newName always in the canonical form "greatparent/parentname/leafname"
    PRInt32 leafpos = leafname.RFindChar('/');
    if (leafpos >0)
        leafname.Cut(0, leafpos+1);

    m_msgParser = null_nsCOMPtr();
    PrepareToRename();
    ForceDBClosed();

    nsresult rv = NS_OK;
    nsCOMPtr<nsIFileSpec> oldPathSpec;
    rv = GetPath(getter_AddRefs(oldPathSpec));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFolder> parent;
    rv = GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIMsgFolder> parentFolder = do_QueryInterface(parent);
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
    nsCAutoString newNameStr = leafname;
    newNameStr += ".msf";
    rv = oldSummarySpec.Rename(newNameStr.GetBuffer());
    if (NS_SUCCEEDED(rv))
    {
        if (parentFolder)
        {
            SetParent(nsnull);
            parentFolder->PropagateDelete(this, PR_FALSE);
        }
        if (cnt > 0)
        {
            newNameStr = leafname;
            newNameStr += ".sbd";
            dirSpec.Rename(newNameStr.GetBuffer());
        }
    }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetPrettyName(PRUnichar ** prettyName)
{
  return GetName(prettyName);
}
    
NS_IMETHODIMP nsImapMailFolder::GetFolderURL(char **url)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::UpdateSummaryTotals(PRBool force) 
{
  // could we move this into nsMsgDBFolder, or do we need to deal
  // with the pending imap counts?
  nsresult rv = NS_OK;

  PRInt32 oldUnreadMessages = mNumUnreadMessages + mNumPendingUnreadMessages;
  PRInt32 oldTotalMessages = mNumTotalMessages + mNumPendingTotalMessages;
  //We need to read this info from the database
  ReadDBFolderInfo(force);

  // If we asked, but didn't get any, stop asking
  if (mNumUnreadMessages == -1)
    mNumUnreadMessages = -2;

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

    return rv;
}
    
NS_IMETHODIMP nsImapMailFolder::GetExpungedBytesCount(PRUint32 *count)
{
    nsresult rv = NS_ERROR_FAILURE;
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
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::GetCanCreateSubfolders(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = !(mFlags & MSG_FOLDER_FLAG_IMAP_NOINFERIORS);
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

NS_IMETHODIMP nsImapMailFolder::UserNeedsToAuthenticateForFolder(PRBool
                                                                 displayOnly,
                                                                 PRBool
                                                                 *authenticate)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::RememberPassword(const char *password)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::GetRememberedPassword(char ** password)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}


NS_IMETHODIMP
nsImapMailFolder::MarkMessagesRead(nsISupportsArray *messages, PRBool markRead)
{
  nsresult rv;

  // tell the folder to do it, which will mark them read in the db.
  rv = nsMsgFolder::MarkMessagesRead(messages, markRead);
  if (NS_SUCCEEDED(rv))
  {
    nsCString messageIds;
        nsMsgKeyArray keysToMarkRead;
    rv = BuildIdsAndKeyArray(messages, messageIds, keysToMarkRead);
    if (NS_FAILED(rv)) return rv;

    rv = StoreImapFlags(kImapMsgSeenFlag, markRead,  keysToMarkRead);
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}

NS_IMETHODIMP
nsImapMailFolder::MarkAllMessagesRead(void)
{
  nsresult rv = GetDatabase(nsnull);
  
  if(NS_SUCCEEDED(rv))
  {
    nsMsgKeyArray thoseMarked;
    rv = mDatabase->MarkAllRead(&thoseMarked);
    if (NS_SUCCEEDED(rv))
    {
      rv = StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, thoseMarked);
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
  if (NS_SUCCEEDED(element->GetInt32Property("hierDelim", &hierarchyDelimiter)))
    m_hierarchyDelimiter = (PRUnichar) hierarchyDelimiter;
  rv = element->GetStringProperty("onlineName", getter_Copies(onlineName));
  if (NS_SUCCEEDED(rv) && (const char *) onlineName && nsCRT::strlen((const char *) onlineName))
    m_onlineFolderName = onlineName;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::WriteToFolderCacheElem(nsIMsgFolderCacheElement *element)
{
  nsresult rv = nsMsgDBFolder::WriteToFolderCacheElem(element);
  element->SetInt32Property("boxFlags", m_boxFlags);
  element->SetInt32Property("hierDelim", (PRInt32) m_hierarchyDelimiter);
  element->SetStringProperty("onlineName", m_onlineFolderName.GetBuffer());
  return rv;
}



NS_IMETHODIMP
nsImapMailFolder::MarkMessagesFlagged(nsISupportsArray *messages, PRBool markFlagged)
{
  nsresult rv;

  // tell the folder to do it, which will mark them read in the db.
  rv = nsMsgFolder::MarkMessagesFlagged(messages, markFlagged);
  if (NS_SUCCEEDED(rv))
  {
    nsCString messageIds;
        nsMsgKeyArray keysToMarkFlagged;
    rv = BuildIdsAndKeyArray(messages, messageIds, keysToMarkFlagged);
    if (NS_FAILED(rv)) return rv;

    rv = StoreImapFlags(kImapMsgFlaggedFlag, markFlagged,  keysToMarkFlagged);
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::Adopt(nsIMsgFolder *srcFolder, 
                                      PRUint32 *outPos)
{
    nsresult rv = NS_ERROR_FAILURE;
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::SetOnlineName(const char * aOnlineFolderName)
{
  nsresult rv;

  nsCOMPtr<nsIMsgDatabase> db; 
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  m_onlineFolderName = aOnlineFolderName;
  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if(NS_SUCCEEDED(rv))
  {
    nsAutoString onlineName(aOnlineFolderName);
    rv = folderInfo->SetProperty("onlineName", &onlineName);
    // so, when are we going to commit this? Definitely not every time!
    // We could check if the online name has changed.
    db->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  folderInfo = null_nsCOMPtr();
  return rv;
}


NS_IMETHODIMP nsImapMailFolder::GetOnlineName(char ** aOnlineFolderName)
{
  if (!aOnlineFolderName)
    return NS_ERROR_NULL_POINTER;
  ReadDBFolderInfo(PR_FALSE); // update cache first.
  *aOnlineFolderName = m_onlineFolderName.ToNewCString();
  return (*aOnlineFolderName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;

  // ### do we want to read from folder cache first, or has that been done?
}


NS_IMETHODIMP
nsImapMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult openErr=NS_ERROR_UNEXPECTED;
    if(!db || !folderInfo)
    return NS_ERROR_NULL_POINTER; //ducarroz: should we use NS_ERROR_INVALID_ARG?

  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
  nsCOMPtr<nsIMsgDatabase> mailDB;

  nsresult rv = nsComponentManager::CreateInstance(kCImapDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
  if (NS_SUCCEEDED(rv) && mailDBFactory)
  {

    nsCOMPtr<nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_FAILED(rv)) return rv;

    openErr = mailDBFactory->Open(pathSpec, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) &mailDB);
  }

    *db = mailDB;
  NS_IF_ADDREF(*db);
    if (NS_SUCCEEDED(openErr)&& *db)
  {
        openErr = (*db)->GetDBFolderInfo(folderInfo);
    if (NS_SUCCEEDED(openErr) && folderInfo)
    {
      nsXPIDLCString onlineName;
      if (NS_SUCCEEDED((*folderInfo)->GetCharPtrProperty("onlineName", getter_Copies(onlineName))))
      {
        if ((const char*) onlineName && nsCRT::strlen((const char *) onlineName) > 0)
          m_onlineFolderName = onlineName;
        else
        {
          char *uri = nsnull;
          rv = GetURI(&uri);
          if (NS_FAILED(rv)) return rv;
          char * hostname = nsnull;
          rv = GetHostname(&hostname);
          if (NS_FAILED(rv)) return rv;
          nsXPIDLCString name;
          rv = nsImapURI2FullName(kImapRootURI, hostname, uri, getter_Copies(name));
          m_onlineFolderName = name;
          nsAutoString autoOnlineName(name);
          rv = (*folderInfo)->SetProperty("onlineName", &autoOnlineName);
          PR_FREEIF(uri);
          PR_FREEIF(hostname);
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
    nsCOMPtr<nsISupports> msgSupports;
    nsCOMPtr<nsIMessage> message;

    if (!messages) return rv;

    rv = messages->Count(&count);
    if (NS_FAILED(rv)) return rv;

    // build up message keys.
    for (i = 0; i < count; i++)
    {
        msgSupports = getter_AddRefs(messages->ElementAt(i));
        message = do_QueryInterface(msgSupports);
        if (message)
        {
            nsMsgKey key;
            rv = message->GetMessageKey(&key);
            if (NS_SUCCEEDED(rv))
                keyArray.Add(key);
        }
    }
    
  return AllocateUidStringFromKeyArray(keyArray, msgIds);
}

/* static */ nsresult
nsImapMailFolder::AllocateUidStringFromKeyArray(nsMsgKeyArray &keyArray, nsCString &msgIds)
{
    nsresult rv = NS_OK;
  PRInt32 startSequence = -1;
    if (keyArray.GetSize() > 0)
        startSequence = keyArray[0];
  PRInt32 curSequenceEnd = startSequence;
  PRUint32 total = keyArray.GetSize();
  // sort keys and then generate ranges instead of singletons!
  keyArray.QuickSort();
    for (PRUint32 keyIndex = 0; keyIndex < total; keyIndex++)
  {
    PRUint32 curKey = keyArray[keyIndex];
    PRUint32 nextKey = (keyIndex + 1 < total) ? keyArray[keyIndex + 1] : 0xFFFFFFFF;
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
      msgIds.Append(startSequence, 10);
      msgIds += ':';
      msgIds.Append(curSequenceEnd, 10);
      if (!lastKey)
        msgIds += ',';
      startSequence = nextKey;
      curSequenceEnd = startSequence;
    }
    else
    {
      startSequence = nextKey;
      curSequenceEnd = startSequence;
      msgIds.Append(keyArray[keyIndex], 10);
      if (!lastKey)
        msgIds += ',';
    }
  }
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::DeleteMessages(nsISupportsArray *messages,
                                               nsIMsgWindow *msgWindow,
                                               PRBool deleteStorage, PRBool isMove)
{
    nsresult rv = NS_ERROR_FAILURE;
    // *** jt - assuming delete is move to the trash folder for now
    nsCOMPtr<nsIEnumerator> aEnumerator;
    nsCOMPtr<nsIRDFResource> res;
    nsCString uri;
    PRBool deleteImmediatelyNoTrash = PR_FALSE;
    nsCString messageIds;
    nsMsgKeyArray srcKeyArray;

    nsMsgImapDeleteModel deleteModel = nsMsgImapDeleteModels::MoveToTrash;

  nsCOMPtr<nsIImapIncomingServer> imapServer;
    nsCOMPtr<nsIMsgIncomingServer> server;

  rv = GetFlag(MSG_FOLDER_FLAG_TRASH, &deleteImmediatelyNoTrash);

  if (NS_SUCCEEDED(GetServer(getter_AddRefs(server))) && server)
  {
    imapServer = do_QueryInterface(server);
    if (imapServer)
      imapServer->GetDeleteModel(&deleteModel);
    if (deleteModel != nsMsgImapDeleteModels::MoveToTrash)
      deleteImmediatelyNoTrash = PR_TRUE;
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
			// if we can't find the trash, we'll just have to do an imap delete and pretend this is the trash
			if (NS_FAILED(rv) || !trashFolder)
				deleteImmediatelyNoTrash = PR_TRUE;
		}
	}
    if (NS_SUCCEEDED(rv) && deleteImmediatelyNoTrash)
    {
        rv = StoreImapFlags(kImapMsgDeletedFlag, PR_TRUE, srcKeyArray);
    if (NS_SUCCEEDED(rv))
    {
            if (mDatabase) 
      {
                mDatabase->DeleteMessages(&srcKeyArray,NULL);
//        if(!isMove)
        {
          NotifyDeleteOrMoveMessagesCompleted(this);
        }
      }

      return rv;
    }
    }
    else
    {
    if (msgWindow)
    {
      nsCOMPtr <nsITransactionManager> txnMgr;

      msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));

    if (txnMgr) SetTransactionManager(txnMgr);
    }
        
      if(trashFolder)
	  {
        nsCOMPtr<nsIMsgFolder> srcFolder;
        nsCOMPtr<nsISupports>srcSupport;
        PRUint32 count = 0;
        rv = messages->Count(&count);

        rv = QueryInterface(NS_GET_IID(nsIMsgFolder),
						getter_AddRefs(srcFolder));
        rv = trashFolder->CopyMessages(srcFolder, messages, PR_TRUE, msgWindow, nsnull);
	  }
    }
    return rv;
}

PRBool
nsImapMailFolder::InTrash(nsIMsgFolder* folder)
{
    nsCOMPtr<nsIMsgFolder> parent;
    nsCOMPtr<nsIFolder> iFolder;
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsresult rv;
    PRUint32 flags = 0;

    if (!folder) return PR_FALSE;
    curFolder = do_QueryInterface(folder, &rv);
    if (NS_FAILED(rv)) return PR_FALSE;

    do 
    {
        rv = curFolder->GetParent(getter_AddRefs(iFolder));
        if (NS_FAILED(rv)) return PR_FALSE;
        parent = do_QueryInterface(iFolder, &rv);
        if (NS_FAILED(rv)) return PR_FALSE;
        rv = parent->GetFlags(&flags);
        if (NS_FAILED(rv)) return PR_FALSE;
        if (flags & MSG_FOLDER_FLAG_TRASH)
            return PR_TRUE;
        curFolder = do_QueryInterface(parent, &rv);
    } while (NS_SUCCEEDED(rv) && curFolder);

    return PR_FALSE;
}
NS_IMETHODIMP
nsImapMailFolder::DeleteSubFolders(nsISupportsArray* folders)
{
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsCOMPtr<nsISupports> folderSupport;
    nsCOMPtr<nsIUrlListener> urlListener;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    PRUint32 i, folderCount = 0;
    nsresult rv;

    NS_WITH_SERVICE (nsIImapService, imapService, kCImapService, &rv);
    if (NS_SUCCEEDED(rv))
    {
        rv = folders->Count(&folderCount);
        if (NS_SUCCEEDED(rv))
        {
            rv = GetTrashFolder(getter_AddRefs(trashFolder));
            for (i = 0; i < folderCount; i++)
            {
                folderSupport = getter_AddRefs(folders->ElementAt(i));
                curFolder = do_QueryInterface(folderSupport, &rv);
                if (NS_SUCCEEDED(rv))
                {
                    urlListener = do_QueryInterface(curFolder);
                    if (InTrash(curFolder))
                        rv = imapService->DeleteFolder(m_eventQueue,
                                                       curFolder,
                                                       urlListener,
                                                       nsnull);
                    else
                        rv = imapService->MoveFolder(m_eventQueue,
                                                     curFolder,
                                                     trashFolder,
                                                     urlListener,
                                                     nsnull);
                }
            }
        }
    }
        
    return nsMsgFolder::DeleteSubFolders(folders);
}

NS_IMETHODIMP nsImapMailFolder::GetNewMessages(nsIMsgWindow *aWindow)
{
    nsresult rv = NS_ERROR_FAILURE;
    NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
    if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIMsgFolder> inbox;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    PRUint32 numFolders;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
  }
    nsCOMPtr <nsIEventQueue> eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pEventQService)
      pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                        getter_AddRefs(eventQ));
    rv = imapService->SelectFolder(eventQ, inbox, this, aWindow, nsnull);
    return rv;
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxInfo(
  nsIImapProtocol* aProtocol, nsIMailboxSpec* aSpec)
{
  nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;

  nsCOMPtr<nsIFileSpec> pathSpec;
  rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;

    nsFileSpec dbName;
  rv = pathSpec->GetFileSpec(&dbName);
  if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::CreateInstance(kCImapDB, nsnull,
                                            NS_GET_IID(nsIMsgDatabase),
                                            (void **) getter_AddRefs(mailDBFactory));
    if (NS_FAILED(rv)) return rv;

  ChangeNumPendingTotalMessages(-GetNumPendingTotalMessages());
  ChangeNumPendingUnread(-GetNumPendingUnread());

    if (!mDatabase)
    {
        // if we pass in PR_TRUE for upgrading, the db code will ignore the
        // summary out of date problem for now.
        rv = mailDBFactory->Open(pathSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **)
                                 getter_AddRefs(mDatabase));
        if (NS_FAILED(rv))
            return rv;
        if (!mDatabase) 
            return NS_ERROR_NULL_POINTER;
        if (mDatabase && mAddListener)
            mDatabase->AddListener(this);
    }
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

        if (mDatabase) {
            mDatabase->ListAllKeys(existingKeys);
            if (mDatabase->ListAllOfflineDeletes(&existingKeys) > 0)
                existingKeys.QuickSort();
        }
    PRInt32 folderValidity;
    aSpec->GetFolder_UIDVALIDITY(&folderValidity);

    nsCOMPtr <nsIImapFlagAndUidState> flagState;

    aSpec->GetFlagState(getter_AddRefs(flagState));

      if ((imapUIDValidity != folderValidity) /* && // if UIDVALIDITY Changed 
        !NET_IsOffline() */)
      {

#if TRANSFER_INFO
      TNeoFolderInfoTransfer *originalInfo = NULL;
      originalInfo = new TNeoFolderInfoTransfer(dbFolderInfo);
#endif // 0
            if (mDatabase)
      {
        dbFolderInfo = null_nsCOMPtr();
                mDatabase->ForceClosed();
      }
      mDatabase = null_nsCOMPtr();
        
      nsLocalFolderSummarySpec  summarySpec(dbName);
      // Remove summary file.
      summarySpec.Delete(PR_FALSE);
      
      // Create a new summary file, update the folder message counts, and
      // Close the summary file db.
      rv = mailDBFactory->Open(pathSpec, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase));

      // ********** Important *************
      // David, help me here I don't know this is right or wrong
      if (rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
          rv = NS_OK;

      if (NS_FAILED(rv) && mDatabase)
      {
        mDatabase->ForceClosed();
        mDatabase = null_nsCOMPtr();
      }
      else if (NS_SUCCEEDED(rv) && mDatabase)
      {
#if TRANSFER_INFO
        if (originalInfo && mDatabase)
        {
          originalInfo->TransferFolderInfo(mDatabase->m_dbFolderInfo);
          delete originalInfo;
        }
#endif
        SummaryChanged();
                rv = NS_ERROR_UNEXPECTED;
                if (mDatabase) {
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
            if (mDatabase) {
                mDatabase->DeleteMessages(&keysToDelete,NULL);
                total = keysToDelete.GetSize();
            }
    }
    // if this is the INBOX, tell the stand-alone biff about the new high water mark
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
    {
      PRInt32 numRecentMessages = 0;

      if (keysToFetch.GetSize() > 0)
      {
        SetBiffState(nsMsgBiffState_NewMail);
        if (flagState)
        {
          flagState->GetNumberOfRecentMessages(&numRecentMessages);
          SetNumNewMessages(numRecentMessages);
        }
      }
    }
    SyncFlags(flagState);
      if (keysToFetch.GetSize())
      {     
            PrepareToAddHeadersToMailDB(aProtocol, keysToFetch, aSpec);
      if (aProtocol)
        aProtocol->NotifyBodysToDownload(NULL, 0/*keysToFetch.GetSize() */);
      }
      else 
      {
            // let the imap libnet module know that we don't need headers
      if (aProtocol)
        aProtocol->NotifyHdrsToDownload(NULL, 0);
      // wait until we can get body id monitor before continuing.
//      IMAP_BodyIdMonitor(adoptedBoxSpec->connection, PR_TRUE);
      // I think the real fix for this is to seperate the header ids from body id's.
      // this is for fetching bodies for offline use
      if (aProtocol)
        aProtocol->NotifyBodysToDownload(NULL, 0/*keysToFetch.GetSize() */);
//      NotifyFetchAnyNeededBodies(aSpec->connection, mailDB);
//      IMAP_BodyIdMonitor(adoptedBoxSpec->connection, PR_FALSE);
      }
    }

    if (NS_FAILED(rv))
        dbName.Delete(PR_FALSE);

  return rv;
}

NS_IMETHODIMP nsImapMailFolder::UpdateImapMailboxStatus(
  nsIImapProtocol* aProtocol, nsIMailboxSpec* aSpec)
{
  nsresult rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::ChildDiscoverySucceeded(
  nsIImapProtocol* aProtocol)
{
  nsresult rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::PromptUserForSubscribeUpdatePath(
  nsIImapProtocol* aProtocol, PRBool* aBool)
{
  nsresult rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::SetupHeaderParseStream(
    nsIImapProtocol* aProtocol, PRUint32 aSize, const char *content_type, nsIMailboxSpec *boxSpec)
{
    nsresult rv = NS_ERROR_FAILURE;

  if (!mDatabase)
    GetDatabase(nsnull);

  if (mFlags & MSG_FOLDER_FLAG_INBOX && !m_filterList)
  {
    NS_WITH_SERVICE(nsIMsgFilterService, filterService, kMsgFilterServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
    {

      nsCOMPtr <nsIMsgFolder> rootFolder;
      rv = GetRootFolder(getter_AddRefs(rootFolder));

      if (NS_SUCCEEDED(rv))
      {

        nsCOMPtr <nsIFileSpec> rootDir;

        rv = rootFolder->GetPath(getter_AddRefs(rootDir));

        if (NS_SUCCEEDED(rv))
        {
          nsFileSpec    filterFile;

          rootDir->GetFileSpec(&filterFile);
          // need a file spec for filters...
          filterFile += "rules.dat";
          nsresult res;
                    res = filterService->OpenFilterList(&filterFile, getter_AddRefs(m_filterList));
        }
      }
    }

  }
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

NS_IMETHODIMP nsImapMailFolder::ParseAdoptedHeaderLine(
    nsIImapProtocol* aProtocol, const char *aMessageLine, PRUint32 aMsgKey)
{
    // we can get blocks that contain more than one line, 
    // but they never contain partial lines
  const char *str = aMessageLine;
  m_curMsgUid = aMsgKey;
  m_msgParser->SetEnvelopePos(m_curMsgUid); // OK, this is silly (but
                                                // we'll fix
                                                // it). m_envelope_pos, for
                                                // local folders, 
    // is the msg key. Setting this will set the msg key for the new header.

  PRInt32 len = nsCRT::strlen(str);
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
    
NS_IMETHODIMP nsImapMailFolder::NormalEndHeaderParseStream(nsIImapProtocol*
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

    newMsgHdr->SetMessageKey(m_curMsgUid);
    TweakHeaderFlags(aProtocol, newMsgHdr);
    m_msgMovedByFilter = PR_FALSE;
    // If this is the inbox, try to apply filters.
    if (mFlags & MSG_FOLDER_FLAG_INBOX)
    {
      PRUint32 msgFlags;

      newMsgHdr->GetFlags(&msgFlags);
      if (!(msgFlags & MSG_FLAG_READ)) // only fire on unread msgs
      {
        rv = m_msgParser->GetAllHeaders(&headers, &headersSize);

        if (NS_SUCCEEDED(rv) && headers)
        {
          if (m_filterList)
            m_filterList->ApplyFiltersToHdr(nsMsgFilterType::InboxRule, newMsgHdr, this, mDatabase, 
              headers, headersSize, this);
        }
      }
    }
    // here we need to tweak flags from uid state..
    if (!m_msgMovedByFilter)
      mDatabase->AddNewHdrToDB(newMsgHdr, PR_TRUE);
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
 
NS_IMETHODIMP nsImapMailFolder::CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message)
{
  
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

  nsFileSpec path;
  nsMsgKey key;
    nsCOMPtr<nsIRDFResource> res;

  rv = msgDBHdr->GetMessageKey(&key);

  nsCAutoString msgURI;

  if(NS_SUCCEEDED(rv))
    rv = nsBuildImapMessageURI(mBaseMessageURI, key, msgURI);


  if(NS_SUCCEEDED(rv))
    rv = rdfService->GetResource(msgURI.GetBuffer(), getter_AddRefs(res));

  if(NS_SUCCEEDED(rv))
  {
    nsCOMPtr<nsIDBMessage> messageResource = do_QueryInterface(res);
    if(messageResource)
    {
      messageResource->SetMsgDBHdr(msgDBHdr);
      *message = messageResource;
      NS_IF_ADDREF(*message);
    }
  }
  return rv;
}

  
NS_IMETHODIMP nsImapMailFolder::BeginCopy(nsIMessage *message)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
    if (!m_copyState) return rv;
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
        m_copyState->m_tmpFileSpec = null_nsCOMPtr();
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
    {
        m_copyState->m_dataBuffer = (char*) PR_CALLOC(FOUR_K+1);
        if (!m_copyState->m_dataBuffer)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::CopyData(nsIInputStream *aIStream,
                     PRInt32 aLength)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
    NS_ASSERTION(m_copyState && m_copyState->m_dataBuffer &&
                 m_copyState->m_tmpFileSpec, "Fatal copy operation error\n");
    if (!m_copyState || !m_copyState->m_dataBuffer ||
        !m_copyState->m_tmpFileSpec) return rv;

    PRUint32 readCount, maxReadCount = FOUR_K - m_copyState->m_leftOver;
    PRInt32 writeCount;
    char *start, *end;
    PRUint32 linebreak_len = 0;

    while (aLength > 0)
    {
        if (aLength < (PRInt32) maxReadCount)
            maxReadCount = aLength;
        rv = aIStream->Read(m_copyState->m_dataBuffer+m_copyState->m_leftOver,
                            maxReadCount,
                            &readCount);
        if (NS_FAILED(rv)) return rv;

        m_copyState->m_leftOver += readCount;
        m_copyState->m_dataBuffer[m_copyState->m_leftOver] = '\0';

        start = m_copyState->m_dataBuffer;
        end = PL_strstr(start, "\r");
        if (!end)
            end = PL_strstr(start, "\n");
        else if (*(end+1) == LF && linebreak_len == 0)
            linebreak_len = 2;

        if (linebreak_len == 0) // not initialize yet
            linebreak_len = 1;

        aLength -= readCount;
        maxReadCount = FOUR_K - m_copyState->m_leftOver;

        if (!end && aLength > (PRInt32) maxReadCount)
            // must be a very very long line; sorry cannot handle it
            return NS_ERROR_FAILURE;

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
                maxReadCount = FOUR_K;
                m_copyState->m_leftOver = 0;
                break;
            }
            end = PL_strstr(start, "\r");
            if (!end)
                end = PL_strstr(start, "\n");
            if (start && !end)
            {
                m_copyState->m_leftOver -= (start - m_copyState->m_dataBuffer);
                nsCRT::memcpy(m_copyState->m_dataBuffer, start,
                              m_copyState->m_leftOver+1); // including null
                maxReadCount = FOUR_K - m_copyState->m_leftOver;
            }
        }
        if (NS_FAILED(rv)) return rv;
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
        NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
        if (NS_FAILED(rv)) return rv;
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
                                                copySupport);
    }
  return rv;
}

NS_IMETHODIMP nsImapMailFolder::EndMove()
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

NS_IMETHODIMP nsImapMailFolder::ApplyFilterHit(nsIMsgFilter *filter, PRBool *applyMore)
{
  nsMsgRuleActionType actionType;
  void        *value = nsnull;
  PRUint32  newFlags;
  nsresult rv = NS_OK;

  if (!applyMore)
  {
    NS_ASSERTION(PR_FALSE, "need to return status!");
    return NS_ERROR_NULL_POINTER;
  }
  // look at action - currently handle move
#ifdef DEBUG_bienvenu
  printf("got a rule hit!\n");
#endif
  if (NS_SUCCEEDED(filter->GetAction(&actionType, &value)))
  {
    nsCOMPtr<nsIMsgDBHdr> msgHdr;

    if (m_msgParser)
      m_msgParser->GetNewMsgHdr(getter_AddRefs(msgHdr));
    if (NS_SUCCEEDED(rv) && msgHdr)

    {
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
        PRBool deleteToTrash = DeleteIsMoveToTrash();
        if (deleteToTrash)
        {
          // set value to trash folder
          nsCOMPtr <nsIMsgFolder> mailTrash;
          rv = GetTrashFolder(getter_AddRefs(mailTrash));;
          if (NS_SUCCEEDED(rv) && mailTrash)
          {
            // this sucks - but we need value to live past this scope
            // so we use an nsString from above.
            PRUnichar *folderName = nsnull;
            rv = mailTrash->GetName(&folderName);
            trashNameVal = nsCAutoString(folderName);
            nsCRT::free(folderName);
            value = (void *) trashNameVal.GetBuffer();
          }

          msgHdr->OrFlags(MSG_FLAG_READ, &newFlags);  // mark read in trash.
        }
        else  // (!deleteToTrash)
        {
          msgHdr->OrFlags(MSG_FLAG_READ | MSG_FLAG_IMAP_DELETED, &newFlags);
          nsMsgKeyArray keysToFlag;

          keysToFlag.Add(msgKey);
          StoreImapFlags(kImapMsgSeenFlag | kImapMsgDeletedFlag, PR_TRUE, keysToFlag);
//          if (!showDeletedMessages)
//            msgMoved = PR_TRUE; // this will prevent us from adding the header to the db.

        }
      }
      // note that delete falls through to move.
      case nsMsgFilterAction::MoveToFolder:
      {
        // if moving to a different file, do it.
        PRUnichar *folderName = nsnull;
        rv = GetName(&folderName);

        if (value && nsCRT::strcasecmp(folderName, (char *) value))
        {
          msgHdr->GetFlags(&msgFlags);

          if (msgFlags & MSG_FLAG_MDN_REPORT_NEEDED &&
            !isRead)
          {

#if DOING_MDN // leave it to the user aciton
            struct message_header to;
            struct message_header cc;
            GetAggregateHeader (m_toList, &to);
            GetAggregateHeader (m_ccList, &cc);
            msgHdr->SetFlags(msgFlags & ~MSG_FLAG_MDN_REPORT_NEEDED);
            msgHdr->OrFlags(MSG_FLAG_MDN_REPORT_SENT, &newFlags);
            if (actionType == nsMsgFilterActionDelete)
            {
              MSG_ProcessMdnNeededState processMdnNeeded
                (MSG_ProcessMdnNeededState::eDeleted,
                 m_pane, m_folder, msgHdr->GetMessageKey(),
                 &state->m_return_path, &state->m_mdn_dnt, 
                 &to, &cc, &state->m_subject, 
                 &state->m_date, &state->m_mdn_original_recipient,
                 &state->m_message_id, state->m_headers, 
                 (PRInt32) state->m_headers_fp, PR_TRUE);
            }
            else
            {
              MSG_ProcessMdnNeededState processMdnNeeded
                (MSG_ProcessMdnNeededState::eProcessed,
                 m_pane, m_folder, msgHdr->GetMessageKey(),
                 &state->m_return_path, &state->m_mdn_dnt, 
                 &to, &cc, &state->m_subject, 
                 &state->m_date, &state->m_mdn_original_recipient,
                 &state->m_message_id, state->m_headers, 
                 (PRInt32) state->m_headers_fp, PR_TRUE);
            }
            char *tmp = (char*) to.value;
            PR_FREEIF(tmp);
            tmp = (char*) cc.value;
            PR_FREEIF(tmp);
#endif
          }
          nsresult err = MoveIncorporatedMessage(msgHdr, mDatabase, (char *) value, filter);
          if (NS_SUCCEEDED(err))
          {
            m_msgMovedByFilter = PR_TRUE;
            *applyMore = PR_FALSE;
          }

        }
        nsAllocator::Free(folderName);
      }
        break;
      case nsMsgFilterAction::MarkRead:
        {
          nsMsgKeyArray keysToFlag;

          keysToFlag.Add(msgKey);
          StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, keysToFlag);
        }
//        MarkFilteredMessageRead(msgHdr);
        break;
      case nsMsgFilterAction::KillThread:
        // for ignore and watch, we will need the db
        // to check for the flags in msgHdr's that
        // get added, because only then will we know
        // the thread they're getting added to.
        msgHdr->OrFlags(MSG_FLAG_IGNORED, &newFlags);
        break;
      case nsMsgFilterAction::WatchThread:
        msgHdr->OrFlags(MSG_FLAG_WATCHED, &newFlags);
        break;
      case nsMsgFilterAction::ChangePriority:
        msgHdr->SetPriority(*(nsMsgPriority *) &value);
        break;
      default:
        break;
      }
    }
  }
  return rv;
}


nsresult nsImapMailFolder::StoreImapFlags(imapMessageFlagsType flags, PRBool addFlags, nsMsgKeyArray &keysToFlag)
{
  nsresult rv = NS_OK;
  if (PR_TRUE/* !NET_IsOffline() */)
  {
      NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
        if (NS_SUCCEEDED(rv))
        {
            if (addFlags)
            {
                nsCString msgIds;
                
                AllocateUidStringFromKeyArray(keysToFlag, msgIds);
                imapService->AddMessageFlags(m_eventQueue, this, this,
                                             nsnull, msgIds, flags, PR_TRUE);
            }
            else
            {
                nsCString msgIds;
                
                AllocateUidStringFromKeyArray(keysToFlag, msgIds);
                imapService->SubtractMessageFlags(m_eventQueue, this, this,
                                                  nsnull, msgIds, flags,
                                                  PR_TRUE);
            }
        }
  }
  else
  {
#ifdef OFFLINE_IMAP
    MailDB *mailDb = NULL; // change flags offline
    PRBool wasCreated=PR_FALSE;

    ImapMailDB::Open(GetPathname(), PR_TRUE, &mailDb, GetMaster(), &wasCreated);
    if (mailDb)
    {
      UndoManager *undoManager = NULL;
      uint32 total = keysToFlag.GetSize();

      for (int keyIndex=0; keyIndex < total; keyIndex++)
      {
        OfflineImapOperation  *op = mailDb->GetOfflineOpForKey(keysToFlag[keyIndex], PR_TRUE);
        if (op)
        {
          MailDB *originalDB = NULL;
          if (op->GetOperationFlags() & kMoveResult)
          {
            // get the op in the source db and change the flags there
            OfflineImapOperation  *originalOp = GetOriginalOp(op, &originalDB);
            if (originalOp)
            {
              if (undoManager && undoManager->GetState() == UndoIdle && NET_IsOffline()) {
                OfflineIMAPUndoAction *undoAction = new 
                    OfflineIMAPUndoAction(paneForFlagUrl, (MSG_FolderInfo*) this, op->GetMessageKey(), kFlagsChanged,
                    this, NULL, flags, NULL, addFlags);
                if (undoAction)
                  undoManager->AddUndoAction(undoAction);
              }
              op->unrefer();
              op = originalOp;
            }
          }
            
          if (addFlags)
            op->SetImapFlagOperation(op->GetNewMessageFlags() | flags);
          else
            op->SetImapFlagOperation(op->GetNewMessageFlags() & ~flags);
          op->unrefer();

          if (originalDB)
          {
            originalDB->Close();
            originalDB = NULL;
          }
        }
      }
      mailDb->Commit(); // flush offline flags
      mailDb->Close();
      mailDb = NULL;
    }
#endif // OFFLINE_IMAP
  }
  return rv;
}

nsresult nsImapMailFolder::MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
                         nsIMsgDatabase *sourceDB, 
                         char *destFolder,
                         nsIMsgFilter *filter)
{
  nsresult err = NS_OK;
  
  if (!m_moveCoalescer)
    m_moveCoalescer = new nsImapMoveCoalescer(this);
  if (m_moveCoalescer)
  {

    // look for matching imap folders, then pop folders
    nsCOMPtr<nsIMsgIncomingServer> server;
    nsresult rv = GetServer(getter_AddRefs(server));
 
    nsCOMPtr<nsIFolder> rootFolder;
    if (NS_SUCCEEDED(rv)) 
      rv = server->GetRootFolder(getter_AddRefs(rootFolder));

    if (!NS_SUCCEEDED(rv))
      return rv;

    nsCOMPtr <nsIFolder> destIFolder;
    rootFolder->FindSubFolder (destFolder, getter_AddRefs(destIFolder));

    nsCOMPtr <nsIMsgFolder> msgFolder;
    msgFolder = do_QueryInterface(destIFolder);

    if (destIFolder)
    {
      // put the header into the source db, since it needs to be there when we copy it
      // and we need a valid header to pass to StartAsyncCopyMessagesInto
      nsMsgKey keyToFilter;
      mailHdr->GetMessageKey(&keyToFilter);

      if (sourceDB && msgFolder)
      {
        PRBool imapDeleteIsMoveToTrash = DeleteIsMoveToTrash();

        m_moveCoalescer->AddMove (msgFolder, keyToFilter);
        // For each folder, we need to keep track of the ids we want to move to that
        // folder - we used to store them in the MSG_FolderInfo and then when we'd finished
        // downloading headers, we'd iterate through all the folders looking for the ones
        // that needed messages moved into them - perhaps instead we could
        // keep track of nsIMsgFolder, nsMsgKeyArray pairs here in the imap code.
//        nsMsgKeyArray *idsToMoveFromInbox = msgFolder->GetImapIdsToMoveFromInbox();
//        idsToMoveFromInbox->Add(keyToFilter);

        // this is our last best chance to log this
//        if (m_filterList->LoggingEnabled())
//          filter->LogRuleHit(GetLogFile(), mailHdr);

        if (imapDeleteIsMoveToTrash)
        {
        }
        
        msgFolder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
        
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
  PRBool imapDeleteIsMoveToTrash = DeleteIsMoveToTrash();
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
       ((flags & kImapMsgDeletedFlag) && imapDeleteIsMoveToTrash) )
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
      if (showDeletedMessages || ! (flags & kImapMsgDeletedFlag))
      {
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
        
//        m_DownLoadState = kDownLoadingAllMessageHeaders;

        nsresult res = NS_OK; /*ImapMailDB::Open(m_pathName,
                                         PR_TRUE, // create if necessary
                                         &mailDB,
                                         m_master,
                                         &dbWasCreated); */

    // don't want to download headers in a composition pane
        if (NS_SUCCEEDED(res))
        {
#if 0
      SetParseMailboxState(new ParseIMAPMailboxState(m_master, m_host, this,
                               urlQueue,
                               boxSpec->flagState));
          boxSpec->flagState = NULL;    // adopted by ParseIMAPMailboxState
      GetParseMailboxState()->SetPane(url_pane);

            GetParseMailboxState()->SetDB(mailDB);
            GetParseMailboxState()->SetIncrementalUpdate(PR_TRUE);
          GetParseMailboxState()->SetMaster(m_master);
          GetParseMailboxState()->SetContext(url_pane->GetContext());
          GetParseMailboxState()->SetFolder(this);
          
          GetParseMailboxState()->BeginParsingFolder(0);
#endif // 0 hook up parsing later.
          // the imap libnet module will start downloading message headers imap.h
      if (aProtocol)
        aProtocol->NotifyHdrsToDownload(theKeys, total /*keysToFetch.GetSize() */);
      // now, tell it we don't need any bodies.
      if (aProtocol)
        aProtocol->NotifyBodysToDownload(NULL, 0);
        }
        else
        {
      if (aProtocol)
        aProtocol->NotifyHdrsToDownload(NULL, 0);
        }
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
    nsresult res = aProtocol->GetFlagsForUID(m_curMsgUid, &foundIt, &imap_flags);
    if (NS_SUCCEEDED(res) && foundIt)
    {
      // make a mask and clear these message flags
      PRUint32 mask = MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_MARKED | MSG_FLAG_IMAP_DELETED;
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
      else
      {
        if (!(imap_flags & kImapMsgRecentFlag) && 
          dbHdrFlags & MSG_FLAG_MDN_REPORT_NEEDED)
          tweakMe->AndFlags(~MSG_FLAG_MDN_REPORT_NEEDED, &dbHdrFlags);
      }

      if (imap_flags & kImapMsgAnsweredFlag)
        newFlags |= MSG_FLAG_REPLIED;
      if (imap_flags & kImapMsgFlaggedFlag)
        newFlags |= MSG_FLAG_MARKED;
      if (imap_flags & kImapMsgDeletedFlag)
        newFlags |= MSG_FLAG_IMAP_DELETED;
      if (imap_flags & kImapMsgForwardedFlag)
        newFlags |= MSG_FLAG_FORWARDED;

      if (newFlags)
        tweakMe->OrFlags(newFlags, &dbHdrFlags);
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
        nsCString result;
        char *ct;
        PRUint32 writeCount;
        time_t now = time ((time_t*) 0);
        ct = ctime(&now);
        ct[24] = 0;
        result = "From - ";
        result += ct;
        result += MSG_LINEBREAK;
        
        m_tempMessageStream->Write(result.GetBuffer(), result.Length(),
                                   &writeCount);
        result = "X-Mozilla-Status: 0001";
        result += MSG_LINEBREAK;
        m_tempMessageStream->Write(result.GetBuffer(), result.Length(),
                                   &writeCount);
        result =  "X-Mozilla-Status2: 00000000";
        result += MSG_LINEBREAK;
        m_tempMessageStream->Write(result.GetBuffer(), result.Length(),
                                   &writeCount);
    }
    return rv;
}

NS_IMETHODIMP 
nsImapMailFolder::ParseAdoptedMsgLine(const char *adoptedMessageLine, nsMsgKey uidOfMessage)
{
  PRUint32 count = 0;
  // remember the uid of the message we're downloading.
  m_curMsgUid = uidOfMessage;
  if (m_tempMessageStream)
     m_tempMessageStream->Write(adoptedMessageLine, 
                  PL_strlen(adoptedMessageLine), &count);
                                                                                                                                                  
     return NS_OK;                                                               
}
    
NS_IMETHODIMP
nsImapMailFolder::NormalEndMsgWriteStream(nsMsgKey uidOfMessage)
{
  nsresult res = NS_OK;
  if (m_tempMessageStream)
    m_tempMessageStream->Close();

  nsCOMPtr<nsIMsgDBHdr> msgHdr;

  m_curMsgUid = uidOfMessage;
  res = GetMessageHeader(m_curMsgUid, getter_AddRefs(msgHdr));
  if (NS_SUCCEEDED(res))
    msgHdr->MarkRead(PR_TRUE);

  return res;
}

NS_IMETHODIMP
nsImapMailFolder::AbortMsgWriteStream()
{
    return NS_ERROR_FAILURE;
}

nsresult nsImapMailFolder::GetMessageHeader(nsMsgKey key, nsIMsgDBHdr ** aMsgHdr)
{
  nsresult rv = NS_OK;
  if (aMsgHdr)
  {
    rv = GetDatabase(nsnull);
    // In theory, there shouldn't be contention over
    // m_curMsgUid, but it currently describes both the most
    // recent header we downloaded, and most recent message we've
    // downloaded. We may want to break this up.
    if (NS_SUCCEEDED(rv) && mDatabase) // did we get a db back?
      rv = mDatabase->GetMsgHdrForKey(key, aMsgHdr);
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
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
            NS_WITH_SERVICE(nsIEventQueueService, pEventQService,
                            kEventQueueServiceCID, &rv);
            if (NS_FAILED(rv)) return rv;

            rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                     getter_AddRefs(queue));
            if (NS_FAILED(rv)) return rv;
            
            NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
            if (NS_FAILED(rv)) return rv;
       
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
              mDatabase->DeleteMessages(&affectedMessages,NULL);
            nsCRT::free(keyTokenString);
            return rv;
          }
        }
        /* unhandled action */
        else return NS_ERROR_FAILURE;
    }

    /* unhandled copystate */
    else return NS_ERROR_FAILURE;

    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::PrepareToReleaseUrl(nsIMsgMailNewsUrl * aUrl)
{
  mUrlToRelease = aUrl;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::ReleaseUrl()
{
  mUrlToRelease = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::BeginMessageUpload()
{
    return NS_ERROR_FAILURE;
}

// synchronize the message flags in the database with the server flags
nsresult nsImapMailFolder::SyncFlags(nsIImapFlagAndUidState *flagState)
{
    // update all of the database flags
  PRInt32 messageIndex;
  
  flagState->GetNumberOfMessages(&messageIndex);

  for (PRInt32 flagIndex=0; flagIndex < messageIndex; flagIndex++)
  {
    PRUint32 uidOfMessage;
    flagState->GetUidOfMessage(flagIndex, &uidOfMessage);
    imapMessageFlagsType flags;
    flagState->GetMessageFlags(flagIndex, &flags);
    // ### dmb need to do something about imap deleted flag;
    NotifyMessageFlags(flags, uidOfMessage);
    }
  return NS_OK;
}

    // message flags operation
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
		if (!NS_SUCCEEDED(rv) || !containsKey)
			return rv;

		rv = mDatabase->GetMsgHdrForKey(msgKey, getter_AddRefs(dbHdr));

		if(NS_SUCCEEDED(rv) && dbHdr)
		{
	    mDatabase->MarkHdrRead(dbHdr, (flags & kImapMsgSeenFlag) != 0, nsnull);
		  mDatabase->MarkHdrReplied(dbHdr, (flags & kImapMsgAnsweredFlag) != 0, nsnull);
			mDatabase->MarkHdrMarked(dbHdr, (flags & kImapMsgFlaggedFlag) != 0, nsnull);
		}
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::NotifyMessageDeleted(const char *onlineFolderName,PRBool deleteAllMsgs, const char *msgIdString)
{
  const char *doomedKeyString = msgIdString;

  PRBool showDeletedMessages = ShowDeletedMessages();

  if (deleteAllMsgs)
  {
#ifdef HAVE_PORT    
    TNeoFolderInfoTransfer *originalInfo = NULL;
    nsIMsgDatabase *folderDB;
    if (ImapMailDB::Open(GetPathname(), PR_FALSE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
    {
      originalInfo = new TNeoFolderInfoTransfer(*folderDB->m_neoFolderInfo);
      folderDB->ForceClosed();
    }
      
    // Remove summary file.
    XP_FileRemove(GetPathname(), xpMailFolderSummary);
    
    // Create a new summary file, update the folder message counts, and
    // Close the summary file db.
    if (ImapMailDB::Open(GetPathname(), PR_TRUE, &folderDB, GetMaster(), &wasCreated) == eSUCCESS)
    {
      if (originalInfo)
      {
        originalInfo->TransferFolderInfo(*folderDB->m_neoFolderInfo);
        delete originalInfo;
      }
      SummaryChanged();
      folderDB->Close();
    }
#endif
    // ### DMB - how to do this? Reload any thread pane because it's invalid now.
    return NS_OK;
  }

  char *keyTokenString = PL_strdup(doomedKeyString);
  nsMsgKeyArray affectedMessages;
  ParseUidString(keyTokenString, affectedMessages);

  if (doomedKeyString && !showDeletedMessages)
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
  PR_FREEIF(keyTokenString);
  return NS_OK;
}

PRBool nsImapMailFolder::ShowDeletedMessages()
{
  nsresult err;
    NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                    kCImapHostSessionList, &err);
  PRBool rv = PR_FALSE;

    if (NS_SUCCEEDED(err) && hostSession)
  {
        char *serverKey = nsnull;
        GetServerKey(&serverKey);
        err = hostSession->GetShowDeletedMessagesForHost(serverKey, rv);
        PR_FREEIF(serverKey);
  }
  return rv;
}


PRBool nsImapMailFolder::DeleteIsMoveToTrash()
{
  nsresult err;
    NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                    kCImapHostSessionList, &err);
  PRBool rv = PR_TRUE;

    if (NS_SUCCEEDED(err) && hostSession)
  {
        char *serverKey = nsnull;
        GetServerKey(&serverKey);
        err = hostSession->GetDeleteIsMoveToTrashForHost(serverKey, rv);
        PR_FREEIF(serverKey);
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
    if (*pTrashFolder)
      NS_ADDREF(*pTrashFolder);
  }
  return rv;
}

void nsImapMailFolder::ParseUidString(char *uidString, nsMsgKeyArray &keys)
{
  // This is in the form <id>,<id>, or <id1>:<id2>
  char curChar = *uidString;
  PRBool isRange = PR_FALSE;
  int32 curToken;
  int32 saveStartToken=0;

  for (char *curCharPtr = uidString; curChar && *curCharPtr;)
  {
    char *currentKeyToken = curCharPtr;
    curChar = *curCharPtr;
    while (curChar != ':' && curChar != ',' && curChar != '\0')
      curChar = *curCharPtr++;
    *(curCharPtr - 1) = '\0';
    curToken = atoi(currentKeyToken);
    if (isRange)
    {
      while (saveStartToken < curToken)
        keys.Add(saveStartToken++);
    }
    keys.Add(curToken);
    isRange = (curChar == ':');
    if (isRange)
      saveStartToken = curToken + 1;
  }
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
nsImapMailFolder::OnStartRunningUrl(nsIURI *aUrl)
{
  NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
  m_urlRunning = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::OnStopRunningUrl(nsIURI *aUrl, nsresult aExitCode)
{
  NS_PRECONDITION(aUrl, "just a sanity check since this is a test program");
  nsresult rv = NS_OK;
  m_urlRunning = PR_FALSE;
    NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv); 
  if (aUrl)
  {
    nsCOMPtr<nsIMsgWindow> aWindow;
    nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
        nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(aUrl);
    PRBool folderOpen = PR_FALSE;
    if (mailUrl)
      mailUrl->GetMsgWindow(getter_AddRefs(aWindow));
    if (session)
      session->IsFolderOpenInWindow(this, &folderOpen);

        if (imapUrl)
        {
            nsImapAction imapAction = nsIImapUrl::nsImapTest;
            imapUrl->GetImapAction(&imapAction);
            switch(imapAction)
            {
            case nsIImapUrl::nsImapDeleteMsg:
            case nsIImapUrl::nsImapOnlineMove:
            case nsIImapUrl::nsImapOnlineCopy:
                if (m_copyState)
                {
                    if (NS_SUCCEEDED(aExitCode))
                    {
            if (folderOpen)
              UpdateFolder(aWindow);
            else
              UpdatePendingCounts(PR_TRUE, PR_FALSE);
                        if (m_copyState->m_isMove)
                        {
                            nsCOMPtr<nsIMsgFolder> srcFolder;
                            srcFolder =
                                do_QueryInterface(m_copyState->m_srcSupport,
                                                  &rv);
                            nsCOMPtr<nsIMsgDatabase> srcDB;
                            if (NS_SUCCEEDED(rv))
                                rv = srcFolder->GetMsgDatabase(aWindow,
                                    getter_AddRefs(srcDB));
                            if (NS_SUCCEEDED(rv) && srcDB)
                            {
                                nsCOMPtr<nsImapMoveCopyMsgTxn> msgTxn;
                                nsMsgKeyArray srcKeyArray;
                                msgTxn =
                                    do_QueryInterface(m_copyState->m_undoMsgTxn); 
                                if (msgTxn)
                                    msgTxn->GetSrcKeyArray(srcKeyArray);
                                srcDB->DeleteMessages(&srcKeyArray, nsnull);
                NotifyDeleteOrMoveMessagesCompleted(srcFolder);
                            }
                        }
                        if (m_transactionManager)
                            m_transactionManager->Do(m_copyState->m_undoMsgTxn);
                    }
                    ClearCopyState(aExitCode);
                }
                break;
            case nsIImapUrl::nsImapAddMsgFlags:
              // this isn't really right - we'd like to know we were 
              // deleting a message to start with, but it probably
              // won't do any harm.
                NotifyDeleteOrMoveMessagesCompleted(this);

              break;
            case nsIImapUrl::nsImapAppendMsgFromFile:
            case nsIImapUrl::nsImapAppendDraftFromFile:
                if (m_copyState)
                {
          if (folderOpen)
            UpdateFolder(aWindow);
          else
            UpdatePendingCounts(PR_TRUE, PR_FALSE);
                    m_copyState->m_curIndex++;
                    if (m_copyState->m_curIndex >= m_copyState->m_totalCount)
                    {
                        if (m_transactionManager && m_copyState->m_undoMsgTxn)
                            m_transactionManager->Do(m_copyState->m_undoMsgTxn);
                        ClearCopyState(aExitCode);
                    }
                }
                break;
            case nsIImapUrl::nsImapRenameFolder:
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
  return rv;
}

void nsImapMailFolder::UpdatePendingCounts(PRBool countUnread, PRBool missingAreRead)
{
  nsresult rv;
  if (m_copyState)
  {
    ChangeNumPendingTotalMessages(m_copyState->m_totalCount);

    if (countUnread)
    {
      // count the moves that were unread
      int numUnread = 0;
      nsCOMPtr <nsIMsgFolder> srcFolder = do_QueryInterface(m_copyState->m_srcSupport);
      for (PRUint32 keyIndex=0; keyIndex < m_copyState->m_totalCount; keyIndex++)
      {
        nsCOMPtr<nsIMessage> message;

        nsCOMPtr<nsISupports> aSupport =
          getter_AddRefs(m_copyState->m_messages->ElementAt(keyIndex));
        message = do_QueryInterface(aSupport, &rv);
        // if the key is not there, then assume what the caller tells us to.
        PRBool isRead = missingAreRead;
        PRUint32 flags;
        if (message )
        {
          message->GetFlags(&flags);
          isRead = flags & MSG_FLAG_READ;
        }

        if (!isRead)
          numUnread++;
      }
    
      if (numUnread)
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
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::AddFolderRights(nsIImapProtocol* aProtocol,
                                  nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::RefreshFolderRights(nsIImapProtocol* aProtocol,
                                      nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::FolderNeedsACLInitialized(nsIImapProtocol* aProtocol,
                                            nsIMAPACLRightsInfo* aclRights)
{
    return NS_ERROR_FAILURE;
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
        //            nsImapMailCopyState* mailCopyState = 
        //                (nsImapMailCopyState*) copyState;
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
nsImapMailFolder::GetArbitraryHeaders(nsIImapProtocol* aProtocol,
                                      GenericInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetShouldDownloadArbitraryHeaders(nsIImapProtocol* aProtocol,
                                                    GenericInfo* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::GetShowAttachmentsInline(nsIImapProtocol* aProtocol,
                                           PRBool* aBool)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::HeaderFetchCompleted(nsIImapProtocol* aProtocol)
{
  nsresult rv;
  if (mDatabase)
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  if (m_moveCoalescer)
  {
    nsCOMPtr <nsIEventQueue> eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, pEventQService, kEventQueueServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && pEventQService)
      pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                        getter_AddRefs(eventQ));
    m_moveCoalescer->PlaybackMoves (eventQ);
    delete m_moveCoalescer;
    m_moveCoalescer = nsnull;
  }
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::UpdateSecurityStatus(nsIImapProtocol* aProtocol)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::SetBiffStateAndUpdate(nsIImapProtocol* aProtocol,
                                        nsMsgBiffState biffState)
{
  SetBiffState(biffState);
    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::GetStoredUIDValidity(nsIImapProtocol* aProtocol,
                                       uid_validity_info* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::LiteSelectUIDValidity(nsIImapProtocol* aProtocol,
                                        PRUint32 uidValidity)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsImapMailFolder::GetPath(nsIFileSpec ** aPathName)
{
  nsresult rv;
  if (! m_pathName) 
  {
    m_pathName = new nsNativeFileSpec("");
    if (! m_pathName)
       return NS_ERROR_OUT_OF_MEMORY;

    rv = nsImapURI2Path(kImapRootURI, mURI, *m_pathName);
    //    printf("constructing path %s\n", (const char *) *m_pathName);
    if (NS_FAILED(rv)) return rv;
  }
  rv = NS_NewFileSpecWithSpec(*m_pathName, aPathName);
  return NS_OK;
}


NS_IMETHODIMP nsImapMailFolder::SetPath(nsIFileSpec * aPathName)                
{                                                                               
 if (!aPathName)
     return NS_ERROR_NULL_POINTER;

  if (!m_pathName)
  {
  m_pathName = new nsFileSpec("");
    if (! m_pathName)
         return NS_ERROR_OUT_OF_MEMORY;

  }
  return aPathName->GetFileSpec(m_pathName);                                  
}                                                                               
                                                                                 
                                                               
NS_IMETHODIMP
nsImapMailFolder::ProgressStatus(nsIImapProtocol* aProtocol,
                                 PRUint32 aMsgId, const char *extraInfo)
{
  PRUnichar *progressMsg = nsnull;

  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server)
  {
    nsCOMPtr<nsIImapServerSink> serverSink = do_QueryInterface(server);
    if (serverSink)
      serverSink->GetImapStringByID(aMsgId, &progressMsg);
  }
  if (!progressMsg)
    progressMsg = IMAPGetStringByID(aMsgId);

  if (aProtocol && progressMsg)
  {
    nsCOMPtr <nsIImapUrl> imapUrl;
    aProtocol->GetRunningImapURL(getter_AddRefs(imapUrl));
    if (imapUrl)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(imapUrl);
      if (mailnewsUrl)
      {
        nsCOMPtr <nsIMsgStatusFeedback> feedback;
        mailnewsUrl->GetStatusFeedback(getter_AddRefs(feedback));

        if (extraInfo)
        {
          PRUnichar *printfString = nsTextFormatter::smprintf(progressMsg, extraInfo);
          if (printfString)
          {
            progressMsg = nsCRT::strdup(printfString);  
            nsTextFormatter::smprintf_free(printfString);

          }
        }
        if (feedback)
          feedback->ShowStatusString(progressMsg);
      }
    }
  }
  PR_FREEIF(progressMsg);

    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::PercentProgress(nsIImapProtocol* aProtocol,
                                  ProgressInfo* aInfo)
{
#ifdef DEBUG_bienvenu1
  nsCString message(aInfo->message);
  printf("progress: %d %s\n", aInfo->percent, message.GetBuffer());
#endif
  if (aProtocol)
  {
    nsCOMPtr <nsIImapUrl> imapUrl;
    aProtocol->GetRunningImapURL(getter_AddRefs(imapUrl));
    if (imapUrl)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(imapUrl);
      if (mailnewsUrl)
      {
        nsCOMPtr <nsIMsgStatusFeedback> feedback;
        mailnewsUrl->GetStatusFeedback(getter_AddRefs(feedback));
        if (feedback)
        {
          feedback->ShowProgress(aInfo->percent);
          if (aInfo->message)
            feedback->ShowStatusString(aInfo->message);
        }
      }
    }
  }

    return NS_OK;
}

NS_IMETHODIMP
nsImapMailFolder::TunnelOutStream(nsIImapProtocol* aProtocol,
                                  msg_line_info* aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::ProcessTunnel(nsIImapProtocol* aProtocol,
                                TunnelInfo *aInfo)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsImapMailFolder::CopyNextStreamMessage(nsIImapProtocol* aProtocol,
                                        nsIImapUrl * aUrl)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!aUrl) return rv;
    nsCOMPtr<nsISupports> copyState;
    aUrl->GetCopyState(getter_AddRefs(copyState));
    if (!copyState) return rv;

    nsCOMPtr<nsImapMailCopyState> mailCopyState = do_QueryInterface(copyState,
                                                                    &rv);
    if (NS_FAILED(rv)) return rv;

    if (!mailCopyState->m_streamCopy) return NS_OK;
    if (mailCopyState->m_curIndex < mailCopyState->m_totalCount)
    {
        nsCOMPtr<nsISupports> aSupport =
            getter_AddRefs(mailCopyState->m_messages->ElementAt
                           (mailCopyState->m_curIndex));
        mailCopyState->m_message = do_QueryInterface(aSupport,
                                                     &rv);
        if (NS_SUCCEEDED(rv))
        {
            rv = CopyStreamMessage(mailCopyState->m_message,
                                   this, mailCopyState->m_isMove);
        }
    }
    else if (mailCopyState->m_isMove)
    {
        nsCOMPtr<nsIMsgFolder> srcFolder =
            do_QueryInterface(mailCopyState->m_srcSupport, &rv);
        if (NS_SUCCEEDED(rv) && srcFolder)
        {
            srcFolder->DeleteMessages(mailCopyState->m_messages, nsnull,
                                      PR_TRUE, PR_TRUE);
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
    ProgressStatus(aProtocol, IMAP_DONE, nsnull);

    if (aUrl)
        return aUrl->SetUrlState(isRunning, statusCode);
    return statusCode;
}

nsresult
nsImapMailFolder::CreateDirectoryForFolder(nsFileSpec &path) //** dup
{
  nsresult rv = NS_OK;

  if(!path.IsDirectory())
  {
    //If the current path isn't a directory, add directory separator
    //and test it out.
    rv = AddDirectorySeparator(path);
    if(NS_FAILED(rv))
      return rv;

    nsFileSpec tempPath(path.GetNativePathCString(), PR_TRUE);  // create incoming directories.

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
// used when copying from local mail folder (or other imap server?)
nsresult
nsImapMailFolder::CopyMessagesWithStream(nsIMsgFolder* srcFolder,
                                nsISupportsArray* messages,
                                PRBool isMove,
                                nsIMsgWindow *msgWindow,
                                nsIMsgCopyServiceListener* listener)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!srcFolder || !messages) return rv;

    nsCOMPtr<nsISupports> aSupport(do_QueryInterface(srcFolder, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = InitCopyState(aSupport, messages, isMove, PR_TRUE, listener);
    if(NS_FAILED(rv)) return rv;

    m_copyState->m_streamCopy = PR_TRUE;

    // ** jt - needs to create server to server move/copy undo msg txn
    nsCString messageIds;
    nsMsgKeyArray srcKeyArray;
    nsCOMPtr<nsIUrlListener> urlListener;

  rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));
    rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);

    nsImapMoveCopyMsgTxn* undoMsgTxn = new nsImapMoveCopyMsgTxn(
        srcFolder, &srcKeyArray, messageIds.GetBuffer(), this,
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
    
  nsCOMPtr<nsISupports> msgSupport;
  msgSupport = getter_AddRefs(messages->ElementAt(0));
  if (msgSupport)
  {
    nsCOMPtr<nsIMessage> aMessage;
    aMessage = do_QueryInterface(msgSupport, &rv);
    if (NS_SUCCEEDED(rv))
      CopyStreamMessage(aMessage, this, isMove);
    else
      ClearCopyState(rv);
  }
  else
  {
    rv = NS_ERROR_FAILURE;
  }
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::CopyMessages(nsIMsgFolder* srcFolder,
                               nsISupportsArray* messages,
                               PRBool isMove,
                               nsIMsgWindow *msgWindow,
                               nsIMsgCopyServiceListener* listener)
{
    nsresult rv = NS_OK;
    nsCString messageIds;
    nsMsgKeyArray srcKeyArray;
    nsCOMPtr<nsIUrlListener> urlListener;
    nsCOMPtr<nsISupports> srcSupport;
    nsCOMPtr<nsISupports> copySupport;

  if (msgWindow)
  {
    nsCOMPtr <nsITransactionManager> txnMgr;

    msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
    if (txnMgr) SetTransactionManager(txnMgr);
  }

    NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);

    if (!srcFolder || !messages) return NS_ERROR_NULL_POINTER;
    nsCOMPtr <nsIMsgIncomingServer> srcServer;
    nsCOMPtr <nsIMsgIncomingServer> dstServer;
    
    rv = srcFolder->GetServer(getter_AddRefs(srcServer));
    if(NS_FAILED(rv)) goto done;

    rv = GetServer(getter_AddRefs(dstServer));
    if(NS_FAILED(rv)) goto done;

    PRBool sameServer;
    rv = dstServer->Equals(srcServer, &sameServer);
    if (NS_FAILED(rv)) goto done;

    // if the folders aren't on the same server, do a stream base copy
    if (!sameServer) {
        rv = CopyMessagesWithStream(srcFolder, messages, isMove, msgWindow, listener);
        goto done;
    }

    rv = BuildIdsAndKeyArray(messages, messageIds, srcKeyArray);
    if(NS_FAILED(rv)) goto done;

    srcSupport = do_QueryInterface(srcFolder);
  rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));

    rv = InitCopyState(srcSupport, messages, isMove, PR_TRUE, listener);
    if (NS_FAILED(rv)) goto done;

    m_copyState->m_curIndex = m_copyState->m_totalCount;

    copySupport = do_QueryInterface(m_copyState);
    if (imapService)
        rv = imapService->OnlineMessageCopy(m_eventQueue,
                                            srcFolder, messageIds.GetBuffer(),
                                            this, PR_TRUE, isMove,
                                            urlListener, nsnull,
                                            copySupport);
    if (NS_SUCCEEDED(rv))
    {
        nsImapMoveCopyMsgTxn* undoMsgTxn = new nsImapMoveCopyMsgTxn(
            srcFolder, &srcKeyArray, messageIds.GetBuffer(), this,
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

done:
    return rv;
}

nsresult
nsImapMailFolder::SetTransactionManager(nsITransactionManager* txnMgr)
{
    nsresult rv = NS_OK;
    if (txnMgr && !m_transactionManager)
        m_transactionManager = do_QueryInterface(txnMgr, &rv);
    return rv;
}

NS_IMETHODIMP
nsImapMailFolder::CopyFileMessage(nsIFileSpec* fileSpec,
                                  nsIMessage* msgToReplace,
                                  PRBool isDraftOrTemplate,
                                  nsIMsgWindow *msgWindow,
                                  nsIMsgCopyServiceListener* listener)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsMsgKey key = 0xffffffff;
    nsCString messageId;
    nsCOMPtr<nsIUrlListener> urlListener;
    nsCOMPtr<nsISupports> srcSupport;
    nsCOMPtr<nsISupportsArray> messages;

    if (!fileSpec) return rv;

    srcSupport = do_QueryInterface(fileSpec, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(messages));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = QueryInterface(NS_GET_IID(nsIUrlListener), getter_AddRefs(urlListener));

    if (msgToReplace)
    {
        rv = msgToReplace->GetMessageKey(&key);
        if (NS_SUCCEEDED(rv))
            messageId.Append((PRInt32) key);
    }

    rv = InitCopyState(srcSupport, messages, PR_FALSE, isDraftOrTemplate,
                       listener);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> copySupport;
    if( m_copyState ) 
        copySupport = do_QueryInterface(m_copyState);
    rv = imapService->AppendMessageFromFile(m_eventQueue, fileSpec, this,
                                            messageId.GetBuffer(),
                                            PR_TRUE, isDraftOrTemplate,
                                            urlListener, nsnull,
                                            copySupport);

    return rv;
}

nsresult 
nsImapMailFolder::CopyStreamMessage(nsIMessage* message,
                                    nsIMsgFolder* dstFolder, // should be this
                                    PRBool isMove)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!m_copyState) return rv;

    nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener;

    rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID,
               NULL, NS_GET_IID(nsICopyMessageStreamListener),
         getter_AddRefs(copyStreamListener)); 
  if(NS_FAILED(rv))
    return rv;

    nsCOMPtr<nsICopyMessageListener>
        copyListener(do_QueryInterface(dstFolder, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgFolder>
        srcFolder(do_QueryInterface(m_copyState->m_srcSupport, &rv));
    if (NS_FAILED(rv)) return rv;
    rv = copyStreamListener->Init(srcFolder, copyListener, nsnull);
    if (NS_FAILED(rv)) return rv;
       
    nsCOMPtr<nsIRDFResource> messageNode(do_QueryInterface(message));
    if (!messageNode) return NS_ERROR_FAILURE;
    nsXPIDLCString uri;
    messageNode->GetValue(getter_Copies(uri));

    if (!m_copyState->m_msgService)
    {
        rv = GetMessageServiceFromURI(uri, &m_copyState->m_msgService);
    }

    if (NS_SUCCEEDED(rv) && m_copyState->m_msgService)
    {
        nsIURI * url = nsnull;
    nsCOMPtr<nsIStreamListener>
            streamListener(do_QueryInterface(copyStreamListener, &rv));
    if(NS_FAILED(rv) || !streamListener)
      return NS_ERROR_NO_INTERFACE;

        rv = m_copyState->m_msgService->CopyMessage(uri, streamListener,
                                                     isMove, nsnull, &url);
  }
    return rv;
}

nsImapMailCopyState::nsImapMailCopyState() : m_msgService(nsnull),
    m_isMove(PR_FALSE), m_selectedState(PR_FALSE), m_curIndex(0),
    m_totalCount(0), m_streamCopy(PR_FALSE), m_dataBuffer(nsnull),
    m_leftOver(0)
{
    NS_INIT_REFCNT();
}

nsImapMailCopyState::~nsImapMailCopyState()
{
    PR_FREEIF(m_dataBuffer);
    if (m_msgService && m_message)
    {
        nsCOMPtr<nsIRDFResource> msgNode(do_QueryInterface(m_message));
        if (msgNode)
        {
            nsXPIDLCString uri;
            msgNode->GetValue(getter_Copies(uri));
            ReleaseMessageServiceFromURI(uri, m_msgService);
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
                                nsIMsgCopyServiceListener* listener)
{
    nsresult rv = NS_ERROR_NULL_POINTER;

    if (!srcSupport || !messages) return rv;
    if (m_copyState) return NS_ERROR_FAILURE;

    nsImapMailCopyState* copyState = new nsImapMailCopyState();
    m_copyState = do_QueryInterface(copyState);

    if (!m_copyState) return NS_ERROR_OUT_OF_MEMORY;

    if (srcSupport)
        m_copyState->m_srcSupport = do_QueryInterface(srcSupport, &rv);

    if (NS_SUCCEEDED(rv))
    {
        m_copyState->m_messages = do_QueryInterface(messages, &rv);
        if (NS_SUCCEEDED(rv))
            rv = messages->Count(&m_copyState->m_totalCount);
    }
    m_copyState->m_isMove = isMove;
    m_copyState->m_selectedState = selectedState;

    if (listener)
        m_copyState->m_listener = do_QueryInterface(listener, &rv);
        
    return rv;
}

void
nsImapMailFolder::ClearCopyState(nsresult rv)
{
    if (m_copyState)
    {
        nsresult result;
        NS_WITH_SERVICE(nsIMsgCopyService, copyService, 
                        kMsgCopyServiceCID, &result); 
        if (NS_SUCCEEDED(result))
            copyService->NotifyCompletion(m_copyState->m_srcSupport, this, rv);
      
        m_copyState = null_nsCOMPtr();
    }
}

NS_IMETHODIMP nsImapMailFolder::MatchName(nsString *name, PRBool *matches)
{
  if (!matches)
    return NS_ERROR_NULL_POINTER;
    PRBool isInbox = mName.EqualsIgnoreCase("inbox");
  *matches = mName.Equals(*name, isInbox);
  return NS_OK;
}

nsresult nsImapMailFolder::CreateBaseMessageURI(const char *aURI)
{
  nsresult rv;

  rv = nsCreateImapBaseMessageURI(aURI, &mBaseMessageURI);
  return rv;
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

NS_IMETHODIMP nsImapMailFolder::GetFolderNeedsACLListed(PRBool *bVal)
{
    if (!bVal)
        return NS_ERROR_NULL_POINTER;
    // *** jt -- come back later; still need to worry about if the folder
    // itself is a namespace
    *bVal = (m_folderNeedsACLListed && !(mFlags &
                                         MSG_FOLDER_FLAG_IMAP_NOSELECT) 
             /* && !GetFolderIsNamespace() */ );
    return NS_OK;
}

NS_IMETHODIMP nsImapMailFolder::SetFolderNeedsACLListed(PRBool bVal)
{
    m_folderNeedsACLListed = bVal;
    return NS_OK;
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


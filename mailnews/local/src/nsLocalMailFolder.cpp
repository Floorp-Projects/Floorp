/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   jefft@netscape.com
 *   putterman@netscape.com
 *   bienvenu@netscape.com
 *   warren@netscape.com
 *   alecf@netscape.com
 *   sspitzer@netscape.com
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS
#include "nsIPref.h"
#include "prlog.h"

#include "msgCore.h"    // precompiled header...

#include "nsLocalMailFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "prprf.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsIMailboxService.h"
#include "nsIMessage.h"
#include "nsParseMailbox.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgWindow.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"
#include "nsLocalMessage.h"
#include "nsMsgUtils.h"
#include "nsLocalUtils.h"
#include "nsIPop3IncomingServer.h"
#include "nsILocalMailIncomingServer.h"
#include "nsIPop3Service.h"
#include "nsIMsgIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsString.h"
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
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"


static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

//////////////////////////////////////////////////////////////////////////////
// nsFolderCompactState
//////////////////////////////////////////////////////////////////////////////

class nsFolderCompactState : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsFolderCompactState(void);
  virtual ~nsFolderCompactState(void);
  nsresult FinishCompact();
  nsresult GetMessage(nsIMessage **message);
  
  char *m_baseMessageUri; // base message uri
  nsCString m_messageUri; // current message uri being copy
  nsCOMPtr<nsIMsgFolder> m_folder; // current folder being compact
  nsCOMPtr<nsIMsgDatabase> m_db; // new database for the compact folder
  nsFileSpec m_fileSpec; // new mailbox for the compact folder
  nsOutputFileStream *m_fileStream; // output file stream for writing
  nsMsgKeyArray m_keyArray; // all message keys need to be copied over
  PRInt32 m_size; // size of the message key array
  PRInt32 m_curIndex; // index of the current copied message key in key array
  nsMsgKey m_newKey; // new message key for the copied message
  char m_dataBuffer[FOUR_K + 1]; // temp data buffer for copying message
  nsresult m_status; // the status of the copying operation
  nsIMsgMessageService* m_messageService; // message service for copying
                                          // message 
};

NS_IMPL_ISUPPORTS2(nsFolderCompactState, nsIStreamObserver, nsIStreamListener)

nsFolderCompactState::nsFolderCompactState()
{
  NS_INIT_ISUPPORTS();
  m_baseMessageUri = nsnull;
  m_fileStream = nsnull;
  m_size = 0;
  m_curIndex = -1;
  m_status = NS_OK;
  m_messageService = nsnull;
}

nsFolderCompactState::~nsFolderCompactState()
{
  if (m_fileStream)
  {
    m_fileStream->close();
    delete m_fileStream;
    m_fileStream = nsnull;
  }

  if (m_messageService)
  {
    ReleaseMessageServiceFromURI(m_baseMessageUri, m_messageService);
    m_messageService = nsnull;
  }

  if (m_baseMessageUri)
  {
    nsCRT::free(m_baseMessageUri);
    m_baseMessageUri = nsnull;
  }

  if (NS_FAILED(m_status))
  {
    // if for some reason we failed remove the temp folder and database
    if (m_db)
      m_db->ForceClosed();
    nsLocalFolderSummarySpec summarySpec(m_fileSpec);
    m_fileSpec.Delete(PR_FALSE);
    summarySpec.Delete(PR_FALSE);
  }
}

nsresult
nsFolderCompactState::FinishCompact()
{
    // All okay time to finish up the compact process
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFileSpec> pathSpec;
  nsCOMPtr<nsIFolder> parent;
  nsCOMPtr<nsIMsgFolder> parentFolder;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  nsFileSpec fileSpec;

    // get leaf name and database name of the folder
  rv = m_folder->GetPath(getter_AddRefs(pathSpec));
  pathSpec->GetFileSpec(&fileSpec);

  nsLocalFolderSummarySpec summarySpec(fileSpec);
  nsXPIDLCString idlName;
  nsString dbName;

  pathSpec->GetLeafName(getter_Copies(idlName));
  summarySpec.GetLeafName(dbName);

    // close down the temp file stream; preparing for deleting the old folder
    // and its database; then rename the temp folder and database
  m_fileStream->flush();
  m_fileStream->close();
  delete m_fileStream;
  m_fileStream = nsnull;

    // make sure the new database is valid
  m_db->SetSummaryValid(PR_TRUE);
  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
  m_db->ForceClosed();
  m_db = null_nsCOMPtr();

  nsLocalFolderSummarySpec newSummarySpec(m_fileSpec);

    // close down database of the original folder and remove the folder node
    // and all it's message node from the tree
  m_folder->ForceDBClosed();
  m_folder->GetParent(getter_AddRefs(parent));
  parentFolder = do_QueryInterface(parent, &rv);
  m_folder->SetParent(nsnull);
  parentFolder->PropagateDelete(m_folder, PR_FALSE);

    // remove the old folder and database
  fileSpec.Delete(PR_FALSE);
  summarySpec.Delete(PR_FALSE);
    // rename the copied folder and database to be the original folder and
    // database 
  m_fileSpec.Rename((const char*) idlName);
  newSummarySpec.Rename(dbName);

    // add the node back the tree
  nsCOMPtr<nsIMsgFolder> child;
  nsAutoString folderName = (const char*) idlName;
  rv = parentFolder->AddSubfolder(&folderName, getter_AddRefs(child));
  if (child) 
  {
    nsCOMPtr<nsISupports> childSupports = do_QueryInterface(child);
    nsCOMPtr<nsISupports> parentSupports = do_QueryInterface(parentFolder);
    if (childSupports && parentSupports)
    {
      parentFolder->NotifyItemAdded(parentSupports, childSupports,
                                    "folderView");
    }
  }
  return rv;
}

nsresult
nsFolderCompactState::GetMessage(nsIMessage **message)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (!message) return rv;

	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFResource> msgResource;
		if(NS_SUCCEEDED(rdfService->GetResource(m_messageUri,
                                            getter_AddRefs(msgResource))))
			rv = msgResource->QueryInterface(NS_GET_IID(nsIMessage),
                                       (void**)message);
	}
  return rv;
}


NS_IMETHODIMP
nsFolderCompactState::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(m_fileStream, "Fatal, null m_fileStream...\n");
  if (m_fileStream)
  {
    // record the new message key for the message
    m_fileStream->flush();
    m_newKey = m_fileStream->tell();
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsFolderCompactState::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                    nsresult status, 
                                    const PRUnichar *errorMsg)
{
  nsresult rv = status;
  nsCOMPtr<nsIMessage> message;
  nsCOMPtr<nsIDBMessage> dbMessage;
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsCOMPtr<nsIMsgDBHdr> newMsgHdr;

  if (NS_FAILED(rv)) goto done;
  uri = do_QueryInterface(ctxt, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = GetMessage(getter_AddRefs(message));
  if (NS_FAILED(rv)) goto done;
  dbMessage = do_QueryInterface(message, &rv);
  if (NS_FAILED(rv)) goto done;
  rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgHdr));
  if (NS_FAILED(rv)) goto done;

    // okay done with the current message; copying the existing message header
    // to the new database
  m_db->CopyHdrFromExistingHdr(m_newKey, msgHdr,
                               getter_AddRefs(newMsgHdr));

  m_db->Commit(nsMsgDBCommitType::kLargeCommit);
    // advance to next message 
  m_curIndex ++;
  if (m_curIndex >= m_size)
  {
    // no more to copy finish it up
    FinishCompact();
    Release(); // kill self
  }
  else
  {
    m_messageUri.SetLength(0); // clear the previous message uri
    rv = nsBuildLocalMessageURI(m_baseMessageUri, m_keyArray[m_curIndex],
                                m_messageUri);
    if (NS_FAILED(rv)) goto done;
    rv = m_messageService->CopyMessage(m_messageUri, this, PR_FALSE, nsnull,
                                       nsnull);
    
  }

done:
  if (NS_FAILED(rv)) {
    m_status = rv; // set the status to rv so the destructor can remove the
                   // temp folder and database
    Release(); // kill self
    return rv;
  }
  return rv;
}

NS_IMETHODIMP
nsFolderCompactState::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                                      nsIInputStream *inStr, 
                                      PRUint32 sourceOffset, PRUint32 count)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (!m_fileStream || !inStr) return rv;

  PRUint32 maxReadCount, readCount, writeCount;
  rv = NS_OK;
  while (NS_SUCCEEDED(rv) && (PRInt32) count > 0)
  {
    maxReadCount = count > FOUR_K ? FOUR_K : count;
    rv = inStr->Read(m_dataBuffer, maxReadCount, &readCount);
    if (NS_SUCCEEDED(rv))
    {
      writeCount = m_fileStream->write(m_dataBuffer, readCount);
      count -= readCount;
      NS_ASSERTION (writeCount == readCount, 
                    "Oops, write fail, folder can be corrupted!\n");
    }
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////////////
// nsLocal
/////////////////////////////////////////////////////////////////////////////

nsLocalMailCopyState::nsLocalMailCopyState() :
  m_fileStream(nsnull), m_curDstKey(0xffffffff), m_curCopyIndex(0),
  m_messageService(nsnull), m_totalMsgCount(0), m_isMove(PR_FALSE),
  m_dummyEnvelopeNeeded(PR_FALSE), m_leftOver(0), m_fromLineSeen(PR_FALSE)
{
}

nsLocalMailCopyState::~nsLocalMailCopyState()
{
  if (m_fileStream)
  {
    m_fileStream->close();
    delete m_fileStream;
  }
  if (m_messageService)
  {
    nsCOMPtr<nsIRDFResource> msgNode(do_QueryInterface(m_message));
    if (msgNode)
    {
      nsXPIDLCString uri;
      msgNode->GetValue(getter_Copies(uri));
      ReleaseMessageServiceFromURI(uri, m_messageService);
    }
  }
}
  
///////////////////////////////////////////////////////////////////////////////
// nsMsgLocalMailFolder interface
///////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(void)
  : mExpungedBytes(0), 
    mHaveReadNameFromDB(PR_FALSE), mGettingMail(PR_FALSE),
    mInitialized(PR_FALSE), mCopyState(nsnull), mType(nsnull)
{
//  NS_INIT_REFCNT(); done by superclass
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
}

NS_IMPL_ADDREF_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgLocalMailFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(NS_GET_IID(nsIMsgLocalMailFolder)))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMsgLocalMailFolder*, this);
	}              
	else if(aIID.Equals(NS_GET_IID(nsICopyMessageListener)))
	{
		*aInstancePtr = NS_STATIC_CAST(nsICopyMessageListener*, this);
	}

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgDBFolder::QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////

static PRBool
nsStringEndsWith(nsString& name, const char *ending)
{
  if (!ending) return PR_FALSE;

  PRInt32 len = name.Length();
  if (len == 0) return PR_FALSE;

  PRInt32 endingLen = PL_strlen(ending);
  if (len > endingLen && name.RFind(ending, PR_TRUE) == len - endingLen) {
	return PR_TRUE;
  }
  else {
	return PR_FALSE;
  }
}
  
static PRBool
nsShouldIgnoreFile(nsString& name)
{
  PRUnichar firstChar=name.CharAt(0);
  if (firstChar == '.' || firstChar == '#' || name.CharAt(name.Length() - 1) == '~')
    return PR_TRUE;

  if (name.EqualsIgnoreCase("rules.dat"))
    return PR_TRUE;


  // don't add summary files to the list of folders;
  // don't add popstate files to the list either, or rules (sort.dat). 
  if (nsStringEndsWith(name, ".snm") ||
      name.EqualsIgnoreCase("popstate.dat") ||
      name.EqualsIgnoreCase("sort.dat") ||
      name.EqualsIgnoreCase("mailfilt.log") ||
      name.EqualsIgnoreCase("filters.js") ||
      nsStringEndsWith(name, ".toc"))
    return PR_TRUE;

  if (nsStringEndsWith(name,".sbd") || nsStringEndsWith(name,".msf"))
	  return PR_TRUE;

  return PR_FALSE;
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
    nsAutoString convertedFolderNameStr;
    const nsString fileCharset = nsMsgI18NFileSystemCharset();
	nsCOMPtr<nsIMsgFolder> child;
	char *folderName;
	for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++) {
		nsFileSpec currentFolderPath = (nsFileSpec&)dir;

		folderName = currentFolderPath.GetLeafName();
		currentFolderNameStr = folderName;
		if (nsShouldIgnoreFile(currentFolderNameStr))
		{
			PL_strfree(folderName);
			continue;
		}

		AddSubfolder(&currentFolderNameStr, getter_AddRefs(child));
        ConvertToUnicode(fileCharset, folderName, convertedFolderNameStr);
        child->SetName(convertedFolderNameStr.GetUnicode());
		PL_strfree(folderName);
    }
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::AddSubfolder(nsAutoString *name,
                                                 nsIMsgFolder **child)
{
	if(!child)
		return NS_ERROR_NULL_POINTER;

  PRInt32 flags = 0;
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  
	if(NS_FAILED(rv)) return rv;

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');
	uri.Append(*name);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;

  folder->GetFlags((PRUint32 *)&flags);

  flags |= MSG_FOLDER_FLAG_MAIL;

	folder->SetParent(this);

	PRBool isServer;
    rv = GetIsServer(&isServer);

	//Only set these is these are top level children.
	if(NS_SUCCEEDED(rv) && isServer)
	{
		if(name->Compare("Inbox", PR_TRUE) == 0)
		{
			flags |= MSG_FOLDER_FLAG_INBOX;
			mBiffState = nsMsgBiffState_Unknown;
		}
		else if(name->Compare("Trash", PR_TRUE) == 0)
			flags |= MSG_FOLDER_FLAG_TRASH;
		else if(name->Compare("Unsent Messages", PR_TRUE) == 0 
			|| name->Compare("Outbox", PR_TRUE) == 0)
			flags |= MSG_FOLDER_FLAG_QUEUE;
#if 0
		//These should probably be read in from a preference.  Hacking in here for the moment.
		else if(name->Compare("Sent", PR_TRUE) == 0)
			folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
		else if(name->Compare("Drafts", PR_TRUE) == 0)
			folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
		else if(name->Compare("Templates", PR_TRUE) == 0)
			folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
#endif 
  }

	folder->SetFlags(flags);

//at this point we must be ok and we don't want to return failure in case GetIsServer failed.
	rv = NS_OK;

	nsCOMPtr<nsISupports> supports = do_QueryInterface(folder);
	if(folder)
		mSubFolders->AppendElement(supports);
	*child = folder;
	NS_ADDREF(*child);

	return rv;
}


//run the url to parse the mailbox
nsresult nsMsgLocalMailFolder::ParseFolder(nsIMsgWindow *aMsgWindow, nsFileSpec& path)
{
	nsresult rv = NS_OK;

	NS_WITH_SERVICE(nsIMailboxService, mailboxService,
                  kMailboxServiceCID, &rv);
  
	if (NS_FAILED(rv)) return rv; 
	nsMsgMailboxParser *parser = new nsMsgMailboxParser;
	if(!parser)
		return NS_ERROR_OUT_OF_MEMORY;
  
	rv = mailboxService->ParseMailbox(aMsgWindow, path, parser, this, nsnull);

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
    nsAutoString str((nsFilePath)path);
    str += sep;
    path = nsFilePath(str);

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
    
    if (!path.IsDirectory())
      AddDirectorySeparator(path);
    
    if(NS_FAILED(rv)) return rv;
    
    // we have to treat the root folder specially, because it's name
    // doesn't end with .sbd
    PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
    if (path.IsDirectory()) {
      newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
      SetFlag(newFlags);

      if (isServer) {
          nsCOMPtr<nsIMsgIncomingServer> server;
          rv = GetServer(getter_AddRefs(server));
	  if (NS_FAILED(rv)) return rv;
	  if (!server) return NS_ERROR_FAILURE;

          nsCOMPtr<nsILocalMailIncomingServer> localMailServer = do_QueryInterface(server, &rv);
	  if (NS_FAILED(rv)) return rv;
	  if (!localMailServer) return NS_ERROR_FAILURE;

          nsCOMPtr<nsIFileSpec> spec;
          rv = NS_NewFileSpecWithSpec(path, getter_AddRefs(spec));
          if (NS_FAILED(rv)) return rv;

          rv = localMailServer->CreateDefaultMailboxes(spec);
          if (NS_FAILED(rv)) return rv;
      }

      rv = CreateSubFolders(path);

    }
    UpdateSummaryTotals(PR_FALSE);
    
    if (NS_FAILED(rv)) return rv;
    mInitialized = PR_TRUE;      // XXX do this on failure too?
  }
  rv = mSubFolders->Enumerate(result);
  if (isServer)
  {
        // *** setting identity pref special folders flag
        SetPrefFlag();
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::AddUnique(nsISupports* element)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
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

		nsFileSpec path;
		rv = pathSpec->GetFileSpec(&path);
		if (NS_FAILED(rv)) return rv;

		nsresult folderOpen = NS_OK;
		nsCOMPtr<nsIMsgDatabase> mailDBFactory;

		rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
		if (NS_SUCCEEDED(rv) && mailDBFactory)
		{
			folderOpen = mailDBFactory->Open(pathSpec, PR_TRUE, PR_FALSE, getter_AddRefs(mDatabase));
			if(!NS_SUCCEEDED(folderOpen) &&
				folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
			{
				// if it's out of date then reopen with upgrade.
				if(!NS_SUCCEEDED(rv = mailDBFactory->Open(pathSpec, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase))))
				{
					return rv;
				}
			}
	
		}

		if(mDatabase)
		{

			if(mAddListener)
				mDatabase->AddListener(this);

			// if we have to regenerate the folder, run the parser url.
			if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
			{
				if(NS_FAILED(rv = ParseFolder(aMsgWindow, path)))
					return rv;
				else
					return NS_ERROR_NOT_INITIALIZED;
			}
			else
			{
				//We must have loaded the folder so send a notification
				NotifyFolderLoaded();
				//Otherwise we have a valid database so lets extract necessary info.
				UpdateSummaryTotals(PR_TRUE);
			}
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::UpdateFolder(nsIMsgWindow *aWindow)
{
	nsresult rv = NS_OK;
	//If we don't currently have a database, get it.  Otherwise, the folder has been updated (presumably this
	//changes when we download headers when opening inbox).  If it's updated, send NotifyFolderLoaded.
	if(!mDatabase)
		rv = GetDatabase(aWindow); // this will cause a reparse, if needed.
	else
		NotifyFolderLoaded();
	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetMessages(nsIMsgWindow *aMsgWindow, nsISimpleEnumerator* *result)
{
	nsresult rv = GetDatabase(aMsgWindow);

	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsISimpleEnumerator> msgHdrEnumerator;
		nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
		rv = mDatabase->EnumerateMessages(getter_AddRefs(msgHdrEnumerator));
		if(NS_SUCCEEDED(rv))
			rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator,
												   this, &messageEnumerator);
		*result = messageEnumerator;
	}
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
	*url = PR_smprintf("%s%s", urlScheme, (const char *) tmpPath);
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

nsresult
nsMsgLocalMailFolder::CreateSubfolder(const PRUnichar *folderName)
{
	nsresult rv = NS_OK;
    
	nsFileSpec path;
    nsCOMPtr<nsIMsgFolder> child;
	//Get a directory based on our current path.
	rv = CreateDirectoryForFolder(path);
	if(NS_FAILED(rv))
		return rv;


	//Now we have a valid directory or we have returned.
	//Make sure the new folder name is valid
	path += folderName;
	path.MakeUnique();

	nsOutputFileStream outputStream(path);	
   
	// Create an empty database for this mail folder, set its name from the user  
	nsCOMPtr<nsIMsgDatabase> mailDBFactory;

	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
        nsCOMPtr<nsIMsgDatabase> unusedDB;
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(path, getter_AddRefs(dbFileSpec));
		rv = mailDBFactory->Open(dbFileSpec, PR_TRUE, PR_TRUE, getter_AddRefs(unusedDB));

        if (NS_SUCCEEDED(rv) && unusedDB)
        {
			//need to set the folder name
			nsAutoString folderNameStr(folderName);
			nsCOMPtr<nsIDBFolderInfo> folderInfo;
			rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
			if(NS_SUCCEEDED(rv))
			{
				folderInfo->SetMailboxName(&folderNameStr);
			}

			//Now let's create the actual new folder
			rv = AddSubfolder(&folderNameStr, getter_AddRefs(child));
            unusedDB->SetSummaryValid(PR_TRUE);
            unusedDB->Close(PR_TRUE);
        }
        else
        {
			path.Delete(PR_FALSE);
            rv = NS_MSG_CANT_CREATE_FOLDER;
        }
	}
	if(rv == NS_OK && child)
	{
		nsCOMPtr<nsISupports> childSupports(do_QueryInterface(child));
		nsCOMPtr<nsISupports> folderSupports(do_QueryInterface(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this), &rv));
		if(childSupports && NS_SUCCEEDED(rv))
		{

			NotifyItemAdded(folderSupports, childSupports, "folderView");
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Compact()
{
    // **** jefft -- needs to provide nsIMsgWindow for the compact status
    // update; come back later

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIMsgDatabase> db;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  PRInt32 expungedBytes = 0;
  nsCOMPtr<nsIMsgDatabase> mailDBFactory;
  nsresult folderOpen = NS_OK;
  nsCOMPtr<nsIFileSpec> pathSpec;
  nsCOMPtr<nsIFileSpec> newPathSpec;

  rv = GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(db));
  if (NS_FAILED(rv)) return rv;
  rv = folderInfo->GetExpungedBytes(&expungedBytes);

    // check if we need to compact the folder
  if (expungedBytes > 0)
  {
    nsFolderCompactState *compactState = new nsFolderCompactState();
    if (!compactState) return NS_ERROR_OUT_OF_MEMORY;
    rv = QueryInterface(NS_GET_IID(nsIMsgFolder), (void **)
                        getter_AddRefs(compactState->m_folder));
    if (NS_FAILED(rv)) goto done;
    
    compactState->m_baseMessageUri = nsCRT::strdup(mBaseMessageURI);
    if (!compactState->m_baseMessageUri) {
      rv = NS_ERROR_OUT_OF_MEMORY; goto done;
    }

    db ->ListAllKeys(compactState->m_keyArray);
    compactState->m_size = compactState->m_keyArray.GetSize();
    compactState->m_curIndex = 0;
    rv = GetPath(getter_AddRefs(pathSpec));
    
    if (NS_FAILED(rv)) goto done;
    pathSpec->GetFileSpec(&compactState->m_fileSpec);
    compactState->m_fileSpec.SetLeafName("nstmp");
    
    compactState->m_fileStream = new
      nsOutputFileStream(compactState->m_fileSpec);
    if (!compactState->m_fileStream) {
      rv = NS_ERROR_OUT_OF_MEMORY; goto done;
    }
    rv = NS_NewFileSpecWithSpec(compactState->m_fileSpec,
                                getter_AddRefs(newPathSpec));

    rv = nsComponentManager::CreateInstance(kCMailDB, nsnull,
                                            NS_GET_IID(nsIMsgDatabase),
                                            getter_AddRefs(mailDBFactory));
    if (NS_FAILED(rv)) goto done;
    folderOpen = mailDBFactory->Open(newPathSpec, PR_TRUE,
                                     PR_FALSE,
                                     getter_AddRefs(compactState->m_db));

    if(!NS_SUCCEEDED(folderOpen) &&
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || 
       folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
    {
      // if it's out of date then reopen with upgrade.
      rv = mailDBFactory->Open(newPathSpec,
                               PR_TRUE, PR_TRUE,
                               getter_AddRefs(compactState->m_db));
      if (NS_FAILED(rv)) goto done;
    }
    rv = GetMessageServiceFromURI(mBaseMessageURI,
                                  &compactState->m_messageService);
  done:
    if (NS_SUCCEEDED(rv))
    {
      if (compactState->m_size > 0)
      {
        compactState->AddRef();
        rv = nsBuildLocalMessageURI(mBaseMessageURI,
                                    compactState->m_keyArray[0],
                                    compactState->m_messageUri);
        if (NS_SUCCEEDED(rv))
          rv = compactState->m_messageService->CopyMessage(
            compactState->m_messageUri, compactState, PR_FALSE, nsnull,
            nsnull);
      }
      else
      { // no messages to copy with
        compactState->FinishCompact();
        delete compactState;
      }
    }
    if (NS_FAILED(rv))
    {
      compactState->m_status = rv;
      delete compactState;
    }
  }
  return rv;
}


NS_IMETHODIMP nsMsgLocalMailFolder::EmptyTrash(nsIMsgWindow *msgWindow)
{
    nsresult rv;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    rv = GetTrashFolder(getter_AddRefs(trashFolder));
    if (NS_SUCCEEDED(rv))
    {
        trashFolder->RecursiveSetDeleteIsMoveToTrash(PR_FALSE);
        nsCOMPtr<nsIFolder> parent;
        rv = trashFolder->GetParent(getter_AddRefs(parent));
        if (NS_SUCCEEDED(rv) && parent)
        {
            nsCOMPtr<nsIMsgFolder> parentFolder = do_QueryInterface(parent, &rv);
            if (NS_SUCCEEDED(rv) && parentFolder)
            {
                nsXPIDLString idlFolderName;
                rv = trashFolder->GetName(getter_Copies(idlFolderName));
                if (NS_SUCCEEDED(rv))
                {
                    nsString folderName(idlFolderName);
                    trashFolder->SetParent(nsnull);
                    parentFolder->PropagateDelete(trashFolder, PR_TRUE);
                    nsCOMPtr<nsISupports> trashSupport = do_QueryInterface(trashFolder);
                    nsCOMPtr<nsISupports> parentSupport = do_QueryInterface(parent);
                    if (trashSupport && parentSupport)
                      NotifyItemDeleted(parentSupport, trashSupport, "folderView");
                    parentFolder->CreateSubfolder(folderName.GetUnicode());
                }
            }
        }
    }
    return rv;
}


nsresult nsMsgLocalMailFolder::IsChildOfTrash(PRBool *result)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  PRBool isServer = PR_FALSE;

  if (!result) return rv;
  *result = PR_FALSE;

  rv = GetIsServer(&isServer);
  if (NS_FAILED(rv) || isServer) return rv;

  nsCOMPtr<nsIFolder> parent;
  nsCOMPtr<nsIMsgFolder> parentFolder;
  nsCOMPtr<nsIMsgFolder> thisFolder;
  rv = QueryInterface(NS_GET_IID(nsIMsgFolder), (void **)
                      getter_AddRefs(thisFolder));

  PRUint32 parentFlags = 0;

  while (!isServer && thisFolder) {
    rv = thisFolder->GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;
    parentFolder = do_QueryInterface(parent, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = parentFolder->GetIsServer(&isServer);
    if (NS_FAILED(rv) || isServer) return rv;
    rv = parentFolder->GetFlags(&parentFlags);
    if (NS_FAILED(rv)) return rv;
    if (parentFlags & MSG_FOLDER_FLAG_TRASH) {
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
		mDatabase = null_nsCOMPtr();
	}

	nsCOMPtr<nsIFileSpec> pathSpec;
	rv = GetPath(getter_AddRefs(pathSpec));
	if (NS_FAILED(rv)) return rv;

	nsFileSpec path;
	rv = pathSpec->GetFileSpec(&path);
	if (NS_FAILED(rv)) return rv;

  nsLocalFolderSummarySpec	summarySpec(path);

  if (!mDeleteIsMoveToTrash)
  {
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
        {
          path.Delete(PR_TRUE);
        }
    }
  }
  else
  {   // move to trash folder
    nsXPIDLString idlName;
    nsCOMPtr<nsIMsgFolder> child;
    nsAutoString folderName;
    nsCOMPtr<nsIMsgFolder> trashFolder;
    nsCOMPtr<nsIFileSpec> trashSpec;
    nsFileSpec trashPath;

    GetName(getter_Copies(idlName));
    folderName.Assign(idlName);

    rv = GetTrashFolder(getter_AddRefs(trashFolder));
    if (NS_FAILED(rv)) return rv;
    rv = trashFolder->GetPath(getter_AddRefs(trashSpec));
    if (NS_FAILED(rv)) return rv;
    
    rv = trashSpec->GetFileSpec(&trashPath);
    if (NS_FAILED(rv)) return rv;
    AddDirectorySeparator(trashPath);
    if (!trashPath.IsDirectory())
      trashPath.CreateDirectory();
    
    nsFileSpec oldPath = path;

    path.MoveToDir(trashPath);
    summarySpec.MoveToDir(trashPath);

    AddDirectorySeparator(oldPath);
    if (oldPath.IsDirectory())
      oldPath.Delete(PR_TRUE);

    trashFolder->AddSubfolder(&folderName, getter_AddRefs(child));
    if (child) 
    {
      nsCOMPtr<nsISupports> childSupports = do_QueryInterface(child);
      nsCOMPtr<nsISupports> trashSupports = do_QueryInterface(trashFolder);
      if (childSupports && trashSupports)
      {
        NotifyItemAdded(trashSupports, childSupports, "folderView");
      }
    }
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
    return nsMsgFolder::DeleteSubFolders(folders, msgWindow);

  nsCOMPtr<nsIWebShell> webShell;
  if (!msgWindow) return NS_ERROR_NULL_POINTER;
  msgWindow->GetRootWebShell(getter_AddRefs(webShell));
  if (!mMsgStringService)
    mMsgStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_PROGID);
  if (!mMsgStringService) return NS_ERROR_FAILURE;
  PRUnichar *alertString = nsnull;
  mMsgStringService->GetStringByID(POP3_MOVE_FOLDER_TO_TRASH, &alertString);
  if (!alertString) return rv;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webShell));
  if (docShell)
  {
    nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
    if (dialog)
    {
      PRBool okToDelete = PR_FALSE;
      dialog->Confirm(alertString, &okToDelete);
      if (okToDelete)
        return nsMsgFolder::DeleteSubFolders(folders, msgWindow);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Rename(const PRUnichar *aNewName)
{
    nsCOMPtr<nsIFileSpec> oldPathSpec;
    nsCOMPtr<nsIFolder> parent;
    nsresult rv = GetPath(getter_AddRefs(oldPathSpec));
    if (NS_FAILED(rv)) return rv;
    rv = GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;
    ForceDBClosed();
    nsCOMPtr<nsIMsgFolder> parentFolder = do_QueryInterface(parent);
    nsCOMPtr<nsISupports> parentSupport = do_QueryInterface(parent);
    
    nsFileSpec fileSpec;
    oldPathSpec->GetFileSpec(&fileSpec);
    nsLocalFolderSummarySpec oldSummarySpec(fileSpec);
    nsFileSpec dirSpec;

    PRUint32 cnt = 0;
    if (mSubFolders)
      mSubFolders->Count(&cnt);

    if (cnt > 0)
      rv = CreateDirectoryForFolder(dirSpec);

    if (parentFolder)
    {
        SetParent(nsnull);
        parentFolder->PropagateDelete(this, PR_FALSE);
    }

    nsCAutoString newNameStr(aNewName);
    oldPathSpec->Rename(newNameStr.GetBuffer());
    newNameStr += ".msf";
    oldSummarySpec.Rename(newNameStr.GetBuffer());
    if (NS_SUCCEEDED(rv) && cnt > 0)
    {
      nsCAutoString newNameDirStr(aNewName);
      newNameDirStr += ".sbd";
      dirSpec.Rename(newNameDirStr.GetBuffer());
    }
    
    if (parentSupport)
    {
        nsCOMPtr<nsIMsgFolder> newFolder;
        nsAutoString newFolderName(aNewName);
        parentFolder->AddSubfolder(&newFolderName, getter_AddRefs(newFolder));
        nsCOMPtr<nsISupports> newFolderSupport = do_QueryInterface(newFolder);
        NotifyItemAdded(parentSupport, newFolderSupport, "folderView");
        Release(); // really remove ourself from the system; since we need to
                   // regenerate the folder uri from the parent folder.
        /***** jefft -
        * Needs to find a way to reselect the new renamed folder and the
        * message being selected.
        */
    }
    return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos)
{
#ifdef HAVE_PORT
  nsresult err = NS_OK;
  PR_ASSERT (srcFolder->GetType() == GetType());  // we can only adopt the same type of folder
  MSG_FolderInfoMail *mailFolder = (MSG_FolderInfoMail*) srcFolder;

  if (srcFolder == this)
    return MK_MSG_CANT_COPY_TO_SAME_FOLDER;

  if (ContainsChildNamed(mailFolder->GetName()))
    return MK_MSG_FOLDER_ALREADY_EXISTS;  
  
  // If we aren't already a directory, create the directory and set the flag bits
  if (0 == m_subFolders->GetSize())
  {
    XP_Dir dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
    if (dir)
      XP_CloseDir (dir);
    else
    {
      XP_MakeDirectory (m_pathName, xpMailSubdirectory);
      dir = XP_OpenDir (m_pathName, xpMailSubdirectory);
      if (dir)
        XP_CloseDir (dir);
      else
        err = MK_COULD_NOT_CREATE_DIRECTORY;
    }
    if (NS_OK == err)
    {
      m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
      m_flags |= MSG_FOLDER_FLAG_ELIDED;
    }
  }

  // Recurse the tree to adopt srcFolder's children
  err = mailFolder->PropagateAdopt (m_pathName, m_depth);

  // Add the folder to our tree in the right sorted position
  if (NS_OK == err)
  {
    PR_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
    *pOutPos = m_subFolders->Add (srcFolder);
  }

  return err;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(PRUnichar ** prettyName)
{
	return nsMsgFolder::GetPrettyName(prettyName);
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult openErr=NS_ERROR_UNEXPECTED;
    if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;	//ducarroz: should we use NS_ERROR_INVALID_ARG?


	nsCOMPtr<nsIMsgDatabase> mailDBFactory;
	nsCOMPtr<nsIMsgDatabase> mailDB;

	nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		openErr = mailDBFactory->Open(mPath, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) &mailDB);
	}

    *db = mailDB;
	NS_IF_ADDREF(*db);
    if (NS_SUCCEEDED(openErr)&& *db)
        openErr = (*db)->GetDBFolderInfo(folderInfo);
    return openErr;
}

NS_IMETHODIMP nsMsgLocalMailFolder::UpdateSummaryTotals(PRBool force)
{
	PRInt32 oldUnreadMessages = mNumUnreadMessages;
	PRInt32 oldTotalMessages = mNumTotalMessages;
	//We need to read this info from the database
	ReadDBFolderInfo(force);

	// If we asked, but didn't get any, stop asking
	if (mNumUnreadMessages == -1)
		mNumUnreadMessages = -2;

	//Need to notify listeners that total count changed.
	if(oldTotalMessages != mNumTotalMessages)
	{
		NotifyIntPropertyChanged(kTotalMessagesAtom, oldTotalMessages, mNumTotalMessages);
	}

	if(oldUnreadMessages != mNumUnreadMessages)
	{
		NotifyIntPropertyChanged(kTotalUnreadMessagesAtom, oldUnreadMessages, mNumUnreadMessages);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetExpungedBytesCount(PRUint32 *count)
{
  if(!count)
    return NS_ERROR_NULL_POINTER;

  *count = mExpungedBytes;

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetDeletable(PRBool *deletable)
{
  if(!deletable)
    return NS_ERROR_NULL_POINTER;

  PRBool isServer;
  GetIsServer(&isServer);
  // These are specified in the "Mail/News Windows" UI spec
  if (mFlags & MSG_FOLDER_FLAG_TRASH)
  {
    PRBool moveToTrash;
    GetDeleteIsMoveToTrash(&moveToTrash);
    if(moveToTrash)
      *deletable = PR_TRUE;  // allow delete of trash if we don't use trash
  }
  else if (isServer)
    *deletable = PR_FALSE;
  else if (mFlags & MSG_FOLDER_FLAG_INBOX || 
    mFlags & MSG_FOLDER_FLAG_DRAFTS || 
    mFlags & MSG_FOLDER_FLAG_TRASH ||
    mFlags & MSG_FOLDER_FLAG_TEMPLATES)
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
    PRBool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();;
    return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
  }
  return PR_FALSE;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetSizeOnDisk(PRUint32* size)
{
#ifdef HAVE_PORT
  PRInt32 ret = 0;
  XP_StatStruct st;

  if (!XP_Stat(GetPathname(), &st, xpMailFolder))
  ret += st.st_size;

  if (!XP_Stat(GetPathname(), &st, xpMailFolderSummary))
  ret += st.st_size;

  return ret;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
{
#ifdef HAVE_PORT
    PRBool ret = PR_FALSE;
  if (m_master->IsCachePasswordProtected() && !m_master->IsUserAuthenticated() && !m_master->AreLocalFoldersAuthenticated())
  {
    char *savedPassword = GetRememberedPassword();
    if (savedPassword && nsCRT::strlen(savedPassword))
      ret = PR_TRUE;
    FREEIF(savedPassword);
  }
  return ret;
#endif

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::RememberPassword(const char *password)
{
#ifdef HAVE_DB
    MailDB *mailDb = NULL;
  MailDB::Open(m_pathName, PR_TRUE, &mailDb);
  if (mailDb)
  {
    mailDb->SetCachedPassword(password);
    mailDb->Close();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetRememberedPassword(char ** password)
{
#ifdef HAVE_PORT
  PRBool serverIsIMAP = m_master->GetPrefs()->GetMailServerIsIMAP4();
  char *savedPassword = NULL; 
  if (serverIsIMAP)
  {
    MSG_IMAPHost *defaultIMAPHost = m_master->GetIMAPHostTable()->GetDefaultHost();
    if (defaultIMAPHost)
    {
      MSG_FolderInfo *hostFolderInfo = defaultIMAPHost->GetHostFolderInfo();
      MSG_FolderInfo *defaultHostIMAPInbox = NULL;
      if (hostFolderInfo->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &defaultHostIMAPInbox, 1) == 1 
        && defaultHostIMAPInbox != NULL)
      {
        savedPassword = defaultHostIMAPInbox->GetRememberedPassword();
      }
    }
  }
  else
  {
    MSG_FolderInfo *offlineInbox = NULL;
    if (m_flags & MSG_FOLDER_FLAG_INBOX)
    {
      char *retPassword = NULL;
      MailDB *mailDb = NULL;
      MailDB::Open(m_pathName, PR_FALSE, &mailDb, PR_FALSE);
      if (mailDb)
      {
        mailDb->GetCachedPassword(cachedPassword);
        retPassword = nsCRT::strdup(cachedPassword);
        mailDb->Close();

      }
      return retPassword;
    }
    if (m_master->GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &offlineInbox, 1) && offlineInbox)
      savedPassword = offlineInbox->GetRememberedPassword();
  }
  return savedPassword;
#endif
  return NS_OK;
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
                                     PRBool deleteStorage, PRBool isMove)
{
  nsresult rv = NS_ERROR_FAILURE;
  PRBool isTrashFolder = mFlags & MSG_FOLDER_FLAG_TRASH;
  if (!deleteStorage && !isTrashFolder)
  {
      nsCOMPtr<nsIMsgFolder> trashFolder;
      rv = GetTrashFolder(getter_AddRefs(trashFolder));
      if (NS_SUCCEEDED(rv))
      {
          NS_WITH_SERVICE(nsIMsgCopyService, copyService, kMsgCopyServiceCID,
                          &rv);
          if (NS_SUCCEEDED(rv))
              return copyService->CopyMessages(this, messages, trashFolder,
                                        PR_TRUE, nsnull, msgWindow);
      }
      return rv;
  }
  else
  {
	  nsCOMPtr <nsITransactionManager> txnMgr;

	  if (msgWindow)
	  {
		  msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));

		  if (txnMgr) SetTransactionManager(txnMgr);
	  }
  	
      rv = GetDatabase(msgWindow);
      if(NS_SUCCEEDED(rv))
      {
          PRUint32 messageCount;
          nsCOMPtr<nsIMessage> message;
          nsCOMPtr<nsISupports> msgSupport;
          rv = messages->Count(&messageCount);
          if (NS_FAILED(rv)) return rv;
          for(PRUint32 i = 0; i < messageCount; i++)
          {
              msgSupport = getter_AddRefs(messages->ElementAt(i));
              if (msgSupport)
              {
                message = do_QueryInterface(msgSupport, &rv);
                if(message)
                  DeleteMessage(message, msgWindow, PR_TRUE);
              }
          }
		  if(!isMove)
			  NotifyDeleteOrMoveMessagesCompleted(this);
      }
  }
  return rv;
}

nsresult
nsMsgLocalMailFolder::SetTransactionManager(nsITransactionManager* txnMgr)
{
  nsresult rv = NS_OK;
  if (!mTxnMgr)
    mTxnMgr = do_QueryInterface(txnMgr, &rv);
  return rv;
}

nsresult
nsMsgLocalMailFolder::InitCopyState(nsISupports* aSupport, 
                                    nsISupportsArray* messages,
                                    PRBool isMove,
                                    nsIMsgCopyServiceListener* listener)
{
  nsresult rv = NS_OK;
	nsFileSpec path;
	nsCOMPtr<nsIFileSpec> pathSpec;

  if (mCopyState) return NS_ERROR_FAILURE; // already has a  copy in progress

	PRBool isLocked;

	GetLocked(&isLocked);
	if(!isLocked)
		AcquireSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
	else
		return NS_MSG_FOLDER_BUSY;

	rv = GetPath(getter_AddRefs(pathSpec));
	if (NS_FAILED(rv)) goto done;

	rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) goto done;

	mCopyState = new nsLocalMailCopyState();
	if(!mCopyState)
  {
    rv =  NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

	//Before we continue we should verify that there is enough diskspace.
	//XXX How do we do this?
	mCopyState->m_fileStream = new nsOutputFileStream(path, PR_WRONLY |
                                                  PR_CREATE_FILE);
	if(!mCopyState->m_fileStream)
	{
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }
	//The new key is the end of the file
	mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
  mCopyState->m_srcSupport = do_QueryInterface(aSupport, &rv);
  if (NS_FAILED(rv)) goto done;
  mCopyState->m_messages = do_QueryInterface(messages, &rv);
  if (NS_FAILED(rv)) goto done;
  mCopyState->m_curCopyIndex = 0;
  mCopyState->m_isMove = isMove;
  rv = messages->Count(&mCopyState->m_totalMsgCount);
  if (listener)
    mCopyState->m_listener = do_QueryInterface(listener, &rv);
  mCopyState->m_copyingMultipleMessages = PR_FALSE;

done:

  if (NS_FAILED(rv))
    ClearCopyState();

  return rv;
}

void
nsMsgLocalMailFolder::ClearCopyState()
{
  if (mCopyState)
    delete mCopyState;
  mCopyState = nsnull;
  
  PRBool haveSemaphore;
  nsresult result;
  result = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this),
    &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
}
                
NS_IMETHODIMP
nsMsgLocalMailFolder::CopyMessages(nsIMsgFolder* srcFolder, nsISupportsArray*
                                   messages, PRBool isMove,
                                   nsIMsgWindow *msgWindow,
                                   nsIMsgCopyServiceListener* listener)
{
  if (!srcFolder || !messages)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr <nsITransactionManager> txnMgr;

  if (msgWindow)
  {
	  msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));

	if (txnMgr) SetTransactionManager(txnMgr);
  }

	nsresult rv;
  nsCOMPtr<nsISupports> aSupport(do_QueryInterface(srcFolder, &rv));
  if (NS_FAILED(rv)) return rv;

  rv = InitCopyState(aSupport, messages, isMove, listener);
  if (NS_FAILED(rv)) return rv;
  char *uri = nsnull;
  rv = srcFolder->GetURI(&uri);
  nsCString protocolType(uri);
  PR_FREEIF(uri);
  protocolType.SetLength(protocolType.FindChar(':'));

  if (!protocolType.EqualsIgnoreCase("mailbox"))
  {
    mCopyState->m_dummyEnvelopeNeeded = PR_TRUE;
    nsParseMailMessageState* parseMsgState = new nsParseMailMessageState();
    if (parseMsgState)
    {
      nsCOMPtr<nsIMsgDatabase> msgDb;
      mCopyState->m_parseMsgState = do_QueryInterface(parseMsgState, &rv);
      rv = GetMsgDatabase(msgWindow, getter_AddRefs(msgDb));
      if (msgDb)
        parseMsgState->SetMailDB(msgDb);
    }
  }

  // undo stuff
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
    ClearCopyState();
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
	  PRUint32 numMsgs = 0;
	  messages->Count(&numMsgs);
	if (numMsgs > 1 && protocolType.EqualsIgnoreCase("imap"))
	{
		mCopyState->m_copyingMultipleMessages = PR_TRUE;
		rv = CopyMessagesTo(messages, this, isMove);
	}
	else
	{
		nsCOMPtr<nsISupports> msgSupport;
		msgSupport = getter_AddRefs(messages->ElementAt(0));
		if (msgSupport)
		{
		  nsCOMPtr<nsIMessage> aMessage = do_QueryInterface(msgSupport, &rv);
		  if(NS_SUCCEEDED(rv))
			rv = CopyMessageTo(aMessage, this, isMove);
		  else
			ClearCopyState();
		}
	}
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CopyFileMessage(nsIFileSpec* fileSpec, nsIMessage*
                                      msgToReplace, PRBool isDraftOrTemplate,
                                      nsIMsgWindow *msgWindow,
                                      nsIMsgCopyServiceListener* listener)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!fileSpec) return rv;

  nsInputFileStream *inputFileStream = nsnull;
  nsCOMPtr<nsIInputStream> inputStream;
  nsParseMailMessageState* parseMsgState = nsnull;
  PRUint32 fileSize = 0;
  nsCOMPtr<nsISupports> fileSupport(do_QueryInterface(fileSpec, &rv));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupportsArray> messages;
  rv = NS_NewISupportsArray(getter_AddRefs(messages));
  
  if (msgToReplace)
  {
    nsCOMPtr<nsISupports> msgSupport(do_QueryInterface(msgToReplace, &rv));
    if (NS_SUCCEEDED(rv))
      messages->AppendElement(msgSupport);
  }

  rv = InitCopyState(fileSupport, messages, msgToReplace ? PR_TRUE:PR_FALSE,
                     listener);
  if (NS_FAILED(rv)) goto done;

  parseMsgState = new nsParseMailMessageState();
  if (parseMsgState)
  {
    nsCOMPtr<nsIMsgDatabase> msgDb;
    mCopyState->m_parseMsgState = do_QueryInterface(parseMsgState, &rv);
    rv = GetMsgDatabase(msgWindow, getter_AddRefs(msgDb));
    if (msgDb)
      parseMsgState->SetMailDB(msgDb);
  }

  inputFileStream = new nsInputFileStream(fileSpec);
  if (!inputFileStream)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  inputStream = do_QueryInterface(inputFileStream->GetIStream());
  if (!inputStream) {
	rv = NS_ERROR_FAILURE;
	goto done;
  }

  rv = inputStream->Available(&fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = BeginCopy(nsnull);
  if (NS_FAILED(rv)) goto done;
  rv = CopyData(inputStream, (PRInt32) fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = EndCopy(PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  if (msgToReplace)
  {
    rv = DeleteMessage(msgToReplace, msgWindow, PR_TRUE);
  }

done:
  if(NS_FAILED(rv))
  {
    ClearCopyState();
  }

  if (inputFileStream)
  {
    inputFileStream->close();
    inputStream = null_nsCOMPtr();
    delete inputFileStream;
  }
  return rv;
}

nsresult nsMsgLocalMailFolder::DeleteMessage(nsIMessage *message,
                                             nsIMsgWindow *msgWindow,
                                             PRBool deleteStorage)
{
	nsresult rv = NS_OK;
  if (deleteStorage)
	{
		nsCOMPtr <nsIMsgDBHdr> msgDBHdr;
		nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(message, &rv));

		if(NS_SUCCEEDED(rv))
		{
			rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));
			if(NS_SUCCEEDED(rv))
				rv = mDatabase->DeleteHeader(msgDBHdr, nsnull, PR_TRUE, PR_TRUE);
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr,
                                                nsIMessage **message)
{
	
    nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	nsCAutoString msgURI;
	nsMsgKey key;
    nsCOMPtr<nsIRDFResource> resource;

	rv = msgDBHdr->GetMessageKey(&key);
  
	if(NS_SUCCEEDED(rv))
		rv = nsBuildLocalMessageURI(mBaseMessageURI, key, msgURI);
  
	if(NS_SUCCEEDED(rv))
	{
		rv = rdfService->GetResource(msgURI.GetBuffer(), getter_AddRefs(resource));
    }

	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(resource, &rv));
		if(NS_SUCCEEDED(rv))
		{
			//We know from our factory that mailbox message resources are going to be
			//nsLocalMessages.
			dbMessage->SetMsgDBHdr(msgDBHdr);
			*message = dbMessage;
			NS_IF_ADDREF(*message);
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetNewMessages(nsIMsgWindow *aWindow)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server)); 
    if (NS_FAILED(rv)) return rv;
    if (!server) return NS_ERROR_FAILURE;

    nsCOMPtr<nsILocalMailIncomingServer> localMailServer = do_QueryInterface(server, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!localMailServer) return NS_ERROR_FAILURE;

    rv = localMailServer->GetNewMail(aWindow, nsnull, nsnull);
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

    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
    result = "X-Mozilla-Status: 0001" MSG_LINEBREAK;
    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
    result = "X-Mozilla-Status2: 00000000" MSG_LINEBREAK;
    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
    mCopyState->m_fromLineSeen = PR_TRUE;
  }
  else 
  {
    mCopyState->m_fromLineSeen = PR_FALSE;
  }
	return NS_OK;
}

//nsICopyMessageListener
NS_IMETHODIMP nsMsgLocalMailFolder::BeginCopy(nsIMessage *message)
{
  if (!mCopyState) return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);

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
  
	if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;

	PRUint32 readCount, maxReadCount = FOUR_K - mCopyState->m_leftOver;
	mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
  char *start, *end;
  PRUint32 linebreak_len = 0;

  while (aLength > 0)
  {
    if (aLength < (PRInt32) maxReadCount)
      maxReadCount = aLength;
    
    rv = aIStream->Read(mCopyState->m_dataBuffer + mCopyState->m_leftOver,
                        maxReadCount, &readCount);
    if (NS_FAILED(rv)) return rv;
    mCopyState->m_leftOver += readCount;
    mCopyState->m_dataBuffer[mCopyState->m_leftOver] ='\0';
    start = mCopyState->m_dataBuffer;

    end = PL_strstr(start, "\r");
    if (!end)
      	end = PL_strstr(start, "\n");
    else if (*(end+1) == LF && linebreak_len == 0)
        linebreak_len = 2;

    if (linebreak_len == 0) // not set yet
        linebreak_len = 1;

    aLength -= readCount;
    maxReadCount = FOUR_K - mCopyState->m_leftOver;

    if (!end && aLength > (PRInt32) maxReadCount)
    // this must be a gigantic line with no linefeed; sorry pal cannot handle
        return NS_ERROR_FAILURE;

    nsCString line;
    char tmpChar = 0;

    while (start && end)
    {
        if (mCopyState->m_fromLineSeen)
        {
          if (nsCRT::strncmp(start, "From ", 5) == 0)
          {
            line = ">";
        
            tmpChar = *end;
            *end = 0;
            line += start;
            *end = tmpChar;
            line += MSG_LINEBREAK;
        
            mCopyState->m_fileStream->write(line.GetBuffer(), line.Length()); 
            if (mCopyState->m_parseMsgState)
              mCopyState->m_parseMsgState->ParseAFolderLine(line.GetBuffer(),
                                                            line.Length());
            goto keepGoing;
          }
        }
        else
        {
          mCopyState->m_fromLineSeen = PR_TRUE;
          NS_ASSERTION(nsCRT::strncmp(start, "From ", 5) == 0, 
                       "Fatal ... bad message format\n");
        }
        
        mCopyState->m_fileStream->write(start, end-start+linebreak_len); 
        if (mCopyState->m_parseMsgState)
            mCopyState->m_parseMsgState->ParseAFolderLine(start,
                                                  end-start+linebreak_len);
    keepGoing:
        start = end+linebreak_len;
        if (start >=
            &mCopyState->m_dataBuffer[mCopyState->m_leftOver])
        {
            maxReadCount = FOUR_K;
            mCopyState->m_leftOver = 0;
            break;
        }
        end = PL_strstr(start, "\r");
        if (!end)
            end = PL_strstr(start, "\n");
        if (start && !end)
        {
            mCopyState->m_leftOver -= (start - mCopyState->m_dataBuffer);
            nsCRT::memcpy (mCopyState->m_dataBuffer, start,
                           mCopyState->m_leftOver+1);
            maxReadCount = FOUR_K - mCopyState->m_leftOver;
        }
    }
  }
  
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndCopy(PRBool copySucceeded)
{
  nsresult rv = copySucceeded ? NS_OK : NS_ERROR_FAILURE;
  nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;
  nsCOMPtr<nsIMsgWindow> msgWindow;

  if (mCopyState->m_undoMsgTxn)
  {
		localUndoTxn = do_QueryInterface(mCopyState->m_undoMsgTxn, &rv);
    if (NS_SUCCEEDED(rv))
      localUndoTxn->GetMsgWindow(getter_AddRefs(msgWindow));
  }

  
	//Copy the header to the new database
	if(copySucceeded && mCopyState->m_message)
	{ //  CopyMessages() goes here; CopyFileMessage() never gets in here because
    //  the mCopyState->m_message will be always null for file message

    nsCOMPtr<nsIMsgDBHdr> newHdr;
    nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
    nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(mCopyState->m_message,
                                                       &rv));
    
    rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));
    
    if(!mCopyState->m_parseMsgState)
    {
      nsCOMPtr<nsIMsgDatabase> msgDatabase;
      if(NS_SUCCEEDED(rv))
        rv = GetMsgDatabase(msgWindow, getter_AddRefs(msgDatabase));
      
      if(NS_SUCCEEDED(rv))
      {
        rv = mDatabase->CopyHdrFromExistingHdr(mCopyState->m_curDstKey,
                                               msgDBHdr, 
                                               getter_AddRefs(newHdr));
        msgDatabase->SetSummaryValid(PR_TRUE);
        msgDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
      }
    }

    if (NS_SUCCEEDED(rv) && localUndoTxn && msgDBHdr)
    {
      nsMsgKey aKey;
      msgDBHdr->GetMessageKey(&aKey);
      localUndoTxn->AddSrcKey(aKey);
      localUndoTxn->AddDstKey(mCopyState->m_curDstKey);
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
    nsresult result;
    nsCOMPtr<nsIMsgDatabase> msgDb;
    nsCOMPtr<nsIMsgDBHdr> newHdr;

    mCopyState->m_parseMsgState->FinishHeader();

    result = GetMsgDatabase(msgWindow, getter_AddRefs(msgDb));
    if (NS_SUCCEEDED(result) && msgDb)
	{
	  if (!mCopyState->m_copyingMultipleMessages)
	  {
		  result = mCopyState->m_parseMsgState->GetNewMsgHdr(getter_AddRefs(newHdr));
		  if (NS_SUCCEEDED(result) && newHdr)
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
	  }
	  msgDb->SetSummaryValid(PR_TRUE);
	  msgDb->Commit(nsMsgDBCommitType::kLargeCommit);
    }
    mCopyState->m_parseMsgState->Clear();

    if (mCopyState->m_listener) // CopyFileMessage() only
      mCopyState->m_listener->SetMessageKey((PRUint32) mCopyState->m_curDstKey);
    }

  mCopyState->m_curCopyIndex++;

  if (mCopyState->m_curCopyIndex < mCopyState->m_totalMsgCount)
  { // CopyMessages() goes here; CopyFileMessage() never gets in here because
    // curCopyIndex will always be less than the mCopyState->m_totalMsgCount
    nsCOMPtr<nsISupports> aSupport =
      getter_AddRefs(mCopyState->m_messages->ElementAt
                     (mCopyState->m_curCopyIndex));
    nsCOMPtr<nsIMessage>aMessage = do_QueryInterface(aSupport, &rv);
    if (NS_SUCCEEDED(rv))
      rv = CopyMessageTo(aMessage, this, mCopyState->m_isMove);
  }
  else
  { // both CopyMessages() & CopyFileMessage() go here if they have
    // done copying operation; notify completion to copy service
    nsresult result;
	if(!mCopyState->m_isMove)
	{
		NS_WITH_SERVICE(nsIMsgCopyService, copyService, 
						kMsgCopyServiceCID, &result); 

		if (NS_SUCCEEDED(result))
		  copyService->NotifyCompletion(mCopyState->m_srcSupport, this, rv);

		if (mTxnMgr && NS_SUCCEEDED(rv) && mCopyState->m_undoMsgTxn)
		  mTxnMgr->Do(mCopyState->m_undoMsgTxn);
    
		ClearCopyState();
	}
  }

	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndMove()
{

	nsresult result;

	if (mCopyState->m_curCopyIndex == mCopyState->m_totalMsgCount)
	{

		NS_WITH_SERVICE(nsIMsgCopyService, copyService, 
						kMsgCopyServiceCID, &result); 

		if (NS_SUCCEEDED(result))
		{
			//Notify that a completion finished.
			nsCOMPtr<nsIFolder> srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
			if(srcFolder)
			{
					srcFolder->NotifyDeleteOrMoveMessagesCompleted(srcFolder);
			}

			//passing in NS_OK because we only get in here if copy portion succeeded
			copyService->NotifyCompletion(mCopyState->m_srcSupport, this, NS_OK);

			if (mTxnMgr  && mCopyState->m_undoMsgTxn)
				mTxnMgr->Do(mCopyState->m_undoMsgTxn);

			ClearCopyState();
		}

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

    result =
      mCopyState->m_parseMsgState->GetNewMsgHdr(getter_AddRefs(newHdr));
    if (NS_SUCCEEDED(result) && newHdr)
    {
      result = GetMsgDatabase(msgWindow, getter_AddRefs(msgDb));
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
                                             nsIMsgFolder *dstFolder,
                                             PRBool isMove)
{
  if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;
	nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener; 
	nsresult rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID, NULL,
											NS_GET_IID(nsICopyMessageStreamListener),
											getter_AddRefs(copyStreamListener)); 
	if(NS_FAILED(rv))
		return rv;

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
    rv = GetMessageServiceFromURI(uri, &mCopyState->m_messageService);
  }
   
  if (NS_SUCCEEDED(rv) && mCopyState->m_messageService)
  {
    nsIURI * url = nsnull;
	nsMsgKeyArray keyArray;
	PRUint32 numMessages = 0;
	messages->Count(&numMessages);
	for (PRUint32 i = 0; i < numMessages; i++)
	{
        nsCOMPtr<nsISupports> msgSupport;
		msgSupport = getter_AddRefs(messages->ElementAt(i));
		if (msgSupport)
		{
		  nsCOMPtr<nsIMessage> aMessage = do_QueryInterface(msgSupport, &rv);
		  if(NS_SUCCEEDED(rv) && aMessage)
		  {
			  nsMsgKey key;
			  aMessage->GetMessageKey(&key);
			  keyArray.Add(key);
		  }
		}
	}
		nsCOMPtr<nsIStreamListener>
      streamListener(do_QueryInterface(copyStreamListener));
		if(!streamListener)
			return NS_ERROR_NO_INTERFACE;
		// since we're doing all the copies at once, advance curCopyIndex.
		mCopyState->m_curCopyIndex = mCopyState->m_totalMsgCount;
		mCopyState->m_messageService->CopyMessages(&keyArray, srcFolder, streamListener, isMove,
                                            nsnull, &url);
	}

	return rv;
}

nsresult nsMsgLocalMailFolder::CopyMessageTo(nsIMessage *message, 
                                             nsIMsgFolder *dstFolder,
                                             PRBool isMove)
{
  if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;

	nsresult rv;
	nsCOMPtr<nsIRDFResource> messageNode(do_QueryInterface(message));
	if(!messageNode)
		return NS_ERROR_FAILURE;

  if (message)
    mCopyState->m_message = do_QueryInterface(message, &rv);

	nsXPIDLCString uri;
	messageNode->GetValue(getter_Copies(uri));

	nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener; 
	rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID, NULL,
											NS_GET_IID(nsICopyMessageStreamListener),
											getter_AddRefs(copyStreamListener)); 
	if(NS_FAILED(rv))
		return rv;

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
    rv = GetMessageServiceFromURI(uri, &mCopyState->m_messageService);
  }
   
  if (NS_SUCCEEDED(rv) && mCopyState->m_messageService)
  {
    nsIURI * url = nsnull;
		nsCOMPtr<nsIStreamListener>
      streamListener(do_QueryInterface(copyStreamListener));
		if(!streamListener)
			return NS_ERROR_NO_INTERFACE;
		mCopyState->m_messageService->CopyMessage(uri, streamListener, isMove,
                                            nsnull, &url);
	}

	return rv;
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

  nsCOMPtr<nsIURL> url;
  
  // this is totally hacky - we have to re-parse the URI, then
  // guess at "none" or "pop3"
  // if anyone has any better ideas mail me! -alecf@netscape.com
  rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull,
                                          NS_GET_IID(nsIURL),
                                          (void **)getter_AddRefs(url));

  if (NS_FAILED(rv)) return "";

  rv = url->SetSpec(mURI);
  if (NS_FAILED(rv)) return "";

  nsXPIDLCString userName;
  rv = url->GetPreHost(getter_Copies(userName));
  if (NS_FAILED(rv)) return "";
  if ((const char *) userName)
	nsUnescape(NS_CONST_CAST(char*,(const char*)userName));

  nsXPIDLCString hostName;
  rv = url->GetHost(getter_Copies(hostName));
  if (NS_FAILED(rv)) return "";
  if ((const char *) hostName)
	nsUnescape(NS_CONST_CAST(char*,(const char*)hostName));

  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) return "";

  nsCOMPtr<nsIMsgIncomingServer> server;

  // try "none" first
  rv = accountManager->FindServer(userName,
                                  hostName,
                                  "none",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) {
    mType = "none";
    return mType;
  }

  // next try "pop3"
  rv = accountManager->FindServer(userName,
                                  hostName,
                                  "pop3",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) {
    mType = "pop3";
    return mType;
  }

  return "";
}

nsresult nsMsgLocalMailFolder::CreateBaseMessageURI(const char *aURI)
{
	nsresult rv;

	rv = nsCreateLocalBaseMessageURI(aURI, &mBaseMessageURI);
	return rv;

}

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
#ifdef HAVE_MOVEMAIL
#include "nsMovemailIncomingServer.h"
#endif /* HAVE_MOVEMAIL */
#include "nsILocalMailIncomingServer.h"
#include "nsIPop3Service.h"
#ifdef HAVE_MOVEMAIL
#include "nsIMovemailService.h"
#endif /* HAVE_MOVEMAIL */
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


static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
#ifdef HAVE_MOVEMAIL
static NS_DEFINE_CID(kCMovemailServiceCID, NS_MOVEMAILSERVICE_CID);
#endif /* HAVE_MOVEMAIL */
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

extern char* ReadPopData(const char *hostname, const char* username, nsIFileSpec* mailDirectory);
extern void SavePopData(char *data, nsIFileSpec* maildirectory);
extern void net_pop3_delete_if_in_server(char *data, char *uidl, PRBool *changed);
extern void KillPopData(char* data);

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
    nsCOMPtr <nsIMsgFolder> srcFolder = do_QueryInterface(m_srcSupport);
    if (srcFolder && m_message)
    {
      nsXPIDLCString uri;
      srcFolder->GetUriForMsg(m_message, getter_Copies(uri));
      ReleaseMessageServiceFromURI(uri, m_messageService);
    }
  }
}
  
///////////////////////////////////////////////////////////////////////////////
// nsMsgLocalMailFolder interface
///////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(void)
  : mHaveReadNameFromDB(PR_FALSE), mGettingMail(PR_FALSE),
    mInitialized(PR_FALSE), mCopyState(nsnull), mType(nsnull),
    mCheckForNewMessagesAfterParsing(PR_FALSE), mParsingInbox(PR_FALSE)

{
//  NS_INIT_REFCNT(); done by superclass
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
}

NS_IMPL_ADDREF_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)
NS_IMPL_QUERY_INTERFACE_INHERITED2(nsMsgLocalMailFolder,
                                   nsMsgDBFolder,
                                   nsICopyMessageListener,
                                   nsIMsgLocalMailFolder)

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
	nsCOMPtr<nsIMsgFolder> child;

	for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++) {
		nsFileSpec currentFolderPath = dir.Spec();

    currentFolderPath.GetLeafName(currentFolderNameStr);
		if (nsShouldIgnoreFile(currentFolderNameStr))
			continue;

    rv = AddSubfolder(&currentFolderNameStr, getter_AddRefs(child));
    if (child)
      child->SetName(currentFolderNameStr.get());
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
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  
	if(NS_FAILED(rv)) return rv;

  nsCAutoString uri(mURI);
  uri.Append('/');

  // URI should use UTF-8
  // (see RFC2396 Uniform Resource Identifiers (URI): Generic Syntax)
  nsXPIDLCString escapedName;
  rv = NS_MsgEscapeEncodeURLPath((*name).get(), getter_Copies(escapedName));
  NS_ENSURE_SUCCESS(rv,rv);
  uri.Append(escapedName.get());

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri.get(), getter_AddRefs(res));
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
		if(name->EqualsIgnoreCase(nsAutoString(kInboxName)))
		{
			flags |= MSG_FOLDER_FLAG_INBOX;
			mBiffState = nsMsgBiffState_Unknown;
		}
		else if (name->EqualsIgnoreCase(nsAutoString(kTrashName)))
			flags |= MSG_FOLDER_FLAG_TRASH;
		else if (name->EqualsIgnoreCase(nsAutoString(kUnsentName))
             || name->CompareWithConversion("Outbox", PR_TRUE) == 0)
			flags |= MSG_FOLDER_FLAG_QUEUE;
#if 0
		// the logic for this has been moved into 
		// SetFlagsOnDefaultMailboxes()
    else if(name->EqualsIgnoreCase(kSentName))
			folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
		else if(name->EqualsIgnoreCase(kDraftsName))
			folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
		else if(name->EqualsIgnoreCase(kTemplatesName))
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
	nsMsgMailboxParser *parser = new nsMsgMailboxParser;
	if(!parser)
		return NS_ERROR_OUT_OF_MEMORY;
  
	rv = mailboxService->ParseMailbox(aMsgWindow, path, parser, listener, nsnull);

	return rv;
}

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
	  nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDBFactory));
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
 
    nsAutoString str;
    path.GetNativePathString(str);
    str.Append(sep);
    path = str;

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

      PRBool createdDefaultMailboxes = PR_FALSE;
      nsCOMPtr<nsILocalMailIncomingServer> localMailServer;

      if (isServer) {
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
      if (createdDefaultMailboxes && isServer) {
        nsCOMPtr<nsIFolder> rootFolder;
        rv = localMailServer->SetFlagsOnDefaultMailboxes();
        if (NS_FAILED(rv)) return rv;
      }
	
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
        PRBool exists;
        rv = pathSpec->Exists(&exists);
        NS_ENSURE_SUCCESS(rv,rv);
        if (!exists) return NS_ERROR_NULL_POINTER;  //mDatabase will be null at this point.

		nsresult folderOpen = NS_OK;
		nsCOMPtr<nsIMsgDatabase> mailDBFactory;

		rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(mailDBFactory));
		if (NS_SUCCEEDED(rv) && mailDBFactory)
		{
			folderOpen = mailDBFactory->OpenFolderDB(this, PR_TRUE, PR_FALSE, getter_AddRefs(mDatabase));
			if(!NS_SUCCEEDED(folderOpen) &&
				folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
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
				// if it's out of date then reopen with upgrade.
				if(!NS_SUCCEEDED(rv = mailDBFactory->OpenFolderDB(this, PR_TRUE, PR_TRUE, getter_AddRefs(mDatabase))))
				{
					return rv;
				}
        else if (transferInfo && mDatabase)
        {
          mDatabase->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
          if (dbFolderInfo)
            dbFolderInfo->InitFromTransferInfo(transferInfo);
        }
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
              if(mFlags & MSG_FOLDER_FLAG_INBOX)
                mParsingInbox = PR_TRUE;
				if(NS_FAILED(rv = ParseFolder(aMsgWindow, this)))
					return rv;
				else
					return NS_ERROR_NOT_INITIALIZED;
			}
			else
            {
				//We must have loaded the folder so send a notification
        NotifyFolderEvent(mFolderLoadedAtom);
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
  {
    NotifyFolderEvent(mFolderLoadedAtom);
    rv = AutoCompact(aWindow);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
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

  *url = urlStr.ToNewCString();
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

NS_IMETHODIMP nsMsgLocalMailFolder::CreateStorageIfMissing(nsIUrlListener* urlListener)
{
	nsresult status = NS_OK;
  nsCOMPtr <nsIFolder> parent;
  GetParent(getter_AddRefs(parent));
  nsCOMPtr <nsIMsgFolder> msgParent;
  if (parent)
    msgParent = do_QueryInterface(parent);
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
    status = msgParent->CreateSubfolder(folderName,nsnull);
  }
  return status;
}

nsresult
nsMsgLocalMailFolder::AlertFolderExists(nsIMsgWindow *msgWindow) 
{
    nsresult rv = NS_OK;
	nsCOMPtr<nsIDocShell> docShell;
	msgWindow->GetRootDocShell(getter_AddRefs(docShell));
	if (!mMsgStringService)
		mMsgStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_CONTRACTID);
	if (!mMsgStringService) return NS_ERROR_FAILURE;
		PRUnichar *alertString = nsnull;
	mMsgStringService->GetStringByID(POP3_FOLDER_ALREADY_EXISTS, &alertString);
	if (!alertString) return rv;
		if (docShell)
		{
			nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
			if (dialog)
			{
				rv = dialog->Alert(nsnull, alertString);
				return rv;
			}
		}
	return rv;
}

nsresult 
nsMsgLocalMailFolder::CheckIfFolderExists(const PRUnichar *folderName, nsFileSpec &path, nsIMsgWindow *msgWindow)
{
   nsresult rv = NS_OK;
   char *leafName=nsnull;
   for (nsDirectoryIterator dir(path, PR_FALSE); dir.Exists(); dir++)
   {
      nsFileSpec currentFolderPath = dir.Spec();
      leafName = currentFolderPath.GetLeafName();
      if (leafName && nsCRT::strcasecmp(folderName,leafName) == 0)
      {
           if (msgWindow)
              AlertFolderExists(msgWindow);
           PL_strfree(leafName);
           return NS_MSG_FOLDER_EXISTS;
      }
   }
   if (leafName)
      PL_strfree(leafName);
   return rv;
}

nsresult
nsMsgLocalMailFolder::CreateSubfolder(const PRUnichar *folderName, nsIMsgWindow *msgWindow )
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
    rv = CheckIfFolderExists(folderName, path, msgWindow);
    if(NS_FAILED(rv))
       return rv;

	path += nsAutoString(folderName);
		
	nsOutputFileStream outputStream(path, PR_WRONLY | PR_CREATE_FILE, 00600);	
    if (outputStream.is_open())
    {
      outputStream.flush();
      outputStream.close();
    }
   
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
			if (child)
				child->SetName(folderNameStr.get());
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
      {
        return NotifyCompactCompleted();
    }
  }
  }
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor =  do_CreateInstance(NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && folderCompactor)
    if (aFolderArray)
       rv = folderCompactor->StartCompactingAll(aFolderArray, aMsgWindow, aCompactOfflineAlso, aOfflineFolderArray);  
    else if (folderArray)
       rv = folderCompactor->StartCompactingAll(folderArray, aMsgWindow, aCompactOfflineAlso, aOfflineFolderArray);  
  return rv;
}

// **** jefft -- needs to provide nsIMsgWindow for the compact status
// update; come back later
NS_IMETHODIMP nsMsgLocalMailFolder::Compact(nsIUrlListener *aListener, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  nsCOMPtr <nsIMsgFolderCompactor> folderCompactor =  do_CreateInstance(NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && folderCompactor)
  {
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIMsgDatabase> db;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    PRUint32 expungedBytes = 0;
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    nsCOMPtr<nsIFileSpec> pathSpec;

    rv = GetExpungedBytes(&expungedBytes);

    // check if we need to compact the folder
    if (expungedBytes > 0)
    {
      rv = GetMsgDatabase(nsnull, getter_AddRefs(db));
      if (NS_FAILED(rv)) return rv;

      rv = GetPath(getter_AddRefs(pathSpec));
    
      if (NS_SUCCEEDED(rv)) 
      {
        rv = folderCompactor->Init(this, mBaseMessageURI, db, pathSpec, aMsgWindow);
        if (NS_SUCCEEDED(rv))
          rv = folderCompactor->StartCompacting();
      }
    }
    else
      rv = NotifyCompactCompleted();

    return rv;
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
        trashFolder->RecursiveSetDeleteIsMoveToTrash(PR_FALSE);
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
                    nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
                    nsCOMPtr <nsIDBFolderInfo> transferInfo;
                    nsCOMPtr <nsIMsgDatabase> db;
                    trashFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(db));
                    if (dbFolderInfo)
                      dbFolderInfo->GetTransferInfo(getter_AddRefs(transferInfo));

                    nsString folderName(idlFolderName);
                    trashFolder->SetParent(nsnull);
                    parentFolder->PropagateDelete(trashFolder, PR_TRUE, msgWindow);
                    parentFolder->CreateSubfolder(folderName.get(),nsnull);
                    nsCOMPtr<nsIMsgFolder> newTrashFolder;
                    rv = GetTrashFolder(getter_AddRefs(newTrashFolder));
                    if (NS_SUCCEEDED(rv) && newTrashFolder)
                      newTrashFolder->GetMsgDatabase(msgWindow, getter_AddRefs(db));

                    if (transferInfo && db)
                    {
                      db->GetDBFolderInfo(getter_AddRefs(dbFolderInfo));
                      if (dbFolderInfo)
                        dbFolderInfo->InitFromTransferInfo(transferInfo);
                    }
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
  PRUint32 parentFlags = 0;

  if (!result) return rv;
  *result = PR_FALSE;

  rv = GetIsServer(&isServer);
  if (NS_FAILED(rv) || isServer) return rv;

  rv= GetFlags(&parentFlags);  //this is the parent folder
  if (parentFlags & MSG_FOLDER_FLAG_TRASH) {
      *result = PR_TRUE;
      return rv;
  }

  nsCOMPtr<nsIFolder> parent;
  nsCOMPtr<nsIMsgFolder> parentFolder;
  nsCOMPtr<nsIMsgFolder> thisFolder;
  rv = QueryInterface(NS_GET_IID(nsIMsgFolder), (void **)
                      getter_AddRefs(thisFolder));

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
    NotifyStoreClosedAllHeaders();
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
      child->SetName(folderName.get());
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
  {
	PRUint32 count;
	rv = folders->Count(&count);
	nsCOMPtr<nsIMsgFolder> folder;
	for(PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(folders->ElementAt(i));
		folder = do_QueryInterface(supports);
		if(folder)	
	          folder->RecursiveSetDeleteIsMoveToTrash(PR_FALSE);
	}
    return nsMsgFolder::DeleteSubFolders(folders, msgWindow);
  }

  nsCOMPtr<nsIDocShell> docShell;
  if (!msgWindow) return NS_ERROR_NULL_POINTER;
  msgWindow->GetRootDocShell(getter_AddRefs(docShell));
  if (!mMsgStringService)
    mMsgStringService = do_GetService(NS_MSG_POPSTRINGSERVICE_CONTRACTID);
  if (!mMsgStringService) return NS_ERROR_FAILURE;
  PRUnichar *alertString = nsnull;
  mMsgStringService->GetStringByID(POP3_MOVE_FOLDER_TO_TRASH, &alertString);
  if (!alertString) return rv;
  if (docShell)
  {
    nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
    if (dialog)
    {
      PRBool okToDelete = PR_FALSE;
      dialog->Confirm(nsnull, alertString, &okToDelete);
      if (okToDelete)
      { 
        nsCOMPtr<nsIMsgFolder> trashFolder;
        rv = GetTrashFolder(getter_AddRefs(trashFolder));
        if (NS_SUCCEEDED(rv))
        {
                      // we don't allow multiple folder selection so this is ok.
          nsCOMPtr<nsISupports> supports = getter_AddRefs(folders->ElementAt(0));
          nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports);
          if (folder)
             trashFolder->CopyFolder(folder, PR_TRUE, msgWindow, nsnull);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Rename(const PRUnichar *aNewName, nsIMsgWindow *msgWindow)
{
    nsCOMPtr<nsIFileSpec> oldPathSpec;
    nsCOMPtr<nsIFolder> parent;
    nsresult rv = GetPath(getter_AddRefs(oldPathSpec));
    if (NS_FAILED(rv)) return rv;
    rv = GetParent(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return rv;
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
	
	// convert from PRUnichar* to char* due to not having Rename(PRUnichar*)
	// function in nsIFileSpec

	const nsString fileCharset(nsMsgI18NFileSystemCharset());
	nsXPIDLCString convertedNewName;
	if (NS_FAILED(ConvertFromUnicode(fileCharset, nsAutoString(aNewName), getter_Copies(convertedNewName))))
		return NS_ERROR_FAILURE;
	nsCAutoString newNameStr(convertedNewName.get());

    nsXPIDLCString oldLeafName;
	oldPathSpec->GetLeafName(getter_Copies(oldLeafName));

    if (PL_strcasecmp(oldLeafName, convertedNewName) == 0) {
       if(msgWindow)
           rv=AlertFolderExists(msgWindow);
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

       rv = CheckIfFolderExists(aNewName, parentPath, msgWindow);
       if (NS_FAILED(rv)) return rv;

    }

	NotifyStoreClosedAllHeaders();
	ForceDBClosed();

    oldPathSpec->Rename(newNameStr.get());

	newNameStr += ".msf";
	oldSummarySpec.Rename(newNameStr.get());

	
	if (NS_SUCCEEDED(rv) && cnt > 0) {
		// rename "*.sbd" directory
		nsCAutoString newNameDirStr(convertedNewName.get());
		newNameDirStr += ".sbd";
		dirSpec.Rename(newNameDirStr.get());
	}

    nsCOMPtr<nsIMsgFolder> newFolder;
    if (parentSupport)
    {
        nsAutoString newFolderName(aNewName);
		rv = parentFolder->AddSubfolder(&newFolderName, getter_AddRefs(newFolder));
		if (newFolder) 
        {
          newFolder->SetName(newFolderName.get());
          PRBool changed = PR_FALSE;
          MatchOrChangeFilterDestination(newFolder, PR_TRUE /*caseInsenstive*/, &changed);
          if (changed)
            AlertFilterChanged(msgWindow);
          nsCOMPtr<nsISupports> newFolderSupport = do_QueryInterface(newFolder);
          NotifyItemAdded(parentSupport, newFolderSupport, "folderView");
        /***** jefft -
        * Needs to find a way to reselect the new renamed folder and the
        * message being selected.
        */
          if (cnt > 0)
            newFolder->RenameSubFolders(msgWindow, this);
          if (parentFolder)
          {
            SetParent(nsnull);
            parentFolder->PropagateDelete(this, PR_FALSE, msgWindow);
          }
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
     nsAutoString folderNameStr(folderName.get());
     nsCOMPtr <nsIMsgFolder> newFolder;
     AddSubfolder(&folderNameStr, getter_AddRefs(newFolder));
     if (newFolder)
     {
       newFolder->SetName(folderName);
       PRBool changed = PR_FALSE;
       msgFolder->MatchOrChangeFilterDestination(newFolder, PR_TRUE /*caseInsenstive*/, &changed);
       if (changed)
         msgFolder->AlertFilterChanged(msgWindow);

       nsCOMPtr <nsISupports> parentSupport = do_QueryInterface(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
       nsCOMPtr <nsISupports> newFolderSupport = do_QueryInterface(newFolder);
       if (parentSupport && newFolderSupport)
         NotifyItemAdded(parentSupport, newFolderSupport, "folderView");

       newFolder->RenameSubFolders(msgWindow, msgFolder);
     }
     rv = aEnumerator->Next();
  }
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
  if(!db || !folderInfo || !mPath)
    return NS_ERROR_NULL_POINTER;	//ducarroz: should we use NS_ERROR_INVALID_ARG?

  nsresult rv;
  nsCOMPtr<nsIMsgDatabase> mailDBFactory( do_CreateInstance(kCMailDB, &rv) );
	nsCOMPtr<nsIMsgDatabase> mailDB;
  if (NS_SUCCEEDED(rv) && mailDBFactory)
  {
    openErr = mailDBFactory->OpenFolderDB(this, PR_FALSE, PR_FALSE, getter_AddRefs(mailDB));
  }

  *db = mailDB;
  NS_IF_ADDREF(*db);
  if (NS_SUCCEEDED(openErr)&& *db)
    openErr = (*db)->GetDBFolderInfo(folderInfo);
  return openErr;
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
                                     PRBool deleteStorage, PRBool isMove,
                                     nsIMsgCopyServiceListener* listener, PRBool allowUndo)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (!messages) return rv;
  PRUint32 messageCount;
  rv = messages->Count(&messageCount);
  if (messageCount == 0) return rv;

  PRBool isTrashFolder = mFlags & MSG_FOLDER_FLAG_TRASH;
  if (!deleteStorage && !isTrashFolder)
  {
      nsCOMPtr<nsIMsgFolder> trashFolder;
      rv = GetTrashFolder(getter_AddRefs(trashFolder));
      if (NS_SUCCEEDED(rv))
      {
          nsCOMPtr<nsIMsgCopyService> copyService = 
                   do_GetService(kMsgCopyServiceCID, &rv);
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
          DeleteMsgsOnPop3Server(messages);

          if (NS_FAILED(rv)) return rv;
          EnableNotifications(allMessageCountNotifications, PR_FALSE);
          for(PRUint32 i = 0; i < messageCount; i++)
          {
            msgSupport = getter_AddRefs(messages->ElementAt(i));
            if (msgSupport)
              DeleteMessage(msgSupport, msgWindow, PR_TRUE, PR_FALSE);
          }
          // we are the source folder here for a move or shift delete
          //enable notifications first, because that will close the file stream
          // we've been caching, and truly make the summary valid.
          EnableNotifications(allMessageCountNotifications, PR_TRUE);
          mDatabase->SetSummaryValid(PR_TRUE);
          mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
		  if(!isMove)
        NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
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
  mCopyState->m_isFolder = isFolder;
  mCopyState->m_allowUndo = allowUndo;
  mCopyState->m_msgWindow = msgWindow;
  rv = messages->Count(&mCopyState->m_totalMsgCount);
  if (listener)
    mCopyState->m_listener = do_QueryInterface(listener, &rv);
  mCopyState->m_copyingMultipleMessages = PR_FALSE;

done:

  if (NS_FAILED(rv))
    ClearCopyState(PR_FALSE);

  return rv;
}

void
nsMsgLocalMailFolder::ClearCopyState(PRBool moveCopySucceeded)
{
  if (mCopyState)
    delete mCopyState;
  mCopyState = nsnull;
   
  // we are the destination folder for a move/copy
  if (moveCopySucceeded && mDatabase)
  {
    mDatabase->SetSummaryValid(PR_TRUE);
    mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  }

  PRBool haveSemaphore;
  nsresult result;
  result = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this),
    &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
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
  if (!srcFolder || !messages)
    return NS_ERROR_NULL_POINTER;

  PRBool isServer;
  nsresult rv = GetIsServer(&isServer);
  if (NS_SUCCEEDED(rv) && isServer)
  {
    NS_ASSERTION(0, "Destination is the root folder. Cannot move/copy here");
    return NS_OK;
  }

  nsCOMPtr<nsISupports> srcSupport(do_QueryInterface(srcFolder, &rv));
  if (NS_FAILED(rv)) return rv;

  // make sure we turn notifications on when the move is done!
  if (isMove)
    srcFolder->EnableNotifications(allMessageCountNotifications, PR_FALSE);

  // don't update the counts in the dest folder until it is all over
  EnableNotifications(allMessageCountNotifications, PR_FALSE);

  rv = InitCopyState(srcSupport, messages, isMove, listener, msgWindow, isFolder, allowUndo);
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
          ClearCopyState(PR_FALSE);
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
	if (numMsgs > 1 && (protocolType.EqualsIgnoreCase("imap") || protocolType.EqualsIgnoreCase("mailbox")))
	{
		mCopyState->m_copyingMultipleMessages = PR_TRUE;
		rv = CopyMessagesTo(mCopyState->m_messages, msgWindow, this, isMove);
	}
	else
	{
		nsCOMPtr<nsISupports> msgSupport;
		msgSupport = getter_AddRefs(mCopyState->m_messages->ElementAt(0));
		if (msgSupport)
		{
			rv = CopyMessageTo(msgSupport, this, msgWindow, isMove);
		  if (!NS_SUCCEEDED(rv))
      {
        NS_ASSERTION(PR_FALSE, "copy message failed");
			  ClearCopyState(PR_FALSE);
      }
		}
	}
  return rv;
}
// for srcFolder that are on different server than the dstFolder. 
nsresult
nsMsgLocalMailFolder::CopyFolderAcrossServer(nsIMsgFolder* srcFolder, nsIMsgWindow *msgWindow,
								  nsIMsgCopyServiceListener *listener )
{
 	
  nsresult rv;
  mInitialized = PR_TRUE;
  nsCOMPtr<nsIFolder> newFolder;
  nsCOMPtr<nsIMsgFolder> newMsgFolder;
    
  nsXPIDLString folderName;
  srcFolder->GetName(getter_Copies(folderName));
	
  rv = CreateSubfolder(folderName,msgWindow);
  if (NS_FAILED(rv)) return rv;

  FindSubFolder(NS_ConvertUCS2toUTF8(folderName.get()).get(), getter_AddRefs(newFolder));
  newMsgFolder = do_QueryInterface(newFolder,&rv);
  
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
      localFolder->DoNextSubFolder(srcFolder, msgWindow, listener);
	}	    
  return NS_OK;  // otherwise the front-end will say Exception::CopyFolder
}

nsresult    //Continue with next subfolder
nsMsgLocalMailFolder::DoNextSubFolder(nsIMsgFolder *srcFolder, 
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
  nsCOMPtr<nsIFolder> newFolder;
  nsCOMPtr<nsIMsgFolder> newMsgFolder;
	PRBool isChildOfTrash=PR_FALSE;
	rv = IsChildOfTrash(&isChildOfTrash);
  if (isChildOfTrash)
  {
    PRBool match = PR_FALSE;
		PRBool confirmed = PR_FALSE;
    rv = srcFolder->MatchOrChangeFilterDestination(nsnull, PR_FALSE, &match);
    if (match)
		{
      srcFolder->ConfirmFolderDeletionForFilter(msgWindow, &confirmed);
      if (!confirmed) return NS_MSG_ERROR_COPY_FOLDER_ABORTED;
		}
  }
    
  nsXPIDLString idlName;
  srcFolder->GetName(getter_Copies(idlName));
  nsAutoString folderName;
  folderName.Assign(idlName);

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

  rv = CheckIfFolderExists(idlName.get(), newPath, msgWindow);
  if(NS_FAILED(rv)) return rv;
 
  nsFileSpec path = oldPath;

  path.CopyToDir(newPath);   //necessary for aborting....
  summarySpec.CopyToDir(newPath);
   
  AddSubfolder(&folderName, getter_AddRefs(newMsgFolder));  

  PRUint32 flags;
  srcFolder->GetFlags(&flags);
  newMsgFolder->SetFlags(flags);
  PRBool changed = PR_FALSE;
  rv = srcFolder->MatchOrChangeFilterDestination(newMsgFolder, PR_TRUE, &changed);
  if (changed)
    srcFolder->AlertFilterChanged(msgWindow);
  if (newMsgFolder) 
  {
      newMsgFolder->SetName(folderName.get());
      nsCOMPtr<nsISupports> supports = do_QueryInterface(newMsgFolder);
      nsCOMPtr <nsISupports> parentSupports = do_QueryInterface((nsIMsgLocalMailFolder*)this);

      if (supports && parentSupports)
      {
        NotifyItemAdded(parentSupports, supports, "folderView");
      }
  }
  
  nsCOMPtr<nsIEnumerator> aEnumerator;
  srcFolder->GetSubFolders(getter_AddRefs(aEnumerator));
  nsCOMPtr<nsIMsgFolder>folder;
  nsCOMPtr<nsISupports> supports;
  rv = aEnumerator->First();
	nsresult copyStatus = NS_OK;
  while (NS_SUCCEEDED(rv))
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
     nsCOMPtr <nsIFolder> parent;
     nsCOMPtr<nsIMsgFolder> msgParent;
     srcFolder->GetParent(getter_AddRefs(parent));
	   srcFolder->SetParent(nsnull);
     if (parent) 
		 {
			 msgParent = do_QueryInterface(parent);
			 if (msgParent)
				 msgParent->PropagateDelete(srcFolder, PR_FALSE, msgWindow);  // The files have already been moved, so delete storage PR_FALSE 
			 oldPath.Delete(PR_TRUE);  //berkeley mailbox
			 summarySpec.Delete(PR_TRUE); //msf file
			 if (!oldPath.IsDirectory())   
			 {
				 AddDirectorySeparator(oldPath);
				 oldPath.Delete(PR_TRUE);   //delete the .sbd directory and it's content. All subfolders have been moved
			 }
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
  if (!fileSpec) return rv;

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
                     listener, msgWindow, PR_FALSE, PR_FALSE);
  if (NS_FAILED(rv)) goto done;

  parseMsgState = new nsParseMailMessageState();
  if (parseMsgState)
  {
    nsCOMPtr<nsIMsgDatabase> msgDb;
    mCopyState->m_parseMsgState = do_QueryInterface(parseMsgState, &rv);
    rv = GetMsgDatabase(msgWindow, getter_AddRefs(msgDb));
    if (NS_FAILED(rv))
      goto done;
    if (msgDb)
      parseMsgState->SetMailDB(msgDb);
  }

  rv = fileSpec->OpenStreamForReading();
  NS_ENSURE_SUCCESS(rv,rv);

  rv = fileSpec->GetInputStream(getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv,rv);
  
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

  if (msgToReplace)
  {
    rv = DeleteMessage(msgToReplace, msgWindow, PR_TRUE, PR_TRUE);
  }

done:
  if(NS_FAILED(rv))
  {
    ClearCopyState(PR_FALSE);
  }

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

NS_IMETHODIMP nsMsgLocalMailFolder::GetNewMessages(nsIMsgWindow *aWindow, nsIUrlListener *aListener)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server)); 
  if (NS_FAILED(rv)) return rv;
  if (!server) return NS_MSG_INVALID_OR_MISSING_SERVER;
  
  nsCOMPtr<nsILocalMailIncomingServer> localMailServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!localMailServer) return NS_MSG_INVALID_OR_MISSING_SERVER;
  
  //GGGGGGG
  nsCOMPtr<nsIMsgFolder> inbox;
  nsCOMPtr<nsIMsgFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  if(NS_SUCCEEDED(rv) && rootFolder)
  {
    PRUint32 numFolders;
    rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
  }
  PRBool parsingInbox;
  nsCOMPtr<nsIMsgLocalMailFolder> localInbox = do_QueryInterface(inbox, &rv);
  if (NS_SUCCEEDED(rv) && localInbox)
  {
    rv = localInbox->GetParsingInbox(&parsingInbox);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!parsingInbox)
      rv = localMailServer->GetNewMail(aWindow, aListener, inbox, nsnull); 
    else
      rv = localInbox->SetCheckForNewMessagesAfterParsing(PR_TRUE);
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
    nsCOMPtr<nsISupports> aSupport =
        getter_AddRefs(mCopyState->m_messages->ElementAt(mCopyState->m_curCopyIndex));
    curSourceMessage = do_QueryInterface(aSupport, &rv);

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
        statusStrBuf, nsCRT::strlen(statusStrBuf));
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
  if (!mCopyState) return NS_ERROR_NULL_POINTER;
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
  
	if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;

	PRUint32 readCount, maxReadCount = COPY_BUFFER_SIZE - mCopyState->m_leftOver;
	mCopyState->m_fileStream->seek(PR_SEEK_END, 0);
  char *start, *end;
  PRUint32 linebreak_len = 0;

  while (aLength > 0)
  {
    if (aLength < (PRInt32) maxReadCount)
      maxReadCount = aLength;
    
    rv = aIStream->Read(mCopyState->m_dataBuffer + mCopyState->m_leftOver,
                        maxReadCount, &readCount);
    NS_ENSURE_SUCCESS(rv, rv);
    mCopyState->m_leftOver += readCount;
    mCopyState->m_dataBuffer[mCopyState->m_leftOver] ='\0';
    start = mCopyState->m_dataBuffer;
    end = PL_strchr(start, '\r');
    if (!end)
      	end = PL_strchr(start, '\n');
    else if (*(end+1) == nsCRT::LF && linebreak_len == 0)
        linebreak_len = 2;

    if (linebreak_len == 0) // not set yet
        linebreak_len = 1;

    aLength -= readCount;
    maxReadCount = COPY_BUFFER_SIZE - mCopyState->m_leftOver;

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
        
            mCopyState->m_fileStream->write(line.get(), line.Length()); 
            if (mCopyState->m_parseMsgState)
              mCopyState->m_parseMsgState->ParseAFolderLine(line.get(),
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
            maxReadCount = COPY_BUFFER_SIZE;
            mCopyState->m_leftOver = 0;
            break;
        }
        end = PL_strchr(start, '\r');
        if (end)
        {
          if (*(end+1) == nsCRT::LF)  //need to set the linebreak_len each time
             linebreak_len = 2;  //CRLF
          else
             linebreak_len = 1;  //only CR
        }
        if (!end)
        {
         end = PL_strchr(start, '\n');
         if (end)
            linebreak_len = 1;   //LF
         else
            linebreak_len =0;  //no LF
        }

        if (start && !end)
        {
            mCopyState->m_leftOver -= (start - mCopyState->m_dataBuffer);
            nsCRT::memcpy (mCopyState->m_dataBuffer, start,
                           mCopyState->m_leftOver+1);
            maxReadCount = COPY_BUFFER_SIZE - mCopyState->m_leftOver;
        }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndCopy(PRBool copySucceeded)
{
  // we are the destination folder for a move/copy
  nsresult rv = copySucceeded ? NS_OK : NS_ERROR_FAILURE;
  if (!mCopyState) return NS_OK;
  if (!copySucceeded)
  {
    if (mCopyState->m_fileStream)
      mCopyState->m_fileStream->close();
    nsCOMPtr <nsIFileSpec> pathSpec;
    rv = GetPath(getter_AddRefs(pathSpec));
    if (NS_SUCCEEDED(rv) && pathSpec)
      pathSpec->Truncate(mCopyState->m_curDstKey);
    ClearCopyState(PR_FALSE);
    return NS_OK;
  }
   
  nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;
  nsCOMPtr<nsIMsgWindow> msgWindow;
  PRBool multipleCopiesFinished = (mCopyState->m_curCopyIndex >= mCopyState->m_totalMsgCount);

  if (mCopyState->m_undoMsgTxn)
  {
		localUndoTxn = do_QueryInterface(mCopyState->m_undoMsgTxn, &rv);
    if (NS_SUCCEEDED(rv))
      localUndoTxn->GetMsgWindow(getter_AddRefs(msgWindow));
  }

  if (mCopyState)
  {
    NS_ASSERTION(mCopyState->m_leftOver == 0, "whoops, something wrong with previous copy");
    mCopyState->m_leftOver = 0; // reset to 0.
    // need to reset this in case we're move/copying multiple msgs.
    mCopyState->m_fromLineSeen = PR_FALSE;
    // flush the copied message.
    if (mCopyState->m_fileStream)
    {
      rv = mCopyState->m_fileStream->flush();
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }
	//Copy the header to the new database
	if(copySucceeded && mCopyState->m_message)
	{ //  CopyMessages() goes here; CopyFileMessage() never gets in here because
    //  the mCopyState->m_message will be always null for file message

    nsCOMPtr<nsIMsgDBHdr> newHdr;
    
    if(!mCopyState->m_parseMsgState)
    {
      nsCOMPtr<nsIMsgDatabase> msgDatabase;
      if(NS_SUCCEEDED(rv))
        rv = GetMsgDatabase(msgWindow, getter_AddRefs(msgDatabase));
      
      if(NS_SUCCEEDED(rv))
      {
        rv = mDatabase->CopyHdrFromExistingHdr(mCopyState->m_curDstKey,
                                               mCopyState->m_message, PR_TRUE,
                                               getter_AddRefs(newHdr));
        PRUint32 newHdrFlags;


        // turn off offline flag - it's not valid for local mail folders.
        if (newHdr)
          newHdr->AndFlags(~MSG_FLAG_OFFLINE, &newHdrFlags);
      }
    }
    PRBool isImap;
    if (NS_SUCCEEDED(rv) && localUndoTxn)
      localUndoTxn->GetSrcIsImap(&isImap);
    if (NS_SUCCEEDED(rv) && localUndoTxn && (!isImap || !mCopyState->m_copyingMultipleMessages))
    {
      nsMsgKey aKey;
      mCopyState->m_message->GetMessageKey(&aKey);
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
//	    msgDb->SetSummaryValid(PR_TRUE);
//	    msgDb->Commit(nsMsgDBCommitType::kLargeCommit);
    }
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
    rv = CopyMessageTo(aSupport, this, msgWindow, mCopyState->m_isMove);
  }
  else
  { // both CopyMessages() & CopyFileMessage() go here if they have
    // done copying operation; notify completion to copy service
    nsresult result;
    if(!mCopyState->m_isMove)
    {
      if (multipleCopiesFinished)
      {
        nsCOMPtr<nsIMsgFolder> srcFolder;
        srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
        if (mCopyState->m_isFolder)
          DoNextSubFolder(srcFolder, nsnull, nsnull);  //Copy all subfolders then notify completion
        
        nsCOMPtr<nsIMsgCopyService> copyService = 
                 do_GetService(kMsgCopyServiceCID, &result);
        
        if (mCopyState->m_msgWindow && mCopyState->m_undoMsgTxn)
        {
          nsCOMPtr<nsITransactionManager> txnMgr;
          mCopyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
          if (txnMgr)
            txnMgr->DoTransaction(mCopyState->m_undoMsgTxn);
        }
        if (srcFolder && !mCopyState->m_isFolder)
          srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);	 
        
        nsCOMPtr<nsISupports> srcSupport = do_QueryInterface(mCopyState->m_srcSupport);
        nsCOMPtr<nsIMsgCopyServiceListener> listener =do_QueryInterface(mCopyState->m_listener);
        
        if (!mCopyState->m_copyingMultipleMessages || multipleCopiesFinished)
          ClearCopyState(PR_TRUE);

        if (listener) //notify after clearing the copy state;
          listener->OnStopCopy(NS_OK);
        // OnStopCopy() should be called only once (listener is null when drag and drop msgs).
        else if (NS_SUCCEEDED(result))
          copyService->NotifyCompletion(srcSupport, this, rv);
        
      }
      
      // enable the dest folder
      EnableNotifications(allMessageCountNotifications, PR_TRUE);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndMove()
{

  nsresult result;
  
  if (mCopyState && mCopyState->m_curCopyIndex >= mCopyState->m_totalMsgCount)
  {
    
    nsCOMPtr <nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &result); 
    if (copyService && NS_SUCCEEDED(result))
    {
      //Notify that a completion finished.
      nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(mCopyState->m_srcSupport);
      if(srcFolder)
      {
        // lets delete these all at once - much faster that way
        result = srcFolder->DeleteMessages(mCopyState->m_messages, nsnull, PR_TRUE, PR_TRUE, nsnull, mCopyState->m_allowUndo);
        srcFolder->EnableNotifications(allMessageCountNotifications, PR_TRUE);
        srcFolder->NotifyFolderEvent(mDeleteOrMoveMsgCompletedAtom);
      }
      
      // enable the dest folder
      EnableNotifications(allMessageCountNotifications, PR_TRUE);
      
      if (mCopyState->m_msgWindow && mCopyState->m_undoMsgTxn)
      {
        nsCOMPtr<nsITransactionManager> txnMgr;
        mCopyState->m_msgWindow->GetTransactionManager(getter_AddRefs(txnMgr));
        if (txnMgr)
          txnMgr->DoTransaction(mCopyState->m_undoMsgTxn);
      }
    
      nsCOMPtr<nsISupports> srcSupport = do_QueryInterface(mCopyState->m_srcSupport);
      nsCOMPtr<nsIMsgCopyServiceListener> listener =do_QueryInterface(mCopyState->m_listener);
      ClearCopyState(PR_TRUE);  //clear the copy state so that the next message from a different folder can be moved
      if (listener) //notify after clearing the copy state;
        listener->OnStopCopy(NS_OK);
      // ### we don't know if it succeeded
      //passing in NS_OK because we only get in here if copy portion succeeded
    
      copyService->NotifyCompletion(srcSupport, this, NS_OK);
                        
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
                                             nsIMsgWindow *aMsgWindow, nsIMsgFolder *dstFolder,
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
	nsMsgKeyArray keyArray;
	PRUint32 numMessages = 0;
	messages->Count(&numMessages);
	for (PRUint32 i = 0; i < numMessages; i++)
	{
        nsCOMPtr<nsISupports> msgSupport;
		msgSupport = getter_AddRefs(messages->ElementAt(i));
		if (msgSupport)
		{
		  nsCOMPtr<nsIMsgDBHdr> aMessage = do_QueryInterface(msgSupport, &rv);
		  if(NS_SUCCEEDED(rv) && aMessage)
		  {
			  nsMsgKey key;
			  aMessage->GetMessageKey(&key);
			  keyArray.Add(key);
		  }
		}
	}
    keyArray.QuickSort();
    rv = SortMessagesBasedOnKey(messages, &keyArray, srcFolder);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIStreamListener>
      streamListener(do_QueryInterface(copyStreamListener));
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
                                             nsIMsgFolder *dstFolder,
                                             nsIMsgWindow *aMsgWindow,
                                             PRBool isMove)
{
  if (!mCopyState) return NS_ERROR_OUT_OF_MEMORY;

	nsresult rv;
	nsCOMPtr<nsIMsgDBHdr> msgHdr(do_QueryInterface(message));
	if(!msgHdr)
		return NS_ERROR_FAILURE;

  if (msgHdr)
    mCopyState->m_message = do_QueryInterface(msgHdr, &rv);

  nsCOMPtr<nsIMsgFolder> srcFolder(do_QueryInterface(mCopyState->m_srcSupport));
  if(!srcFolder)
	return NS_ERROR_NO_INTERFACE;
  nsXPIDLCString uri;
  srcFolder->GetUriForMsg(msgHdr, getter_Copies(uri));

	nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener; 
	rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID, NULL,
											NS_GET_IID(nsICopyMessageStreamListener),
											getter_AddRefs(copyStreamListener)); 
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsICopyMessageListener> copyListener(do_QueryInterface(dstFolder));
	if(!copyListener)
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

nsresult nsMsgLocalMailFolder::DeleteMsgsOnPop3Server(nsISupportsArray *messages)
{
	char		*uidl;
	char		*header = NULL;
	PRUint32		size = 0, len = 0, i = 0;
	nsCOMPtr <nsIMsgDBHdr> hdr;
	PRBool leaveOnServer = PR_FALSE;
	PRBool deleteMailLeftOnServer = PR_FALSE;
	PRBool changed = PR_FALSE;
	char *popData = nsnull;
  nsCOMPtr<nsIPop3IncomingServer> pop3MailServer;
  nsCOMPtr<nsIFileSpec> localPath;
  nsCOMPtr<nsIFileSpec> mailboxSpec;

  nsCOMPtr<nsIMsgIncomingServer> server;
  nsresult rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) 
    return rv;
  if (!server) 
    return NS_MSG_INVALID_OR_MISSING_SERVER;

  server->GetLocalPath(getter_AddRefs(localPath));
  pop3MailServer = do_QueryInterface(server, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  if (!pop3MailServer) 
    return NS_MSG_INVALID_OR_MISSING_SERVER;
	
  pop3MailServer->GetDeleteMailLeftOnServer(&deleteMailLeftOnServer);
	if (!deleteMailLeftOnServer)
		return NS_OK;

  pop3MailServer->GetLeaveMessagesOnServer(&leaveOnServer);
  rv = GetPath(getter_AddRefs(mailboxSpec));

  if (NS_FAILED(rv)) 
    return rv;

  rv = mailboxSpec->OpenStreamForReading();
  NS_ENSURE_SUCCESS(rv,rv);
  
  PRUint32 srcCount;
  messages->Count(&srcCount);

  nsXPIDLCString hostName;
  nsXPIDLCString userName;
  
  server->GetHostName(getter_Copies(hostName));
  server->GetUsername(getter_Copies(userName));

	header = (char*) PR_MALLOC(512);
	for (i = 0; header && (i < srcCount); i++)
	{
		/* get uidl for this message */
		uidl = nsnull;
    nsCOMPtr<nsISupports> aSupport =
        getter_AddRefs(messages->ElementAt(i));
    
    nsCOMPtr<nsIMsgDBHdr> msgDBHdr (do_QueryInterface(aSupport, &rv));;

    PRUint32 flags = 0;

		if (msgDBHdr && ((msgDBHdr->GetFlags(&flags), flags & MSG_FLAG_PARTIAL) || leaveOnServer))
		{
			len = 0;
      PRUint32 messageOffset;

      msgDBHdr->GetMessageOffset(&messageOffset);
			rv = mailboxSpec->Seek(messageOffset);
            NS_ENSURE_SUCCESS(rv,rv);
			msgDBHdr->GetMessageSize(&len);
            PRBool wasTruncated = PR_FALSE;
			while ((len > 0) && !uidl)
			{
				size = len;
				if (size > 512)
					size = 512;
				rv = mailboxSpec->ReadLine(&header, size, &wasTruncated);
                if (NS_SUCCEEDED(rv) && !wasTruncated)
				{
					size = strlen(header);
					if (!size)
						len = 0;
					else {
						len -= size;
						uidl = PL_strstr(header, X_UIDL);
					}
				} else
					len = 0;
			}
			if (uidl)
			{
				if (!popData)
					popData = ReadPopData(hostName, userName, localPath);
				uidl += X_UIDL_LEN + 2; // skip UIDL: header
				len = strlen(uidl);
				
				// Remove CR or LF at end of line
				char	*lastChar = uidl + len - 1;
				
				while ( (lastChar > uidl) && (*lastChar == nsCRT::LF || *lastChar == nsCRT::CR) ) {
					*lastChar = '\0';
					lastChar --;
				}

				net_pop3_delete_if_in_server(popData, uidl, &changed);
			}
		}
	}
	PR_FREEIF(header);
	if (popData)
	{
		if (changed)
			SavePopData(popData, localPath);
		KillPopData(popData);
		popData = nsnull;
	}
  mailboxSpec->CloseStream();
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

  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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

#ifdef HAVE_MOVEMAIL
  // next try "movemail"
  rv = accountManager->FindServer(userName,
                                  hostName,
                                  "movemail",
                                  getter_AddRefs(server));
  if (NS_SUCCEEDED(rv) && server) {
    mType = "movemail";
    return mType;
  }
#endif /* HAVE_MOVEMAIL */

  return "";
}

nsresult nsMsgLocalMailFolder::CreateBaseMessageURI(const char *aURI)
{
	nsresult rv;

	rv = nsCreateLocalBaseMessageURI(aURI, &mBaseMessageURI);
	return rv;

}

NS_IMETHODIMP
nsMsgLocalMailFolder::OnStartRunningUrl(nsIURI * aUrl)
{
  nsresult rv;
  nsCOMPtr<nsIPop3URL> popurl = do_QueryInterface(aUrl, &rv);
  if (NS_SUCCEEDED(rv))
  {
    nsXPIDLCString aSpec;
    aUrl->GetSpec(getter_Copies(aSpec));
    if (aSpec && PL_strstr((const char *) aSpec, "uidl="))
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
  if (NS_SUCCEEDED(aExitCode))
  {
    nsresult rv;
    nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;	
    nsCOMPtr<nsIMsgWindow> msgWindow;
    rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
    nsXPIDLCString aSpec;
    aUrl->GetSpec(getter_Copies(aSpec));
    if (PL_strstr(aSpec, "uidl="))
    {
      nsCOMPtr<nsIPop3URL> popurl = do_QueryInterface(aUrl, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsXPIDLCString messageuri;
        rv = popurl->GetMessageUri(getter_Copies(messageuri));
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIRDFService> rdfService = 
                   do_GetService(kRDFServiceCID, &rv); 
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
    if (mParsingInbox)
    {
      mParsingInbox = PR_FALSE;
      if (mCheckForNewMessagesAfterParsing)
      {
        if (msgWindow)
           rv = GetNewMessages(msgWindow, nsnull);
        mCheckForNewMessagesAfterParsing = PR_FALSE;
      }
    }
  }
  mParsingInbox = PR_FALSE;  //make sure we turn off this flag even if parsing fails or else it will be a deadlock
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
    setSubfolderFlag(kInboxName, MSG_FOLDER_FLAG_INBOX);

  if (flags & MSG_FOLDER_FLAG_SENTMAIL)
    setSubfolderFlag(kSentName, MSG_FOLDER_FLAG_SENTMAIL);
  
  if (flags & MSG_FOLDER_FLAG_DRAFTS)
    setSubfolderFlag(kDraftsName, MSG_FOLDER_FLAG_DRAFTS);

  if (flags & MSG_FOLDER_FLAG_TEMPLATES)
    setSubfolderFlag(kTemplatesName, MSG_FOLDER_FLAG_TEMPLATES);
  
  if (flags & MSG_FOLDER_FLAG_TRASH)
    setSubfolderFlag(kTrashName, MSG_FOLDER_FLAG_TRASH);

  if (flags & MSG_FOLDER_FLAG_QUEUE)
    setSubfolderFlag(kUnsentName, MSG_FOLDER_FLAG_QUEUE);
	
	return NS_OK;
}

nsresult
nsMsgLocalMailFolder::setSubfolderFlag(PRUnichar* aFolderName,
                                       PRUint32 flags)
{

  nsresult rv;

  nsCOMPtr<nsIFolder> folder;
	rv = FindSubFolder(NS_ConvertUCS2toUTF8(aFolderName).get(), getter_AddRefs(folder));
  
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
  
 	nsCOMPtr<nsIMsgFolder> msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
    
	rv = msgFolder->SetFlag(flags);
	if (NS_FAILED(rv)) return rv;

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
nsMsgLocalMailFolder::GetParsingInbox(PRBool *aParsingInbox)
{
  NS_ENSURE_ARG(aParsingInbox);
  *aParsingInbox = mParsingInbox;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::NotifyCompactCompleted()
{
  nsCOMPtr <nsIAtom> compactCompletedAtom;
  compactCompletedAtom = getter_AddRefs(NS_NewAtom("CompactCompleted"));
  NotifyFolderEvent(compactCompletedAtom);
  return NS_OK;
}


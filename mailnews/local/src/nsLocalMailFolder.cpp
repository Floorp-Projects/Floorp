/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#define NS_IMPL_IDS
#include "nsIPref.h"

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
#include "nsIFolderListener.h"
#include "nsIMsgMailSession.h"
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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID,		NS_MSGCOPYSERVICE_CID);

//////////////////////////////////////////////////////////////////////////////
// nsLocal
/////////////////////////////////////////////////////////////////////////////

nsLocalMailCopyState::nsLocalMailCopyState() :
  m_fileStream(nsnull), m_curDstKey(0xffffffff), m_curCopyIndex(0),
  m_messageService(nsnull), m_totalMsgCount(0), m_isMove(PR_FALSE),
  m_dummyEnvelopeNeeded(PR_FALSE)
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
      char* uri;
      msgNode->GetValue(&uri);
      if (uri)
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
    mInitialized(PR_FALSE), mCopyState(nsnull)
{
	mPath = nsnull;
//  NS_INIT_REFCNT(); done by superclass
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
	if (mPath)
		delete mPath;
}

NS_IMPL_ADDREF_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgLocalMailFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgLocalMailFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsIMsgLocalMailFolder::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMsgLocalMailFolder*, this);
	}              
	else if(aIID.Equals(nsICopyMessageListener::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsICopyMessageListener*, this);
	}

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgFolder::QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////

static PRBool
nsShouldIgnoreFile(nsString& name)
{
  PRUnichar firstChar=name.CharAt(0);
  if (firstChar == '.' || firstChar == '#' || name.CharAt(name.Length() - 1) == '~')
    return PR_TRUE;

  if (name.EqualsIgnoreCase("rules.dat"))
    return PR_TRUE;

  PRInt32 len = name.Length();

  // don't add summary files to the list of folders;
  // don't add popstate files to the list either, or rules (sort.dat). 
  if ((len > 4 && name.RFind(".snm", PR_TRUE) == len - 4) ||
      name.EqualsIgnoreCase("popstate.dat") ||
      name.EqualsIgnoreCase("sort.dat") ||
      name.EqualsIgnoreCase("mailfilt.log") ||
      name.EqualsIgnoreCase("filters.js") ||
      name.RFind(".toc", PR_TRUE) == len - 4)
    return PR_TRUE;

  if ((len > 4 && name.RFind(".sbd", PR_TRUE) == len - 4) ||
		(len > 4 && name.RFind(".msf", PR_TRUE) == len - 4))
	  return PR_TRUE;
  return PR_FALSE;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::Init(const char* aURI)
{
  nsresult rv;
  rv = nsMsgDBFolder::Init(aURI);
  if (NS_FAILED(rv)) return rv;


#if 0
  // find the server from the account manager
  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupportsArray> matchingServers;
  accountManager->FindServersByHostname();
#endif

  return rv;

}
  
nsresult
nsMsgLocalMailFolder::CreateSubFolders(nsFileSpec &path)
{
	nsresult rv = NS_OK;
	nsAutoString currentFolderNameStr;
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

		AddSubfolder(currentFolderNameStr, getter_AddRefs(child));
		PL_strfree(folderName);
    }
	return rv;
}

nsresult nsMsgLocalMailFolder::AddSubfolder(nsAutoString name, nsIMsgFolder **child)
{
	if(!child)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  
	if(NS_FAILED(rv)) return rv;

	nsAutoString uri (eOneByte);
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri.GetBuffer(), getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;

	folder->SetFlag(MSG_FOLDER_FLAG_MAIL);

	folder->SetDepth(mDepth + 1);
	if(name.Compare("Inbox", PR_TRUE) == 0)
	{
		folder->SetFlag(MSG_FOLDER_FLAG_INBOX);
		mBiffState = nsMsgBiffState_Unknown;
	}
	else if(name.Compare("Trash", PR_TRUE) == 0)
		folder->SetFlag(MSG_FOLDER_FLAG_TRASH);
	else if(name.Compare("Unsent Messages", PR_TRUE) == 0 
		|| name.Compare("Outbox", PR_TRUE) == 0)
		folder->SetFlag(MSG_FOLDER_FLAG_QUEUE);
	//These should probably be read in from a preference.  Hacking in here for the moment.
	else if(name.Compare("Sent", PR_TRUE) == 0)
		folder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
	else if(name.Compare("Drafts", PR_TRUE) == 0)
		folder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
	else if(name.Compare("Templates", PR_TRUE) == 0)
		folder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);

	nsCOMPtr<nsISupports> supports = do_QueryInterface(folder);
	if(folder)
		mSubFolders->AppendElement(supports);
	folder->SetParent(this);
	*child = folder;
	NS_ADDREF(*child);

	return rv;
}


//run the url to parse the mailbox
nsresult nsMsgLocalMailFolder::ParseFolder(nsFileSpec& path)
{
	nsresult rv = NS_OK;

	NS_WITH_SERVICE(nsIMailboxService, mailboxService,
                  kMailboxServiceCID, &rv);
  
	if (NS_FAILED(rv)) return rv; 
	nsMsgMailboxParser *parser = new nsMsgMailboxParser;
	if(!parser)
		return NS_ERROR_OUT_OF_MEMORY;
  
	rv = mailboxService->ParseMailbox(path, parser, nsnull, nsnull);

	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::Enumerate(nsIEnumerator* *result)
{
  nsresult rv; 

  // local mail folders contain both messages and folders:
  nsCOMPtr<nsIEnumerator> folders;
  nsCOMPtr<nsIEnumerator> messages;
  rv = GetSubFolders(getter_AddRefs(folders));
  if (NS_FAILED(rv)) return rv;
  rv = GetMessages(getter_AddRefs(messages));
  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
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
  if (!mInitialized) {
    nsCOMPtr<nsIFileSpec> pathSpec;
    nsresult rv = GetPath(getter_AddRefs(pathSpec));
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
      if (mDepth == 0)
      {
        // make sure we have all the default mailbox created
        nsFileSpec fileSpec;
        fileSpec = path + "Inbox";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
        fileSpec = path + "Trash";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
        fileSpec = path + "Sent";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
        fileSpec = path + "Drafts";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
        fileSpec = path + "Templates";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
        fileSpec = path + "Unsent Messages";
        if (!fileSpec.Exists()) { nsOutputFileStream outStream(fileSpec); }
      }
      rv = CreateSubFolders(path);
    }
    UpdateSummaryTotals(PR_FALSE);

    if (NS_FAILED(rv)) return rv;
    mInitialized = PR_TRUE;      // XXX do this on failure too?
  }
  return mSubFolders->Enumerate(result);
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

NS_IMETHODIMP
nsMsgLocalMailFolder::GetMsgDatabase(nsIMsgDatabase** aMsgDatabase)
{
  GetDatabase();
  return nsMsgDBFolder::GetMsgDatabase(aMsgDatabase);
}

//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsMsgLocalMailFolder::GetDatabase()
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

		rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(mailDBFactory));
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

			mDatabase->AddListener(this);

			// if we have to regenerate the folder, run the parser url.
			if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
			{
				if(NS_FAILED(rv = ParseFolder(path)))
					return rv;
				else
					return NS_ERROR_NOT_INITIALIZED;
			}
			else
			{
				//Otherwise we have a valid database so lets extract necessary info.
				UpdateSummaryTotals(PR_TRUE);
			}
		}
	}
	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetMessages(nsIEnumerator* *result)
{
	nsresult rv = GetDatabase();

	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIEnumerator> msgHdrEnumerator;
		nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
		rv = mDatabase->EnumerateMessages(getter_AddRefs(msgHdrEnumerator));
		if(NS_SUCCEEDED(rv))
			rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator,
												   this, &messageEnumerator);
		*result = messageEnumerator;
	}
	return rv;
}



NS_IMETHODIMP nsMsgLocalMailFolder::BuildFolderURL(char **url)
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

	nsAutoString tmpPath((nsFilePath)path, eOneByte);
	*url = PR_smprintf("%s%s", urlScheme, tmpPath.GetBuffer());
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

NS_IMETHODIMP nsMsgLocalMailFolder::CreateSubfolder(const char *folderName)
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

	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(mailDBFactory));
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
        nsIMsgDatabase *unusedDB = NULL;
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(path, getter_AddRefs(dbFileSpec));
		rv = mailDBFactory->Open(dbFileSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) &unusedDB);

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
			rv = AddSubfolder(folderName, getter_AddRefs(child));
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
		nsCOMPtr<nsISupports> folderSupports(do_QueryInterface(child, &rv));

		if(NS_SUCCEEDED(rv))
		{
			NotifyItemAdded(folderSupports);
		}
	}
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Delete()
{
	nsresult rv = GetDatabase();

	if(NS_SUCCEEDED(rv))
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

	//Clean up .sbd folder if it exists.
	if(NS_SUCCEEDED(rv))
	{
		nsLocalFolderSummarySpec	summarySpec(path);
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
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Rename(const char *newName)
{
#ifdef HAVE_PORT
  // change the leaf name (stored separately)
  nsresult status = MSG_FolderInfo::Rename (newUserLeafName);
  if (status == 0) {
    char *baseDir = XP_STRDUP(m_pathName);
    if (baseDir) {
      char *base_slash = XP_STRRCHR (baseDir, '/'); 
      if (base_slash)
        *base_slash = '\0';
    }

    char *leafNameForDisk = CreatePlatformLeafNameForDisk(newUserLeafName,m_master, baseDir);
    if (!leafNameForDisk)
      status = MK_OUT_OF_MEMORY;

    if (0 == status) {
      // calculate the new path name
      char *newPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + 1);
      XP_STRCPY (newPath, m_pathName);
      char *slash = XP_STRRCHR (newPath, '/'); 
      if (slash)
        XP_STRCPY (slash + 1, leafNameForDisk);

      // rename the mail summary file, if there is one
      nsMsgDatabase *db = NULL;
      status = CloseDatabase (m_pathName, &db);
          
      XP_StatStruct fileStat;
      if (!XP_Stat(m_pathName, &fileStat, xpMailFolderSummary))
        status = XP_FileRename(m_pathName, xpMailFolderSummary, newPath, xpMailFolderSummary);
      if (0 == status) {
        if (db) {
          if (ReopenDatabase (db, newPath) == 0) {  
            //need to set mailbox name
          }
        }
        else {
          MailDB *mailDb = NULL;
          MailDB::Open(newPath, TRUE, &mailDb, TRUE);
          if (mailDb) {
            //need to set mailbox name
            mailDb->Close();
          }
        }
      }

      // rename the mail folder file, if its local
      if ((status == 0) && (GetType() == FOLDER_MAIL))
        status = XP_FileRename (m_pathName, xpMailFolder, newPath, xpMailFolder);

      if (status == 0) {
        // rename the subdirectory if there is one
        if (m_subFolders->GetSize() > 0)
          status = XP_FileRename (m_pathName, xpMailSubdirectory, newPath, xpMailSubdirectory);

        // tell all our children about the new pathname
        if (status == 0) {
          int startingAt = XP_STRLEN (newPath) - XP_STRLEN (leafNameForDisk) + 1; // add one for trailing '/'
          status = PropagateRename (leafNameForDisk, startingAt);
        }
      }
    }
    FREEIF(baseDir);
  }
  return status;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos)
{
#ifdef HAVE_PORT
  nsresult err = NS_OK;
  XP_ASSERT (srcFolder->GetType() == GetType());  // we can only adopt the same type of folder
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
    XP_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
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

nsresult  nsMsgLocalMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult openErr=NS_ERROR_UNEXPECTED;
    if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;	//ducarroz: should we use NS_ERROR_INVALID_ARG?

	if (!mPath)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIMsgDatabase> mailDBFactory;
	nsCOMPtr<nsIMsgDatabase> mailDB;

	nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(mailDBFactory));
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(*mPath, getter_AddRefs(dbFileSpec));
		openErr = mailDBFactory->Open(dbFileSpec, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) &mailDB);
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
		char *oldTotalMessagesStr = PR_smprintf("%d", oldTotalMessages);
		char *totalMessagesStr = PR_smprintf("%d",mNumTotalMessages);
		NotifyPropertyChanged("TotalMessages", oldTotalMessagesStr, totalMessagesStr);
		PR_smprintf_free(totalMessagesStr);
		PR_smprintf_free(oldTotalMessagesStr);
	}

	if(oldUnreadMessages != mNumUnreadMessages)
	{
		char *oldUnreadMessagesStr = PR_smprintf("%d", oldUnreadMessages);
		char *totalUnreadMessages = PR_smprintf("%d",mNumUnreadMessages);
		NotifyPropertyChanged("TotalUnreadMessages", oldUnreadMessagesStr, totalUnreadMessages);
		PR_smprintf_free(totalUnreadMessages);
		PR_smprintf_free(oldUnreadMessagesStr);
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

  // These are specified in the "Mail/News Windows" UI spec

  if (mFlags & MSG_FOLDER_FLAG_TRASH)
  {
    PRBool moveToTrash;
    GetDeleteIsMoveToTrash(&moveToTrash);
    if(moveToTrash)
      *deletable = PR_TRUE;  // allow delete of trash if we don't use trash
  }
  else if (mDepth == 1)
    *deletable = PR_FALSE;
  else if (mFlags & MSG_FOLDER_FLAG_INBOX || 
    mFlags & MSG_FOLDER_FLAG_DRAFTS || 
    mFlags & MSG_FOLDER_FLAG_TRASH ||
    mFlags & MSG_FOLDER_FLAG_TEMPLATES)
    *deletable = PR_FALSE;
  else *deletable =  PR_TRUE;

  return NS_OK;
}
 
NS_IMETHODIMP nsMsgLocalMailFolder::GetCanCreateChildren(PRBool *canCreateChildren)
{  
  if(!canCreateChildren)
    return NS_ERROR_NULL_POINTER;

  *canCreateChildren = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetCanBeRenamed(PRBool *canBeRenamed)
{
  if(!canBeRenamed)
    return NS_ERROR_NULL_POINTER;

    // The root mail folder can't be renamed
  if (mDepth < 2)
    *canBeRenamed = PR_FALSE;

  // Here's a weird case necessitated because we don't have a separate
  // preference for any folder name except the FCC folder (Sent). Others
  // are known by name, and as such, can't be renamed. I guess.
  else if (mFlags & MSG_FOLDER_FLAG_TRASH ||
      mFlags & MSG_FOLDER_FLAG_DRAFTS ||
      mFlags & MSG_FOLDER_FLAG_QUEUE ||
      mFlags & MSG_FOLDER_FLAG_INBOX ||
      mFlags & MSG_FOLDER_FLAG_TEMPLATES)
      *canBeRenamed = PR_FALSE;
  else 
    *canBeRenamed = PR_TRUE;

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
  return FALSE;
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetUsername(char** userName)
{
  return nsGetMailboxUserName(kMailboxRootURI, mURI, userName);
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetHostname(char** hostName)
{
	nsresult rv;
	char *host;
	rv = nsGetMailboxHostName(kMailboxRootURI, mURI, &host);
	//I'm recopying it because otherwise we'll have a free mismatched memory.
	//We should really be using allocators to do all of this.
	if(NS_SUCCEEDED(rv) && host)
	{
        *hostName = PL_strdup(host);
		PL_strfree(host);
		if(!*hostName)
			return NS_ERROR_OUT_OF_MEMORY;
	}
	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
{
#ifdef HAVE_PORT
    PRBool ret = FALSE;
  if (m_master->IsCachePasswordProtected() && !m_master->IsUserAuthenticated() && !m_master->AreLocalFoldersAuthenticated())
  {
    char *savedPassword = GetRememberedPassword();
    if (savedPassword && XP_STRLEN(savedPassword))
      ret = TRUE;
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
  MailDB::Open(m_pathName, TRUE, &mailDb);
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
      MailDB::Open(m_pathName, FALSE, &mailDb, FALSE);
      if (mailDb)
      {
        mailDb->GetCachedPassword(cachedPassword);
        retPassword = XP_STRDUP(cachedPassword);
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetPath(nsIFileSpec ** aPathName)
{
	nsresult rv;
  if (! mPath) {
  	mPath = new nsFileSpec("");
  	if (! mPath)
  		return NS_ERROR_OUT_OF_MEMORY;
  
    rv = nsLocalURI2Path(kMailboxRootURI, mURI, *mPath);
    if (NS_FAILED(rv)) return rv;
  }
  rv = NS_NewFileSpecWithSpec(*mPath, aPathName);

  return rv;
}

// OK, this is kind of silly, but for now, we'll just tack the subFolderName
// onto our URI, and ask RDF to find it for us.
NS_IMETHODIMP 
nsMsgLocalMailFolder::FindSubFolder(const char *subFolderName, nsIFolder **aFolder)
{
	return nsMsgFolder::FindSubFolder(subFolderName, aFolder);

/*	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  
	if(NS_FAILED(rv)) 
		return rv;

	nsCString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(subFolderName);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uri.GetBuffer(), getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;
	if (aFolder)
	{
		*aFolder = folder;
		NS_ADDREF(*aFolder);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
*/
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
			rv = rootFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, result,
                                          1, &numFolders);
#if 0
      // ** jt -- This shouldn't be needed. The OS should prevent
      // user from deleting the trash folder while we are running the app.
      // The only thing we need to do is to make sure that we have all
      // the default mailboxes created when startup.
      if (NS_FAILED(rv))
      {
        nsCOMPtr<nsIFileSpec> pathSpec;
        nsFileSpec fileSpec;
        rv = rootFolder->GetPath(getter_AddRefs(pathSpec));
        if (NS_SUCCEEDED(rv))
        {
          rv = pathSpec->GetFileSpec(&fileSpec);
          if (NS_SUCCEEDED(rv))
          {
            nsFileSpec trashSpec = fileSpec + "Trash";
            nsOutputFileStream trashStream(fileSpec);
          }
        }
      }
#endif
    }
    return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::DeleteMessages(nsISupportsArray *messages,
                                     nsITransactionManager *txnMgr, 
                                     PRBool deleteStorage)
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
                                        PR_TRUE, nsnull, txnMgr);
      }
      return rv;
  }
  else
  {
      if (txnMgr) SetTransactionManager(txnMgr);
  	
      rv = GetDatabase();
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
                  DeleteMessage(message, txnMgr, PR_TRUE);
              }
          }
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

	IsLocked(&isLocked);
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
                                   nsITransactionManager* txnMgr,
                                   nsIMsgCopyServiceListener* listener)
{
  if (!srcFolder || !messages)
    return NS_ERROR_NULL_POINTER;
  if (txnMgr) SetTransactionManager(txnMgr);

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
      rv = GetMsgDatabase(getter_AddRefs(msgDb));
      if (msgDb)
        parseMsgState->SetMailDB(msgDb);
    }
  }

  // undo stuff
  nsLocalMoveCopyMsgTxn* msgTxn = nsnull;

  msgTxn = new nsLocalMoveCopyMsgTxn(srcFolder, this, isMove);

  if (msgTxn)
    rv =
      msgTxn->QueryInterface(nsCOMTypeInfo<nsLocalMoveCopyMsgTxn>::GetIID(),
                             getter_AddRefs(mCopyState->m_undoMsgTxn));
  else
    rv = NS_ERROR_OUT_OF_MEMORY;
  
  if (NS_FAILED(rv))
  {
    ClearCopyState();
  }
  else
  {
    nsCOMPtr<nsISupports> msgSupport;
    msgSupport = getter_AddRefs(messages->ElementAt(0));
    if (msgSupport)
    {
      nsCOMPtr<nsIMessage> aMessage;
      aMessage = do_QueryInterface(msgSupport, &rv);
      if(NS_SUCCEEDED(rv))
        rv = CopyMessageTo(aMessage, this, isMove);
      else
        ClearCopyState();
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::CopyFileMessage(nsIFileSpec* fileSpec, nsIMessage*
                                      msgToReplace, PRBool isDraftOrTemplate,
                                      nsITransactionManager* txnMgr,
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
    rv = GetMsgDatabase(getter_AddRefs(msgDb));
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

  rv = inputStream->GetLength(&fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = BeginCopy(nsnull);
  if (NS_FAILED(rv)) goto done;
  rv = CopyData(inputStream, (PRInt32) fileSize);
  if (NS_FAILED(rv)) goto done;
  rv = EndCopy(PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  if (msgToReplace)
  {
    rv = DeleteMessage(msgToReplace, txnMgr, PR_TRUE);
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
                                             nsITransactionManager *txnMgr,
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

	char* msgURI = nsnull;
	nsMsgKey key;
    nsCOMPtr<nsIRDFResource> resource;

	rv = msgDBHdr->GetMessageKey(&key);
  
	if(NS_SUCCEEDED(rv))
		rv = nsBuildLocalMessageURI(mURI, key, &msgURI);
  
	if(NS_SUCCEEDED(rv))
	{
		rv = rdfService->GetResource(msgURI, getter_AddRefs(resource));
    }
	if(msgURI)
		PR_smprintf_free(msgURI);

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

NS_IMETHODIMP nsMsgLocalMailFolder::GetNewMessages()
{
    nsresult rv;
    
    NS_WITH_SERVICE(nsIPop3Service, pop3Service, kCPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));

    nsCOMPtr<nsIPop3IncomingServer> popServer;
    rv = server->QueryInterface(nsIPop3IncomingServer::GetIID(),
                                (void **)&popServer);
    if (NS_SUCCEEDED(rv)) {
        rv = pop3Service->GetNewMail(nsnull,popServer,nsnull);
    }
    

	return rv;
}


//nsICopyMessageListener
NS_IMETHODIMP nsMsgLocalMailFolder::BeginCopy(nsIMessage *message)
{
  if (!mCopyState) return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);

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
    result.Append(CRLF);

    // *** jt - hard code status line for now; come back later

    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
    result = "X-Mozilla-Status: 0001" CRLF;
    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
    result = "X-Mozilla-Status2: 00000000" CRLF;
    *(mCopyState->m_fileStream) << result.GetBuffer();
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(
          result.GetBuffer(), result.Length());
  }

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

	PRUint32 readCount, maxReadCount = FOUR_K -1;
  mCopyState->m_fileStream->seek(PR_SEEK_END, 0);

  while (aLength > 0)
  {
    if (aLength < (PRInt32) maxReadCount)
      maxReadCount = aLength;
    
    rv = aIStream->Read(mCopyState->m_dataBuffer, maxReadCount, &readCount);
    mCopyState->m_dataBuffer[readCount] ='\0';
    *(mCopyState->m_fileStream) << mCopyState->m_dataBuffer;

    // CopyFileMessage() and CopyMessages() from servers other than mailbox
    if (mCopyState->m_parseMsgState)
    {
      char* start = mCopyState->m_dataBuffer;
      char* end = PL_strstr(mCopyState->m_dataBuffer, CRLF);
      
      while (start && end)
      {
        // +2 counting for the CRLF
        mCopyState->m_parseMsgState->ParseAFolderLine(start, end-start+2);
        start = end+2;
        if (start >= &mCopyState->m_dataBuffer[readCount])
          break;
        end = PL_strstr(start, CRLF);
      }
    }

    aLength -= readCount;
  }

	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndCopy(PRBool copySucceeded)
{
  nsresult rv = copySucceeded ? NS_OK : NS_ERROR_FAILURE;
  nsCOMPtr<nsLocalMoveCopyMsgTxn> localUndoTxn;

  if (mCopyState->m_undoMsgTxn)
    rv = mCopyState->m_undoMsgTxn->QueryInterface(
      nsCOMTypeInfo<nsLocalMoveCopyMsgTxn>::GetIID(),
      getter_AddRefs(localUndoTxn));
  
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
      if(NS_SUCCEEDED(rv))
        rv = GetDatabase();
      
      if(NS_SUCCEEDED(rv))
      {
        rv = mDatabase->CopyHdrFromExistingHdr(mCopyState->m_curDstKey,
                                               msgDBHdr, 
                                               getter_AddRefs(newHdr));
        mDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
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
    *(mCopyState->m_fileStream) << CRLF;
    if (mCopyState->m_parseMsgState)
        mCopyState->m_parseMsgState->ParseAFolderLine(CRLF, 2);
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
      result = GetMsgDatabase(getter_AddRefs(msgDb));
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
        msgDb->Commit(nsMsgDBCommitType::kLargeCommit);
      }
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
    NS_WITH_SERVICE(nsIMsgCopyService, copyService, 
                    kMsgCopyServiceCID, &result); 

    if (NS_SUCCEEDED(result))
      copyService->NotifyCompletion(mCopyState->m_srcSupport, this, rv);

    if (mTxnMgr && NS_SUCCEEDED(rv) && mCopyState->m_undoMsgTxn)
      mTxnMgr->Do(mCopyState->m_undoMsgTxn);
    
    ClearCopyState();
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

	char *uri;
	messageNode->GetValue(&uri);

	nsCOMPtr<nsICopyMessageStreamListener> copyStreamListener; 
	rv = nsComponentManager::CreateInstance(kCopyMessageStreamListenerCID, NULL,
											nsICopyMessageStreamListener::GetIID(),
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


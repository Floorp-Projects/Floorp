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


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);


////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(void)
  : mExpungedBytes(0), 
    mHaveReadNameFromDB(PR_FALSE), mGettingMail(PR_FALSE),
    mInitialized(PR_FALSE)
{
	mPath = nsnull;
//  NS_INIT_REFCNT(); done by superclass
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
	if (mPath)
		delete mPath;
	if (mDatabase)
		mDatabase->RemoveListener(this);
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
  rv = nsRDFResource::Init(aURI);
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
	for (nsDirectoryIterator dir(path); dir.Exists(); dir++) {
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

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
	{
		delete[] uriStr;
		return rv;
	}

	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
	{
		delete[] uriStr;
		return rv;
	}

	delete[] uriStr;
	folder->SetFlag(MSG_FOLDER_FLAG_MAIL);

	if(name == "Inbox")
		folder->SetFlag(MSG_FOLDER_FLAG_INBOX);
	else if(name == "Trash")
		folder->SetFlag(MSG_FOLDER_FLAG_TRASH);
	else if(name == "Unsent Messages" || name == "Outbox")
		folder->SetFlag(MSG_FOLDER_FLAG_QUEUE);
  
	mSubFolders->AppendElement(folder);
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
    nsFileSpec path;
    nsresult rv = GetPath(path);
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
      rv = CreateSubFolders(path);
    }
    else {
      UpdateSummaryTotals();
      // Look for a directory for this mail folder, and recurse into it.
      // e.g. if the folder is "inbox", look for "inbox.sbd". 
    }

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

//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsMsgLocalMailFolder::GetDatabase()
{
	if (!mDatabase)
	{
		nsNativeFileSpec path;
		nsresult rv = GetPath(path);
		if (NS_FAILED(rv)) return rv;

		nsresult folderOpen = NS_OK;
		nsCOMPtr<nsIMsgDatabase> mailDBFactory;

		rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(mailDBFactory));
		if (NS_SUCCEEDED(rv) && mailDBFactory)
		{
			folderOpen = mailDBFactory->Open(path, PR_TRUE, getter_AddRefs(mDatabase), PR_FALSE);
			if(!NS_SUCCEEDED(folderOpen) &&
				folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
			{
				// if it's out of date then reopen with upgrade.
				if(!NS_SUCCEEDED(rv = mailDBFactory->Open(path, PR_TRUE, getter_AddRefs(mDatabase), PR_TRUE)))
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
				UpdateSummaryTotals();
			}
		}
	}
	return NS_OK;
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

  nsFileSpec path;
  nsresult rv = GetPath(path);
  if (NS_FAILED(rv)) return rv;
  nsAutoString tmpPath((nsFilePath)path);
  *url = PR_smprintf("%s%s", urlScheme, (const char*)nsAutoCString(tmpPath));
  return NS_OK;

}

/* Finds the directory associated with this folder.  That is if the path is
   c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
   currently exist then it will create it
  */
nsresult nsMsgLocalMailFolder::CreateDirectoryForFolder(nsFileSpec &path)
{
	nsresult rv = NS_OK;

	rv = GetPath(path);
	if(NS_FAILED(rv))
		return rv;

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
		rv = mailDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &unusedDB, PR_TRUE);

        if (NS_SUCCEEDED(rv) && unusedDB)
        {
			//need to set the folder name
			nsAutoString folderNameStr(folderName);
			nsCOMPtr<nsIDBFolderInfo> folderInfo;
			rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
			if(NS_SUCCEEDED(rv))
			{
				folderInfo->SetMailboxName(folderNameStr);
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

NS_IMETHODIMP nsMsgLocalMailFolder::RemoveSubFolder(nsIMsgFolder *which)
{
#if 0
  // Let the base class do list management
  nsMsgFolder::RemoveSubFolder(which);
#endif

  // Derived class is responsible for managing the subdirectory
#ifdef HAVE_PORT
  if (0 == m_subFolders->GetSize())
    XP_RemoveDirectory (m_pathName, xpMailSubdirectory);
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Delete()
{
#ifdef HAVE_PORT
  nsMsgDatabase   *db;
  // remove the summary file
  nsresult status = CloseDatabase (m_pathName, &db);
  if (0 == status) {
    if (db != NULL)
      db->Close();    // decrement ref count, so it will leave cache
    XP_FileRemove (m_pathName, xpMailFolderSummary);
  }

  if (0 == status) {
    // remove the mail folder file
    status = XP_FileRemove (m_pathName, xpMailFolder);

    // if the delete seems to have failed, but the file doesn't
    // exist, that's not really an error condition, is it now?
    if (status) {
      XP_StatStruct fileStat;
      if (0 == XP_Stat(m_pathName, &fileStat, xpMailFolder))
        status = 0;
    }
  }

  if (0 != status)
    status = MK_UNABLE_TO_DELETE_FILE;
  return status;  
#endif
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

NS_IMETHODIMP 
nsMsgLocalMailFolder::GetChildNamed(const char *name, nsISupports ** aChild)
{
	NS_ASSERTION(aChild, "NULL child");
	nsresult rv;
	// will return nsnull if we can't find it
	*aChild = nsnull;

	nsCOMPtr<nsIMsgFolder> folder;

	PRUint32 count = mSubFolders->Count();

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubFolders->ElementAt(i));
		folder = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv))
		{
			char *folderName;

			folder->GetName(&folderName);
			// case-insensitive compare is probably LCD across OS filesystems
			if (folderName && PL_strcasecmp(name, folderName)!=0)
			{
				*aChild = folder;
				delete[] folderName;
				return NS_OK;
			}
		delete[] folderName;
		}
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetName(char **name)
{
  if(!name)
    return NS_ERROR_NULL_POINTER;

  if (!mHaveReadNameFromDB)
  {
    if (mDepth == 1) 
    {
      SetName("Local Mail");
      mHaveReadNameFromDB = TRUE;
      *name = mName.ToNewCString();
	  if(!(*name))
		  return NS_ERROR_OUT_OF_MEMORY;
      return NS_OK;
    }
    else
    {
      //Need to read the name from the database
    }
  }
	nsAutoString folderName;
	nsLocalURI2Name(kMailboxRootURI, mURI, folderName);
	*name = folderName.ToNewCString();

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(char ** prettyName)
{
  if (mDepth == 1) {
    // Depth == 1 means we are on the mail server level
    // override the name here to say "Local Mail"
    *prettyName = PL_strdup("Local Mail");
	if(!(*prettyName))
		return NS_ERROR_OUT_OF_MEMORY;
  }
  else
    return nsMsgFolder::GetPrettyName(prettyName);

  return NS_OK;
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
		openErr = mailDBFactory->Open(*mPath, PR_FALSE, (nsIMsgDatabase **) &mailDB, PR_FALSE);
	}

    *db = mailDB;
	NS_IF_ADDREF(*db);
    if (NS_SUCCEEDED(openErr)&& *db)
        openErr = (*db)->GetDBFolderInfo(folderInfo);
    return openErr;
}

NS_IMETHODIMP nsMsgLocalMailFolder::UpdateSummaryTotals()
{
	PRInt32 oldUnreadMessages = mNumUnreadMessages;
	PRInt32 oldTotalMessages = mNumTotalMessages;
	//We need to read this info from the database
	ReadDBFolderInfo(PR_TRUE);

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

NS_IMETHODIMP nsMsgLocalMailFolder::GetUsersName(char** userName)
{
#ifdef HAVE_PORT
  return NET_GetPopUsername();
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetHostName(char** hostName)
{
#ifdef HAVE_PORT
  PRBool serverIsIMAP = m_master->GetPrefs()->GetMailServerIsIMAP4();
  if (serverIsIMAP)
  {
    MSG_IMAPHost *defaultIMAPHost = m_master->GetIMAPHostTable()->GetDefaultHost();
    return (defaultIMAPHost) ? defaultIMAPHost->GetHostName() : 0;
  }
  else
    return m_master->GetPrefs()->GetPopHost();
#endif
  return NS_OK;
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetPath(nsFileSpec& aPathName)
{
  if (! mPath) {
  	mPath = new nsNativeFileSpec("");
  	if (! mPath)
  		return NS_ERROR_OUT_OF_MEMORY;
  
    nsresult rv = nsLocalURI2Path(kMailboxRootURI, mURI, *mPath);
    if (NS_FAILED(rv)) return rv;
  }
  aPathName = *mPath;
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::DeleteMessages(nsISupportsArray *messages)
{
	nsresult rv = GetDatabase();
	if(NS_SUCCEEDED(rv))
	{
		PRUint32 messageCount = messages->Count();
		for(PRUint32 i = 0; i < messageCount; i++)
		{
			nsCOMPtr<nsISupports> msgSupports = getter_AddRefs(messages->ElementAt(i));
			nsCOMPtr<nsIMessage> message(do_QueryInterface(msgSupports));
			if(message)
			{
				DeleteMessage(message);
			}
		}
	}
	return rv;
}

nsresult nsMsgLocalMailFolder::DeleteMessage(nsIMessage *message)
{
	nsresult rv;
	nsCOMPtr <nsIMsgDBHdr> msgDBHdr;
	nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(message, &rv));

	if(NS_SUCCEEDED(rv))
	{
		rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));
		if(NS_SUCCEEDED(rv))
		{
			rv =mDatabase->DeleteHeader(msgDBHdr, nsnull, PR_TRUE, PR_TRUE);
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
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    NS_WITH_SERVICE(nsIPop3Service, pop3Service, kCPop3ServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

	//Are we assured this is the server for this folder?
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = mailSession->GetCurrentServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;

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

	PRBool isLocked;
	IsLocked(&isLocked);
	if(!isLocked)
		AcquireSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));
	else
		return NS_MSG_FOLDER_BUSY;

	nsFileSpec path;
	GetPath(path);
	mCopyState = new nsLocalMailCopyState;
	if(!mCopyState)
		return NS_ERROR_OUT_OF_MEMORY;
	//Before we continue we should verify that there is enough diskspace.
	//XXX How do we do this?
	mCopyState->fileStream = new nsOutputFileStream(path, PR_WRONLY | PR_CREATE_FILE);
	if(!mCopyState->fileStream)
		return NS_ERROR_OUT_OF_MEMORY;
	//The new key is the end of the file
	mCopyState->fileStream->seek(PR_SEEK_END, 0);
	mCopyState->dstKey = mCopyState->fileStream->tell();

	mCopyState->message = dont_QueryInterface(message);
	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::CopyData(nsIInputStream *aIStream, PRInt32 aLength)
{
	//check to make sure we have control of the write.
    PRBool haveSemaphore;
	nsresult rv = NS_OK;

	rv = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this), &haveSemaphore);
	if(NS_FAILED(rv))
		return rv;
	if(!haveSemaphore)
		return NS_MSG_FOLDER_BUSY;

	char *data = (char*)PR_MALLOC(aLength + 1);

	if(!data)
		return NS_ERROR_OUT_OF_MEMORY;

	PRUint32 readCount;
	rv = aIStream->Read(data, aLength, &readCount);
	data[readCount] ='\0';
	mCopyState->fileStream->seek(PR_SEEK_END, 0);
	*(mCopyState->fileStream) << data;
	PR_Free(data);

	return rv;
}

NS_IMETHODIMP nsMsgLocalMailFolder::EndCopy(PRBool copySucceeded)
{
	nsresult rv = NS_OK;
	//Copy the header to the new database
	if(copySucceeded && mCopyState->message)
	{
		nsCOMPtr<nsIMsgDBHdr> newHdr;
		nsCOMPtr<nsIMsgDBHdr> msgDBHdr;
		nsCOMPtr<nsIDBMessage> dbMessage(do_QueryInterface(mCopyState->message, &rv));


		rv = dbMessage->GetMsgDBHdr(getter_AddRefs(msgDBHdr));

		if(NS_SUCCEEDED(rv))
			rv = mDatabase->CopyHdrFromExistingHdr(mCopyState->dstKey, msgDBHdr, getter_AddRefs(newHdr));
	}

	if(mCopyState->fileStream)
	{
		mCopyState->fileStream->close();
		delete mCopyState->fileStream;
	}
	
	delete mCopyState;
	mCopyState = nsnull;

	//we finished the copy so someone else can write to us.
	PRBool haveSemaphore;
	rv = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this), &haveSemaphore);
	if(NS_SUCCEEDED(rv) && haveSemaphore)
		ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));

	return rv;
}


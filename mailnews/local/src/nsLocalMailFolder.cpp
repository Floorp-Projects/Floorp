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
#include "nsMailDatabase.h"
#include "nsIMailboxService.h"
#include "nsParseMailbox.h"
#include "nsIFolderListener.h"
#include "nsCOMPtr.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsRDFCID.h"
#include "nsFileStream.h"
#include "nsMsgDBCID.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID,							NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMailboxServiceCID,					NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(void)
  : nsMsgFolder(), mPath(""), mExpungedBytes(0), 
    mHaveReadNameFromDB(PR_FALSE), mGettingMail(PR_FALSE),
    mInitialized(PR_FALSE), mMailDatabase(nsnull)
{
		//XXXX This is a hack for the moment.  I'm assuming the only listener is our rdf:mailnews datasource.
		//In reality anyone should be able to listen to folder changes. 

		nsIRDFService* rdfService = nsnull;
		nsIRDFDataSource* datasource = nsnull;

		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &rdfService);
		if(NS_SUCCEEDED(rv))
		{
			if(NS_SUCCEEDED(rv = rdfService->GetDataSource("rdf:mailnews", &datasource)))
			{
				nsIFolderListener *folderListener;
				if(NS_SUCCEEDED(datasource->QueryInterface(nsIFolderListener::GetIID(), (void**)&folderListener)))
				{
					AddFolderListener(folderListener);
					NS_RELEASE(folderListener);
				}
				NS_RELEASE(datasource);
			}
			nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
		}

//  NS_INIT_REFCNT(); done by superclass
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder(void)
{
  NS_IF_RELEASE(mMailDatabase);
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
	else if (aIID.Equals(nsIDBChangeListener::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIDBChangeListener*, this);
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
  if (name[0] == '.' || name[0] == '#' || name[name.Length() - 1] == '~')
    return PR_TRUE;

  if (name.EqualsIgnoreCase("rules.dat"))
    return PR_TRUE;

  PRInt32 len = name.Length();

#if defined (XP_PC) || defined (XP_MAC) 
  // don't add summary files to the list of folders;
  //don't add popstate files to the list either, or rules (sort.dat). 
  if ((len > 4 && name.RFind(".snm", PR_TRUE) == len - 4) ||
      name.EqualsIgnoreCase("popstate.dat") ||
      name.EqualsIgnoreCase("sort.dat") ||
      name.EqualsIgnoreCase("mailfilt.log") ||
      name.EqualsIgnoreCase("filters.js") ||
      name.RFind(".toc", PR_TRUE) == len - 4)
    return PR_TRUE;
#endif
  if ((len > 4 && name.RFind(".sbd", PR_TRUE) == len - 4) ||
		(len > 4 && name.RFind(".msf", PR_TRUE) == len - 4))
	  return PR_TRUE;
  return PR_FALSE;
}

nsresult
nsMsgLocalMailFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;
  nsAutoString currentFolderName;

  nsIRDFService* rdf;
  rv = nsServiceManager::GetService(kRDFServiceCID,
                                    nsIRDFService::GetIID(),
                                    (nsISupports**)&rdf);
  if (NS_SUCCEEDED(rv)) {

    for (nsDirectoryIterator dir(path); dir.Exists(); dir++) {
      nsFileSpec currentFolderPath = (nsFileSpec&)dir;

      currentFolderName = currentFolderPath.GetLeafName();
      if (nsShouldIgnoreFile(currentFolderName))
        continue;

      nsAutoString uri;
      uri.Append(mURI);
      uri.Append('/');

      uri.Append(currentFolderName);
      char* uriStr = uri.ToNewCString();
      if (uriStr == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;
	
      // XXX trim off .sbd from uriStr
      nsIRDFResource* res;
      rv = rdf->GetResource(uriStr, &res);
      if (NS_FAILED(rv))
        return rv;
      nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
      if (NS_FAILED(rv))
        return rv;        // continue?
      delete[] uriStr;
      folder->SetFlag(MSG_FOLDER_FLAG_MAIL);

	  if(currentFolderName == "Inbox")
		folder->SetFlag(MSG_FOLDER_FLAG_INBOX);
	  else if(currentFolderName == "Trash")
		folder->SetFlag(MSG_FOLDER_FLAG_TRASH);
	  else if(currentFolderName == "Unsent Messages" || currentFolderName == "Outbox")
		folder->SetFlag(MSG_FOLDER_FLAG_QUEUE);
	  
      mSubFolders->AppendElement(folder);
    }
    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);
  }
  return rv;
}


//run the url to parse the mailbox
nsresult nsMsgLocalMailFolder::ParseFolder(nsFileSpec& path)
{
	nsresult rv = NS_OK;

	nsIMailboxService *mailboxService;
	rv = nsServiceManager::GetService(kMailboxServiceCID, nsIMailboxService::GetIID(),
																	(nsISupports**)&mailboxService);
	if (NS_FAILED(rv)) return rv; 
	nsMsgMailboxParser *parser = new nsMsgMailboxParser;
	if(!parser)
		return NS_ERROR_OUT_OF_MEMORY;
	rv = mailboxService->ParseMailbox(path, parser, nsnull, nsnull);

	nsServiceManager::ReleaseService(kMailboxServiceCID, mailboxService);
	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::Enumerate(nsIEnumerator* *result)
{
  nsresult rv; 

  // local mail folders contain both messages and folders:
  nsIEnumerator* folders;
  nsIEnumerator* messages;
  rv = GetSubFolders(&folders);
  if (NS_FAILED(rv)) return rv;
  rv = GetMessages(&messages);
  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
}

nsresult
nsMsgLocalMailFolder::AddDirectorySeparator(nsFileSpec &path)
{
	nsresult rv = NS_OK;
	if (nsCRT::strcmp(mURI, kMailboxRootURI) == 0) {
      // don't concat the full separator with .sbd
    }
    else {
      nsAutoString sep;
      rv = nsGetMailFolderSeparator(sep);
      if (NS_FAILED(rv)) return rv;

      // see if there's a dir with the same name ending with .sbd
      // unfortunately we can't just say:
      //          path += sep;
      // here because of the way nsFileSpec concatenates
#if defined(XP_MAC)     
	  nsAutoString str((nsFilePath)path);	//ducarroz: please don't cast a nsFilePath to char* on Mac
#else
      const char *chpath = path;
      nsAutoString str(chpath);
#endif
      str += sep;
      path = str;
    }

	return rv;
}

NS_IMETHODIMP
nsMsgLocalMailFolder::GetSubFolders(nsIEnumerator* *result)
{
  if (!mInitialized) {
    nsFileSpec path;
    nsresult rv = GetPath(path);
    if (NS_FAILED(rv)) return rv;
	
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
    else {
      UpdateSummaryTotals();
      // Look for a directory for this mail folder, and recurse into it.
      // e.g. if the folder is "inbox", look for "inbox.sbd". 
#if 0
      char *folderName = path->GetLeafName();
      char *newLeafName = (char*)malloc(PL_strlen(folderName) + PL_strlen(kDirExt) + 2);
      PL_strcpy(newLeafName, folderName);
      PL_strcat(newLeafName, kDirExt);
      path->SetLeafName(newLeafName);
      if(folderName)
        delete[] folderName;
      if(newLeafName)
        delete[] newLeafName;
#endif
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

NS_IMETHODIMP
nsMsgLocalMailFolder::GetMessages(nsIEnumerator* *result)
{
  if (mMailDatabase == nsnull)
  {
    nsNativeFileSpec path;
    nsresult rv = GetPath(path);
    if (NS_FAILED(rv)) return rv;

	nsresult folderOpen;
	nsIMsgDatabase * mailDBFactory = nsnull;
	nsIMsgDatabase *mailDB;

	nsIMessage * msgHdr = nsnull;
	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		folderOpen = mailDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &mMailDatabase, PR_FALSE);
	}
	if(!NS_SUCCEEDED(folderOpen) &&
			folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
	{
		// if it's out of date then reopen with upgrade.
		if(!NS_SUCCEEDED(rv = mailDBFactory->Open(path, PR_TRUE, &mMailDatabase, PR_TRUE)))
		{
			mailDBFactory->Release();
			return rv;
		}
	}
	mailDBFactory->Release();

	if(mMailDatabase)
	{

		mMailDatabase->AddListener(this);

		// if we have to regenerate the folder, run the parser url.
		if(folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
		{
			if(NS_FAILED(rv = ParseFolder(path)))
				return rv;
			else
				return NS_ERROR_NOT_INITIALIZED;
		}
	}
  }
  return mMailDatabase->EnumerateMessages(result);

}

NS_IMETHODIMP nsMsgLocalMailFolder::BuildFolderURL(char **url)
{
  const char *urlScheme = "mailbox:";

  if(!url)
    return NS_ERROR_NULL_POINTER;

  nsFileSpec path;
  nsresult rv = GetPath(path);
  if (NS_FAILED(rv)) return rv;
#if defined(XP_MAC)
  nsAutoString tmpPath((nsFilePath)path);	//ducarroz: please don't cast a nsFilePath to char* on Mac
  const char *pathName = tmpPath.ToNewCString();
  *url = PR_smprintf("%s%s", urlScheme, pathName);
  delete [] pathName;
#else
  const char *pathName = path;
  *url = PR_smprintf("%s%s", urlScheme, pathName);
#endif
  return NS_OK;

}

NS_IMETHODIMP nsMsgLocalMailFolder::CreateSubfolder(char *leafNameFromUser,
                                                    nsIMsgFolder **outFolder,
                                                    PRUint32 *outPos)
{
#ifdef HAVE_PORT
  nsresult status = NS_OK;
  *ppOutFolder = NULL;
  *pOutPos = 0;
  XP_StatStruct stat;
    
    
  // Only create a .sbd pathname if we're not in the root folder. The root folder
  // e.g. c:\netscape\mail has to behave differently than subfolders.
  if (m_depth > 1)
  {
    // Look around in our directory to get a subdirectory, creating it 
    // if necessary
        XP_BZERO (&stat, sizeof(stat));
        if (0 == XP_Stat (m_pathName, &stat, xpMailSubdirectory))
        {
            if (!S_ISDIR(stat.st_mode))
                status = MK_COULD_NOT_CREATE_DIRECTORY; // a file .sbd already exists
        }
        else {
            status = XP_MakeDirectory (m_pathName, xpMailSubdirectory);
      if (status == -1)
        status = MK_COULD_NOT_CREATE_DIRECTORY;
    }
    }
    
  char *leafNameForDisk = CreatePlatformLeafNameForDisk(leafNameFromUser,m_master, this);
  if (!leafNameForDisk)
    status = MK_OUT_OF_MEMORY;

    if (0 == status) //ok so far
    {
        // Now that we have a suitable parent directory created/identified, 
        // we can create the new mail folder inside the parent dir. Again,

        char *newFolderPath = (char*) XP_ALLOC(XP_STRLEN(m_pathName) + XP_STRLEN(leafNameForDisk) + XP_STRLEN(".sbd/") + 1);
        if (newFolderPath)
        {
            XP_STRCPY (newFolderPath, m_pathName);
            if (m_depth == 1)
                XP_STRCAT (newFolderPath, "/");
            else
                XP_STRCAT (newFolderPath, ".sbd/");
            XP_STRCAT (newFolderPath, leafNameForDisk);

          if (0 != XP_Stat (newFolderPath, &stat, xpMailFolder))
      {
        XP_File file = XP_FileOpen(newFolderPath, xpMailFolder, XP_FILE_WRITE_BIN);
        if (file)
        {
           // Create an empty database for this mail folder, set its name from the user  
            MailDB *unusedDb = NULL;
           MailDB::Open(newFolderPath, TRUE, &unusedDb, TRUE);
            if (unusedDb)
            {
             //need to set the folder name
           
            MSG_FolderInfoMail *newFolder = BuildFolderTree (newFolderPath, m_depth + 1, m_subFolders, m_master);
             if (newFolder)
             {
               // so we don't show ??? in totals
               newFolder->SummaryChanged();
               *ppOutFolder = newFolder;
               *pOutPos = m_subFolders->FindIndex (0, newFolder);
             }
             else
               status = MK_OUT_OF_MEMORY;
             unusedDb->SetFolderInfoValid(newFolderPath,0,0);
              unusedDb->Close();
            }
          else
          {
            XP_FileClose(file);
            file = NULL;
            XP_FileRemove (newFolderPath, xpMailFolder);
            status = MK_MSG_CANT_CREATE_FOLDER;
          }
          if (file)
          {
            XP_FileClose(file);
            file = NULL;
          }
        }
        else
          status = MK_MSG_CANT_CREATE_FOLDER;
      }
      else
        status = MK_MSG_FOLDER_ALREADY_EXISTS;
      FREEIF(newFolderPath);
        }
        else
            status = MK_OUT_OF_MEMORY;
    }
    FREEIF(leafNameForDisk);
    return status;
#endif //HAVE_PORT
  return NS_OK;
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
nsMsgLocalMailFolder::GetChildNamed(nsString& name, nsISupports ** aChild)
{
  NS_ASSERTION(aChild, "NULL child");

  // will return nsnull if we can't find it
  *aChild = nsnull;

  nsIMsgFolder *folder = nsnull;

  PRUint32 count = mSubFolders->Count();

  for (PRUint32 i = 0; i < count; i++)
  {
    nsISupports *supports;
    supports = mSubFolders->ElementAt(i);
    if(folder)
      NS_RELEASE(folder);
    if(NS_SUCCEEDED(supports->QueryInterface(kISupportsIID, (void**)&folder))) {
      char *folderName;

      folder->GetName(&folderName);
      // case-insensitive compare is probably LCD across OS filesystems
      if (folderName && !name.EqualsIgnoreCase(folderName)) {
        *aChild = folder;
        PR_FREEIF(folderName);
        return NS_OK;
      }
      PR_FREEIF(folderName);
    }
    NS_RELEASE(supports);
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
      return NS_OK;
    }
    else
    {
      //Need to read the name from the database
    }
  }
	nsAutoString folderName;
	nsURI2Name(kMailboxRootURI, mURI, folderName);
	*name = folderName.ToNewCString();

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(nsString& prettyName)
{
  if (mDepth == 1) {
    // Depth == 1 means we are on the mail server level
    // override the name here to say "Local Mail"
    prettyName = PL_strdup("Local Mail");
  }
  else {
    nsresult rv = NS_ERROR_NULL_POINTER;
    char *pName = prettyName.ToNewCString();
    if (pName)
      rv = nsMsgFolder::GetPrettyName(&pName);
    delete[] pName;
    return rv;
  }

  return NS_OK;
}

nsresult  nsMsgLocalMailFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult openErr;
    if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;

	nsIMsgDatabase * mailDBFactory = nsnull;
	nsIMsgDatabase *mailDB;

	nsIMessage * msgHdr = nsnull;
	nsresult rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDBFactory);
	if (NS_SUCCEEDED(rv) && mailDBFactory)
	{
		openErr = mailDBFactory->Open(mPath, PR_FALSE, (nsIMsgDatabase **) &mailDB, PR_FALSE);
		mailDBFactory->Release();
	}
//    openErr = nsMailDatabase::Open(mPath, FALSE, &mailDB, FALSE);

    *db = mailDB;
    if (NS_SUCCEEDED(openErr)&& *db)
        openErr = (*db)->GetDBFolderInfo(folderInfo);
    return openErr;
}

NS_IMETHODIMP nsMsgLocalMailFolder::UpdateSummaryTotals()
{
  //We need to read this info from the database

  // If we asked, but didn't get any, stop asking
  if (mNumUnreadMessages == -1)
    mNumUnreadMessages = -2;

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
    int32 purgeThreshhold = m_master->GetPrefs()->GetPurgeThreshhold();
    PRBool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();;
    return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
  }
  return FALSE;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetSizeOnDisk(PRUint32 size)
{
#ifdef HAVE_PORT
  int32 ret = 0;
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

NS_IMETHODIMP nsMsgLocalMailFolder::RememberPassword(char *password)
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
  nsFileSpec nopath("");
  if (mPath == nopath) {
    nsresult rv = nsURI2Path(kMailboxRootURI, mURI, mPath);
    if (NS_FAILED(rv)) return rv;
  }
  aPathName = mPath;
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::DeleteMessage(nsIMessage *message)
{
	if(mMailDatabase)
		return(mMailDatabase->DeleteHeader(message, nsnull, PR_TRUE, PR_TRUE));

	return NS_OK;
}


NS_IMETHODIMP nsMsgLocalMailFolder::OnKeyChange(nsMsgKey aKeyChanged, int32 aFlags, 
                         nsIDBChangeListener * aInstigator)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::OnKeyDeleted(nsMsgKey aKeyChanged, int32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
	nsIMessage *pMessage;
	mMailDatabase->GetMsgHdrForKey(aKeyChanged, &pMessage);
	nsString author, subject;
	nsISupports *msgSupports;
	if(NS_SUCCEEDED(pMessage->QueryInterface(kISupportsIID, (void**)&msgSupports)))
	{
		for(int i = 0; i < mListeners->Count(); i++)
		{
			nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemRemoved(this, msgSupports);
			NS_RELEASE(listener);
		}
	}
	NS_RELEASE(msgSupports);

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::OnKeyAdded(nsMsgKey aKeyChanged, int32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	nsIMessage *pMessage;
	mMailDatabase->GetMsgHdrForKey(aKeyChanged, &pMessage);
	nsString author, subject;
	nsISupports *msgSupports;
	if(pMessage && NS_SUCCEEDED(pMessage->QueryInterface(kISupportsIID, (void**)&msgSupports)))
	{
		for(int i = 0; i < mListeners->Count(); i++)
		{
			nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemAdded(this, msgSupports);
			NS_RELEASE(listener);
		}
	}
	NS_RELEASE(msgSupports);

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator)
{
	return NS_OK;
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
	//The new key is the end of the file
	mCopyState->fileStream->seek(PR_SEEK_END, 0);
	mCopyState->dstKey = mCopyState->fileStream->tell();

	mCopyState->message = message;
	if(mCopyState->message)	
		mCopyState->message->AddRef();
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
	//Copy the header to the new database
	if(copySucceeded && mCopyState->message)
	{
		nsIMessage *newHdr;
		mMailDatabase->CopyHdrFromExistingHdr(mCopyState->dstKey, mCopyState->message, &newHdr);
		NS_IF_RELEASE(newHdr);
	}

	if(mCopyState->fileStream)
	{
		mCopyState->fileStream->close();
		delete mCopyState->fileStream;
	}
	
	if(mCopyState->message)
		mCopyState->message->Release();
	delete mCopyState;
	mCopyState = nsnull;

	//we finished the copy so someone else can write to us.
	PRBool haveSemaphore;
	nsresult rv = TestSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this), &haveSemaphore);
	if(NS_SUCCEEDED(rv) && haveSemaphore)
		ReleaseSemaphore(NS_STATIC_CAST(nsIMsgLocalMailFolder*, this));

	return NS_OK;
}

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

#include "nsNewsFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "prprf.h"
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

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID,	NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

nsMsgNewsFolder::nsMsgNewsFolder(void)
  : nsMsgFolder(), mPath(""), mExpungedBytes(0), 
    mHaveReadNameFromDB(PR_FALSE), mGettingNews(PR_FALSE),
    mInitialized(PR_FALSE), mNewsDatabase(nsnull)
{

#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::nsMsgNewsFolder\n");
#endif

		//XXXX This is a hack for the moment.  I'm assuming the only listener is our rdf:mailnews datasource.
		//In reality anyone should be able to listen to folder changes. 

		nsIRDFService* rdfService = nsnull;
		nsIRDFDataSource* datasource = nsnull;

		nsresult rv = nsServiceManager::GetService(kRDFServiceCID,
												nsIRDFService::GetIID(),
												(nsISupports**) &rdfService);
		if(NS_SUCCEEDED(rv))
		{
			if(NS_SUCCEEDED(rv = rdfService->GetDataSource("rdf:mailnewsfolders", &datasource)))
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

nsMsgNewsFolder::~nsMsgNewsFolder(void)
{
	if(mNewsDatabase)
		//Close releases db;
		mNewsDatabase->Close(PR_TRUE);
}

NS_IMPL_ADDREF_INHERITED(nsMsgNewsFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgNewsFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgNewsFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::QueryInterface()\n");
#endif
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsIMsgNewsFolder::GetIID()))
	{
#ifdef DEBUG_sspitzer
    printf("aIID is for nsIMsgNewsFolder\n");
#endif
		*aInstancePtr = NS_STATIC_CAST(nsIMsgNewsFolder*, this);
	}              
	else if (aIID.Equals(nsIDBChangeListener::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIDBChangeListener*, this);
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

  // don't add summary files to the list of folders;
  // don't add popstate files to the list either, or rules (sort.dat). 
  if ((len > 4 && name.RFind(".snm", PR_TRUE) == len - 4) ||
      name.EqualsIgnoreCase("popstate.dat") ||
      name.EqualsIgnoreCase("sort.dat") ||
      name.EqualsIgnoreCase("newsfilt.log") ||
      name.EqualsIgnoreCase("filters.js") ||
      name.RFind(".toc", PR_TRUE) == len - 4)
    return PR_TRUE;

  if ((len > 4 && name.RFind(".sbd", PR_TRUE) == len - 4) ||
		(len > 4 && name.RFind(".msf", PR_TRUE) == len - 4))
	  return PR_TRUE;
  return PR_FALSE;
}

nsresult
nsMsgNewsFolder::CreateSubFolders(nsFileSpec &path)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::CreateSubFolder()\n");
#endif

  nsAutoString currentFolderNameStr("netscape.public.mozilla.unix");
  nsIMsgFolder *child;
  AddSubfolder(currentFolderNameStr,&child);
  NS_IF_RELEASE(child);

  return NS_OK;
#if 0
	nsresult rv = NS_OK;
	nsAutoString currentFolderNameStr;
	nsIMsgFolder *child;
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

		AddSubfolder(currentFolderNameStr, &child);
		NS_IF_RELEASE(child);
		PL_strfree(folderName);
    }
	return rv;
#endif
}

nsresult nsMsgNewsFolder::AddSubfolder(nsAutoString name, nsIMsgFolder **child)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::AddSubfolder()\n");
#endif
	if(!child)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsIRDFService* rdf;
	rv = nsServiceManager::GetService(kRDFServiceCID,
                                    nsIRDFService::GetIID(),
                                    (nsISupports**)&rdf);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsIRDFResource* res;
	rv = rdf->GetResource(uriStr, &res);
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;

	folder->SetFlag(MSG_FOLDER_FLAG_NEWSGROUP);

	mSubFolders->AppendElement(folder);
	*child = folder;
	NS_ADDREF(*child);
    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}


nsresult nsMsgNewsFolder::ParseFolder(nsFileSpec& path)
{
	return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::Enumerate(nsIEnumerator **result)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::Enumerate()\n");
#endif
#if 0
  nsresult rv; 

  // for now, news folders contain both messages and folders
  // server is a folder, and it contains folders
  // newsgroup is a folder, and it contains messages
  //
  // eventually the top level server will not be a folder
  // and news folders will only contain messages
  nsIEnumerator* folders;
  //  nsIEnumerator* messages;
  rv = GetSubFolders(&folders);
  if (NS_FAILED(rv)) return rv;
  //  rv = GetMessages(&messages);
  //  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
#endif
  return NS_OK;
}

nsresult
nsMsgNewsFolder::AddDirectorySeparator(nsFileSpec &path)
{
	nsresult rv = NS_OK;
	if (nsCRT::strcmp(mURI, kNewsRootURI) == 0) {
      // don't concat the full separator with .sbd
    }
    else {
      nsAutoString sep;
#if 0
      rv = nsGetNewsFolderSeparator(sep);
#else
      rv = NS_OK;
#endif
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

NS_IMETHODIMP
nsMsgNewsFolder::GetSubFolders(nsIEnumerator* *result)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetSubFolders()\n");
#endif
  if (!mInitialized) {
    nsFileSpec path;
    nsresult rv = GetPath(path);
    if (NS_FAILED(rv)) return rv;
	
    rv = CreateSubFolders(path);
    if (NS_FAILED(rv)) return rv;

    mInitialized = PR_TRUE;      // XXX do this on failure too?
  }
  return mSubFolders->Enumerate(result);
}

NS_IMETHODIMP
nsMsgNewsFolder::AddUnique(nsISupports* element)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgNewsFolder::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

//Makes sure the database is open and exists.  If the database is valid then
//returns NS_OK.  Otherwise returns a failure error value.
nsresult nsMsgNewsFolder::GetDatabase()
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetDatabase()\n");
#endif
#if 0
	if (mNewsDatabase == nsnull)
	{
		nsNativeFileSpec path;
		nsresult rv = GetPath(path);
		if (NS_FAILED(rv)) return rv;

		nsresult folderOpen = NS_OK;
		nsIMsgDatabase * newsDBFactory = nsnull;

		rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDBFactory);
		if (NS_SUCCEEDED(rv) && newsDBFactory)
		{
			folderOpen = newsDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &mNewsDatabase, PR_FALSE);
			if(!NS_SUCCEEDED(folderOpen) &&
				folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE || folderOpen == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING )
			{
				// if it's out of date then reopen with upgrade.
				if(!NS_SUCCEEDED(rv = newsDBFactory->Open(path, PR_TRUE, &mNewsDatabase, PR_TRUE)))
				{
					NS_RELEASE(newsDBFactory);
					return rv;
				}
			}
	
			NS_RELEASE(newsDBFactory);
		}

		if(mNewsDatabase)
		{

			mNewsDatabase->AddListener(this);

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
#endif
	return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetMessages(nsIEnumerator* *result)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetMessages()\n");
#endif
#if 0
	nsresult rv = GetDatabase();

	if(NS_SUCCEEDED(rv))
		return mNewsDatabase->EnumerateMessages(result);
	else
		return rv;
#endif
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgNewsFolder::GetThreads(nsIEnumerator** threadEnumerator)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetThreads()\n");
#endif
#if 0
	nsresult rv = GetDatabase();
	
	if(NS_SUCCEEDED(rv))
		return mNewsDatabase->EnumerateThreads(threadEnumerator);
	else
		return rv;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetThreadForMessage(nsIMessage *message, nsIMsgThread **thread)
{
	nsresult rv = GetDatabase();
	if(NS_SUCCEEDED(rv))
		return mNewsDatabase->GetThreadContainingMsgHdr(message, thread);
	else
		return rv;

}

NS_IMETHODIMP nsMsgNewsFolder::BuildFolderURL(char **url)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::BuildFolderURL()\n");
#endif
  const char *urlScheme = "news:";

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

/* Finds the directory associated with this folder.  That is if the path is
   c:\Inbox, it will return c:\Inbox.sbd if it succeeds.  If that path doesn't
   currently exist then it will create it
  */
nsresult nsMsgNewsFolder::CreateDirectoryForFolder(nsFileSpec &path)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::CreateDirectoryForFolder()\n");
#endif
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

NS_IMETHODIMP nsMsgNewsFolder::CreateSubfolder(const char *folderName)
{
#if 0
	nsresult rv = NS_OK;
    
	nsFileSpec path;
    nsIMsgFolder *child = nsnull;
	//Get a directory based on our current path.
	rv = CreateDirectoryForFolder(path);
	if(NS_FAILED(rv))
		return rv;


	//Now we have a valid directory or we have returned.
	//Make sure the new folder name is valid
	path += folderName;
	path.MakeUnique();

	nsOutputFileStream outputStream(path);	
   
	// Create an empty database for this news folder, set its name from the user  
	nsIMsgDatabase * newsDBFactory = nsnull;

	rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDBFactory);
	if (NS_SUCCEEDED(rv) && newsDBFactory)
	{
        nsIMsgDatabase *unusedDB = NULL;
		rv = newsDBFactory->Open(path, PR_TRUE, (nsIMsgDatabase **) &unusedDB, PR_TRUE);

        if (NS_SUCCEEDED(rv) && unusedDB)
        {
			//need to set the folder name
			nsIDBFolderInfo *folderInfo;
			rv = unusedDB->GetDBFolderInfo(&folderInfo);
			if(NS_SUCCEEDED(rv))
			{
				//folderInfo->SetNewsgroupName(leafNameFromUser);
				NS_IF_RELEASE(folderInfo);
			}

			//Now let's create the actual new folder
			nsAutoString folderNameStr(folderName);
			rv = AddSubfolder(folderName, &child);
            unusedDB->SetSummaryValid(PR_TRUE);
            unusedDB->Close(PR_TRUE);
        }
        else
        {
			path.Delete(PR_FALSE);
            rv = NS_MSG_CANT_CREATE_FOLDER;
        }
		NS_IF_RELEASE(newsDBFactory);
	}
	if(rv == NS_OK && child)
	{
		nsISupports *folderSupports;

		rv = child->QueryInterface(kISupportsIID, (void**)&folderSupports);
		if(NS_SUCCEEDED(rv))
		{
			NotifyItemAdded(folderSupports);
			NS_IF_RELEASE(folderSupports);
		}
	}
	NS_IF_RELEASE(child);
	return rv;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::RemoveSubFolder(nsIMsgFolder *which)
{
#if 0
  // Let the base class do list management
  nsMsgFolder::RemoveSubFolder(which);
#endif

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::Delete()
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::Rename(const char *newName)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgNewsFolder::GetChildNamed(nsString& name, nsISupports ** aChild)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetChildNamed()\n");
#endif
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

NS_IMETHODIMP nsMsgNewsFolder::GetName(char **name)
{
  if(!name)
    return NS_ERROR_NULL_POINTER;

  if (!mHaveReadNameFromDB)
  {
    if (mDepth == 1) 
    {
      SetName("News.Mozilla.Org");
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
	nsURI2Name(kNewsRootURI, mURI, folderName);
	*name = folderName.ToNewCString();

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetPrettyName(nsString& prettyName)
{
  if (mDepth == 1) {
    // Depth == 1 means we are on the news server level
    // override the name here to say "News.Mozilla.Org"
    prettyName = PL_strdup("News.Mozilla.Org");
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

nsresult  nsMsgNewsFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
    nsresult openErr=NS_ERROR_UNEXPECTED;
    if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;

	nsIMsgDatabase *newsDBFactory = nsnull;
	nsIMsgDatabase *newsDB;

	nsresult rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &newsDBFactory);
	if (NS_SUCCEEDED(rv) && newsDBFactory)
	{
		openErr = newsDBFactory->Open(mPath, PR_FALSE, (nsIMsgDatabase **) &newsDB, PR_FALSE);
		newsDBFactory->Release();
	}

    *db = newsDB;
    if (NS_SUCCEEDED(openErr)&& *db)
        openErr = (*db)->GetDBFolderInfo(folderInfo);
    return openErr;
}

NS_IMETHODIMP nsMsgNewsFolder::UpdateSummaryTotals()
{
	PRUint32 oldUnreadMessages = mNumUnreadMessages;
	PRUint32 oldTotalMessages = mNumTotalMessages;
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

NS_IMETHODIMP nsMsgNewsFolder::GetExpungedBytesCount(PRUint32 *count)
{
  if(!count)
    return NS_ERROR_NULL_POINTER;

  *count = mExpungedBytes;

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetDeletable(PRBool *deletable)
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
 
NS_IMETHODIMP nsMsgNewsFolder::GetCanCreateChildren(PRBool *canCreateChildren)
{  
  if(!canCreateChildren)
    return NS_ERROR_NULL_POINTER;

  *canCreateChildren = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetCanBeRenamed(PRBool *canBeRenamed)
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

NS_IMETHODIMP nsMsgNewsFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetSizeOnDisk(PRUint32 size)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetUsersName(char** userName)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetHostName(char** hostName)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::RememberPassword(char *password)
{
#ifdef HAVE_DB
  NewsDB *newsDb = NULL;
  NewsDB::Open(m_pathName, TRUE, &newsDb);
  if (newsDb)
  {
    newsDb->SetCachedPassword(password);
    newsDb->Close();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetRememberedPassword(char ** password)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetPath(nsFileSpec& aPathName)
{
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFolder::GetPath()\n");
#endif
  nsFileSpec nopath("");
  if (mPath == nopath) {
    nsresult rv = nsURI2Path(kNewsRootURI, mURI, mPath);
    if (NS_FAILED(rv)) return rv;
  }
  aPathName = mPath;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::DeleteMessage(nsIMessage *message)
{
	if(mNewsDatabase)
		return(mNewsDatabase->DeleteHeader(message, nsnull, PR_TRUE, PR_TRUE));

	return NS_OK;
}

nsresult nsMsgNewsFolder::NotifyPropertyChanged(char *property, char *oldValue, char* newValue)
{
	nsISupports *supports;
	if(NS_SUCCEEDED(QueryInterface(kISupportsIID, (void**)&supports)))
	{
		PRUint32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemPropertyChanged(supports, property, oldValue, newValue);
			NS_RELEASE(listener);
		}
		NS_RELEASE(supports);
	}

	return NS_OK;

}

nsresult nsMsgNewsFolder::NotifyItemAdded(nsISupports *item)
{

	PRUint32 i;
	for(i = 0; i < mListeners->Count(); i++)
	{
		nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
		listener->OnItemAdded(this, item);
		NS_RELEASE(listener);
	}

	return NS_OK;

}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyChange(nsMsgKey aKeyChanged, int32 aFlags, 
                         nsIDBChangeListener * aInstigator)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyDeleted(nsMsgKey aKeyChanged, int32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
	nsIMessage *pMessage;
	mNewsDatabase->GetMsgHdrForKey(aKeyChanged, &pMessage);
	nsString author, subject;
	nsISupports *msgSupports;
	if(NS_SUCCEEDED(pMessage->QueryInterface(kISupportsIID, (void**)&msgSupports)))
	{
		PRUint32 i;
		for(i = 0; i < mListeners->Count(); i++)
		{
			nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
			listener->OnItemRemoved(this, msgSupports);
			NS_RELEASE(listener);
		}
	}
	UpdateSummaryTotals();
	NS_RELEASE(msgSupports);

	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyAdded(nsMsgKey aKeyChanged, int32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	nsIMessage *pMessage;
	mNewsDatabase->GetMsgHdrForKey(aKeyChanged, &pMessage);
	nsString author, subject;
	nsISupports *msgSupports;
	if(pMessage && NS_SUCCEEDED(pMessage->QueryInterface(kISupportsIID, (void**)&msgSupports)))
	{
		NotifyItemAdded(msgSupports);
	}
	UpdateSummaryTotals();
	NS_RELEASE(msgSupports);

	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator)
{
	return NS_OK;
}

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
#include "nsNewsMessage.h"
#include "nsNewsUtils.h"

//not using nsMsgLineBuffer yet, but I need to soon...
//#include "nsMsgLineBuffer.h"

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
    mInitialized(PR_FALSE), mNewsDatabase(nsnull), m_optionLines(nsnull)
{

#ifdef DEBUG_sspitzer_
  printf("nsMsgNewsFolder::nsMsgNewsFolder\n");
#endif

//  NS_INIT_REFCNT(); done by superclass
}

nsMsgNewsFolder::~nsMsgNewsFolder(void)
{
	if(mNewsDatabase)
		//Close releases db;
		mNewsDatabase->Close(PR_TRUE);

  PR_FREEIF(m_optionLines);
}

NS_IMPL_ADDREF_INHERITED(nsMsgNewsFolder, nsMsgFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgNewsFolder, nsMsgFolder)

NS_IMETHODIMP nsMsgNewsFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
#ifdef DEBUG_sspitzer_
  printf("nsMsgNewsFolder::QueryInterface()\n");
#endif
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsIMsgNewsFolder::GetIID()))
	{
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
#if 0
static PRBool
nsShouldIgnoreFile(nsString& name)
{
  PRUnichar theFirstChar=name.CharAt(0);
  if (theFirstChar == '.' || theFirstChar == '#' || name.CharAt(name.Length() - 1) == '~')
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
#endif

PRBool
nsMsgNewsFolder::isNewsHost() 
{
  // this will do for now.  Eventually, this will go away...

  PRInt32 rootURIlen = PL_strlen(kNewsRootURI);
  PRInt32 mURIlen = PL_strlen(mURI);

  // if we are shorter than news://, we are too short to be a host
  if (mURIlen <= rootURIlen) {
    return PR_FALSE;
  }
  
  if (PL_strncmp(mURI, kNewsRootURI, rootURIlen) == 0) {
    // if we get here, mURI looks like this:  news://x
    // where x is non-empty, and may contain "/"
    char *rightAfterTheRoot = mURI+rootURIlen+1;
    printf("search for a slash in %s\n",rightAfterTheRoot);
    if (PL_strstr(rightAfterTheRoot,"/") == nsnull) {
      // there is no slashes after news://,
      // so mURI is of the form news://x
      return PR_TRUE;
    }
    else {
      // there is another slash, so mURI is of the form
      // news://x/y, so it is not a host
      return PR_FALSE;
    }
  }
  else {
    // mURI doesn't start with news:// so it can't be a host
    return PR_FALSE;
  }
}
#ifdef USE_NEWSRC_MAP_FILE

#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"

nsresult
nsMsgNewsFolder::MapHostToNewsrcFile(nsFileSpec &fatFile, char *newshostname, nsFileSpec &newsrcFile)
{
  char lookingFor[512];
	char buffer[512];
	char psuedo_name[512];
	char filename[512];
	char is_newsgroup[512];
  PRBool rv;

#ifdef DEBUG_sspitzer
  printf("MapHostToNewsrcFile(%s,%s,??)\n",(const char *)fatFile, newshostname);
#endif
  sprintf(lookingFor,"newsrc-%s",newshostname);
  
  nsInputFileStream inputStream(fatFile);
 
  if (inputStream.eof()) {
    newsrcFile = "";
    inputStream.close();
    return NS_ERROR_FAILURE;
  }

  /* we expect the first line to be NEWSRC_MAP_FILE_COOKIE */
	rv = inputStream.readline(buffer, sizeof(buffer));
#ifdef DEBUG_sspitzer
  printf("buffer = %s\n", buffer);
#endif
  if ((!rv) || (PL_strncmp(buffer, NEWSRC_MAP_FILE_COOKIE, PL_strlen(NEWSRC_MAP_FILE_COOKIE)))) {
    newsrcFile = "";
    inputStream.close();
    return NS_ERROR_FAILURE;
  }   

	while (!inputStream.eof()) {
    char * p;
    int i;

    rv = inputStream.readline(buffer, sizeof(buffer));
    if (!rv) {
      newsrcFile = "";
      inputStream.close();
      return NS_ERROR_FAILURE;
    }  

#ifdef DEBUG_sspitzer
    printf("buffer = %s\n", buffer);    
#endif
    
    /*
      This used to be scanf() call which would incorrectly
      parse long filenames with spaces in them.  - JRE
    */
    
    filename[0] = '\0';
    is_newsgroup[0]='\0';
    
    for (i = 0, p = buffer; *p && *p != '\t' && i < 500; p++, i++)
      psuedo_name[i] = *p;
    psuedo_name[i] = '\0';
    if (*p) 
      {
        for (i = 0, p++; *p && *p != '\t' && i < 500; p++, i++)
          filename[i] = *p;
        filename[i]='\0';
        if (*p) 
          {
            for (i = 0, p++; *p && *p != '\r' && i < 500; p++, i++)
              is_newsgroup[i] = *p;
            is_newsgroup[i]='\0';
          }
      }

		if(!PL_strncmp(is_newsgroup, "TRUE", 4)) {
#ifdef DEBUG_sspitzer
      printf("is_newsgroups_file = TRUE\n");
#endif
    }
    else {
#ifdef DEBUG_sspitzer
      printf("is_newsgroups_file = FALSE\n");
#endif
    }
    
    printf("psuedo_name=%s,filename=%s\n", psuedo_name, filename);
    if (!PL_strncmp(psuedo_name,lookingFor,PL_strlen(lookingFor))) {
#ifdef DEBUG_sspitzer
      printf("found a match for %s\n",lookingFor);
#endif
      newsrcFile = filename;
      inputStream.close();
      return NS_OK;
    }
  }

  // failed to find a match in the map file
  newsrcFile = "";
  inputStream.close();
  return NS_ERROR_FAILURE;
}
#endif /* USE_NEWSRC_MAP_FILE */

nsresult 
nsMsgNewsFolder::GetNewsrcFile(nsFileSpec &path, nsFileSpec &newsrcFile)
{
  nsresult rv = NS_OK;
  char *newshostname = path.GetLeafName();

#ifdef USE_NEWSRC_MAP_FILE
  // the fat file lives in the same directory as
  // the newsrc files
  nsFileSpec fatFile(path);
  fatFile.SetLeafName("fat");

  rv = MapHostToNewsrcFile(fatFile, newshostname, newsrcFile);
#else
  char str[1024];

  if ((newshostname == nsnull) || (PL_strlen(newshostname) == 0)) {
    newsrcFile = "";
    rv = NS_ERROR_NULL_POINTER;
  }
  else {
    sprintf(str, ".newsrc-%s", newshostname);
    newsrcFile = path;
    newsrcFile.SetLeafName(str);
    rv = NS_OK;
  }
#endif /* USE_NEWSRC_MAP_FILE */
  if (newshostname) {
    delete [] newshostname;
  }
  return rv;
}

nsresult
nsMsgNewsFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;

  if (isNewsHost()) {
#ifdef DEBUG_sspitzer
    printf("CreateSubFolders:  %s = %s\n", mURI, (const char *)path);
#endif

    nsFileSpec newsrcFile("");
    rv = GetNewsrcFile(path, newsrcFile);
    if (rv == NS_OK) {
#ifdef DEBUG_sspitzer
      printf("newsrc file = %s\n", (const char *)newsrcFile);
#endif
      rv = LoadNewsrcFileAndCreateNewsgroups(newsrcFile);
    }
  }
  else {
#ifdef DEBUG_sspitzer
    printf("%s is not a host, so it has no newsgroups.\n", mURI);
#endif
    rv = NS_OK;
  }

  return rv;
}

nsresult nsMsgNewsFolder::AddSubfolder(nsAutoString name, nsIMsgFolder **child)
{
#ifdef DEBUG_sspitzer_
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
#ifdef DEBUG_sspitzer_
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
	if (PL_strcmp(mURI, kNewsRootURI) == 0) {
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
#ifdef DEBUG_sspitzer_
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
#ifdef DEBUG_sspitzer_
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
#ifdef DEBUG_sspitzer_
  printf("nsMsgNewsFolder::GetMessages()\n");
#endif
#if 0
	nsresult rv = GetDatabase();

	if(NS_SUCCEEDED(rv))
	{
		nsIEnumerator *msgHdrEnumerator = nsnull;
		nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
		rv = mNewsDatabase->EnumerateMessages(&msgHdrEnumerator);
		if(NS_SUCCEEDED(rv))
			rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator,
												   this, &messageEnumerator);
		*result = messageEnumerator;
		NS_IF_RELEASE(msgHdrEnumerator);
	}
	return rv;
#endif
  return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgNewsFolder::GetThreads(nsIEnumerator** threadEnumerator)
{
#ifdef DEBUG_sspitzer_
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
	{
		nsIMsgDBHdr *msgDBHdr = nsnull;
		//We know from our factory that mailbox message resources are going to be
  	//nsNewsMessages.
	  nsNewsMessage *newsMessage = NS_STATIC_CAST(nsNewsMessage*, message);
		rv = newsMessage->GetMsgDBHdr(&msgDBHdr);
		if(NS_SUCCEEDED(rv))
		{
			rv = mNewsDatabase->GetThreadContainingMsgHdr(msgDBHdr, thread);
			NS_IF_RELEASE(msgDBHdr);
		}
	}
	return rv;

}

NS_IMETHODIMP nsMsgNewsFolder::BuildFolderURL(char **url)
{
#ifdef DEBUG_sspitzer_
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
#ifdef DEBUG_sspitzer_
  printf("nsMsgNewsFolder::CreateDirectoryForFolder()\n");
#endif
#if 0
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
#else
  return NS_OK;
#endif
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
#ifdef DEBUG_sspitzer_
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
      SetName("News.Foo.Bar");
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
	nsNewsURI2Name(kNewsRootURI, mURI, folderName);
	*name = folderName.ToNewCString();

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetPrettyName(nsString& prettyName)
{
  if (mDepth == 1) {
    // Depth == 1 means we are on the news server level
    // override the name here to say "News.Foo.Bar"
    prettyName = PL_strdup("News.Foo.Bar");
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
#ifdef DEBUG_sspitzer_
  printf("nsMsgNewsFolder::GetPath()\n");
#endif
  nsFileSpec nopath("");
  if (mPath == nopath) {
    nsresult rv = nsNewsURI2Path(kNewsRootURI, mURI, mPath);
    if (NS_FAILED(rv)) return rv;
  }
  aPathName = mPath;
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::DeleteMessage(nsIMessage *message)
{
	nsresult rv = GetDatabase();
	if(NS_SUCCEEDED(rv))
	{
		nsIMsgDBHdr *msgDBHdr = nsnull;
		//We know from our factory that news message resources are going to be
  	//nsNewsMessages.
	  nsNewsMessage *newsMessage = NS_STATIC_CAST(nsNewsMessage*, message);

		rv = newsMessage->GetMsgDBHdr(&msgDBHdr);
		if(NS_SUCCEEDED(rv))
		{
			rv =mNewsDatabase->DeleteHeader(msgDBHdr, nsnull, PR_TRUE, PR_TRUE);
			NS_IF_RELEASE(msgDBHdr);
		}
	}
	return rv;}

NS_IMETHODIMP nsMsgNewsFolder::CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message)
{

	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyChange(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                         nsIDBChangeListener * aInstigator)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyDeleted(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                          nsIDBChangeListener * aInstigator)
{
	nsIMsgDBHdr *pMsgDBHdr;
	nsresult rv = mNewsDatabase->GetMsgHdrForKey(aKeyChanged, &pMsgDBHdr);
	if(NS_SUCCEEDED(rv))
	{
		nsIMessage *message = nsnull;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, &message);
		if(NS_SUCCEEDED(rv))
		{
			nsISupports *msgSupports;
			if(NS_SUCCEEDED(message->QueryInterface(kISupportsIID, (void**)&msgSupports)))
			{
				PRUint32 i;
				for(i = 0; i < mListeners->Count(); i++)
				{
					nsIFolderListener *listener = (nsIFolderListener*)mListeners->ElementAt(i);
					listener->OnItemRemoved(this, msgSupports);
					NS_RELEASE(listener);
				}
				NS_IF_RELEASE(msgSupports);
			}
			NS_IF_RELEASE(message);
			UpdateSummaryTotals();
		}
		NS_IF_RELEASE(pMsgDBHdr);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnKeyAdded(nsMsgKey aKeyChanged, PRInt32 aFlags, 
                        nsIDBChangeListener * aInstigator)
{
	nsIMsgDBHdr *pMsgDBHdr;
	nsresult rv = mNewsDatabase->GetMsgHdrForKey(aKeyChanged, &pMsgDBHdr);
	if(NS_SUCCEEDED(rv))
	{
		nsIMessage *message;
		rv = CreateMessageFromMsgDBHdr(pMsgDBHdr, &message);
		if(NS_SUCCEEDED(rv))
		{
			nsISupports *msgSupports;
			if(message && NS_SUCCEEDED(message->QueryInterface(kISupportsIID, (void**)&msgSupports)))
			{
				NotifyItemAdded(msgSupports);
				NS_IF_RELEASE(msgSupports);
			}
			UpdateSummaryTotals();
			NS_IF_RELEASE(message);
		}
		NS_IF_RELEASE(pMsgDBHdr);
	}
	return NS_OK;

	return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::OnAnnouncerGoingAway(nsIDBChangeAnnouncer * instigator)
{
	return NS_OK;
}

/* sspitzer:  from mozilla/network/protocol/pop3/mkpop3.c */

static PRInt32
msg_GrowBuffer (PRUint32 desired_size, PRUint32 element_size, PRUint32 quantum,
				char **buffer, PRUint32 *size)
{
  if (*size <= desired_size)
	{
	  char *new_buf;
	  PRUint32 increment = desired_size - *size;
	  if (increment < quantum) /* always grow by a minimum of N bytes */
		increment = quantum;

#ifdef TESTFORWIN16
	  if (((*size + increment) * (element_size / sizeof(char))) >= 64000)
		{
		  /* Make sure we don't choke on WIN16 */
		  PR_ASSERT(0);
		  return -1;
		}
#endif /* DEBUG */

	  new_buf = (*buffer
				 ? (char *) PR_Realloc (*buffer, (*size + increment)
										* (element_size / sizeof(char)))
				 : (char *) PR_Malloc ((*size + increment)
									  * (element_size / sizeof(char))));
	  if (! new_buf)
      return -1; // NS_ERROR_OUT_OF_MEMORY;
	  *buffer = new_buf;
	  *size += increment;
	}
  return 0;
}

/* Take the given buffer, tweak the newlines at the end if necessary, and
   send it off to the given routine.  We are guaranteed that the given
   buffer has allocated space for at least one more character at the end. */
static PRInt32
msg_convert_and_send_buffer(char* buf, PRUint32 length, PRBool convert_newlines_p,
							PRInt32 (*per_line_fn) (char *line,
												  PRUint32 line_length,
												  void *closure),
							void *closure)
{
  /* Convert the line terminator to the native form.
   */
  char* newline;

  PR_ASSERT(buf && length > 0);
  if (!buf || length <= 0) return -1;
  newline = buf + length;
  PR_ASSERT(newline[-1] == CR || newline[-1] == LF);
  if (newline[-1] != CR && newline[-1] != LF) return -1;

  if (!convert_newlines_p)
	{
	}
#if (LINEBREAK_LEN == 1)
  else if ((newline - buf) >= 2 &&
		   newline[-2] == CR &&
		   newline[-1] == LF)
	{
	  /* CRLF -> CR or LF */
	  buf [length - 2] = LINEBREAK[0];
	  length--;
	}
  else if (newline > buf + 1 &&
		   newline[-1] != LINEBREAK[0])
	{
	  /* CR -> LF or LF -> CR */
	  buf [length - 1] = LINEBREAK[0];
	}
#else
  else if (((newline - buf) >= 2 && newline[-2] != CR) ||
		   ((newline - buf) >= 1 && newline[-1] != LF))
	{
	  /* LF -> CRLF or CR -> CRLF */
	  length++;
	  buf[length - 2] = LINEBREAK[0];
	  buf[length - 1] = LINEBREAK[1];
	}
#endif

  return (*per_line_fn)(buf, length, closure);
}

static int
msg_LineBuffer (const char *net_buffer, PRInt32 net_buffer_size,
				char **bufferP, PRUint32 *buffer_sizeP, PRUint32 *buffer_fpP,
				PRBool convert_newlines_p,
				PRInt32 (*per_line_fn) (char *line, PRUint32 line_length,
									  void *closure),
				void *closure)
{
  printf("msg_LineBuffer()\n");

  int status = 0;
  if (*buffer_fpP > 0 && *bufferP && (*bufferP)[*buffer_fpP - 1] == CR &&
	  net_buffer_size > 0 && net_buffer[0] != LF) {
	/* The last buffer ended with a CR.  The new buffer does not start
	   with a LF.  This old buffer should be shipped out and discarded. */
	PR_ASSERT(*buffer_sizeP > *buffer_fpP);
	if (*buffer_sizeP <= *buffer_fpP) return -1;
	status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										 convert_newlines_p,
										 per_line_fn, closure);
	if (status < 0) return status;
	*buffer_fpP = 0;
  }
  while (net_buffer_size > 0)
	{
	  const char *net_buffer_end = net_buffer + net_buffer_size;
	  const char *newline = 0;
	  const char *s;


	  for (s = net_buffer; s < net_buffer_end; s++)
		{
		  /* Move forward in the buffer until the first newline.
			 Stop when we see CRLF, CR, or LF, or the end of the buffer.
			 *But*, if we see a lone CR at the *very end* of the buffer,
			 treat this as if we had reached the end of the buffer without
			 seeing a line terminator.  This is to catch the case of the
			 buffers splitting a CRLF pair, as in "FOO\r\nBAR\r" "\nBAZ\r\n".
		   */
		  if (*s == CR || *s == LF)
			{
			  newline = s;
			  if (newline[0] == CR)
				{
				  if (s == net_buffer_end - 1)
					{
					  /* CR at end - wait for the next character. */
					  newline = 0;
					  break;
					}
				  else if (newline[1] == LF)
					/* CRLF seen; swallow both. */
					newline++;
				}
			  newline++;
			  break;
			}
		}

	  /* Ensure room in the net_buffer and append some or all of the current
		 chunk of data to it. */
	  {
		const char *end = (newline ? newline : net_buffer_end);
		PRUint32 desired_size = (end - net_buffer) + (*buffer_fpP) + 1;

		if (desired_size >= (*buffer_sizeP))
		  {
			status = msg_GrowBuffer (desired_size, sizeof(char), 1024,
									 bufferP, buffer_sizeP);
			if (status < 0) return status;
		  }
		nsCRT::memcpy((*bufferP) + (*buffer_fpP), net_buffer, (end - net_buffer));
		(*buffer_fpP) += (end - net_buffer);
	  }

	  /* Now *bufferP contains either a complete line, or as complete
		 a line as we have read so far.

		 If we have a line, process it, and then remove it from `*bufferP'.
		 Then go around the loop again, until we drain the incoming data.
	   */
	  if (!newline)
		return 0;

	  status = msg_convert_and_send_buffer(*bufferP, *buffer_fpP,
										   convert_newlines_p,
										   per_line_fn, closure);
	  if (status < 0) return status;

	  net_buffer_size -= (newline - net_buffer);
	  net_buffer = newline;
	  (*buffer_fpP) = 0;
	}
  return 0;
}

nsresult 
nsMsgNewsFolder::LoadNewsrcFileAndCreateNewsgroups(nsFileSpec &newsrcFile)
{
	char *ibuffer = 0;
	PRUint32 ibuffer_size = 0;
	PRUint32 ibuffer_fp = 0;
  int size = 1024;
  char *buffer;
  buffer = new char[size];
  int status = 0;
  PRInt32 numread = 0;

  PR_FREEIF(m_optionLines);

  if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

  nsInputFileStream inputStream(newsrcFile); 
  while (1) {
    numread = inputStream.read(buffer, size);
    if (numread == 0) {
      break;
    }
    else {
#ifdef DEBUG_sspitzer
      printf("%d: %s\n", numread, buffer);
#endif
      msg_LineBuffer(buffer, numread,
                     &ibuffer, &ibuffer_size, &ibuffer_fp,
                     FALSE,
#ifdef XP_OS2
                     (PRInt32 (_Optlink*) (char*,PRUint32,void*))
#endif /* XP_OS2 */
                     nsMsgNewsFolder::ProcessLine_s, this);
      if (numread <= 0) {
        break;
      }
    }
  }

  if (status == 0 && ibuffer_fp > 0) {
    status = ProcessLine_s(ibuffer, ibuffer_fp, this);
    ibuffer_fp = 0;
  }

  inputStream.close();
  delete [] buffer;
  
  return NS_OK;
}

PRInt32
nsMsgNewsFolder::ProcessLine_s(char* line, PRUint32 line_size, void* closure)
{
  printf("nsMsgNewsFolder::ProcessLine_s()\n");
	return ((nsMsgNewsFolder*) closure)->ProcessLine(line, line_size);
}

PRInt32
nsMsgNewsFolder::ProcessLine(char* line, PRUint32 line_size)
{
  printf("nsMsgNewsFolder::ProcessLine()\n");

	/* guard against blank line lossage */
	if (line[0] == '#' || line[0] == CR || line[0] == LF) return 0;

	line[line_size] = 0;

	if ((line[0] == 'o' || line[0] == 'O') &&
		!PL_strncasecmp (line, "options", 7)) {
		return RememberLine(line);
	}

	char *s;
	char *end = line + line_size;
#ifdef HAVE_PORT
	static msg_NewsArtSet *set;
#endif

	for (s = line; s < end; s++)
		if (*s == ':' || *s == '!')
			break;
	
	if (*s == 0) {
		/* What is this?? Well, don't just throw it away... */
		return RememberLine(line);
	}

#ifdef HAVE_PORT
	set = msg_NewsArtSet::Create(s + 1, this);
	if (!set) return NS_OUT_OF_MEMORY;
#endif

	PRBool subscribed = (*s == ':');
	*s = '\0';

	if (strlen(line) == 0)
	{
#ifdef HAVE_PORT
		delete set;
#endif
		return 0;
	}
  
  if (subscribed) {
    printf("subscribed: %s\n", line);

    // were subscribed, so add it
    nsIMsgFolder *child = nsnull;
    nsAutoString currentFolderNameStr(line);
    AddSubfolder(currentFolderNameStr,&child);
    NS_IF_RELEASE(child);
    child = nsnull;
  }
  else {
    printf("NOT subscribed: %s\n", line);
  }

#ifdef HAVE_PORT
	MSG_FolderInfoNews* info;

	if (subscribed && IsCategoryContainer(line))
	{
		info = new MSG_FolderInfoCategoryContainer(line, set, subscribed,
												   this,
												   m_hostinfo->GetDepth() + 1);
		msg_GroupRecord* group = FindOrCreateGroup(line);
		// Go add all of our categories to the newsrc.
		AssureAllDescendentsLoaded(group);
		msg_GroupRecord* end = group->GetSiblingOrAncestorSibling();
		msg_GroupRecord* child;
		for (child = group->GetNextAlphabetic() ;
			 child != end ;
			 child = child->GetNextAlphabetic()) {
			PR_ASSERT(child);
			if (!child) break;
			char* fullname = child->GetFullName();
			if (!fullname) break;
			MSG_FolderInfoNews* info = FindGroup(fullname);
			if (!info) {	// autosubscribe, if we haven't seen this one.
				char* groupLine = PR_smprintf("%s:", fullname);
				if (groupLine) {
					ProcessLine(groupLine, PL_strlen(groupLine));
					XP_FREE(groupLine);
				}
			}
			delete [] fullname;
		}
	}
	else
		info = new MSG_FolderInfoNews(line, set, subscribed, this,
							   m_hostinfo->GetDepth() + 1);

	if (!info) return -1; // NS_ERROR_OUT_OF_MEMORY;

	// for now, you can't subscribe to category by itself.
	if (! info->IsCategory())
	{
		XPPtrArray* infolist = (XPPtrArray*) m_hostinfo->GetSubFolders();
		infolist->Add(info);
	}

	m_groups->Add(info);

	// prime the folder info from the folder cache while it's still around.
	// Except this might disable the update of new counts - check it out...
	m_master->InitFolderFromCache (info);
#endif /* HAVE_PORT */

  return 0;
}

PRInt32
nsMsgNewsFolder::RememberLine(char* line)
{
	char* new_data;
	if (m_optionLines) {
		new_data =
			(char *) PR_Realloc(m_optionLines,
								PL_strlen(m_optionLines)
								+ PL_strlen(line) + 4);
	} else {
		new_data = (char *) PR_Malloc(PL_strlen(line) + 3);
	}
	if (!new_data) return -1; // NS_ERROR_OUT_OF_MEMORY;
	PL_strcpy(new_data, line);
	PL_strcat(new_data, LINEBREAK);

	m_optionLines = new_data;

	return 0;

}


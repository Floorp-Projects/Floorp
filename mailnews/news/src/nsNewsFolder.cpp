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
#include "nsMsgNewsCID.h"
#include "nsNewsMessage.h"
#include "nsMsgUtils.h"
#include "nsNewsUtils.h"

#include "nsCOMPtr.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsINewsDatabase.h"
#include "nsMsgBaseCID.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID,	NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#define PREF_NEWS_MAX_HEADERS_TO_SHOW "news.max_headers_to_show"
#define PREF_NEWS_ABBREVIATE_PRETTY_NAMES "news.abbreviate_pretty_name"
#define NEWSRC_FILE_BUFFER_SIZE 1024

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

nsMsgNewsFolder::nsMsgNewsFolder(void) : nsMsgLineBuffer(nsnull, PR_FALSE),
    mPath(nsnull), mExpungedBytes(0), mGettingNews(PR_FALSE),
    mInitialized(PR_FALSE), mOptionLines(nsnull), mHostname(nsnull)
{
  mIsNewsHost = PR_FALSE;
  mIsNewsHostInitialized = PR_FALSE;
  /* we're parsing the newsrc file, and the line breaks are platform specific.
   * if MSG_LINEBREAK != CRLF, then we aren't looking for CRLF 
   */
  if (PL_strcmp(MSG_LINEBREAK, CRLF)) {
    SetLookingForCRLF(PR_FALSE);
  }
//  NS_INIT_REFCNT(); done by superclass
}

nsMsgNewsFolder::~nsMsgNewsFolder(void)
{
	if (mPath) {
		delete mPath;
    mPath = nsnull;
  }

  // mHostname allocated in nsGetNewsHostName() with new char[]
  if (mHostname) {
    delete [] mHostname;
    mHostname = nsnull;
  }

  PR_FREEIF(mOptionLines);
  mOptionLines = nsnull;
}

NS_IMPL_ADDREF_INHERITED(nsMsgNewsFolder, nsMsgDBFolder)
NS_IMPL_RELEASE_INHERITED(nsMsgNewsFolder, nsMsgDBFolder)

NS_IMETHODIMP nsMsgNewsFolder::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;
	if (aIID.Equals(nsIMsgNewsFolder::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMsgNewsFolder*, this);
	}              

	if(*aInstancePtr)
	{
		AddRef();
		return NS_OK;
	}

	return nsMsgFolder::QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////

PRBool
nsMsgNewsFolder::isNewsHost() 
{
  if (mIsNewsHostInitialized) {
    return mIsNewsHost;
  }

  // this will do for now.  Eventually, this will go away...
  PRInt32 uriLen = PL_strlen(mURI);

  // if we are shorter than news://, we are too short to be a host
  if (uriLen <= kNewsRootURILen) {
    mIsNewsHost = PR_FALSE;
  }
  else {
    if (PL_strncmp(mURI, kNewsRootURI, kNewsRootURILen) == 0) {
      // if we get here, mURI looks like this:  news://x
      // where x is non-empty, and may contain "/"
      char *rightAfterTheRoot = mURI+kNewsRootURILen+1;
#ifdef DEBUG_NEWS
      printf("search for a slash in %s\n",rightAfterTheRoot);
#endif
      if (!PL_strstr(rightAfterTheRoot,"/")) {
        // there is no slashes after news://,
        // so mURI is of the form news://x
        mIsNewsHost=PR_TRUE;
      }
      else {
        // there is another slash, so mURI is of the form
        // news://x/y, so it is not a host
        mIsNewsHost=PR_FALSE;
      }
    }
    else {
      // mURI doesn't start with news:// so it can't be a host
      mIsNewsHost=PR_FALSE;
    }
  }
  mIsNewsHostInitialized = PR_TRUE;
  return mIsNewsHost;
}

nsresult
nsMsgNewsFolder::CreateSubFolders(nsFileSpec &path)
{
  nsresult rv = NS_OK;

  char *hostname;
  rv = GetHostname(&hostname);
  if (NS_FAILED(rv)) return rv;
      
  if (isNewsHost()) {  
#ifdef DEBUG_NEWS
    printf("CreateSubFolders:  %s = %s\n", mURI, (const char *)path);
#endif

    char *newsrcFilePathStr = nsnull;
    
    //Are we assured this is the server for this folder?
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = GetServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;
  
    nsCOMPtr<nsINntpIncomingServer> nntpServer;
    rv = server->QueryInterface(nsINntpIncomingServer::GetIID(),
                                getter_AddRefs(nntpServer));
    if (NS_FAILED(rv)) return rv;
    
    rv = nntpServer->GetNewsrcFilePath(&newsrcFilePathStr);
    if (NS_FAILED(rv)) return rv;
      
    nsFileSpec newsrcFile(newsrcFilePathStr);
    PR_FREEIF(newsrcFilePathStr);
    newsrcFilePathStr = nsnull;

    if (NS_SUCCEEDED(rv)) {
#ifdef DEBUG_NEWS
      printf("uri = %s newsrc file = %s\n", mURI, (const char *)newsrcFile);
#endif
      rv = LoadNewsrcFileAndCreateNewsgroups(newsrcFile);
    }
  }
  else {
#ifdef DEBUG_NEWS
    printf("%s is not a host, so it has no newsgroups.  (what about categories??)\n", mURI);
#endif
    rv = NS_OK;
  }
  
  PR_FREEIF(hostname);

  return rv;
}

nsresult
nsMsgNewsFolder::AddSubfolder(nsAutoString name, nsIMsgFolder **child, char *setStr)
{
	if (!child)
		return NS_ERROR_NULL_POINTER;

  if (!setStr)
    return NS_ERROR_NULL_POINTER;
  
#ifdef DEBUG_NEWS
  nsString nameStr(name,eOneByte);
  printf("AddSubfolder(%s,??,%s)\n",nameStr.GetBuffer(),setStr);
#endif
  
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv); 

	if(NS_FAILED(rv))
		return rv;

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

	rv = folder->SetFlag(MSG_FOLDER_FLAG_NEWSGROUP);
  if (NS_FAILED(rv))
    return rv;        
  
  nsCOMPtr<nsIMsgNewsFolder> newsFolder(do_QueryInterface(res, &rv));
  if (NS_FAILED(rv))
    return rv;        
  
  rv = newsFolder->SetUnreadSetStr(setStr);
  if (NS_FAILED(rv))
    return rv;
  
#ifdef DEBUG_NEWS
  char *testStr = nsnull;
  rv = newsFolder->GetUnreadSetStr(&testStr);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  printf("set str = %s\n",testStr);
  if (testStr) {
    delete [] testStr;
    testStr = nsnull;
  }
#endif
 
	//convert to an nsISupports before appending
	nsCOMPtr<nsISupports> folderSupports(do_QueryInterface(folder));
	if(folderSupports)
		mSubFolders->AppendElement(folderSupports);
	*child = folder;
	NS_ADDREF(*child);

	return rv;
}


nsresult nsMsgNewsFolder::ParseFolder(nsFileSpec& path)
{
  PR_ASSERT(0);
 	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgNewsFolder::Enumerate(nsIEnumerator **result)
{
  nsresult rv = NS_OK;

  // for now, news folders contain both messages and folders
  // server is a folder, and it contains folders
  // newsgroup is a folder, and it contains messages
  //
  // eventually the top level server will not be a folder
  // and news folders will only contain messages
  nsIEnumerator* folders;
  nsIEnumerator* messages;
  rv = GetSubFolders(&folders);
  if (NS_FAILED(rv)) return rv;
  rv = GetMessages(&messages);
  if (NS_FAILED(rv)) return rv;
  return NS_NewConjoiningEnumerator(folders, messages, 
                                    (nsIBidirectionalEnumerator**)result);
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
  if (!mInitialized) {
	nsresult rv;
	nsCOMPtr<nsIFileSpec> pathSpec;
	rv = GetPath(getter_AddRefs(pathSpec));
	if (NS_FAILED(rv)) return rv;

	nsFileSpec path;
	rv = pathSpec->GetFileSpec(&path);
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
	if (!mDatabase)
	{
		nsCOMPtr<nsIFileSpec> pathSpec;
		nsresult rv;
		rv = GetPath(getter_AddRefs(pathSpec));
		if (NS_FAILED(rv)) return rv;

		nsresult folderOpen = NS_OK;
		nsCOMPtr <nsIMsgDatabase> newsDBFactory;

		rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
		if (NS_SUCCEEDED(rv) && newsDBFactory)
		{
			folderOpen = newsDBFactory->Open(pathSpec, PR_TRUE, PR_FALSE, getter_AddRefs(mDatabase));
#ifdef DEBUG_NEWS
      if (NS_SUCCEEDED(folderOpen)) {
        printf ("newsDBFactory->Open() succeeded\n");
      }
      else {
        printf ("newsDBFactory->Open() failed\n");
        return rv;
      }
#endif
		}

		if (mDatabase) {
			rv = mDatabase->AddListener(this);
      if (NS_FAILED(rv)) return rv;
       
      rv = UpdateSummaryTotals(PR_TRUE);
      if (NS_FAILED(rv)) return rv;
		}
	}
	return NS_OK;
}

NS_IMETHODIMP
nsMsgNewsFolder::GetMessages(nsIEnumerator* *result)
{
#ifdef DEBUG_NEWS
  printf("nsMsgNewsFolder::GetMessages(%s)\n",mURI);
#endif
  // number_to_show is a tempory hack to allow newsgroups
  // with thousands of message to work.  the way it works is
  // we return a cropped enumerator back to to the caller
  // instead of the full one.  This gets around the problem
  // where tree layout (and probably other things) don't scale

  PRInt32 number_to_show;
  nsresult rv = NS_OK;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && prefs) {
    rv = prefs->GetIntPref(PREF_NEWS_MAX_HEADERS_TO_SHOW, &number_to_show);
    if (NS_FAILED(rv)) {
      // failed to get the pref...show them all the headers
      number_to_show = 0;
    }
  }
  else {
    // failed to get pref service...show them all headers
    number_to_show = 0;
  }

  // if the user asks for a negative value, I'll just ignore them
  if (number_to_show < 0) {
  	number_to_show = 0;
  }
  
  if (number_to_show) {
    rv = GetDatabase();
    *result = nsnull;
    
    if(NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIEnumerator> msgHdrEnumerator;
      nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
      rv = mDatabase->EnumerateMessages(getter_AddRefs(msgHdrEnumerator));
      nsCOMPtr <nsISupportsArray> shortlist;
      
      if(NS_SUCCEEDED(rv)) {
        NS_NewISupportsArray(getter_AddRefs(shortlist));
        PRInt32 total = 0;
        for (msgHdrEnumerator->First(); NS_FAILED(msgHdrEnumerator->IsDone()); msgHdrEnumerator->Next()) {
          total++;
        }
#ifdef DEBUG_NEWS
        printf("total = %d\n",total);
#endif
        PRInt32 count = 0;
        for (msgHdrEnumerator->First(); NS_FAILED(msgHdrEnumerator->IsDone()); msgHdrEnumerator->Next()) {
          if (count >= (total - number_to_show)) {
            nsCOMPtr<nsISupports> i;
            rv = msgHdrEnumerator->CurrentItem(getter_AddRefs(i));
            if (NS_FAILED(rv)) return rv;
            shortlist->AppendElement(i);
#ifdef DEBUG_NEWS
            printf("not skipping %d\n", count);
#endif
          }
#ifdef DEBUG_NEWS
          else {
            printf("skipping %d\n", count);
          }
#endif
          count++;
        }
        
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr <nsIBidirectionalEnumerator> enumerator;
          rv = NS_NewISupportsArrayEnumerator(shortlist, getter_AddRefs(enumerator));
          if (NS_SUCCEEDED(rv)) {
            rv = NS_NewMessageFromMsgHdrEnumerator(enumerator,
                                                   this, &messageEnumerator);
            *result = messageEnumerator;
          }
        }
      }
    }
  }
  else {
    rv = GetDatabase();
    *result = nsnull;
    
    if(NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIEnumerator> msgHdrEnumerator;
        nsMessageFromMsgHdrEnumerator *messageEnumerator = nsnull;
        rv = mDatabase->EnumerateMessages(getter_AddRefs(msgHdrEnumerator));
        if(NS_SUCCEEDED(rv))
          rv = NS_NewMessageFromMsgHdrEnumerator(msgHdrEnumerator, this, &messageEnumerator);
        *result = messageEnumerator;
    }
  }

  rv = GetNewMessages();
  if (NS_FAILED(rv)) return rv;
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::BuildFolderURL(char **url)
{
  const char *urlScheme = "news:";

  if(!url)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIFileSpec> pathSpec;
  nsresult rv = GetPath(getter_AddRefs(pathSpec));
  if (NS_FAILED(rv)) return rv;

  nsFileSpec path;
  rv = pathSpec->GetFileSpec(&path);
  if (NS_FAILED(rv)) return rv;
#if defined(XP_MAC)
  nsAutoString tmpPath((nsFilePath)path, eOneByte);	//ducarroz: please don't cast a nsFilePath to char* on Mac
  *url = PR_smprintf("%s%s", urlScheme, tmpPath.GetBuffer());
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
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP nsMsgNewsFolder::CreateSubfolder(const char *folderName)
{
#if 0
	nsresult rv = NS_OK;
    
	nsFileSpec path;
  nsCOMPtr <nsIMsgFolder> child;
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
	nsCOMPtr<nsIMsgDatabase> newsDBFactory;

	rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
	if (NS_SUCCEEDED(rv) && newsDBFactory) {
    nsIMsgDatabase *unusedDB = nsnull;
		rv = newsDBFactory->Open(path, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) &unusedDB);
    
    if (NS_SUCCEEDED(rv) && unusedDB) {
      //need to set the folder name
			nsCOMPtr <nsIDBFolderInfo> folderInfo;
			rv = unusedDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
			if(NS_SUCCEEDED(rv)) {
				//folderInfo->SetNewsgroupName(leafNameFromUser);
			}
      
			//Now let's create the actual new folder
			nsAutoString folderNameStr(folderName);
			rv = AddSubfolder(folderName, getter_AddRefs(child), "");
      unusedDB->SetSummaryValid(PR_TRUE);
      unusedDB->Close(PR_TRUE);
    }
    else {
			path.Delete(PR_FALSE);
      rv = NS_MSG_CANT_CREATE_FOLDER;
    }
	}
	if(NS_SUCCEEDED(rv) && child) {
		nsCOMPtr <nsISupports> folderSupports;
    
		rv = child->QueryInterface(kISupportsIID, getter_AddRefs(folderSupports));
		if(NS_SUCCEEDED(rv)) {
			NotifyItemAdded(folderSupports);
		}
	}
	return rv;
#endif
  PR_ASSERT(0);  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::RemoveSubFolder(nsIMsgFolder *which)
{
#if 0
  // Let the base class do list management
  nsMsgFolder::RemoveSubFolder(which);
#endif
  PR_ASSERT(0);  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::Delete()
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::Rename(const char *newName)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::Adopt(nsIMsgFolder *srcFolder, PRUint32 *outPos)
{
  PR_ASSERT(0);  
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsMsgNewsFolder::GetChildNamed(const char *name, nsISupports ** aChild)
{
  NS_ASSERTION(aChild, "NULL child");

  // will return nsnull if we can't find it
  *aChild = nsnull;

  nsCOMPtr <nsIMsgFolder> folder;

  PRUint32 cnt;
  nsresult rv = mSubFolders->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  PRUint32 count = cnt;

  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr <nsISupports> supports;
    supports = mSubFolders->ElementAt(i);

    if(NS_SUCCEEDED(supports->QueryInterface(kISupportsIID, getter_AddRefs(folder)))) {
      PRUnichar *folderName;
      
      folder->GetName(&folderName);
      
      // case-insensitive compare is probably LCD across OS filesystems
      if (folderName && nsCRT::strcasecmp(folderName, name)!=0) {
        *aChild = folder;
        PR_FREEIF(folderName);
        return NS_OK;
      }
      PR_FREEIF(folderName);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetName(PRUnichar **name)
{
  if(!name)
    return NS_ERROR_NULL_POINTER;

	nsAutoString folderName;
	nsNewsURI2Name(kNewsRootURI, mURI, folderName);
	*name = folderName.ToNewUnicode();

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetPrettyName(PRUnichar ** prettyName)
{
  nsresult rv = NS_OK;

  if (!prettyName)
    return NS_ERROR_NULL_POINTER;

  rv = nsMsgFolder::GetPrettyName(prettyName);

  //not ready for prime time yet, as I don't know how to test this yet.
#ifdef NOT_READY_FOR_PRIME_TIME
  // only do this for newsgroup names, not for newsgroup hosts.
  if (!isNewsHost()) {
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    PRInt32 numFullWords;
    rv = prefs->GetIntPref(PREF_NEWS_ABBREVIATE_PRETTY_NAMES, &numFullWords);
    if (NS_FAILED(rv)) return rv;
    
    if (numFullWords != 0) { 
      rv = AbbreviatePrettyName(prettyName, numFullWords);
    }
  }
#endif /* NOT_READY_FOR_PRIME_TIME */
  return rv;
}

// original code from Oleg Rekutin
// rekusha@asan.com
// Public domain, created by Oleg Rekutin
//
// takes a newsgroup name, number of words from the end to leave unabberviated
// the newsgroup name, will get reset to the following format:
// x.x.x, where x is the first letter of each word and with the
// exception of last 'fullwords' words, which are left intact.
// If a word has a dash in it, it is abbreviated as a-b, where
// 'a' is the first letter of the part of the word before the
// dash and 'b' is the first letter of the part of the word after
// the dash
nsresult nsMsgNewsFolder::AbbreviatePrettyName(PRUnichar ** prettyName, PRInt32 fullwords)
{
  if (!prettyName)
		return NS_ERROR_NULL_POINTER;
  
  nsString name(*prettyName);
  PRInt32 totalwords = 0; // total no. of words

  // get the total no. of words
  for (PRInt32 pos = 0;
       (pos++) != name.Length();
       pos = name.FindChar('.', PR_FALSE,pos))
    totalwords ++;

  // get the no. of words to abbreviate
  PRInt32 abbrevnum = totalwords - fullwords;
  if (abbrevnum < 1)
    return NS_OK; // nothing to abbreviate
  
  // build the ellipsis
  nsString out;
  
  out += name[0];
  
  PRInt32    length = name.Length();
  PRInt32    newword = 0;     // == 2 if done with all abbreviated words
  
  fullwords = 0;
  for (PRInt32 i = 1; i < length; i++) {
    if (newword < 2) {
      switch (name[i]) {
      case '.':
        fullwords++;
        // check if done with all abbreviated words...
        if (fullwords == abbrevnum)
          newword = 2;
        else
          newword = 1;
        break;
      case '-':
        newword = 1;
        break;
      default:
        if (newword)
          newword = 0;
        else
          continue;
      }
    }
    out += name[i];
  }

  if (!prettyName)
		return NS_ERROR_NULL_POINTER;
  // we are going to set *prettyName to something else, so free what was there
  
  PR_FREEIF(*prettyName);
  *prettyName = out.ToNewUnicode();
  
  return (*prettyName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


nsresult  nsMsgNewsFolder::GetDBFolderInfoAndDB(nsIDBFolderInfo **folderInfo, nsIMsgDatabase **db)
{
  nsresult openErr=NS_ERROR_UNEXPECTED;
  if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;	//ducarroz: should we use NS_ERROR_INVALID_ARG?
		
	if (!mPath)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr <nsIMsgDatabase> newsDBFactory;
	nsIMsgDatabase *newsDB;

	nsresult rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
	if (NS_SUCCEEDED(rv) && newsDBFactory) {
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(*mPath, getter_AddRefs(dbFileSpec));
		openErr = newsDBFactory->Open(dbFileSpec, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) &newsDB);
	}
  else {
    return rv;
  }

  *db = newsDB;
  if (NS_SUCCEEDED(openErr)&& *db)
    openErr = (*db)->GetDBFolderInfo(folderInfo);
  return openErr;
}

NS_IMETHODIMP nsMsgNewsFolder::UpdateSummaryTotals(PRBool force)
{
#ifdef DEBUG_NEWS
	printf("nsMsgNewsFolder::UpdateSummaryTotals(%s)\n",mURI);
#endif

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
		PR_FREEIF(totalMessagesStr);
		PR_FREEIF(oldTotalMessagesStr);
	}

	if(oldUnreadMessages != mNumUnreadMessages)
	{
		char *oldUnreadMessagesStr = PR_smprintf("%d", oldUnreadMessages);
		char *totalUnreadMessages = PR_smprintf("%d",mNumUnreadMessages);
		NotifyPropertyChanged("TotalUnreadMessages", oldUnreadMessagesStr, totalUnreadMessages);
		PR_FREEIF(totalUnreadMessages);
		PR_FREEIF(oldUnreadMessagesStr);
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
#if 0
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
#else
  NS_ASSERTION(0,"GetDeletable() not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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
#if 0
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
#else
  NS_ASSERTION(0,"GetCanBeRenamed() not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP nsMsgNewsFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::GetSizeOnDisk(PRUint32 *size)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgNewsFolder::GetUsername(char** userName)
{
  *userName = PL_strdup("");
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::GetHostname(char** hostName)
{
  nsresult rv = NS_OK;
  
  if (!mHostname) {
    // mHostname gets freed in the destructor
    rv = nsGetNewsHostName(kNewsRootURI, mURI, &mHostname);
    if (NS_FAILED(rv)) return rv;
  }
  
	if (mHostname) {
    *hostName = PL_strdup(mHostname);
		if(!*hostName)
			return NS_ERROR_OUT_OF_MEMORY;
	}
  else {
    return NS_ERROR_FAILURE;
  }
  
	return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::RememberPassword(const char *password)
{
#ifdef HAVE_DB
  NewsDB *newsDb = nsnull;
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

NS_IMETHODIMP nsMsgNewsFolder::GetPath(nsIFileSpec** aPathName)
{
  nsresult rv;
  if (! mPath) {
    mPath = new nsNativeFileSpec("");
    if (! mPath)
    	return NS_ERROR_OUT_OF_MEMORY;

    rv = nsNewsURI2Path(kNewsRootURI, mURI, *mPath);
    if (NS_FAILED(rv)) return rv;
  }
  rv = NS_NewFileSpecWithSpec(*mPath, aPathName);
  return rv;
}

/* this is news, so remember that DeleteMessage is really CANCEL */
NS_IMETHODIMP nsMsgNewsFolder::DeleteMessages(nsISupportsArray *messages,
                                              nsITransactionManager *txnMgr, PRBool deleteStorage)
{
  nsresult rv = NS_OK;
  
  if (!messages) {
    // nothing to CANCEL
    return NS_ERROR_NULL_POINTER;
  }

  NS_WITH_SERVICE(nsINntpService, nntpService, kNntpServiceCID, &rv);
  
  if (NS_SUCCEEDED(rv) && nntpService) {
    char *hostname;
    rv = GetHostname(&hostname);
    if (NS_FAILED(rv)) return rv;
    PRUnichar *newsgroupname;
    rv = GetName(&newsgroupname);
	nsCString asciiName(newsgroupname);
    if (NS_FAILED(rv)) {
      PR_FREEIF(hostname);
      return rv;
    }
    
    rv = nntpService->CancelMessages(hostname, asciiName.GetBuffer(), messages, nsnull, nsnull, nsnull);
    
    PR_FREEIF(hostname);
    PR_FREEIF(newsgroupname);
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::GetNewMessages()
{
  nsresult rv = NS_OK;

#ifdef DEBUG_NEWS
  printf("GetNewMessages (for news)  uri = %s\n",mURI);
#endif

  if (isNewsHost()) {
#ifdef DEBUG_NEWS
		printf("sorry, can't get news for entire news server yet.  try it on a newsgroup by newsgroup level\n");
#endif 
		return NS_OK;
  }

#ifdef DEBUG_NEWS
  char *setStr = nsnull;
  // caller needs to use delete [] to free
  rv = GetUnreadSetStr(&setStr);
  if (NS_FAILED(rv)) return rv;
  if (setStr) {
    printf("GetNewMessage with setStr = %s\n", setStr);
  }
  if (setStr) {
    delete [] setStr;
    setStr = nsnull;
  }
#endif
  
  NS_WITH_SERVICE(nsINntpService, nntpService, kNntpServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  //Are we assured this is the server for this folder?
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = GetServer(getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsINntpIncomingServer> nntpServer;
  rv = server->QueryInterface(nsINntpIncomingServer::GetIID(),
                              getter_AddRefs(nntpServer));
  if (NS_FAILED(rv)) return rv;
  
#ifdef DEBUG_NEWS
  printf("Getting new news articles....\n");
#endif
  rv = nntpService->GetNewNews(nntpServer, mURI, nsnull, nsnull);
  return rv;
}

NS_IMETHODIMP nsMsgNewsFolder::CreateMessageFromMsgDBHdr(nsIMsgDBHdr *msgDBHdr, nsIMessage **message)
{
  nsresult rv; 
  NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
  if (NS_FAILED(rv)) return rv;

	char* msgURI = nsnull;
	nsFileSpec path;
	nsMsgKey key;
	nsCOMPtr <nsIRDFResource> res;

	rv = msgDBHdr->GetMessageKey(&key);
  if (NS_FAILED(rv)) return rv;
  
  rv = nsBuildNewsMessageURI(mURI, key, &msgURI);
  if (NS_FAILED(rv)) return rv;
  
  rv = rdfService->GetResource(msgURI, getter_AddRefs(res));
  
  PR_FREEIF(msgURI);
  msgURI = nsnull;
  
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIDBMessage> messageResource = do_QueryInterface(res);
  if(messageResource) {
    messageResource->SetMsgDBHdr(msgDBHdr);
    *message = messageResource;
    NS_IF_ADDREF(*message);
  }
  
	return rv;
}

nsresult 
nsMsgNewsFolder::LoadNewsrcFileAndCreateNewsgroups(nsFileSpec &newsrcFile)
{
  nsInputFileStream newsrcStream(newsrcFile); 
	nsresult rv = NS_OK;
  PRInt32 numread = 0;

  if (NS_FAILED(m_inputStream.GrowBuffer(NEWSRC_FILE_BUFFER_SIZE))) {
#ifdef DEBUG_NEWS
    printf("GrowBuffer failed\n");
#endif
    return NS_ERROR_FAILURE;
  }
	
  while (1) {
    numread = newsrcStream.read(m_inputStream.GetBuffer(), NEWSRC_FILE_BUFFER_SIZE);
#ifdef DEBUG_NEWS
    printf("numread == %d\n", numread);
#endif
    if (numread == 0) {
      break;
    }
    else {
      rv = BufferInput(m_inputStream.GetBuffer(), numread);
      if (NS_FAILED(rv)) {
#ifdef DEBUG_NEWS
        printf("bufferInput did not return NS_OK\n");
#endif
        break;
      }
    }
  }

  newsrcStream.close();
  
  return rv;
}


PRInt32
nsMsgNewsFolder::HandleLine(char* line, PRUint32 line_size)
{
	/* guard against blank line lossage */
	if (line[0] == '#' || line[0] == CR || line[0] == LF) return 0;

	line[line_size] = 0;

	if ((line[0] == 'o' || line[0] == 'O') &&
		!PL_strncasecmp (line, "options", 7)) {
		return RememberLine(line);
	}

	char *s = nsnull;
  char *setStr = nsnull;
	char *end = line + line_size;

	for (s = line; s < end; s++)
		if (*s == ':' || *s == '!')
			break;
	
	if (*s == 0) {
		/* What is this?? Well, don't just throw it away... */
		return RememberLine(line);
	}
    
	PRBool subscribed = (*s == ':');
  setStr = s+1;
	*s = '\0';
  
	if (PL_strlen(line) == 0) {
		return 0;
	}
 
  // previous versions of Communicator poluted the
  // newsrc files with articles
  // (this would happen when you clicked on a link like
  // news://news.mozilla.org/3746EF3F.6080309@netscape.com)
  //
  // legal newsgroup names can't contain @ or %
  // 
  // News group names are structured into parts separated by dots, 
  // for example "netscape.public.mozilla.mail-news". 
  // Each part may be up to 14 characters long, and should consist 
  // only of letters, digits, "+" and "-", with at least one letter
  //
  // @ indicates an article and %40 is @ escaped.
  // previous versions of Communicator also dumped
  // the escaped version into the newsrc file
  //
  // So lines like this in a newsrc file should be ignored:
  // 3746EF3F.6080309@netscape.com:
  // 3746EF3F.6080309%40netscape.com:
  if (PL_strstr(line,"@") || PL_strstr(line,"%40")) {
#ifdef DEBUG_NEWS
    printf("skipping %s.  it contains @ or %%40\n",line);
#endif
  	subscribed = PR_FALSE;
  }

  if (subscribed) {
#ifdef DEBUG_NEWS
    printf("subscribed: %s\n", line);
#endif

    // we're subscribed, so add it
    nsCOMPtr <nsIMsgFolder> child;
    nsAutoString currentFolderNameStr(line);
    
    nsresult rv = AddSubfolder(currentFolderNameStr,getter_AddRefs(child), setStr);
    
    if (NS_FAILED(rv)) return -1;
  }
  else {
#ifdef DEBUG_NEWS
    printf("NOT subscribed: %s\n", line);
#endif
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
					HandleLine(groupLine, PL_strlen(groupLine));
					PR_FREEIF(groupLine);
          groupLine = nsnull;
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
	if (mOptionLines) {
		new_data =
			(char *) PR_Realloc(mOptionLines,
								PL_strlen(mOptionLines)
								+ PL_strlen(line) + 4);
	} else {
		new_data = (char *) PR_Malloc(PL_strlen(line) + 3);
	}
	if (!new_data) return -1; // NS_ERROR_OUT_OF_MEMORY;
	PL_strcpy(new_data, line);
	PL_strcat(new_data, MSG_LINEBREAK);

	mOptionLines = new_data;

	return 0;

}

nsresult nsMsgNewsFolder::ForgetLine()
{
  PR_FREEIF(mOptionLines);
  mOptionLines = nsnull;
  return NS_OK;
}

// caller needs to use delete [] to free
NS_IMETHODIMP nsMsgNewsFolder::GetUnreadSetStr(char * *aUnreadSetStr)
{
  nsresult rv;
  
  if (!aUnreadSetStr) return NS_ERROR_NULL_POINTER;

  rv = GetDatabase();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mDatabase, "no database!");
  if (!mDatabase) return NS_ERROR_NULL_POINTER;

  nsMsgKeySet * set = nsnull;
  
  nsCOMPtr<nsINewsDatabase> db(do_QueryInterface(mDatabase, &rv));
  if (NS_FAILED(rv))
	return rv;        

  rv = db->GetUnreadSet(&set);
  if (NS_FAILED(rv)) return rv;
      
  *aUnreadSetStr = set->Output();

  if (!*aUnreadSetStr) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsMsgNewsFolder::SetUnreadSetStr(char * aUnreadSetStr)
{
  nsresult rv;
    
  if (!aUnreadSetStr) return NS_ERROR_NULL_POINTER;

  rv = GetDatabase();
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(mDatabase, "no database!");
  if (!mDatabase) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsINewsDatabase> db(do_QueryInterface(mDatabase, &rv));
  if (NS_FAILED(rv))
	return rv;        

  rv = db->SetUnreadSet(aUnreadSetStr);
  return rv;
}

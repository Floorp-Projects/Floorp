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

#include "msgCore.h"    // precompiled header...

#include "nsLocalMailFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "prprf.h"
#include "nsIRDFResourceFactory.h"
#include "nsISupportsArray.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsMailDataBase.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::IID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////

static nsString*
nsGetNameFromPath(const nsNativeFileSpec* path)
{
  const char* pathStr = (const char*)path;
  char* ptr = PL_strrchr(pathStr, '/');
  if (ptr)
    return new nsString(ptr + 1);
  else
    return new nsString(pathStr);
}

static const char kRootPrefix[] = "mailbox:/";
static const char kMsgRootFolderPref[] = "mailnews.rootFolder";

static nsresult
nsURI2Path(const char* uriStr, nsNativeFileSpec& pathResult)
{
  nsAutoString path;
  nsAutoString uri = uriStr;
  if (uri.Find(kRootPrefix) != 0)     // if doesn't start with kRootPrefix
    return NS_ERROR_FAILURE;

  // get mailbox root preference
#if 0
  nsIPref* prefs;
  nsresult rv;
  rv = nsServiceManager::GetService(kPrefCID, kIPrefIID,
                                    (nsISupports**)&prefs);
  if (NS_FAILED(rv)) return rv; 

  #define ROOT_PATH_LENGTH 128 
  char rootPath[ROOT_PATH_LENGTH];
  int rootLen = ROOT_PATH_LENGTH;
  rv = prefs->GetCharPref(kMsgRootFolderPref, rootPath, &rootLen);
  nsServiceManager::ReleaseService(kPrefCID, prefs);
  if (NS_FAILED(rv))
    return rv; 
#else
  char rootPath[] = "c:\\program files\\netscape\\users\\putterman\\mail";
#endif
  path.Append(rootPath);
  uri.Cut(0, nsCRT::strlen(kRootPrefix));

  PRInt32 uriLen = uri.Length();
  PRInt32 trailPos = uri.RFind('/');
  if (trailPos != -1 && (trailPos == uriLen - 1)) {
    // delete trailing slash
    uri.Left(uri, uriLen - 1);
    uriLen--;
  }


  PRInt32 pos;
  while(uriLen > 0) {
    nsAutoString folderName;

   	PRInt32 leadingPos = uri.Find('/');
	//if it's the first character then remove it.
	if(leadingPos == 0)
	{
		uri.Cut(0, 1);
		uriLen--;
	}

	pos = uri.Find('/');
    if (pos < 0)
      pos = uriLen;

    PRInt32 cnt = uri.Left(folderName, pos);
    NS_ASSERTION(cnt == pos, "something wrong with nsString");
    path.Append("\\");
    path.Append(folderName);
    path.Append(".sbd");
    uri.Cut(0, pos);
    uriLen = uri.Length();
  }
  // XXX bogus, nsNativeFileSpec should take an nsString
  char* str = path.ToNewCString();
  pathResult = *new nsNativeFileSpec(str, PR_FALSE);
  delete[] str;
  return NS_OK;
}

static nsresult
nsPath2URI(nsNativeFileSpec& path, const char* *uri)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult
nsURI2Name(const char* uriStr, nsString& name)
{
  nsAutoString uri = uriStr;
  if (uri.Find(kRootPrefix) != 0)     // if doesn't start with kRootPrefix
    return NS_ERROR_FAILURE;
  PRInt32 pos = uri.RFind("/");
  PRInt32 length = uri.Length();
  PRInt32 count = length - (pos + 1);
  return uri.Right(name, count);
}

////////////////////////////////////////////////////////////////////////////////

nsMsgLocalMailFolder::nsMsgLocalMailFolder(const char* uri)
  : nsMsgFolder(uri), mPath(nsnull), mExpungedBytes(0), 
    mHaveReadNameFromDB(PR_FALSE), mGettingMail(PR_FALSE),
    mInitialized(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsMsgLocalMailFolder::~nsMsgLocalMailFolder()
{
}

NS_IMPL_ADDREF(nsMsgLocalMailFolder)
NS_IMPL_RELEASE(nsMsgLocalMailFolder)

NS_IMETHODIMP
nsMsgLocalMailFolder::QueryInterface(REFNSIID iid, void** result)
{
  if (! result)
    return NS_ERROR_NULL_POINTER;

  *result = nsnull;
  if (iid.Equals(nsIMsgLocalMailFolder::IID()) ||
      iid.Equals(kISupportsIID))
  {
    *result = NS_STATIC_CAST(nsIMsgLocalMailFolder*, this);
    AddRef();
    return NS_OK;
  }
  return nsMsgFolder::QueryInterface(iid, result);
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
  if ((len > 4 && name.RFind(".sbd", PR_TRUE) == len - 4))
	  return PR_TRUE;
  return PR_FALSE;
}

nsresult
nsMsgLocalMailFolder::CreateSubFolders(void)
{
  nsresult rv = NS_OK;
  nsAutoString currentFolderName;
  nsNativeFileSpec path;
  rv = GetPath(path);
  if (NS_FAILED(rv)) return rv;
  for (nsDirectoryIterator dir(path); dir; dir++) {
    nsNativeFileSpec currentFolderPath = (nsNativeFileSpec&)dir;
#if 0
    if (currentFolderName)
      delete[] currentFolderName;
#endif
    currentFolderName = currentFolderPath.GetLeafName();
    if (nsShouldIgnoreFile(currentFolderName))
      continue;

    nsAutoString uri;
    uri.Append(mURI);
    uri.Append('/');
/*	if(PL_strcasecmp(mURI,kRootPrefix)==0)
	{
		nsFilePath filePath(path);
		uri.Append(filePath);
		uri.Append('/');
	}
	*/
    uri.Append(currentFolderName);
    char* uriStr = uri.ToNewCString();
    if (uriStr == nsnull) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    // XXX trim off .sbd from uriStr
    nsMsgLocalMailFolder* folder = new nsMsgLocalMailFolder(uriStr);
    delete[] uriStr;
    if (folder == nsnull) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      goto done;
    }
    mSubFolders->AppendElement(NS_STATIC_CAST(nsIMsgFolder*, folder));
  }
  done:
#if 0
  if (currentFolderName)
    delete[] currentFolderName;
#endif
  return rv;
}


nsresult
nsMsgLocalMailFolder::Initialize(void)
{
  nsNativeFileSpec path;
  nsresult rv = GetPath(path);
  if (NS_FAILED(rv)) return rv;
  nsFilePath aFilePath(path);
  PRInt32 newFlags = MSG_FOLDER_FLAG_MAIL;
  if (path.IsDirectory()) {
      newFlags |= (MSG_FOLDER_FLAG_DIRECTORY | MSG_FOLDER_FLAG_ELIDED);
      SetFlag(newFlags);
      return CreateSubFolders();
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
  return NS_OK;
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

NS_IMETHODIMP
nsMsgLocalMailFolder::GetSubFolders(nsIEnumerator* *result)
{
  if (!mInitialized) {
    nsresult rv = Initialize();
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
  // XXX temporary
  nsISupportsArray* messages;
  nsresult rv = NS_NewISupportsArray(&messages);
  if (NS_FAILED(rv))
    return rv;
  NS_ADDREF(messages);
  return messages->Enumerate(result); // empty set for now
}

NS_IMETHODIMP nsMsgLocalMailFolder::BuildFolderURL(char **url)
{
  const char *urlScheme = "mailbox:";

  if(!url)
    return NS_ERROR_NULL_POINTER;

  nsNativeFileSpec path;
  nsresult rv = GetPath(path);
  if (NS_FAILED(rv)) return rv;
  *url = PR_smprintf("%s%s", urlScheme, path);
  return NS_OK;

}

NS_IMETHODIMP nsMsgLocalMailFolder::CreateSubfolder(const char *leafNameFromUser,
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

NS_IMETHODIMP nsMsgLocalMailFolder::RemoveSubFolder(const nsIMsgFolder *which)
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

NS_IMETHODIMP nsMsgLocalMailFolder::Rename (const char *newName)
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

NS_IMETHODIMP nsMsgLocalMailFolder::Adopt(const nsIMsgFolder *srcFolder, PRUint32 *outPos)
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
      nsAutoString folderName;

      folder->GetName(folderName);
      // case-insensitive compare is probably LCD across OS filesystems
      if (!name.EqualsIgnoreCase(folderName)) {
        *aChild = folder;
        return NS_OK;
      }
    }
    NS_RELEASE(supports);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetName(nsString& name)
{
  if(!name)
    return NS_ERROR_NULL_POINTER;

  if (!mHaveReadNameFromDB)
  {
    if (mDepth == 1) 
    {
      SetName("Local Mail");
      mHaveReadNameFromDB = TRUE;
	  name = mName;
	  return NS_OK;
    }
    else
    {
      //Need to read the name from the database
    }
  }
  nsURI2Name(mURI, name);

  return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(nsString& prettyName)
{
  if (mDepth == 1) {
    // Depth == 1 means we are on the mail server level
    // override the name here to say "Local Mail"
    prettyName = PL_strdup("Local Mail");
  }
  else
    return nsMsgFolder::GetPrettyName(prettyName);

  return NS_OK;
}

nsresult  nsMsgLocalMailFolder::GetDBFolderInfoAndDB(nsDBFolderInfo **folderInfo, nsMsgDatabase **db)
{
    nsMailDatabase  *mailDB;

    nsresult openErr;
    if(!db || !folderInfo)
		return NS_ERROR_NULL_POINTER;

	nsFilePath filePath(mPath);
    openErr = nsMailDatabase::Open (filePath, FALSE, &mailDB, FALSE);

    *db = mailDB;
    if (NS_SUCCEEDED(openErr)&& *db)
        *folderInfo = (*db)->GetDBFolderInfo();
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetPath(nsNativeFileSpec& aPathName)
{
  if (mPath == nsnull) {
    nsresult rv = nsURI2Path(mURI, mPath);
    if (NS_FAILED(rv)) return rv;
  }
  aPathName = mPath;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

/**
 * This class creates resources for message folder URIs. It should be
 * registered for the "mailnewsfolder:" prefix.
 */
class nsMsgFolderResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
  nsMsgFolderResourceFactoryImpl(void);
  virtual ~nsMsgFolderResourceFactoryImpl(void);

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};

nsMsgFolderResourceFactoryImpl::nsMsgFolderResourceFactoryImpl(void)
{
  NS_INIT_REFCNT();
}

nsMsgFolderResourceFactoryImpl::~nsMsgFolderResourceFactoryImpl(void)
{
}

NS_IMPL_ISUPPORTS(nsMsgFolderResourceFactoryImpl, nsIRDFResourceFactory::IID());

NS_IMETHODIMP
nsMsgFolderResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgLocalMailFolder *folder = new nsMsgLocalMailFolder(aURI);
  if (! folder)
    return NS_ERROR_OUT_OF_MEMORY;

  folder->QueryInterface(nsIRDFResource::IID(), (void**)aResult);
  return NS_OK;
}

nsresult
NS_NewRDFMsgFolderResourceFactory(nsIRDFResourceFactory** aResult)
{
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsMsgFolderResourceFactoryImpl* factory =
    new nsMsgFolderResourceFactoryImpl();

  if (! factory)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(factory);
  *aResult = factory;
  return NS_OK;
}

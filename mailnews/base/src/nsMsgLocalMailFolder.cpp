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

#include "nsMsgLocalMailFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "prprf.h"

static NS_DEFINE_IID(kIMsgLocalMailFolderIID, NS_IMSGLOCALMAILFOLDER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsMsgLocalMailFolder::nsMsgLocalMailFolder(const char* uri)
:nsMsgFolder(uri)
{
	mHaveReadNameFromDB = PR_FALSE;
	mPathName = nsnull;
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
	if (iid.Equals(kIMsgLocalMailFolderIID) ||
      iid.Equals(kISupportsIID))
	{
		*result = NS_STATIC_CAST(nsIMsgLocalMailFolder*, this);
		AddRef();
		return NS_OK;
	}
	return nsMsgFolder::QueryInterface(iid, result);
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetType(FolderType *type)
{
	if(!type)
		return NS_ERROR_NULL_POINTER;

	*type = FOLDER_MAIL;

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::BuildFolderURL(char **url)
{
	const char *urlScheme = "mailbox:";

	if(!url)
		return NS_ERROR_NULL_POINTER;

	*url = PR_smprintf("%s%s", urlScheme, mPathName);
	return NS_OK;

}

NS_IMETHODIMP nsMsgLocalMailFolder::CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRUint32 *outPos)
{
#ifdef HAVE_PORT
	MsgERR status = 0;
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

NS_IMETHODIMP nsMsgLocalMailFolder::RemoveSubFolder (const nsIMsgFolder *which)
{
	// Let the base class do list management
	nsMsgFolder::RemoveSubFolder (which);

	// Derived class is responsible for managing the subdirectory
#ifdef HAVE_PORT
	if (0 == m_subFolders->GetSize())
		XP_RemoveDirectory (m_pathName, xpMailSubdirectory);
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::Delete ()
{
#ifdef HAVE_PORT
	    nsMsgDatabase   *db;
    // remove the summary file
    MsgERR status = CloseDatabase (m_pathName, &db);
    if (0 == status)
    {
        if (db != NULL)
            db->Close();    // decrement ref count, so it will leave cache
        XP_FileRemove (m_pathName, xpMailFolderSummary);
    }

    if ((0 == status) && (GetType() == FOLDER_MAIL))
	{
        // remove the mail folder file
        status = XP_FileRemove (m_pathName, xpMailFolder);

		// if the delete seems to have failed, but the file doesn't
		// exist, that's not really an error condition, is it now?
		if (status)
		{
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
    MsgERR status = MSG_FolderInfo::Rename (newUserLeafName);
    if (status == 0)
    {
    	char *baseDir = XP_STRDUP(m_pathName);
    	if (baseDir)
    	{
	        char *base_slash = XP_STRRCHR (baseDir, '/'); 
	        if (base_slash)
	            *base_slash = '\0';
        }

    	char *leafNameForDisk = CreatePlatformLeafNameForDisk(newUserLeafName,m_master, baseDir);
    	if (!leafNameForDisk)
    		status = MK_OUT_OF_MEMORY;

        if (0 == status)
		{
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
			if (0 == status)
			{
				if (db)
				{
					if (ReopenDatabase (db, newPath) == 0)
					{	
						//need to set mailbox name
					}
				}
				else
				{
					MailDB *mailDb = NULL;
					MailDB::Open(newPath, TRUE, &mailDb, TRUE);
					if (mailDb)
					{
						//need to set mailbox name
						mailDb->Close();
					}
				}
			}

			// rename the mail folder file, if its local
			if ((status == 0) && (GetType() == FOLDER_MAIL))
				status = XP_FileRename (m_pathName, xpMailFolder, newPath, xpMailFolder);

			if (status == 0)
			{
				// rename the subdirectory if there is one
				if (m_subFolders->GetSize() > 0)
				status = XP_FileRename (m_pathName, xpMailSubdirectory, newPath, xpMailSubdirectory);

				// tell all our children about the new pathname
				if (status == 0)
				{
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
		MsgERR err = eSUCCESS;
	XP_ASSERT (srcFolder->GetType() == GetType());	// we can only adopt the same type of folder
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
		if (eSUCCESS == err)
		{
			m_flags |= MSG_FOLDER_FLAG_DIRECTORY;
			m_flags |= MSG_FOLDER_FLAG_ELIDED;
		}
	}

	// Recurse the tree to adopt srcFolder's children
	err = mailFolder->PropagateAdopt (m_pathName, m_depth);

	// Add the folder to our tree in the right sorted position
	if (eSUCCESS == err)
	{
		XP_ASSERT(m_subFolders->FindIndex(0, srcFolder) == -1);
		*pOutPos = m_subFolders->Add (srcFolder);
	}

	return err;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetName(char** name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	if (!mHaveReadNameFromDB)
	{
		if (mDepth == 1) 
		{
			SetName("Local Mail");
			mHaveReadNameFromDB = TRUE;
		}
		else
		{
			//Need to read the name from the database
		}
	}
	*name = mName;


	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetPrettyName(char ** prettyName)
{
	if (mDepth == 1) {
	// Depth == 1 means we are on the mail server level
	// override the name here to say "Local Mail"
		FolderType type;
		GetType(&type);
		if (type == FOLDER_MAIL)
			*prettyName = PL_strdup("Local Mail");
		else
			return nsMsgFolder::GetPrettyName(prettyName);
	}
	else
		return nsMsgFolder::GetPrettyName(prettyName);

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GenerateUniqueSubfolderName(const char *prefix, 
																													 const nsIMsgFolder *otherFolder,
																													 char** name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	/* only try 256 times */
	for (int count = 0; (count < 256); count++)
	{
		PRUint32 prefixSize = PL_strlen(prefix);

		//allocate string big enough for prefix, 256, and '\0'
		char *uniqueName = (char*)PR_MALLOC(prefixSize + 4);
		PR_snprintf(uniqueName, prefixSize + 4, "%s%d",prefix,count);
		PRBool containsChild;
		PRBool otherContainsChild = PR_FALSE;

		ContainsChildNamed(uniqueName, &containsChild);
		if(otherFolder)
		{
			((nsIMsgFolder*)otherFolder)->ContainsChildNamed(uniqueName, &otherContainsChild);
		}

		if (!containsChild && !otherContainsChild)
		{
			*name = uniqueName;
			return NS_OK;
		}
		else
			PR_FREEIF(uniqueName);
	}
	*name = nsnull;
	return NS_OK;
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetDeletable (PRBool *deletable)
{
	if(!deletable)
		return NS_ERROR_NULL_POINTER;

	// These are specified in the "Mail/News Windows" UI spec

	if (mFlags & MSG_FOLDER_FLAG_TRASH)
	{
		PRBool moveToTrash;
		GetDeleteIsMoveToTrash(&moveToTrash);
		if(moveToTrash)
			*deletable = PR_TRUE;	// allow delete of trash if we don't use trash
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
 
NS_IMETHODIMP nsMsgLocalMailFolder::GetCanCreateChildren (PRBool *canCreateChildren)
{	
	if(!canCreateChildren)
		return NS_ERROR_NULL_POINTER;

	*canCreateChildren = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::GetCanBeRenamed (PRBool *canBeRenamed)
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



NS_IMETHODIMP nsMsgLocalMailFolder::GetRelativePathName (char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;
	*pathName = mPathName;
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetUserName(char** userName)
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

NS_IMETHODIMP nsMsgLocalMailFolder::GetPathName(char * *aPathName)
{
	if(!aPathName)
		return NS_ERROR_NULL_POINTER;

	if(mPathName)
		*aPathName = PL_strdup(mPathName);
	else
		*aPathName = nsnull;

	return NS_OK;
}

NS_IMETHODIMP nsMsgLocalMailFolder::SetPathName(char * aPathName)
{
	if(mPathName)
		PR_FREEIF(mPathName);

	if(aPathName)
		mPathName = PL_strdup(aPathName);
	else
		mPathName = nsnull;

	return NS_OK;
}


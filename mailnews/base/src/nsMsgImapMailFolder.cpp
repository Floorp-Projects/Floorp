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

#include "nsMsgImapMailFolder.h"	 
#include "nsMsgFolderFlags.h"
#include "nsISupportsArray.h"
#include "prprf.h"
#include "nsMsgDBCID.h"
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

nsMsgImapMailFolder::nsMsgImapMailFolder(nsString& name)
  : nsMsgFolder(/*name*/)
{
	mHaveReadNameFromDB = PR_FALSE;
	mPathName = nsnull;
}

nsMsgImapMailFolder::~nsMsgImapMailFolder(void)
{
}

NS_IMPL_ADDREF(nsMsgImapMailFolder)
NS_IMPL_RELEASE(nsMsgImapMailFolder)

NS_IMETHODIMP
nsMsgImapMailFolder::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
	if (iid.Equals(nsCOMTypeInfo<nsIMsgImapMailFolder>::GetIID()) ||
		iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
		*result = NS_STATIC_CAST(nsIMsgImapMailFolder*, this);
		AddRef();
		return NS_OK;
	}
	return nsMsgFolder::QueryInterface(iid, result);
}

NS_IMETHODIMP nsMsgImapMailFolder::BuildFolderURL(char **url)
{
	const char *urlScheme = "mailbox:";

	if(!url)
		return NS_ERROR_NULL_POINTER;

	*url = PR_smprintf("%s%s", urlScheme, mPathName);
	return NS_OK;

}

NS_IMETHODIMP nsMsgImapMailFolder::CreateSubfolder(const char *leafNameFromUser, nsIMsgFolder **outFolder, PRUint32 *outPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgImapMailFolder::RemoveSubFolder(const nsIMsgFolder *which)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgImapMailFolder::Delete()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgImapMailFolder::Rename (const char *newName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgImapMailFolder::Adopt(const nsIMsgFolder *srcFolder, PRUint32 *outPos)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsMsgImapMailFolder::FindChildNamed(const char *name, nsIMsgFolder ** aChild)
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
		if (folder)
			NS_RELEASE(folder);
		if (NS_SUCCEEDED(supports->QueryInterface(kISupportsIID,
                                              (void**)&folder)))
		{
      char *folderName;

			folder->GetName(&folderName);
      // IMAP INBOX is case insensitive
      if (//          !folderName.EqualsIgnoreCase("INBOX")
          folderName &&
          !PL_strcasecmp(folderName, "INBOX")
          )
      {
        NS_RELEASE(supports);
        PR_FREEIF(folderName);
        continue;
      }

      // For IMAP, folder names are case sensitive
      if (folderName &&
          //!folderName.EqualsIgnoreCase(name)
          !PL_strcmp(folderName, name)
          )
      {
        PR_FREEIF(folderName);
        *aChild = folder;
        return NS_OK;
      }
      PR_FREEIF(folderName);
		}
		NS_RELEASE(supports);
  }
	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetName(PRUnichar** name)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgImapMailFolder::UpdateSummaryTotals(PRBool force)
{
	//We need to read this info from the database

	// If we asked, but didn't get any, stop asking
	if (mNumUnreadMessages == -1)
		mNumUnreadMessages = -2;

	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetExpungedBytesCount(PRUint32 *count)
{
	if(!count)
		return NS_ERROR_NULL_POINTER;

	*count = mExpungedBytes;

	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetDeletable (PRBool *deletable)
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
 
NS_IMETHODIMP nsMsgImapMailFolder::GetCanCreateChildren (PRBool *canCreateChildren)
{	
	if(!canCreateChildren)
		return NS_ERROR_NULL_POINTER;

	*canCreateChildren = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetCanBeRenamed (PRBool *canBeRenamed)
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

NS_IMETHODIMP nsMsgImapMailFolder::GetRequiresCleanup(PRBool *requiresCleanup)
{
#ifdef HAVE_PORT
	if (m_expungedBytes > 0)
	{
		int32 purgeThreshhold = m_master->GetPrefs()->GetPurgeThreshhold();
		PRBool purgePrompt = m_master->GetPrefs()->GetPurgeThreshholdEnabled();;
		return (purgePrompt && m_expungedBytes / 1000L > purgeThreshhold);
	}
	return PR_FALSE;
#endif
	return NS_OK;
}



NS_IMETHODIMP nsMsgImapMailFolder::GetRelativePathName (char **pathName)
{
	if(!pathName)
		return NS_ERROR_NULL_POINTER;
	*pathName = mPathName;
	return NS_OK;
}


NS_IMETHODIMP nsMsgImapMailFolder::GetSizeOnDisk(PRUint32 size)
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

NS_IMETHODIMP nsMsgImapMailFolder::GetUserName(char** userName)
{
#ifdef HAVE_PORT
	return NET_GetPopUsername();
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetHostName(char** hostName)
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

NS_IMETHODIMP nsMsgImapMailFolder::UserNeedsToAuthenticateForFolder(PRBool displayOnly, PRBool *authenticate)
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

NS_IMETHODIMP nsMsgImapMailFolder::RememberPassword(const char *password)
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

NS_IMETHODIMP nsMsgImapMailFolder::GetRememberedPassword(char ** password)
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
		if (m_master->GetImapMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &offlineInbox, 1) && offlineInbox)
			savedPassword = offlineInbox->GetRememberedPassword();
	}
	return savedPassword;
#endif
	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::GetPathName(char * *aPathName)
{
	if(!aPathName)
		return NS_ERROR_NULL_POINTER;

	if(mPathName)
		*aPathName = PL_strdup(mPathName);
	else
		*aPathName = nsnull;

	return NS_OK;
}

NS_IMETHODIMP nsMsgImapMailFolder::SetPathName(char * aPathName)
{
	if(mPathName)
		PR_FREEIF(mPathName);

	if(aPathName)
		mPathName = PL_strdup(aPathName);
	else
		mPathName = nsnull;

	return NS_OK;
}


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "xp_file.h"
#include "msgCore.h"
#include "nsImapMailDatabase.h"
#include "nsDBFolderInfo.h"
#include "nsLocalFolderSummarySpec.h"
#include "nsIFileSpec.h"

nsImapMailDatabase::nsImapMailDatabase()
{
}

nsImapMailDatabase::~nsImapMailDatabase()
{
}

NS_IMETHODIMP nsImapMailDatabase::Open(nsIFileSpec *aFolderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
	nsImapMailDatabase	*mailDB;
	PRBool			summaryFileExists;
	struct stat		st;
	PRBool			newFile = PR_FALSE;
#ifdef DEBUG_bienvenu
  NS_ASSERTION(m_folder, "folder should be set");
#endif
	if (!aFolderName)
		return NS_ERROR_NULL_POINTER;

	nsFileSpec		folderName;
	aFolderName->GetFileSpec(&folderName);

	nsLocalFolderSummarySpec	summarySpec(folderName);

	nsIDBFolderInfo	*folderInfo = NULL;

	*pMessageDB = NULL;

	nsFileSpec dbPath(summarySpec);

	mailDB = (nsImapMailDatabase *) FindInCache(dbPath);
	if (mailDB)
	{
		*pMessageDB = mailDB;
		return(NS_OK);
	}

#if defined(DEBUG_bienvenu) || defined(DEBUG_jefft)
    printf("really opening db in nsImapMailDatabase::Open(%s, %s, %p, %s) -> %s\n",
           (const char*)folderName, create ? "TRUE":"FALSE",
           pMessageDB, upgrading ? "TRUE":"FALSE", (const char*)folderName);
#endif
	// if the old summary doesn't exist, we're creating a new one.
	if (!summarySpec.Exists() && create)
		newFile = PR_TRUE;

	mailDB = new nsImapMailDatabase;

	if (!mailDB)
		return NS_ERROR_OUT_OF_MEMORY;
	mailDB->m_folderSpec = new nsFileSpec(folderName);
  mailDB->m_folder = m_folder;
	mailDB->AddRef();
	// stat file before we open the db, because if we've latered
	// any messages, handling latered will change time stamp on
	// folder file.
	summaryFileExists = summarySpec.Exists();

	char	*nativeFolderName = nsCRT::strdup((const char *) folderName);

#if defined(XP_PC) || defined(XP_MAC)
	UnixToNative(nativeFolderName);
#endif
	stat (nativeFolderName, &st);
	PR_FREEIF(nativeFolderName);

	nsresult err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
	
	err = mailDB->OpenMDB((const char *) summarySpec, create);

	if (err == NS_OK)
	{
		mailDB->GetDBFolderInfo(&folderInfo);
		if (folderInfo == NULL)
		{
			err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
		}
		else
		{
			// compare current version of db versus filed out version info.
            PRUint32 version;
            folderInfo->GetVersion(&version);
			if (mailDB->GetCurVersion() != version)
				err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
			NS_RELEASE(folderInfo);
		}
		if (err != NS_OK)
		{
			// this will make the db folder info release its ref to the mail db...
			NS_IF_RELEASE(mailDB->m_dbFolderInfo);
			mailDB->ForceClosed();
			if (err == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
				summarySpec.Delete(PR_FALSE);

			mailDB = NULL;
		}
	}
	if (err != NS_OK || newFile)
	{
		// if we couldn't open file, or we have a blank one, and we're supposed 
		// to upgrade, updgrade it.
		if (newFile && !upgrading)	// caller is upgrading, and we have empty summary file,
		{					// leave db around and open so caller can upgrade it.
			err = NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;
		}
		else if (err != NS_OK)
		{
			*pMessageDB = NULL;
      if (mailDB)
        mailDB->Close(PR_FALSE);
			delete mailDB;
      summarySpec.Delete(PR_FALSE);  // blow away the db if it's corrupt.
			mailDB = NULL;
		}
	}
	if (err == NS_OK || err == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
	{
		*pMessageDB = mailDB;
		if (mailDB)
			GetDBCache()->AppendElement(mailDB);
//		if (err == NS_OK)
//			mailDB->HandleLatered();

	}
	return err;
}

NS_IMETHODIMP	nsImapMailDatabase::SetSummaryValid(PRBool /* valid */)
{
	return NS_OK;
}
	
// IMAP does not set local file flags, override does nothing
void	nsImapMailDatabase::UpdateFolderFlag(nsIMsgDBHdr * /* msgHdr */, PRBool /* bSet */, 
									 MsgFlags /* flag */, nsIOFileStream ** /* ppFileStream */)
{
}

// override so nsMailDatabase methods that deal with m_folderStream are *not* called
NS_IMETHODIMP nsImapMailDatabase::StartBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailDatabase::EndBatch()
{
  return NS_OK;
}

NS_IMETHODIMP nsImapMailDatabase::DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator)
{
	return nsMsgDatabase::DeleteMessages(nsMsgKeys, instigator);
}

nsresult nsImapMailDatabase::AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr)
{
  PRUint32 msgFlags;
  msgHdr->GetFlags(&msgFlags);
  if (msgFlags & MSG_FLAG_OFFLINE && m_dbFolderInfo)
  {
    PRUint32 size = 0;
    (void)msgHdr->GetOfflineMessageSize(&size);
    return m_dbFolderInfo->ChangeExpungedBytes (size);
  }
  return NS_OK;
}



/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgAccount.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsEudoraMac.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportABDescriptor.h"
#include "nsDirectoryServiceDefs.h"
#include "nsEudoraStringBundle.h"
#include "nsEudoraImport.h"
#include "nsIPop3IncomingServer.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "EudoraDebugLog.h"

#include <Resources.h>
#include <Files.h>
#include <TextUtils.h>

#ifdef XP_MACOSX
#include "nsILocalFileMac.h"
#include "MoreFilesX.h"

static nsresult NS_FileSpecToILocalFileMac(nsFileSpec *aSpec, nsILocalFileMac **aLocalFileMac)
{
  nsCOMPtr<nsILocalFile> localFile;
  nsresult rv = NS_FileSpecToIFile(aSpec, getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(localFile);
  if (!macFile)
    return NS_ERROR_FAILURE;
   
  NS_IF_ADDREF(*aLocalFileMac = macFile);
  return NS_OK;
}
#else
#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#endif

static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);

static const char *	kWhitespace = "\b\t\r\n ";

nsEudoraMac::nsEudoraMac()
{
	m_mailImportLocation = nsnull;
}

nsEudoraMac::~nsEudoraMac()
{
	NS_IF_RELEASE( m_mailImportLocation);
}

PRBool nsEudoraMac::FindMailFolder( nsIFileSpec *pFolder)
{
	return( FindEudoraLocation( pFolder));
}

PRBool nsEudoraMac::FindEudoraLocation( nsIFileSpec *pFolder, PRBool findIni, nsIFileSpec *pLookIn)
{
	PRBool	result = PR_FALSE;
	
	// The "default" eudora folder is in the system folder named
	// "Eudora Folder" (not sure if this is true for intl versions of Eudora)
	
	if (!pLookIn) {
        nsCOMPtr<nsIFile> sysDir;
        nsresult rv = NS_GetSpecialDirectory(NS_OS_SYSTEM_DIR, getter_AddRefs(sysDir));
        if (NS_FAILED(rv))
          return (PR_FALSE);

        nsCOMPtr<nsIFileSpec> sysDirSpec;
        rv = NS_NewFileSpecFromIFile(sysDir, getter_AddRefs(sysDirSpec));
        if (NS_FAILED(rv))
          return (PR_FALSE);

		pFolder->FromFileSpec(sysDirSpec);
		pFolder->AppendRelativeUnixPath( "Eudora Folder");
		PRBool	link = PR_FALSE;
		rv = pFolder->IsSymlink( &link);
		if (NS_SUCCEEDED( rv) && link) {
			rv = pFolder->ResolveSymlink();
			if (NS_FAILED( rv))
				return( PR_FALSE);
		}
	}
	else
		pFolder->FromFileSpec( pLookIn);
		
	PRBool	exists = PR_FALSE;
	nsresult rv = pFolder->Exists( &exists);
	PRBool	isFolder = PR_FALSE;
	if (NS_SUCCEEDED( rv) && exists)
		rv = pFolder->IsDirectory( &isFolder);
	if (!exists || !isFolder)
		return( PR_FALSE);
	
	
	nsFileSpec						pref;
	PRBool							foundPref = PR_FALSE;
	
	nsCOMPtr<nsIDirectoryIterator>	dir;
	rv = NS_NewDirectoryIterator( getter_AddRefs( dir));
	if (NS_SUCCEEDED( rv) && dir) {
		exists = PR_FALSE;
		rv = dir->Init( pFolder, PR_TRUE);
		if (NS_SUCCEEDED( rv)) {
			rv = dir->Exists( &exists);			
			nsCOMPtr<nsIFileSpec>	entry;
			int						count = 0;
			OSType					type, creator;
			while (exists && NS_SUCCEEDED( rv) && (count < 2)) {
				rv = dir->GetCurrentSpec( getter_AddRefs( entry));
				if (NS_SUCCEEDED( rv)) {
					nsFileSpec spec;
					rv = entry->GetFileSpec( &spec);
					if (NS_SUCCEEDED( rv)) {
						// find a file with TEXT, CSOm that isn't the nicknames file
						// or just cheat and look for more than 1 file?
#ifdef XP_MACOSX
  {
    nsCOMPtr<nsILocalFileMac> macFile;
    rv = NS_FileSpecToILocalFileMac(&spec, getter_AddRefs(macFile));
    if (NS_SUCCEEDED(rv))
    {
      macFile->GetFileCreator(&creator);
      macFile->GetFileType(&type);
    }
  }
#else
						rv = spec.GetFileTypeAndCreator( &type, &creator);
#endif
						if (NS_SUCCEEDED( rv)) {
							if ((type == 'TEXT') && (creator == 'CSOm'))
								count++;
							else if ((type == 'PREF') && (creator == 'CSOm')) {
								if (!foundPref) {
									pref = spec;
									foundPref = PR_TRUE;
								}
								else {
									// does one of them end in ".bkup"?
									char *pLeafName = spec.GetLeafName();
									PRBool	isBk = PR_FALSE;
									PRInt32 len;
									if (pLeafName) {
										len = strlen( pLeafName);
										if (len > 5)
											isBk = (nsCRT::strcasecmp( pLeafName + len - 5, ".bkup") == 0);
										nsCRT::free( pLeafName);
									}
									if (!isBk) {
										pLeafName = pref.GetLeafName();
										if (pLeafName) {
											len = strlen( pLeafName);
											if (len > 5)
												isBk = (nsCRT::strcasecmp( pLeafName + len - 5, ".bkup") == 0);
											nsCRT::free( pLeafName);
										}
										if (isBk) {
											pref = spec;
										}
										else {
											// Neither of the pref files was named .bkup
											// Pick the newest one?
											nsFileSpec::TimeStamp	modDate1, modDate2;
											
											spec.GetModDate( modDate2);
											pref.GetModDate( modDate1);
											if (modDate2 > modDate1)
												pref = spec;
										}
									}
								}
							}
						}
					}
				}
				rv = dir->Next();
				if (NS_SUCCEEDED( rv))
					rv = dir->Exists( &exists);
			}
			if (count >= 2)
				result = PR_TRUE;
		}
	}
	
	if (!findIni)
		return( result);
			
	if (!foundPref)
		return( PR_FALSE);
	
	pFolder->SetFromFileSpec( pref);
	
	return( PR_TRUE);
}



nsresult nsEudoraMac::FindMailboxes( nsIFileSpec *pRoot, nsISupportsArray **ppArray)
{
	nsresult rv = NS_NewISupportsArray( ppArray);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray\n");
		return( rv);
	}
		
	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_FAILED( rv))
		return( rv);
	
	m_depth = 0;
	NS_IF_RELEASE( m_mailImportLocation);
	m_mailImportLocation = pRoot;
	NS_IF_ADDREF( m_mailImportLocation);

	return( ScanMailDir( pRoot, *ppArray, impSvc));
}


nsresult nsEudoraMac::ScanMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
{

	// On Windows, we look for a descmap file but on Mac we just iterate
	// the directory
	
	m_depth++;

	nsresult rv = IterateMailDir( pFolder, pArray, pImport);
	
	m_depth--;

	return( rv);			
}

nsresult nsEudoraMac::IterateMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
{
	nsCOMPtr<nsIDirectoryIterator>	dir;
	nsresult rv = NS_NewDirectoryIterator( getter_AddRefs( dir));
	if (NS_FAILED( rv))
		return( rv);

	PRBool	exists = PR_FALSE;
	rv = dir->Init( pFolder, PR_TRUE);
	if (NS_FAILED( rv))
		return( rv);

	rv = dir->Exists( &exists);
	if (NS_FAILED( rv))
		return( rv);
	
	PRBool					isFolder;
	PRBool					isFile;
	nsCOMPtr<nsIFileSpec>	entry;
	char *					pName;
	nsCString				fName;
	nsCString				ext;
	nsCString				name;
	nsFileSpec				spec;
	OSType					type;
	OSType					creator;
	
	while (exists && NS_SUCCEEDED( rv)) {
		rv = dir->GetCurrentSpec( getter_AddRefs( entry));
		if (NS_SUCCEEDED( rv)) {
			isFolder = PR_FALSE;
			isFile = PR_FALSE;
			pName = nsnull;
			rv = entry->IsDirectory( &isFolder);
			rv = entry->IsFile( &isFile);
			rv = entry->GetLeafName( &pName);
			if (NS_SUCCEEDED( rv) && pName) {
				fName = pName;
				nsCRT::free( pName);
				if (isFolder) {
					if (IsValidMailFolderName( fName)) {
						rv = FoundMailFolder( entry, fName.get(), pArray, pImport);
						if (NS_SUCCEEDED( rv)) {
							rv = ScanMailDir( entry, pArray, pImport);
							if (NS_FAILED( rv)) {
								IMPORT_LOG0( "*** Error scanning mail directory\n");
							}
						}
					}
				}
				else if (isFile) {
					rv = entry->GetFileSpec( &spec);
					if (NS_SUCCEEDED( rv)) {
						type = 0;
						creator = 0;
#ifdef XP_MACOSX
  {
    nsCOMPtr<nsILocalFileMac> macFile;
    rv = NS_FileSpecToILocalFileMac(&spec, getter_AddRefs(macFile));
    if (NS_SUCCEEDED(rv))
    {
      macFile->GetFileCreator(&creator);
      macFile->GetFileType(&type);
    }
  }
#else
    spec.GetFileTypeAndCreator( &type, &creator);
#endif
						if ((type == 'TEXT') && IsValidMailboxName( fName) && IsValidMailboxFile( entry)) {
							rv = FoundMailbox( entry, fName.get(), pArray, pImport);
						}
					}
				}
			}				
		}

		rv = dir->Next();
		if (NS_SUCCEEDED( rv))
			rv = dir->Exists( &exists);
	}

	return( rv);
}



nsresult nsEudoraMac::FoundMailbox( nsIFileSpec *mailFile, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
	nsAutoString							displayName;
	nsCOMPtr<nsIImportMailboxDescriptor>	desc;
	nsISupports *							pInterface;

	NS_CopyNativeToUnicode(nsDependentCString(pName), displayName);

#ifdef IMPORT_DEBUG
	char *pPath = nsnull;
	mailFile->GetNativePath( &pPath);
	if (pPath) {
		IMPORT_LOG2( "Found eudora mailbox, %s: %s\n", pPath, pName);
		nsCRT::free( pPath);
	}
	else {
		IMPORT_LOG1( "Found eudora mailbox, %s\n", pName);
	}
	IMPORT_LOG1( "\tm_depth = %d\n", (int)m_depth);
#endif

	nsresult rv = pImport->CreateNewMailboxDescriptor( getter_AddRefs( desc));
	if (NS_SUCCEEDED( rv)) {
		PRUint32		sz = 0;
		mailFile->GetFileSize( &sz);	
		desc->SetDisplayName( displayName.get());
		desc->SetDepth( m_depth);
		desc->SetSize( sz);
		nsIFileSpec *pSpec = nsnull;
		desc->GetFileSpec( &pSpec);
		if (pSpec) {
			pSpec->FromFileSpec( mailFile);
			NS_RELEASE( pSpec);
		}
		rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
		pArray->AppendElement( pInterface);
		pInterface->Release();
	}

	return( NS_OK);
}


nsresult nsEudoraMac::FoundMailFolder( nsIFileSpec *mailFolder, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
	nsAutoString  				displayName;
	nsCOMPtr<nsIImportMailboxDescriptor>	desc;
	nsISupports *							pInterface;

	NS_CopyNativeToUnicode(nsDependentCString(pName), displayName);

#ifdef IMPORT_DEBUG
	char *pPath = nsnull;
	mailFolder->GetNativePath( &pPath);
	if (pPath) {
		IMPORT_LOG2( "Found eudora folder, %s: %s\n", pPath, pName);
		nsCRT::free( pPath);
	}
	else {
		IMPORT_LOG1( "Found eudora folder, %s\n", pName);
	}
	IMPORT_LOG1( "\tm_depth = %d\n", (int)m_depth);
#endif

	nsresult rv = pImport->CreateNewMailboxDescriptor( getter_AddRefs( desc));
	if (NS_SUCCEEDED( rv)) {
		PRUint32		sz = 0;
		desc->SetDisplayName( displayName.get());
		desc->SetDepth( m_depth);
		desc->SetSize( sz);
		nsIFileSpec *pSpec = nsnull;
		desc->GetFileSpec( &pSpec);
		if (pSpec) {
			pSpec->FromFileSpec( mailFolder);
			NS_RELEASE( pSpec);
		}
		rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
		pArray->AppendElement( pInterface);
		pInterface->Release();
	}

	return( NS_OK);
}

PRBool nsEudoraMac::CreateTocFromResource( nsIFileSpec *pMail, nsIFileSpec *pToc)
{	
	nsFileSpec	spec;
	nsresult rv = pMail->GetFileSpec( &spec);
	if (NS_FAILED( rv))
		return( PR_FALSE);
        short resFile = -1;
#ifdef XP_MACOSX
        {
          nsCOMPtr<nsILocalFileMac> macFile;
          rv = NS_FileSpecToILocalFileMac(&spec, getter_AddRefs(macFile));
          if (NS_FAILED(rv))
            return PR_FALSE;

          FSSpec fsSpec;
          rv = macFile->GetFSSpec(&fsSpec);
	  if (NS_FAILED(rv))
            return PR_FALSE;

          resFile = FSpOpenResFile( &fsSpec, fsRdPerm);
        }
#else
        resFile = FSpOpenResFile( spec.GetFSSpecPtr(), fsRdPerm);
#endif
	if (resFile == -1)
		return( PR_FALSE);
	Handle	resH = nil;
	short max = Count1Resources( 'TOCF');
	if (max) {
		resH = Get1IndResource( 'TOCF', 1);
	}
	PRBool	 result = PR_FALSE;
	if (resH) {
		PRInt32 sz = (PRInt32) GetHandleSize( resH);
		if (sz) {
            // Create the new TOC file
            nsCOMPtr<nsIFile> tempDir;
            nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempDir));
            if (NS_FAILED(rv))
              return (PR_FALSE);

            nsCOMPtr<nsIFileSpec> dir;
            rv = NS_NewFileSpecFromIFile(tempDir, getter_AddRefs(dir));
            if (NS_FAILED(rv))
              return (PR_FALSE);

			rv = pToc->FromFileSpec( dir);
			if (NS_SUCCEEDED( rv))
				rv = pToc->AppendRelativeUnixPath( "temp.toc");
			if (NS_SUCCEEDED( rv))
				rv = pToc->MakeUnique();
			if (NS_SUCCEEDED( rv))
				rv = pToc->OpenStreamForWriting();
			if (NS_SUCCEEDED( rv)) { 
				HLock( resH);
				PRInt32 written = 0;
				rv = pToc->Write( *resH, sz, &written);
				HUnlock( resH);
				pToc->CloseStream();
				if (NS_FAILED( rv) || (written != sz)) {
					pToc->GetFileSpec( &spec);
					spec.Delete( PR_FALSE);
				}
				else
					result = PR_TRUE;
			}
		}		
		ReleaseResource( resH);
	}
	CloseResFile( resFile); 
	
	return( result);
}


nsresult nsEudoraMac::FindTOCFile( nsIFileSpec *pMailFile, nsIFileSpec **ppTOCFile, PRBool *pDeleteToc)
{
	nsresult		rv;
	char	*		pName = nsnull;

	*pDeleteToc = PR_FALSE;
	*ppTOCFile = nsnull;
	rv = pMailFile->GetLeafName( &pName);
	if (NS_FAILED( rv))
		return( rv);
	rv = pMailFile->GetParent( ppTOCFile);
	if (NS_FAILED( rv))
		return( rv);

	nsCString	leaf(pName);
	nsCRT::free( pName);
	leaf.Append( ".toc");
	
	OSType	type = 0;
	OSType	creator = 0;
	PRBool	exists = PR_FALSE;
	PRBool	isFile = PR_FALSE;
	rv = (*ppTOCFile)->AppendRelativeUnixPath( leaf.get());
	if (NS_SUCCEEDED( rv))
		rv = (*ppTOCFile)->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = (*ppTOCFile)->IsFile( &isFile);
	if (isFile) {
		nsFileSpec	spec;
		rv = (*ppTOCFile)->GetFileSpec( &spec);
		if (NS_SUCCEEDED( rv))
#ifdef XP_MACOSX
  {
    nsCOMPtr<nsILocalFileMac> macFile;
    rv = NS_FileSpecToILocalFileMac(&spec, getter_AddRefs(macFile));
    if (NS_SUCCEEDED(rv))
    {
      macFile->GetFileCreator(&creator);
      macFile->GetFileType(&type);
    }
  }
#else
			spec.GetFileTypeAndCreator( &type, &creator);
#endif
	}
	

	if (exists && isFile && (type == 'TOCF') && (creator == 'CSOm'))
		return( NS_OK);
	
	// try and create the file from a resource.
	if (CreateTocFromResource( pMailFile, *ppTOCFile)) {
		*pDeleteToc = PR_TRUE;
		return( NS_OK);
	}
	return( NS_ERROR_FAILURE);
}


// Strings returned:
//	1 - smtp server
//  2 - pop account user name
//	3 - the pop server
//	4 - Return address
//	5 - Full name
// 	6 - Leave mail on server
#define	kNumSettingStrs			6
#define	kSmtpServerStr			0
#define	kPopAccountNameStr		1
#define	kPopServerStr			2
#define kReturnAddressStr		3
#define	kFullNameStr			4
#define kLeaveOnServerStr		5

// resource IDs 
#define kSmtpServerID         4
#define kEmailAddressID       3
#define kReturnAddressID      5
#define kFullNameID           77
#define kLeaveMailOnServerID  18


PRBool nsEudoraMac::GetSettingsFromResource( nsIFileSpec *pSettings, short resId, nsCString **pStrs, PRBool *pIMAP)
{
	*pIMAP = PR_FALSE;
	// Get settings from the resources...
	nsFileSpec	spec;
	nsresult rv = pSettings->GetFileSpec( &spec);
	if (NS_FAILED( rv))
		return( PR_FALSE);

        short resFile = -1;
#ifdef XP_MACOSX
        {
          nsCOMPtr<nsILocalFileMac> macFile;
          rv = NS_FileSpecToILocalFileMac(&spec, getter_AddRefs(macFile));
	  if (NS_FAILED(rv))
            return PR_FALSE;

          FSSpec fsSpec;
          rv = macFile->GetFSSpec(&fsSpec);
	  if (NS_FAILED(rv))
            return PR_FALSE;

          resFile = FSpOpenResFile( &fsSpec, fsRdPerm);
        }
#else
	resFile = FSpOpenResFile( spec.GetFSSpecPtr(), fsRdPerm);
#endif
	if (resFile == -1)
		return( PR_FALSE);
		
	// smtp server, STR# 1000, 4
	Handle	resH = Get1Resource( 'STR#', resId /* 1000 */);
	int		idx;
	if (resH) {
		ReleaseResource( resH);
		StringPtr	pStr[5];
		StringPtr 	theStr;
		short		i;
		for (i = 0; i < 5; i++) {
			pStr[i] = (StringPtr) new PRUint8[256];
			(pStr[i])[0] = 0;
		}
		GetIndString( pStr[0], resId /* 1000 */, kSmtpServerID);
		GetIndString( pStr[1], resId, kEmailAddressID); // user name@pop server
		GetIndString( pStr[2], resId, kReturnAddressID); 
		GetIndString( pStr[3], resId, kFullNameID);
		GetIndString( pStr[4], resId, kLeaveMailOnServerID);
		CloseResFile( resFile); 
		
		theStr = pStr[0];
		if (*theStr) {
			pStrs[0]->Append( (const char *) (theStr + 1), *theStr);
		}
		theStr = pStr[1];
		if (*theStr) {
			idx = 1;
			while (idx <= *theStr) {
				if (theStr[idx] == '@')
					break;
				else
					idx++;
			}
			if (idx <= *theStr) {
				PRUint8	save = *theStr;
				*theStr = idx - 1;
				if (*theStr) {
					pStrs[1]->Append( (const char *) (theStr + 1), *theStr);
				}
				*theStr = save;
			}
			else
				idx = 0;
			theStr[idx] = theStr[0] - idx;
			if (theStr[idx]) {
				pStrs[2]->Append( (const char *) (theStr + idx + 1), *(theStr + idx));
			}
		}
		theStr = pStr[2];
		if ( *theStr) {
			pStrs[3]->Append( (const char *) (theStr + 1), *theStr);
		}
		theStr = pStr[3];
		if ( *theStr) {
			pStrs[4]->Append( (const char *) (theStr + 1), *theStr);
		}
		theStr = pStr[4];
		if ( *theStr) {
			if (theStr[1] == 'y') {
				*(pStrs[5]) = "Y";
			}
			else {
				*(pStrs[5]) = "N";
			}
		}
		for (i = 0; i < 5; i++) {
			delete pStr[i];
		}
		
		return( PR_TRUE);
	}
	else {
		CloseResFile( resFile); 
		return( PR_FALSE);
	}
}

PRBool nsEudoraMac::ImportSettings( nsIFileSpec *pIniFile, nsIMsgAccount **localMailAccount)
{
	nsresult	rv;

	nsCOMPtr<nsIMsgAccountManager> accMgr = 
	         do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to create a account manager!\n");
		return( PR_FALSE);
	}

	short	baseResId = 1000;
	nsCString **pStrs = new nsCString *[kNumSettingStrs];
	int		i;
	
	for (i = 0; i < kNumSettingStrs; i++) {
		pStrs[i] = new nsCString;
	}
	
	nsString accName(NS_LITERAL_STRING("Eudora Settings"));
	nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_ACCOUNTNAME, accName);
	
	// This is a little overkill but we're not sure yet how multiple accounts
	// are stored in the Mac preferences, hopefully similar to existing prefs
	// which means the following is a good start!
	PRBool	isIMAP = PR_FALSE;	
	
	int				popCount = 0;
	int				accounts = 0;	
	nsIMsgAccount *	pAccount;

	while (baseResId) {
		isIMAP = PR_FALSE;
		if (GetSettingsFromResource( pIniFile, baseResId, pStrs, &isIMAP)) {
			pAccount = nsnull;
			if (!isIMAP) {
				// This is a POP account
				if (BuildPOPAccount( accMgr, pStrs, &pAccount, accName)) {
					accounts++;
					popCount++;
					if (popCount > 1) {
						if (localMailAccount && *localMailAccount) {
							NS_RELEASE( *localMailAccount);
							*localMailAccount = nsnull;
						}
					}
					else {
						if (localMailAccount) {
							*localMailAccount = pAccount;
							NS_IF_ADDREF( pAccount);
						}					
					}
				}
			}
			else {
				// This is an IMAP account
				if (BuildIMAPAccount( accMgr, pStrs, &pAccount, accName)) {
					accounts++;
				}
			}
			if (pAccount && (baseResId == 1000)) {
				accMgr->SetDefaultAccount( pAccount);
			}

			NS_IF_RELEASE( pAccount);
		}
				
		baseResId = 0;
		// Set the next account name???
		
		if (baseResId) {
			for (i = 0; i < kNumSettingStrs; i++) {
				pStrs[i]->Truncate();
			}
		}		
	}

	// Now save the new acct info to pref file.
	rv = accMgr->SaveAccountInfo();
	NS_ASSERTION(NS_SUCCEEDED(rv), "Can't save account info to pref file");

	for (i = 0; i < kNumSettingStrs; i++) {
		delete pStrs[i];
	}
	delete pStrs;

	return( accounts != 0);
}



PRBool nsEudoraMac::BuildPOPAccount( nsIMsgAccountManager *accMgr, nsCString **pStrs, nsIMsgAccount **ppAccount, nsString& accName)
{
	if (ppAccount)
		*ppAccount = nsnull;
	
		
	if (!pStrs[kPopServerStr]->Length() || !pStrs[kPopAccountNameStr]->Length())
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	// I now have a user name/server name pair, find out if it already exists?
	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = accMgr->FindServer( pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), "pop3", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = accMgr->CreateIncomingServer( pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), "pop3", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "pop3");
			// rv = in->SetHostName( pStrs[kPopServerStr]->get());
			// rv = in->SetUsername( pStrs[kPopAccountNameStr]->get());

			IMPORT_LOG2( "Created POP3 server named: %s, userName: %s\n", pStrs[kPopServerStr]->get(), pStrs[kPopAccountNameStr]->get());

			PRUnichar *pretty = ToNewUnicode(accName);
			IMPORT_LOG1( "\tSet pretty name to: %S\n", pretty);
			rv = in->SetPrettyName( pretty);
			nsCRT::free( pretty);
			
			// We have a server, create an account.
			nsCOMPtr<nsIMsgAccount>	account;
			rv = accMgr->CreateAccount( getter_AddRefs( account));
			if (NS_SUCCEEDED( rv) && account) {
				rv = account->SetIncomingServer( in);
				
				IMPORT_LOG0( "Created a new account and set the incoming server to the POP3 server.\n");
					
        nsCOMPtr<nsIPop3IncomingServer> pop3Server = do_QueryInterface(in, &rv);
        NS_ENSURE_SUCCESS(rv,rv);
        pop3Server->SetLeaveMessagesOnServer(pStrs[kLeaveOnServerStr]->First() == 'Y' ? PR_TRUE : PR_FALSE);

        // Fiddle with the identities
				SetIdentities(accMgr, account, pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), pStrs);
				result = PR_TRUE;
				if (ppAccount)
					account->QueryInterface( NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
			}				
		}
	}
	else
		result = PR_TRUE;
	
	return( result);
}


PRBool nsEudoraMac::BuildIMAPAccount( nsIMsgAccountManager *accMgr, nsCString **pStrs, nsIMsgAccount **ppAccount, nsString& accName)
{	

	if (!pStrs[kPopServerStr]->Length() || !pStrs[kPopAccountNameStr]->Length())
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = accMgr->FindServer( pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), "imap", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = accMgr->CreateIncomingServer( pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), "imap", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "imap");
			// rv = in->SetHostName( pStrs[kPopServerStr]->get());
			// rv = in->SetUsername( pStrs[kPopAccountNameStr]->get());
			
			IMPORT_LOG2( "Created IMAP server named: %s, userName: %s\n", pStrs[kPopServerStr]->get(), pStrs[kPopAccountNameStr]->get());
			
			PRUnichar *pretty = ToNewUnicode(accName);
			
			IMPORT_LOG1( "\tSet pretty name to: %S\n", pretty);

			rv = in->SetPrettyName( pretty);
			nsCRT::free( pretty);
			
			// We have a server, create an account.
			nsCOMPtr<nsIMsgAccount>	account;
			rv = accMgr->CreateAccount( getter_AddRefs( account));
			if (NS_SUCCEEDED( rv) && account) {
				rv = account->SetIncomingServer( in);	
				
				IMPORT_LOG0( "Created an account and set the IMAP server as the incoming server\n");

				// Fiddle with the identities
				SetIdentities(accMgr, account, pStrs[kPopAccountNameStr]->get(), pStrs[kPopServerStr]->get(), pStrs);
				result = PR_TRUE;
				if (ppAccount)
					account->QueryInterface( NS_GET_IID(nsIMsgAccount), (void **)ppAccount);
			}				
		}
	}
	else
		result = PR_TRUE;

	return( result);
}


void nsEudoraMac::SetIdentities(nsIMsgAccountManager *accMgr, nsIMsgAccount *acc, const char *userName, const char *serverName, nsCString **pStrs)
{
	nsresult	rv;
	
	nsCOMPtr<nsIMsgIdentity>	id;
	rv = accMgr->CreateIdentity( getter_AddRefs( id));
	if (id) {
		nsAutoString fullName; 
		if (pStrs[kFullNameStr]->Length()) {
			fullName.AssignWithConversion(pStrs[kFullNameStr]->get());
		}
		id->SetFullName( fullName.get());
		id->SetIdentityName( fullName.get());
		if (pStrs[kReturnAddressStr]->Length()) {
			id->SetEmail( pStrs[kReturnAddressStr]->get());
		}
		else {
			nsCAutoString emailAddress;
			emailAddress = userName;
			emailAddress += "@";
			emailAddress += serverName;
			id->SetEmail(emailAddress.get()); 
		}
		acc->AddIdentity( id);

		IMPORT_LOG0( "Created identity and added to the account\n");
		IMPORT_LOG1( "\tname: %s\n", pStrs[kFullNameStr]->get());
		IMPORT_LOG1( "\temail: %s\n", pStrs[kReturnAddressStr]->get());
	}

	SetSmtpServer( accMgr, acc, pStrs[kSmtpServerStr]->get(), userName);
		
}


void nsEudoraMac::SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, const char *pServer, const char *pUser)
{
	nsresult	rv;
	
	nsCOMPtr<nsISmtpService> smtpService(do_GetService(NS_SMTPSERVICE_CONTRACTID, &rv)); 
	if (NS_SUCCEEDED(rv) && smtpService) {
		nsCOMPtr<nsISmtpServer>		foundServer;
	
		rv = smtpService->FindServer( pUser, pServer, getter_AddRefs( foundServer));
		if (NS_SUCCEEDED( rv) && foundServer) {
			IMPORT_LOG1( "SMTP server already exists: %s\n", pServer);
			return;
		}
		nsCOMPtr<nsISmtpServer>		smtpServer;
		
		rv = smtpService->CreateSmtpServer( getter_AddRefs( smtpServer));
		if (NS_SUCCEEDED( rv) && smtpServer) {
			smtpServer->SetHostname( pServer);
			if (pUser)
				smtpServer->SetUsername( pUser);

			IMPORT_LOG1( "Created new SMTP server: %s\n", pServer);
		}
 	}
}


nsresult nsEudoraMac::GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType, nsCString& aAttachment)
{
	mimeType.Truncate();

	// Sample attachment line
	// Internet:sandh.jpg (JPEG/JVWR) (0003C2E8)
	
	OSType		type = '????';
	OSType		creator = '????';
	PRUint32	fNum = 0;
	int			i;
	PRUnichar	c;
	
	nsCString	str(pFileName);
	if (str.Length() > 22) {
		// try and extract the mac file info from the attachment line
		nsCString	fileNum;
		nsCString	types;
		
		str.Right( fileNum, 10);
		if ((fileNum.CharAt( 0) == '(') && (fileNum.CharAt( 9) == ')')) {
			for (i = 1; i < 9; i++) {
				fNum *= 16;
				c = fileNum.CharAt( i);
				if ((c >= '0') && (c <= '9'))
					fNum += (c - '0');
				else if ((c >= 'a') && (c <= 'f'))
					fNum += (c - 'a' + 10);
				else if ((c >= 'A') && (c <= 'F'))
					fNum += (c - 'A' + 10);
				else
					break;
			}
			if (i == 9) {
				str.Left( fileNum, str.Length() - 10);
				str = fileNum;
				str.Trim( kWhitespace);
				str.Right( types, 11);
				if ((types.CharAt( 0) == '(') && (types.CharAt( 5) == '/') && (types.CharAt( 10) == ')')) {
					type = ((PRUint32)types.CharAt( 1)) << 24;
					type |= ((PRUint32)types.CharAt( 2)) << 16;
					type |= types.CharAt( 3) << 8;
					type |= types.CharAt( 4);
					creator = ((PRUint32)types.CharAt( 6)) << 24;
					creator |= ((PRUint32)types.CharAt( 7)) << 16;
					creator |= types.CharAt( 8) << 8;
					creator |= types.CharAt( 9);
					str.Left( types, str.Length() - 11);
					str = types;
					str.Trim( kWhitespace);
				}
			}
			else
				fNum = 0;
		}
	}
	
#ifdef IMPORT_DEBUG
	nsCString	typeStr;
	nsCString	creatStr;
	
	creatStr.Append( (const char *)&creator, 4);
	typeStr.Append( (const char *)&type, 4);
	IMPORT_LOG3( "\tAttachment type: %s, creator: %s, fileNum: %ld\n", typeStr.get(), creatStr.get(), fNum);
	IMPORT_LOG1( "\tAttachment file name: %s\n", str.get());
#endif
	FSSpec	spec;
	memset( &spec, 0, sizeof( spec));
#ifdef XP_MACOSX
        {
          nsresult rv = pSpec->SetNativePath(str.get());
          if (NS_FAILED(rv)) {
            IMPORT_LOG0("\tfailed to set native path\n");
            return rv;
          }
  
          nsFileSpec tempFileSpec;
          rv = pSpec->GetFileSpec(&tempFileSpec);
          if (NS_FAILED(rv)) {
            IMPORT_LOG0("\tfailed to get file spec\n");
            return rv;
          }

          char *pLeaf = nsnull;
		      pSpec->GetLeafName( &pLeaf);
          aAttachment.Adopt(pLeaf);

          nsCOMPtr<nsILocalFileMac> macFile;
          rv = NS_FileSpecToILocalFileMac(&tempFileSpec, getter_AddRefs(macFile));
          if (NS_FAILED(rv)) {
            IMPORT_LOG0("\tfailed to get local mac file\n");
            return rv;
          }
          
          rv = macFile->GetFSSpec(&spec);
          if (NS_FAILED(rv)) {
            IMPORT_LOG0("\tfailed to get FSSpec\n");
            return rv;
          }
        }
#else
	// Now we have all of the pertinent info, find out if the file exists?
	nsCString	fileName;
	if ((str.CharAt( 0) == '"') && (str.Last() == '"')) {
		str.Mid( fileName, 1, str.Length() - 2);
		str = fileName;
	}
	
	PRInt32 idx = str.FindChar( ':');
	if (idx == -1) {
		return( NS_ERROR_FAILURE);
	}
	
	nsCString	volumeName;
	str.Left( volumeName, idx + 1);
	str.Right( fileName, str.Length() - idx - 1);
	
	// Create a FSSpec from the volume name, fileName, and folderNumber
	// Assume that we are looking for a file on the volume with macFileId
	Str63	str63;
	short	vRefNum = 0;
	if (volumeName.Length() > 63) {
		memcpy( &(str63[1]), volumeName.get(), 63);
		str63[0] = 63;
	}
	else {
		memcpy( &(str63[1]), volumeName.get(), volumeName.Length());
		str63[0] = volumeName.Length();
	}
		
	OSErr err = DetermineVRefNum( str63, 0, &vRefNum);
	if (err != noErr) {
		IMPORT_LOG0( "\t*** Error cannot find volume ref num\n");
		return( NS_ERROR_FAILURE);
	}
	
	err = FSpResolveFileIDRef( nil, vRefNum, (long) fNum, &spec);
	if (err != noErr) {
		IMPORT_LOG1( "\t*** Error, cannot resolve fileIDRef: %ld\n", (long) err);
		return( NS_ERROR_FAILURE);
	}
	
	
	FInfo	fInfo;
	err = FSpGetFInfo( &spec, &fInfo);
	if ((err != noErr) || (fInfo.fdType != (OSType) type)) {
		IMPORT_LOG0( "\t*** Error, file type does not match\n");
		return( NS_ERROR_FAILURE);
	}
	
	nsFileSpec	fSpec( spec);
	pSpec->SetFromFileSpec( fSpec);
#endif
	
#ifdef XP_MACOSX
	if (HasResourceFork(&spec)) 
#else
        // Need to find the mime type for the attachment?
        long    dataSize = 0;
        long    rsrcSize = 0;
 
        err = FSpGetFileSize( &spec, &dataSize, &rsrcSize);
 
        // TLR: FIXME: Need to get the mime type from the Mac file type.
        // Currently we just applsingle if there is a resource fork, otherwise,
        // just default to something.
 
        if (rsrcSize)
#endif
		mimeType = "application/applefile";
	else
		mimeType = "application/octet-stream";
	
	IMPORT_LOG1( "\tMimeType: %s\n", mimeType.get());
	
	return( NS_OK);
}
		
PRBool nsEudoraMac::HasResourceFork(FSSpec *fsSpec)
{
  FSRef fsRef;
  if (::FSpMakeFSRef(fsSpec, &fsRef) == noErr)
  {
    FSCatalogInfo catalogInfo;
    OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoRsrcSizes, &catalogInfo, nsnull, nsnull, nsnull);
    return (err == noErr && catalogInfo.rsrcLogicalSize != 0);
  }
  return PR_FALSE;
}


#define		kNumBadFolderNames		7
const char *cBadFolderNames[kNumBadFolderNames] = {
	"Attachments Folder",
	"Eudora Items",
	"Nicknames Folder",
	"Parts Folder",
	"Signature Folder",
	"Spool Folder",
	"Stationery Folder"
};

PRBool nsEudoraMac::IsValidMailFolderName( nsCString& name)
{
	if (m_depth > 1)
		return( PR_TRUE);
	
	for (int i = 0; i < kNumBadFolderNames; i++) {
		if (name.Equals( cBadFolderNames[i], nsCaseInsensitiveCStringComparator()))
			return( PR_FALSE);
	}
	
	return( PR_TRUE);
}


PRBool nsEudoraMac::IsValidMailboxName( nsCString& fName)
{
	if (m_depth > 1)
		return( PR_TRUE);
	if (fName.Equals(NS_LITERAL_CSTRING("Eudora Nicknames"), nsCaseInsensitiveCStringComparator()))
		return( PR_FALSE);
	return( PR_TRUE);
}


PRBool nsEudoraMac::IsValidMailboxFile( nsIFileSpec *pFile)
{
	PRUint32	size = 0;
	nsresult rv = pFile->GetFileSize( &size);
	if (size) {
		if (size < 10)
			return( PR_FALSE);
		rv = pFile->OpenStreamForReading();
		if (NS_FAILED( rv))
			return( PR_FALSE);
		PRInt32	read = 0;
		char	buffer[6];
		char *	pBuf = buffer;
		rv = pFile->Read( &pBuf, 5, &read);
		pFile->CloseStream();
		if (NS_FAILED( rv) || (read != 5))
			return( PR_FALSE);
		buffer[5] = 0;
		if (nsCRT::strcmp( buffer, "From "))
			return( PR_FALSE);
	}
	
	return( PR_TRUE);
}




PRBool nsEudoraMac::FindAddressFolder( nsIFileSpec *pFolder)
{
	return( FindEudoraLocation( pFolder));
}

nsresult nsEudoraMac::FindAddressBooks( nsIFileSpec *pRoot, nsISupportsArray **ppArray)
{
	// Look for the nicknames file in this folder and then
	// additional files in the Nicknames folder
	// Try and find the nickNames file
	nsCOMPtr<nsIFileSpec>	spec;
	nsresult rv = NS_NewFileSpec( getter_AddRefs( spec));
	if (NS_FAILED( rv))
		return( rv);
	rv = spec->FromFileSpec( pRoot);
	if (NS_FAILED( rv))
		return( rv);
	rv = NS_NewISupportsArray( ppArray);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray\n");
		return( rv);
	}
		
	nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
	if (NS_FAILED( rv))
		return( rv);
	
	
	nsString		displayName;
	nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NICKNAMES_NAME, displayName);
	PRUint32		sz = 0;
	
	// First find the Nicknames file itself
	rv = spec->AppendRelativeUnixPath( "Eudora Nicknames");
	PRBool	exists = PR_FALSE;
	PRBool	isFile = PR_FALSE;
	if (NS_SUCCEEDED( rv))
		rv = spec->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = spec->IsFile( &isFile);

    // XXX this should be fixed by implementing bug 323211
    nsFileSpec fileSpec;
    rv = spec->GetFileSpec(&fileSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> fileLoc;
    rv = NS_FileSpecToIFile(&fileSpec, getter_AddRefs(fileLoc));
    NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIImportABDescriptor>	desc;
	nsISupports *					pInterface;
	
	if (exists && isFile) {
		rv = impSvc->CreateNewABDescriptor( getter_AddRefs( desc));
		if (NS_SUCCEEDED( rv)) {
			sz = 0;
			spec->GetFileSize( &sz);	
			desc->SetPreferredName(displayName);
			desc->SetSize( sz);
            desc->SetAbFile(fileLoc);
			rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
			(*ppArray)->AppendElement( pInterface);
			pInterface->Release();
		}
		if (NS_FAILED( rv)) {
			IMPORT_LOG0( "*** Error creating address book descriptor for eudora nicknames\n");
			return( rv);
		}
	}
	
	// Now try the directory of address books!
	rv = spec->FromFileSpec( pRoot);
	if (NS_SUCCEEDED( rv))
		rv = spec->AppendRelativeUnixPath( "Nicknames Folder");
	exists = PR_FALSE;
	PRBool	isDir = PR_FALSE;
	if (NS_SUCCEEDED( rv))
		rv = spec->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = spec->IsDirectory( &isDir);
	
	if (!isDir)
		return( NS_OK);	
	
	// We need to iterate the directory
	nsCOMPtr<nsIDirectoryIterator>	dir;
	rv = NS_NewDirectoryIterator( getter_AddRefs( dir));
	if (NS_FAILED( rv))
		return( rv);

	exists = PR_FALSE;
	rv = dir->Init( spec, PR_TRUE);
	if (NS_FAILED( rv))
		return( rv);

	rv = dir->Exists( &exists);
	if (NS_FAILED( rv))
		return( rv);
	
	char *					pName;
	nsFileSpec				fSpec;
	OSType					type;
	OSType					creator;
	
	while (exists && NS_SUCCEEDED( rv)) {
		rv = dir->GetCurrentSpec( getter_AddRefs( spec));
		if (NS_SUCCEEDED( rv)) {
			isFile = PR_FALSE;
			pName = nsnull;
			rv = spec->IsFile( &isFile);
			rv = spec->GetLeafName( &pName);
			if (pName)	{
				NS_CopyNativeToUnicode(nsDependentCString(pName), displayName);
				nsCRT::free( pName);
			}
			if (NS_SUCCEEDED( rv) && pName && isFile) {
				rv = spec->GetFileSpec( &fSpec);
				if (NS_SUCCEEDED( rv)) {
					type = 0;
					creator = 0;
#ifdef XP_MACOSX
  {
    nsCOMPtr<nsILocalFileMac> macFile;
    rv = NS_FileSpecToILocalFileMac(&fSpec, getter_AddRefs(macFile));
    if (NS_SUCCEEDED(rv))
    {
      macFile->GetFileCreator(&creator);
      macFile->GetFileType(&type);
    }
  }
#else
					fSpec.GetFileTypeAndCreator( &type, &creator);
#endif
					if (type == 'TEXT') {

                        // XXX this should be fixed by implementing bug 323211
                        rv = spec->GetFileSpec(&fileSpec);
                        NS_ENSURE_SUCCESS(rv, rv);

                        rv = NS_FileSpecToIFile(&fileSpec,
                                                getter_AddRefs(fileLoc));
                        NS_ENSURE_SUCCESS(rv, rv);

						rv = impSvc->CreateNewABDescriptor( getter_AddRefs( desc));
						if (NS_SUCCEEDED( rv)) {
							sz = 0;
							spec->GetFileSize( &sz);	
							desc->SetPreferredName(displayName);
							desc->SetSize( sz);
                            desc->SetAbFile(fileLoc);
							rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
							(*ppArray)->AppendElement( pInterface);
							pInterface->Release();
						}
						if (NS_FAILED( rv)) {
							IMPORT_LOG0( "*** Error creating address book descriptor for eudora address book\n");
							return( rv);
						}
					}
				}
			}				
		}

		rv = dir->Next();
		if (NS_SUCCEEDED( rv))
			rv = dir->Exists( &exists);
	}

	
	return( rv);
}


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
#include "nsReadableUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgAccount.h"
#include "nsIPop3IncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCompCID.h"
#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsEudoraWin32.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportABDescriptor.h"
#include "nsEudoraStringBundle.h"
#include "nsEudoraImport.h"
#include "nsUnicharUtils.h"
#include "EudoraDebugLog.h"

static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);

static const char *	kWhitespace = "\b\t\r\n ";

// ::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Accounts", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &sKey) == ERROR_SUCCESS)

BYTE * nsEudoraWin32::GetValueBytes( HKEY hKey, const char *pValueName)
{
	LONG	err;
	DWORD	bufSz;
	LPBYTE	pBytes = NULL;

	err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		pBytes = new BYTE[bufSz];
		err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, pBytes, &bufSz);
		if (err != ERROR_SUCCESS) {
			delete [] pBytes;
			pBytes = NULL;
		}
	}

	return( pBytes);
}


nsEudoraWin32::nsEudoraWin32()
{
	m_mailImportLocation = nsnull;
	m_addressImportFolder = nsnull;
	m_pMimeSection = nsnull;
}

nsEudoraWin32::~nsEudoraWin32()
{
	NS_IF_RELEASE( m_mailImportLocation);
	NS_IF_RELEASE( m_addressImportFolder);
	if (m_pMimeSection)
		delete [] m_pMimeSection;
}

PRBool nsEudoraWin32::FindMailFolder( nsIFileSpec *pFolder)
{
	return( FindEudoraLocation( pFolder));
}

PRBool nsEudoraWin32::FindEudoraLocation( nsIFileSpec *pFolder, PRBool findIni)
{
	PRBool	result = PR_FALSE;

	// look in the registry to see where eudora is installed?
	HKEY	sKey;
	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Qualcomm\\Eudora\\CommandLine", 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
		// get the value of "Current"
		BYTE *pBytes = GetValueBytes( sKey, "Current");
		if (pBytes) {
			nsCString str((const char *)pBytes);
			delete [] pBytes;
			
			// Command line is Eudora mailfolder eudora.ini
			int	idx = -1;
			if (str.CharAt( 0) == '"') {
				idx = str.FindChar( '"', 1);
				if (idx != -1)
					idx++;
			}
			else {
				idx = str.FindChar( ' ');
			}
			
			if ((idx != -1) && findIni) {
				idx++;
				while (str.CharAt( idx) == ' ') idx++;
				int eIdx = -1;
				if (str.CharAt( idx) == '"') {
					eIdx = str.FindChar( '"', idx);
				}
				else {
					eIdx = str.FindChar( ' ', idx);
				}
				idx = eIdx;
			}

			if (idx != -1) {
				idx++;
				while (str.CharAt( idx) == ' ') idx++;
				int endIdx = -1;
				if (str.CharAt( idx) == '"') {
					endIdx = str.FindChar( '"', idx);
				}
				else {
					endIdx = str.FindChar( ' ', idx);
				}
				if (endIdx != -1) {
					nsCString	path;
					str.Mid( path, idx, endIdx - idx);					
					
					/*  
						For some reason, long long ago, it was necessary
						to clean up the path to Eudora so that it didn't contain
						a mix of short file names and long file names.  This is
						no longer true.

					ConvertPath( path);
					
					IMPORT_LOG1( "** GetShortPath returned: %s\n", path.get());
					*/

					pFolder->SetNativePath( path.get());
					PRBool exists = PR_FALSE;
					if (findIni) {
						if (NS_SUCCEEDED( pFolder->IsFile( &exists))) {
							result = exists;
						}
					}
					else {
						if (NS_SUCCEEDED( pFolder->IsDirectory( &exists))) {
							result = exists;
						}
					}
				}
			}
		}
		::RegCloseKey( sKey);
	}

	return( result);
}

nsresult nsEudoraWin32::FindMailboxes( nsIFileSpec *pRoot, nsISupportsArray **ppArray)
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


nsresult nsEudoraWin32::ScanMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
{
	PRBool					exists = PR_FALSE;
	PRBool					isFile = PR_FALSE;
	char *					pContents = nsnull;
	PRInt32					len = 0;
	nsCOMPtr<nsIFileSpec>	descMap;
	nsresult				rv;

	if (NS_FAILED( rv = NS_NewFileSpec( getter_AddRefs( descMap))))
		return( rv);

	m_depth++;

	descMap->FromFileSpec( pFolder);
	rv = descMap->AppendRelativeUnixPath( "descmap.pce");
	if (NS_SUCCEEDED( rv))
		rv = descMap->IsFile( &isFile);
	if (NS_SUCCEEDED( rv))
		rv = descMap->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists && isFile) {
		rv = descMap->GetFileContents( &pContents);
		if (NS_SUCCEEDED( rv) && pContents) {
			len = strlen( pContents);	
			if (NS_SUCCEEDED( rv))
				rv = ScanDescmap( pFolder, pArray, pImport, pContents, len);
			nsCRT::free( pContents);
		}
		else
			rv = NS_ERROR_FAILURE;
	}

	if (NS_FAILED( rv) || !isFile || !exists)
		rv = IterateMailDir( pFolder, pArray, pImport);
	
	m_depth--;

	return( rv);			
}

nsresult nsEudoraWin32::IterateMailDir( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport)
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

	while (exists && NS_SUCCEEDED( rv)) {
		rv = dir->GetCurrentSpec( getter_AddRefs( entry));
		if (NS_SUCCEEDED( rv)) {
			rv = entry->GetLeafName( &pName);
			if (NS_SUCCEEDED( rv) && pName) {
				fName = pName;
				nsCRT::free( pName);
				if (fName.Length() > 4) {
					fName.Right( ext, 4);
					fName.Left( name, fName.Length() - 4);
				}
				else {
					ext.Truncate();
					name = fName;
				}
				ToLowerCase(ext);
				if (ext.EqualsLiteral(".fol")) {
					isFolder = PR_FALSE;
					entry->IsDirectory( &isFolder);
					if (isFolder) {
						// add the folder
						rv = FoundMailFolder( entry, name.get(), pArray, pImport);
						if (NS_SUCCEEDED( rv)) {
							rv = ScanMailDir( entry, pArray, pImport);
							if (NS_FAILED( rv)) {
								IMPORT_LOG0( "*** Error scanning mail directory\n");
							}
						}
					}
				}
				else if (ext.EqualsLiteral(".mbx")) {
					isFile = PR_FALSE;
					entry->IsFile( &isFile);
					if (isFile) {
						rv = FoundMailbox( entry, name.get(), pArray, pImport);
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

nsresult nsEudoraWin32::ScanDescmap( nsIFileSpec *pFolder, nsISupportsArray *pArray, nsIImportService *pImport, const char *pData, PRInt32 len)
{
	// use this to find stuff in the directory.

	nsCOMPtr<nsIFileSpec>	entry;
	nsresult				rv;

	if (NS_FAILED( rv = NS_NewFileSpec( getter_AddRefs( entry))))
		return( rv);
	
	// format is Name,FileName,Type,Flag?
	//	Type = M or S for mailbox
	//		 = F for folder

	PRInt32			fieldLen;
	PRInt32			pos = 0;
	const char *	pStart;
	nsCString		name;
	nsCString		fName;
	nsCString		type;
	nsCString		flag;
	PRBool			isFile;
	PRBool			isFolder;
	while (pos < len) {
		pStart = pData;
		fieldLen = 0;
		while ((pos < len) && (*pData != ',')) {
			pos++;
			pData++;
			fieldLen++;
		}
		name.Truncate();
		if (fieldLen)
			name.Append( pStart, fieldLen);
		name.Trim( kWhitespace);
		pos++;
		pData++;
		pStart = pData;
		fieldLen = 0;
		while ((pos < len) && (*pData != ',')) {
			pos++;
			pData++;
			fieldLen++;
		}
		fName.Truncate();
		if (fieldLen)
			fName.Append( pStart, fieldLen);
		fName.Trim( kWhitespace);
		pos++;
		pData++;
		pStart = pData;
		fieldLen = 0;
		while ((pos < len) && (*pData != ',')) {
			pos++;
			pData++;
			fieldLen++;
		}
		type.Truncate();
		if (fieldLen)
			type.Append( pStart, fieldLen);		
		type.Trim( kWhitespace);
		pos++;
		pData++;
		pStart = pData;
		fieldLen = 0;
		while ((pos < len) && (*pData != 0x0D) && (*pData != 0x0A) && (*pData != ',')) {
			pos++;
			pData++;
			fieldLen++;
		}
		flag.Truncate();
		if (fieldLen)
			flag.Append( pStart, fieldLen);		
		flag.Trim( kWhitespace);
		while ((pos < len) && ((*pData == 0x0D) || (*pData == 0x0A))) {
			pos++;
			pData++;
		}

		IMPORT_LOG2( "name: %s, fName: %s\n", name.get(), fName.get());

		if (!fName.IsEmpty() && !name.IsEmpty() && (type.Length() == 1)) {
			entry->FromFileSpec( pFolder);
			rv = entry->AppendRelativeUnixPath( fName.get());
			if (NS_SUCCEEDED( rv)) {
				if (type.CharAt( 0) == 'F') {
					isFolder = PR_FALSE;
					entry->IsDirectory( &isFolder);
					if (isFolder) {
						rv = FoundMailFolder( entry, name.get(), pArray, pImport);
						if (NS_SUCCEEDED( rv)) {
							rv = ScanMailDir( entry, pArray, pImport);
							if (NS_FAILED( rv)) {
								IMPORT_LOG0( "*** Error scanning mail directory\n");
							}
						}
					}
				}
				else if ((type.CharAt( 0) == 'M') || (type.CharAt( 0) == 'S')) {
					isFile = PR_FALSE;
					entry->IsFile( &isFile);
					if (isFile) {
						FoundMailbox( entry, name.get(), pArray, pImport);
					}
				}
			}
		}
	}

	return( NS_OK);
}


nsresult nsEudoraWin32::FoundMailbox( nsIFileSpec *mailFile, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
	nsString								displayName;
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


nsresult nsEudoraWin32::FoundMailFolder( nsIFileSpec *mailFolder, const char *pName, nsISupportsArray *pArray, nsIImportService *pImport)
{
	nsString								displayName;
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


nsresult nsEudoraWin32::FindTOCFile( nsIFileSpec *pMailFile, nsIFileSpec **ppTOCFile, PRBool *pDeleteToc)
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
	nsCString	name;
	if ((leaf.Length() > 4) && (leaf.CharAt( leaf.Length() - 4) == '.'))
		leaf.Left( name, leaf.Length() - 4);
	else
		name = leaf;
	name.Append( ".toc");
	rv = (*ppTOCFile)->AppendRelativeUnixPath( name.get());
	if (NS_FAILED( rv))
		return( rv);

	PRBool	exists = PR_FALSE;
	rv = (*ppTOCFile)->Exists( &exists);
	if (NS_FAILED( rv))
		return( rv);
	PRBool	isFile = PR_FALSE;
	rv = (*ppTOCFile)->IsFile( &isFile);
	if (NS_FAILED( rv))
		return( rv);
	if (exists && isFile)
		return( NS_OK);
		
	return( NS_ERROR_FAILURE);
}


PRBool nsEudoraWin32::ImportSettings( nsIFileSpec *pIniFile, nsIMsgAccount **localMailAccount)
{
	PRBool		result = PR_FALSE;
	nsresult	rv;

	nsCOMPtr<nsIMsgAccountManager> accMgr = 
	         do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
		IMPORT_LOG0( "*** Failed to create a account manager!\n");
		return( PR_FALSE);
	}

	// Eudora info is arranged by key, 1 for the default, then persona's for additional
	// accounts.
	// Start with the first one, then do each persona
	char *pIniPath = nsnull;
	pIniFile->GetNativePath( &pIniPath);
	if (!pIniPath)
		return( PR_FALSE);

	nsCString	iniPath(pIniPath);
	nsCRT::free( pIniPath);
	
	UINT			valInt;
	SimpleBufferTonyRCopiedOnce	section;
	DWORD			sSize;
	DWORD			sOffset = 0;
	DWORD			start;
	nsCString		sectionName("Settings");
	int				popCount = 0;
	int				accounts = 0;
	
	DWORD	allocSize = 0;
	do {
		allocSize += 2048;
		section.Allocate( allocSize);
		sSize = ::GetPrivateProfileSection( "Personalities", section.m_pBuffer, allocSize, iniPath.get());
	} while (sSize == (allocSize - 2));
	
	nsIMsgAccount *	pAccount;

	do {
		if (!sectionName.IsEmpty()) {
			pAccount = nsnull;
			valInt = ::GetPrivateProfileInt( sectionName.get(), "UsesPOP", 1, iniPath.get());
			if (valInt) {
				// This is a POP account
				if (BuildPOPAccount( accMgr, sectionName.get(), iniPath.get(), &pAccount)) {
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
				valInt = ::GetPrivateProfileInt( sectionName.get(), "UsesIMAP", 0, iniPath.get());
				if (valInt) {
					// This is an IMAP account
					if (BuildIMAPAccount( accMgr, sectionName.get(), iniPath.get(), &pAccount)) {
						accounts++;
					}
				}
			}
			if (pAccount && (sOffset == 0)) {
				accMgr->SetDefaultAccount( pAccount);
			}

			NS_IF_RELEASE( pAccount);
		}
				
		sectionName.Truncate();
		while ((sOffset < sSize) && (section.m_pBuffer[sOffset] != '='))
			sOffset++;
		sOffset++;
		start = sOffset;
		while ((sOffset < sSize) && (section.m_pBuffer[sOffset] != 0))
			sOffset++;
		if (sOffset > start) {
			sectionName.Append( section.m_pBuffer + start, sOffset - start);
			sectionName.Trim( kWhitespace);
		}

	} while (sOffset < sSize);

	// Now save the new acct info to pref file.
	rv = accMgr->SaveAccountInfo();
	NS_ASSERTION(NS_SUCCEEDED(rv), "Can't save account info to pref file");


	return( accounts != 0);
}

// maximium size of settings strings
#define	kIniValueSize		384


void nsEudoraWin32::GetServerAndUserName( const char *pSection, const char *pIni, nsCString& serverName, nsCString& userName, char *pBuff)
{
	DWORD		valSize;
	int			idx;
	nsCString	tStr;

	serverName.Truncate();
	userName.Truncate();

	valSize = ::GetPrivateProfileString( pSection, "PopServer", "", pBuff, kIniValueSize, pIni);
	if (valSize)
		serverName = pBuff;
	else {
		valSize = ::GetPrivateProfileString( pSection, "POPAccount", "", pBuff, kIniValueSize, pIni);
		if (valSize) {
			serverName = pBuff;
			idx = serverName.FindChar( '@');
			if (idx != -1) {
				serverName.Right( tStr, serverName.Length() - idx - 1);
				serverName = tStr;
			}
		}
	}
	valSize = ::GetPrivateProfileString( pSection, "LoginName", "", pBuff, kIniValueSize, pIni);
	if (valSize)
		userName = pBuff;
	else {
		valSize = ::GetPrivateProfileString( pSection, "POPAccount", "", pBuff, kIniValueSize, pIni);
		if (valSize) {
			userName = pBuff;
			idx = userName.FindChar( '@');
			if (idx != -1) {
				userName.Left( tStr, idx);
				userName = tStr;
			}
		}
	}
}

void nsEudoraWin32::GetAccountName( const char *pSection, nsString& str)
{
	str.Truncate();

	nsCString	s(pSection);
	
	if (s.Equals(NS_LITERAL_CSTRING("Settings"), nsCaseInsensitiveCStringComparator())) {
		str.AssignLiteral("Eudora ");
		str.AppendWithConversion( pSection);
	}
	else {
		nsCString tStr;
		str.AssignWithConversion(pSection);
		if (s.Length() > 8) {
			s.Left( tStr, 8); 
			if (tStr.Equals(NS_LITERAL_CSTRING("Persona-"), nsCaseInsensitiveCStringComparator())) {
				s.Right( tStr, s.Length() - 8);
				str.AssignWithConversion(tStr.get());
			}
		}
	}
}


PRBool nsEudoraWin32::BuildPOPAccount( nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount)
{
	if (ppAccount)
		*ppAccount = nsnull;
	
	char		valBuff[kIniValueSize];
	nsCString	serverName;
	nsCString	userName;
	
	GetServerAndUserName( pSection, pIni, serverName, userName, valBuff);
	
	if (serverName.IsEmpty() || userName.IsEmpty())
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	// I now have a user name/server name pair, find out if it already exists?
	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = accMgr->FindServer( userName.get(), serverName.get(), "pop3", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = accMgr->CreateIncomingServer( userName.get(), serverName.get(), "pop3", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "pop3");
			// rv = in->SetHostName( serverName);
			// rv = in->SetUsername( userName);

			IMPORT_LOG2( "Created POP3 server named: %s, userName: %s\n", serverName.get(), userName.get());

			nsString	prettyName;
			GetAccountName( pSection, prettyName);

			PRUnichar *pretty = ToNewUnicode(prettyName);
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
        UINT valInt = ::GetPrivateProfileInt(pSection, "LeaveMailOnServer", 0, pIni);
        pop3Server->SetLeaveMessagesOnServer(valInt ? PR_TRUE : PR_FALSE);
				
				// Fiddle with the identities
				SetIdentities(accMgr, account, pSection, pIni, userName.get(), serverName.get(), valBuff);
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


PRBool nsEudoraWin32::BuildIMAPAccount( nsIMsgAccountManager *accMgr, const char *pSection, const char *pIni, nsIMsgAccount **ppAccount)
{
	char		valBuff[kIniValueSize];
	nsCString	serverName;
	nsCString	userName;
	
	GetServerAndUserName( pSection, pIni, serverName, userName, valBuff);
	
	if (serverName.IsEmpty() || userName.IsEmpty())
		return( PR_FALSE);

	PRBool	result = PR_FALSE;

	nsCOMPtr<nsIMsgIncomingServer>	in;
	nsresult rv = accMgr->FindServer( userName.get(), serverName.get(), "imap", getter_AddRefs( in));
	if (NS_FAILED( rv) || (in == nsnull)) {
		// Create the incoming server and an account for it?
		rv = accMgr->CreateIncomingServer( userName.get(), serverName.get(), "imap", getter_AddRefs( in));
		if (NS_SUCCEEDED( rv) && in) {
			rv = in->SetType( "imap");
			// rv = in->SetHostName( serverName);
			// rv = in->SetUsername( userName);
			
			IMPORT_LOG2( "Created IMAP server named: %s, userName: %s\n", serverName.get(), userName.get());
			
			nsString	prettyName;
			GetAccountName( pSection, prettyName);

			PRUnichar *pretty = ToNewUnicode(prettyName);
			
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
				SetIdentities(accMgr, account, pSection, pIni, userName.get(), serverName.get(), valBuff);
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

void nsEudoraWin32::SetIdentities(nsIMsgAccountManager *accMgr, nsIMsgAccount *acc, const char *pSection, const char *pIniFile, const char *userName, const char *serverName, char *pBuff)
{
	nsCAutoString	realName;
	nsCAutoString	email;
	nsCAutoString	server;
	DWORD		valSize;
	nsresult	rv;

	valSize = ::GetPrivateProfileString( pSection, "RealName", "", pBuff, kIniValueSize, pIniFile);
	if (valSize)
		realName = pBuff;
	valSize = ::GetPrivateProfileString( pSection, "SMTPServer", "", pBuff, kIniValueSize, pIniFile);
	if (valSize)
		server = pBuff;
	valSize = ::GetPrivateProfileString( pSection, "ReturnAddress", "", pBuff, kIniValueSize, pIniFile);
	if (valSize)
		email = pBuff;
	
	nsCOMPtr<nsIMsgIdentity>	id;
	rv = accMgr->CreateIdentity( getter_AddRefs( id));
	if (id) {
		nsAutoString fullName; 
		fullName.AssignWithConversion(realName.get());
		id->SetFullName( fullName.get());
		id->SetIdentityName( fullName.get());
		if (email.IsEmpty()) {
			email = userName;
			email += "@";
			email += serverName;
		}
		id->SetEmail(email.get());
		acc->AddIdentity( id);

		IMPORT_LOG0( "Created identity and added to the account\n");
		IMPORT_LOG1( "\tname: %s\n", realName.get());
		IMPORT_LOG1( "\temail: %s\n", email.get());
	}

	SetSmtpServer( accMgr, acc, server.get(), userName);
		
}


void nsEudoraWin32::SetSmtpServer( nsIMsgAccountManager *pMgr, nsIMsgAccount *pAcc, const char *pServer, const char *pUser)
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

nsresult nsEudoraWin32::GetAttachmentInfo( const char *pFileName, nsIFileSpec *pSpec, nsCString& mimeType)
{
	mimeType.Truncate();
	pSpec->SetNativePath( pFileName);
	nsresult	rv;
	PRBool		isFile = PR_FALSE;
	PRBool		exists = PR_FALSE;
	if (NS_FAILED( rv = pSpec->Exists( &exists)))
		return( rv);
	if (NS_FAILED( rv = pSpec->IsFile( &isFile)))
		return( rv);
	if (exists && isFile) {
		char *pLeaf = nsnull;
		pSpec->GetLeafName( &pLeaf);
		if (!pLeaf)
			return( NS_ERROR_FAILURE);
		nsCString name(pLeaf);
		nsCRT::free( pLeaf);
		if (name.Length() > 4) {
			nsCString ext;
			PRInt32 idx = name.RFindChar( '.');
			if (idx != -1) {
				name.Right( ext, name.Length() - idx);
				GetMimeTypeFromExtension( ext, mimeType);
			}
		}
		if (mimeType.IsEmpty())
			mimeType = "application/octet-stream";

		return( NS_OK);
	}
	
	return( NS_ERROR_FAILURE);
}

PRBool nsEudoraWin32::FindMimeIniFile( nsIFileSpec *pSpec)
{
	nsCOMPtr<nsIDirectoryIterator>	dir;
	nsresult rv = NS_NewDirectoryIterator( getter_AddRefs( dir));
	if (NS_FAILED( rv))
		return( PR_FALSE);

	PRBool	exists = PR_FALSE;
	rv = dir->Init( pSpec, PR_TRUE);
	if (NS_FAILED( rv))
		return( PR_FALSE);

	rv = dir->Exists( &exists);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	PRBool					isFile;
	nsCOMPtr<nsIFileSpec>	entry;
	char *					pName;
	nsCString				fName;
	nsCString				ext;
	nsCString				name;
	PRBool					found = PR_FALSE;

	while (exists && NS_SUCCEEDED( rv)) {
		rv = dir->GetCurrentSpec( getter_AddRefs( entry));
		if (NS_SUCCEEDED( rv)) {
			rv = entry->GetLeafName( &pName);
			if (NS_SUCCEEDED( rv) && pName) {
				fName = pName;
				nsCRT::free( pName);
				if (fName.Length() > 4) {
					fName.Right( ext, 4);
					fName.Left( name, fName.Length() - 4);
				}
				else {
					ext.Truncate();
					name = fName;
				}
				ToLowerCase(ext);
				if (ext.EqualsLiteral(".ini")) {
					isFile = PR_FALSE;
					entry->IsFile( &isFile);
					if (isFile) {
						if (found) {
							// which one of these files is newer?
							PRUint32	modDate1, modDate2;
							entry->GetModDate( &modDate2);
							pSpec->GetModDate( &modDate1);
							if (modDate2 > modDate1)
								pSpec->FromFileSpec( entry);
						}
						else {
							pSpec->FromFileSpec( entry);
							found = PR_TRUE;
						}
					}
				}
			}				
		}
		
		rv = dir->Next();
		if (NS_SUCCEEDED( rv))
			rv = dir->Exists( &exists);
	}
	
	if (found)
		return( PR_TRUE);
	else
		return( PR_FALSE);
}


void nsEudoraWin32::GetMimeTypeFromExtension( nsCString& ext, nsCString& mimeType)
{
	HKEY	sKey;
	if (::RegOpenKeyEx( HKEY_CLASSES_ROOT, ext.get(), 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
		// get the value of "Current"
		BYTE *pBytes = GetValueBytes( sKey, "Content Type");
		if (pBytes) {
			mimeType = (const char *)pBytes;
			delete [] pBytes;
		}
		::RegCloseKey( sKey);
	}

	if (!mimeType.IsEmpty() || !m_mailImportLocation || (ext.Length() > 10))
		return;
	
	// TLR: FIXME: We should/could cache the extension to mime type maps we find
	// and check the cache first before scanning all of eudora's mappings?

	// It's not in the registry, try and find the .ini file for Eudora's private
	// mime type list
	if (!m_pMimeSection) {
		nsIFileSpec *pSpec;
		nsresult rv = NS_NewFileSpec( &pSpec);
		if (NS_FAILED( rv) || !pSpec)
			return;

		pSpec->FromFileSpec( m_mailImportLocation);

		pSpec->AppendRelativeUnixPath( "eudora.ini");
		PRBool exists = PR_FALSE;
		PRBool isFile = PR_FALSE;
		rv = pSpec->Exists( &exists);
		if (NS_SUCCEEDED( rv))
			rv = pSpec->IsFile( &isFile);
		
		if (!isFile || !exists) {
			rv = pSpec->FromFileSpec( m_mailImportLocation);
			if (NS_FAILED( rv))
				return;
			if (!FindMimeIniFile( pSpec))
				return;
		}

		char *pName = nsnull;
		pSpec->GetNativePath( &pName);
		if (!pName)
			return;
		nsCString	fileName(pName);
		nsCRT::free( pName);

		// Read the mime map section
		DWORD	size = 1024;
		DWORD	dSize = size - 2;
		while (dSize == (size - 2)) {
			if (m_pMimeSection)
				delete [] m_pMimeSection;
			size += 1024;
			m_pMimeSection = new char[size];
			dSize = ::GetPrivateProfileSection( "Mappings", m_pMimeSection, size, fileName.get());
		}
	}

	if (!m_pMimeSection)
		return;
	
	IMPORT_LOG1( "Looking for mime type for extension: %s\n", ext.get());

	// out/in/both=extension,mac creator,mac type,mime type1,mime type2
	const char *pExt = ext.get();
	pExt++;

	char	*	pChar = m_pMimeSection;
	char	*	pStart;
	int			len;
	nsCString	tStr;
	for(;;) {
		while (*pChar != '=') {
			if (!(*pChar) && !(*(pChar + 1)))
				return;
			pChar++;
		}
		if (*pChar)
			pChar++;
		pStart = pChar;
		len = 0;
		while (*pChar && (*pChar != ',')) {
			pChar++;
			len++;
		}
		if (!*pChar) return;
		tStr.Truncate();
		tStr.Append( pStart, len);
		tStr.Trim( kWhitespace);
		if (!nsCRT::strcasecmp( tStr.get(), pExt)) {
			// skip the mac creator and type
			pChar++;
			while (*pChar && (*pChar != ','))
				pChar++;
			if (!*pChar) return;
			pChar++;
			while (*pChar && (*pChar != ','))
				pChar++;
			if (!*pChar) return;
			pChar++;
			// Get the first mime type
			len = 0;
			pStart = pChar;
			while (*pChar && (*pChar != ',')) {
				pChar++;
				len++;
			}
			if (!*pChar) return;
			pChar++;
			if (!len) continue;
			tStr.Truncate();
			tStr.Append( pStart, len);
			tStr.Trim( kWhitespace);
			if (tStr.IsEmpty()) continue;
			mimeType.Truncate();
			mimeType.Append( tStr.get());
			mimeType.Append( "/");
			pStart = pChar;
			len = 0;
			while (*pChar && (*pChar != 0x0D) && (*pChar != 0x0A)) {
				pChar++;
				len++;
			}
			if (!len) continue;
			tStr.Truncate();
			tStr.Append( pStart, len);
			tStr.Trim( kWhitespace);
			if (tStr.IsEmpty()) continue;
			mimeType.Append( tStr.get());

			IMPORT_LOG1( "Found Mime Type: %s\n", mimeType.get());

			return;
		}
	}	
}

PRBool nsEudoraWin32::FindAddressFolder( nsIFileSpec *pFolder)
{
	IMPORT_LOG0( "*** Looking for Eudora address folder\n");

	return( FindEudoraLocation( pFolder));
}

nsresult nsEudoraWin32::FindAddressBooks( nsIFileSpec *pRoot, nsISupportsArray **ppArray)
{

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
	
	NS_IF_RELEASE( m_addressImportFolder);
	m_addressImportFolder = pRoot;
	NS_IF_ADDREF( pRoot);

	
	nsString		displayName;
	nsEudoraStringBundle::GetStringByID( EUDORAIMPORT_NICKNAMES_NAME, displayName);

	// First off, get the default nndbase.txt, then scan the default nicknames subdir,
	// then look in the .ini file for additional directories to scan for address books!
	rv = spec->AppendRelativeUnixPath( "nndbase.txt");
	PRBool exists = PR_FALSE;
	PRBool isFile = PR_FALSE;
	if (NS_SUCCEEDED( rv))
		rv = spec->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = spec->IsFile( &isFile);
	if (exists && isFile) {
		if (NS_FAILED( rv = FoundAddressBook( spec, displayName.get(), *ppArray, impSvc)))
			return( rv);
	}
	
	// Try the default directory
	rv = spec->FromFileSpec( pRoot);
	if (NS_FAILED( rv))
		return( rv);
	rv = spec->AppendRelativeUnixPath( "Nickname");
	PRBool isDir = PR_FALSE;
	exists = PR_FALSE;
	if (NS_SUCCEEDED( rv))
		rv = spec->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = spec->IsDirectory( &isDir);
	if (exists && isDir) {
		if (NS_FAILED( rv = ScanAddressDir( spec, *ppArray, impSvc)))
			return( rv);
	}
	
	// Try the ini file to find other directories!
	rv = spec->FromFileSpec( pRoot);
	if (NS_SUCCEEDED( rv))
		rv = spec->AppendRelativeUnixPath( "eudora.ini");
	exists = PR_FALSE;
	isFile = PR_FALSE;
	if (NS_SUCCEEDED( rv))
		rv = spec->Exists( &exists);
	if (NS_SUCCEEDED( rv) && exists)
		rv = spec->IsFile( &isFile);
	
	if (!isFile || !exists) {
		rv = spec->FromFileSpec( pRoot);
		if (NS_FAILED( rv))
			return( NS_OK);
		if (!FindMimeIniFile( spec))
			return( NS_OK);
	}

	char *pName = nsnull;
	spec->GetNativePath( &pName);
	if (!pName)
		return( NS_OK);
	nsCString	fileName(pName);
	nsCRT::free( pName);
	
	// This is the supposed ini file name!
	// Get the extra directories for nicknames and parse it for valid nickname directories
	// to look into...
	char *pBuffer = new char[2048];
	DWORD len = ::GetPrivateProfileString( "Settings", "ExtraNicknameDirs", "", pBuffer, 2048, fileName.get());
	if (len == 2047) {
		// If the value is really that large then don't bother!
		delete [] pBuffer;
		return( NS_OK);
	}
	nsCString	dirs(pBuffer);
	delete [] pBuffer;
	dirs.Trim( kWhitespace);
	PRInt32	idx = 0;
	nsCString	currentDir;
	while ((idx = dirs.FindChar( ';')) != -1) {
		dirs.Left( currentDir, idx);
		currentDir.Trim( kWhitespace);
		if (!currentDir.IsEmpty()) {
			rv = spec->SetNativePath( currentDir.get());
			exists = PR_FALSE;
			isDir = PR_FALSE;
			if (NS_SUCCEEDED( rv))
				rv = spec->Exists( &exists);
			if (NS_SUCCEEDED( rv) && exists)
				rv = spec->IsDirectory( &isDir);
			if (exists && isDir) {
				if (NS_FAILED( rv = ScanAddressDir( spec, *ppArray, impSvc)))
					return( rv);
			}
		}
		dirs.Right( currentDir, dirs.Length() - idx - 1);
		dirs = currentDir;
		dirs.Trim( kWhitespace);
	}
	if (!dirs.IsEmpty()) {
		rv = spec->SetNativePath( dirs.get());
		exists = PR_FALSE;
		isDir = PR_FALSE;
		if (NS_SUCCEEDED( rv))
			rv = spec->Exists( &exists);
		if (NS_SUCCEEDED( rv) && exists)
			rv = spec->IsDirectory( &isDir);
		if (exists && isDir) {
			if (NS_FAILED( rv = ScanAddressDir( spec, *ppArray, impSvc)))
				return( rv);
		}
	}
	
	return( NS_OK);
}


nsresult nsEudoraWin32::ScanAddressDir( nsIFileSpec *pDir, nsISupportsArray *pArray, nsIImportService *impSvc)
{
	nsCOMPtr<nsIDirectoryIterator>	dir;
	nsresult rv = NS_NewDirectoryIterator( getter_AddRefs( dir));
	if (NS_FAILED( rv))
		return( rv);

	PRBool	exists = PR_FALSE;
	rv = dir->Init( pDir, PR_TRUE);
	if (NS_FAILED( rv))
		return( rv);

	rv = dir->Exists( &exists);
	if (NS_FAILED( rv))
		return( rv);
	
	PRBool					isFile;
	nsCOMPtr<nsIFileSpec>	entry;
	char *					pName;
	nsCString				fName;
	nsCString				ext;
	nsCString				name;

	while (exists && NS_SUCCEEDED( rv)) {
		rv = dir->GetCurrentSpec( getter_AddRefs( entry));
		if (NS_SUCCEEDED( rv)) {
			rv = entry->GetLeafName( &pName);
			if (NS_SUCCEEDED( rv) && pName) {
				fName = pName;
				nsCRT::free( pName);
				if (fName.Length() > 4) {
					fName.Right( ext, 4);
					fName.Left( name, fName.Length() - 4);
				}
				else {
					ext.Truncate();
					name = fName;
				}
				ToLowerCase(ext);
				if (ext.EqualsLiteral(".txt")) {
					isFile = PR_FALSE;
					entry->IsFile( &isFile);
					if (isFile) {
						rv = FoundAddressBook( entry, nsnull, pArray, impSvc);
						if (NS_FAILED( rv))
							return( rv);
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


nsresult nsEudoraWin32::FoundAddressBook( nsIFileSpec *spec, const PRUnichar *pName, nsISupportsArray *pArray, nsIImportService *impSvc)
{
	nsCOMPtr<nsIImportABDescriptor>	desc;
	nsISupports *					pInterface;
	nsString						name;
	nsresult						rv;

	if (pName)
		name = pName;
	else {
		char *pLeaf = nsnull;
		rv = spec->GetLeafName( &pLeaf);
		if (NS_FAILED( rv))
			return( rv);
		if (!pLeaf)
			return( NS_ERROR_FAILURE);
		NS_CopyNativeToUnicode(nsDependentCString(pLeaf), name);
		nsCRT::free( pLeaf);
		nsString	tStr;
		name.Right( tStr, 4);
		if (tStr.LowerCaseEqualsLiteral(".txt")) {
			name.Left( tStr, name.Length() - 4);
			name = tStr;
		}
	}

	rv = impSvc->CreateNewABDescriptor( getter_AddRefs( desc));
	if (NS_SUCCEEDED( rv)) {
		PRUint32 sz = 0;
		spec->GetFileSize( &sz);	
		desc->SetPreferredName( name.get());
		desc->SetSize( sz);
		nsIFileSpec *pSpec = nsnull;
		desc->GetFileSpec( &pSpec);
		if (pSpec) {
			pSpec->FromFileSpec( spec);
			NS_RELEASE( pSpec);
		}
		rv = desc->QueryInterface( kISupportsIID, (void **) &pInterface);
		pArray->AppendElement( pInterface);
		pInterface->Release();
	}
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "*** Error creating address book descriptor for eudora\n");
		return( rv);
	}

	return( NS_OK);
}


void nsEudoraWin32::ConvertPath( nsCString& str)
{
	nsCString	temp;
	nsCString	path;
	PRInt32		idx = 0;
	PRInt32		start = 0;
	nsCString	search;

	idx = str.FindChar( '\\', idx);
	if ((idx == 2) && (str.CharAt( 1) == ':')) {
		str.Left( path, 3);
		idx++;
		idx = str.FindChar( '\\', idx);
		start = 3;
		if ((idx == -1) && (str.Length() > 3)) {
			str.Right( temp, str.Length() - start);
			path.Append( temp);
		}
	}
	
	WIN32_FIND_DATA findFileData;
	while (idx != -1) {
		str.Mid( temp, start, idx - start);
		search = path;
		search.Append( temp);
		HANDLE h = FindFirstFile( search.get(), &findFileData);
		if (h == INVALID_HANDLE_VALUE)
			return;
		path.Append( findFileData.cFileName);
		idx++;
		start = idx;
		idx = str.FindChar( '\\', idx);
		FindClose( h);
		if (idx != -1)
			path.Append( '\\');
		else {
			str.Right( temp, str.Length() - start);
			path.Append( '\\');
			path.Append( temp);
		}
	}

	str = path;
}


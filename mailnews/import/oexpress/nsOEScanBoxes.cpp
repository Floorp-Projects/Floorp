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
 * Portions created by the Initial Developer are Copyright (C) 1998
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
#include "nsOEScanBoxes.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"

#include "nsOERegUtil.h"
#include "nsOE5File.h"

#include "OEDebugLog.h"

/*
	.nch file format???

	offset 20 - long = offset to first record

*/

static NS_DEFINE_CID(kImportServiceCID,		NS_IMPORTSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID,			NS_ISUPPORTS_IID);


nsOEScanBoxes::nsOEScanBoxes()
{
	m_pFirst = nsnull;
}

nsOEScanBoxes::~nsOEScanBoxes()
{
	int max = m_entryArray.Count();
	for (int i = 0; i < max; i++) {
		MailboxEntry *pEntry = (MailboxEntry *) m_entryArray.ElementAt( i);
		delete pEntry;
	}
}


// convert methods
void nsOEScanBoxes::ConvertToUnicode(const char *pStr, nsString &dist)
{
	nsresult rv = NS_OK;

	if (!mService)
		mService = do_GetService(kImportServiceCID);

	if (mService)
		rv = mService->SystemStringToUnicode(pStr, dist);

	if (!mService || NS_FAILED(rv)) // XXX bad cast
		dist.AssignWithConversion(pStr);
}


/*
 3.x & 4.x registry
	Software/Microsoft/Outlook Express/

 5.0 registry
	Identies - value of "Default User ID" is {GUID}
	Identities/{GUID}/Software/Microsoft/Outlook Express/5.0/
*/

PRBool nsOEScanBoxes::Find50Mail( nsIFileSpec *pWhere)
{
	nsresult 	rv;
	PRBool		success = PR_FALSE;
	HKEY		sKey;

	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Identities", 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
		BYTE *	pBytes = nsOERegUtil::GetValueBytes( sKey, "Default User ID");
		::RegCloseKey( sKey);
		if (pBytes) {
			nsCString	key( "Identities\\");
			key += (const char *)pBytes;
			nsOERegUtil::FreeValueBytes( pBytes);
			key += "\\Software\\Microsoft\\Outlook Express\\5.0";
			if (::RegOpenKeyEx( HKEY_CURRENT_USER, key.get(), 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
				pBytes = nsOERegUtil::GetValueBytes( sKey, "Store Root");
				if (pBytes) {
					pWhere->SetNativePath((char *)pBytes);
					
					IMPORT_LOG1( "Setting native path: %s\n", pBytes);

					nsOERegUtil::FreeValueBytes( pBytes);
					PRBool	isDir = PR_FALSE;
					rv = pWhere->IsDirectory( &isDir);
					if (isDir && NS_SUCCEEDED( rv))
						success = PR_TRUE;
				}
				::RegCloseKey( sKey);
			}
		}
	}
	
	return( success);
}

PRBool nsOEScanBoxes::FindMail( nsIFileSpec *pWhere)
{
	nsresult 	rv;
	PRBool		success = PR_FALSE;
	HKEY		sKey;
	
	if (Find50Mail( pWhere))
		return( PR_TRUE);

	if (::RegOpenKeyEx( HKEY_CURRENT_USER, "Software\\Microsoft\\Outlook Express", 0, KEY_QUERY_VALUE, &sKey) == ERROR_SUCCESS) {
		LPBYTE	pBytes = nsOERegUtil::GetValueBytes( sKey, "Store Root");
		if (pBytes) {
			pWhere->SetNativePath((char *)pBytes);
			pWhere->AppendRelativeUnixPath( "Mail");
			PRBool	isDir = PR_FALSE;
			rv = pWhere->IsDirectory( &isDir);
			if (isDir && NS_SUCCEEDED( rv))
				success = PR_TRUE;
			delete [] pBytes;
		}
		::RegCloseKey( sKey);		
	}
	
	return( success);
}

PRBool nsOEScanBoxes::GetMailboxes( nsIFileSpec *pWhere, nsISupportsArray **pArray)
{
	char *path = nsnull;
	pWhere->GetNSPRPath( &path);
	if (path) {
		IMPORT_LOG1( "Looking for mail in: %s\n", path);
		nsCRT::free( path);
	}
	else {
		pWhere->GetLeafName( &path);
		if (path) {
			IMPORT_LOG1( "Looking for mail in: %s\n", path);
			nsCRT::free( path);
		}
		else {
			IMPORT_LOG0( "Unable to get info about where to look for mail\n");
		}
	}
	
	nsIFileSpec	*where;
	if (NS_FAILED( NS_NewFileSpec( &where)))
		return( PR_FALSE);
	where->FromFileSpec( pWhere);

	// 1. Look for 5.0 folders.dbx
	// 2. Look for 3.x & 4.x folders.nch
	// 3. Look for 5.0 *.dbx mailboxes
	// 4. Look for 3.x & 4.x *.mbx mailboxes
	
	PRBool	result;

	where->AppendRelativeUnixPath( "folders.dbx");
	if (Find50MailBoxes( where)) {
		where->CloseStream();
		result = GetMailboxList( pWhere, pArray);
	}
	else {
		// 2. Look for 4.x mailboxes
		where->FromFileSpec( pWhere);
		where->AppendRelativeUnixPath( "folders.nch");
	
		if (FindMailBoxes( where)) {
			where->CloseStream();
			result = GetMailboxList( pWhere, pArray);
		}
		else {
			// 3 & 4, look for the specific mailbox files.
			where->CloseStream();
			where->FromFileSpec( pWhere);
			ScanMailboxDir( where);
			result = GetMailboxList( pWhere, pArray);
		}
	}
		
	where->Release();
	return( result);
}



void nsOEScanBoxes::Reset( void)
{
	int max = m_entryArray.Count();
	for (int i = 0; i < max; i++) {
		MailboxEntry *pEntry = (MailboxEntry *) m_entryArray.ElementAt( i);
		delete pEntry;
	}
	m_entryArray.Clear();
	m_pFirst = nsnull;
}


PRBool nsOEScanBoxes::FindMailBoxes( nsIFileSpec* descFile)
{
	Reset();
	
	nsresult	rv;
	PRBool		isFile = PR_FALSE;
	
	rv = descFile->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( PR_FALSE);
	
	rv = descFile->OpenStreamForReading();	
	if (NS_FAILED( rv))
		return( PR_FALSE);

	IMPORT_LOG0( "Reading the folders.nch file\n");
	
	PRUint32		curRec;
	if (!ReadLong( descFile, curRec, 20)) {
		return( PR_FALSE);
	}

	// Now for each record
	PRBool			done = PR_FALSE;
	PRUint32		equal;
	PRUint32		size;
	PRUint32		previous;
	PRUint32		next;
	MailboxEntry *	pEntry;
	PRBool			failed;
	nsCString		ext;
	nsCString		mbxExt( ".mbx");
	
	while (!done) {
		
		if (!ReadLong( descFile, equal, curRec)) return( PR_FALSE);
		if (curRec != equal) {
			IMPORT_LOG1( "Record start invalid: %ld\n", curRec);
			break;
		}
		if (!ReadLong( descFile, size, curRec + 4)) return( PR_FALSE);
		if (!ReadLong( descFile, previous, curRec + 8)) return( PR_FALSE);
		if (!ReadLong( descFile, next, curRec + 12)) return( PR_FALSE);
		failed = PR_FALSE;
		pEntry = new MailboxEntry;
		if (!ReadLong( descFile, pEntry->index, curRec + 16)) failed = PR_TRUE;
		if (!ReadString( descFile, pEntry->mailName, curRec + 20)) failed = PR_TRUE;
		if (!ReadString( descFile, pEntry->fileName, curRec + 279)) failed = PR_TRUE;
		if (!ReadLong( descFile, pEntry->parent, curRec + 539)) failed = PR_TRUE;
		if (!ReadLong( descFile, pEntry->child, curRec + 543)) failed = PR_TRUE;
		if (!ReadLong( descFile, pEntry->sibling, curRec + 547)) failed = PR_TRUE;
		if (!ReadLong( descFile, pEntry->type, curRec + 551)) failed = PR_TRUE;
		if (failed) {
			delete pEntry;
			return( PR_FALSE);
		}

		#ifdef _TRACE_MAILBOX_ENTRIES
		IMPORT_LOG0( "------------\n");
		IMPORT_LOG2( "    Offset: %lx, index: %ld\n", curRec, pEntry->index);
		IMPORT_LOG2( "      previous: %lx, next: %lx\n", previous, next);
		IMPORT_LOG2( "      Name: %S, File: %s\n", (PRUnichar *) pEntry->mailName, (const char *) pEntry->fileName);
		IMPORT_LOG3( "      Parent: %ld, Child: %ld, Sibling: %ld\n", pEntry->parent, pEntry->child, pEntry->sibling);
		#endif
		
		pEntry->fileName.Right( ext, 4);
		if (Compare(ext, mbxExt))
			pEntry->fileName.Append( ".mbx");		
		
		m_entryArray.AppendElement( pEntry);

		curRec = next;
		if (!next)
			done = PR_TRUE;
	}
	
	MailboxEntry *pZero = GetIndexEntry( 0);
	if (pZero)
		m_pFirst = GetIndexEntry( pZero->child);
	
	IMPORT_LOG1( "Read the folders.nch file, found %ld mailboxes\n", (long) m_entryArray.Count());
	
	return( PR_TRUE);
}

PRBool nsOEScanBoxes::Find50MailBoxes( nsIFileSpec* descFile)
{
	Reset();
	
	nsresult	rv;
	PRBool		isFile = PR_FALSE;
	
	rv = descFile->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( PR_FALSE);
	
	rv = descFile->OpenStreamForReading();	
	if (NS_FAILED( rv))
		return( PR_FALSE);

	IMPORT_LOG0( "Reading the folders.dbx file\n");
	
	PRUint32 *		pIndex;
	PRUint32		indexSize = 0;
	if (!nsOE5File::ReadIndex( descFile, &pIndex, &indexSize)) {
		IMPORT_LOG0( "*** NOT USING FOLDERS.DBX!!!\n");
		return( PR_FALSE);
	}
	
	PRUint32	marker;
	PRUint32	size;
	char	*	pBytes;
	PRInt32		cntRead;
	PRInt32		recordId;
	PRInt32		strOffset;

	PRUint8		tag;
	PRUint32	data;
	PRInt32		dataOffset;
	
	PRUint32		id;
	PRUint32		parent;
	PRUint32		numMessages;
	char *			pFileName;
	char *			pDataSource;

	MailboxEntry *	pEntry;
	MailboxEntry *	pLastEntry = nsnull;

	PRUint32	localStoreId = 0;

	for (PRUint32 i = 0; i < indexSize; i++) {
		if (!ReadLong( descFile, marker, pIndex[i])) continue;
		if (marker != pIndex[i]) continue;
		if (!ReadLong( descFile, size, pIndex[i] + 4)) continue;
		size += 4;
		pBytes = new char[size];
		rv = descFile->Read( &pBytes, size, &cntRead);
		if (NS_FAILED( rv) || ((PRUint32)cntRead != size)) {
			delete [] pBytes;
			continue;
		}
		recordId = pBytes[2];
		strOffset = (recordId * 4) + 4;
		if (recordId == 4)
			strOffset += 4;
		
		id = 0;
		parent = 0;
		numMessages = 0;
		pFileName = nsnull;
		pDataSource = nsnull;
		dataOffset = 4;
		while (dataOffset < strOffset) {
			tag = (PRUint8) pBytes[dataOffset];

			nsCRT::memcpy( &data, &(pBytes[dataOffset + 1]), 3);
			switch( tag) {
				case 0x80: // id record
					id = data;
				break;		
				case 0x81:	// parent id
					parent = data;
				break;
				case 0x87:	// number of messages in this mailbox
					numMessages = data;
				break;
				case 0x03:	// file name for this mailbox
					if (((PRUint32)strOffset + data) < size)
						pFileName = (char *)(pBytes + strOffset + data);
				break;
				case 0x05:	// data source for this record (this is not a mailbox!)
					if (((PRUint32)strOffset + data) < size)
						pDataSource = (char *) (pBytes + strOffset + data);
				break;
			}
			dataOffset += 4;
		}
		
		// now build an entry if necessary!
		if (pDataSource) {
			if (!nsCRT::strcasecmp( pDataSource, "LocalStore"))
				localStoreId = id;	
		}
		else if (id && localStoreId && parent) {
			// veryify that this mailbox is in the local store
			data = parent;
			while (data && (data != localStoreId)) {
				pEntry = GetIndexEntry( data);
				if (pEntry)
					data = pEntry->parent;
				else
					data = 0;
			}
			if (data == localStoreId) {
				// Create an entry for this bugger
				pEntry = new MailboxEntry();
				pEntry->index = id;
				pEntry->parent = parent;
				pEntry->child = 0;
				pEntry->type = 0;
				pEntry->sibling = -1;
				ConvertToUnicode((const char *) (pBytes + strOffset), pEntry->mailName);
				if (pFileName)
					pEntry->fileName = pFileName;
				AddChildEntry( pEntry, localStoreId);
			}
		}

		delete [] pBytes;
	}

	
	delete [] pIndex;

	if (m_entryArray.Count())
		return( PR_TRUE);
	else
		return( PR_FALSE);
}


void nsOEScanBoxes::AddChildEntry( MailboxEntry *pEntry, PRUint32 rootIndex)
{
	if (!m_pFirst) {
		if (pEntry->parent == rootIndex) {
			m_pFirst = pEntry;
			m_entryArray.AppendElement( pEntry);
		}
		else {
			delete pEntry;
		}
		return;
	}
	
	MailboxEntry *	pParent = nsnull;
	MailboxEntry *	pSibling = nsnull;
	if (pEntry->parent == rootIndex) {
		pSibling = m_pFirst;
	}
	else {
		pParent = GetIndexEntry( pEntry->parent);
	}
	
	if (!pParent && !pSibling) {
		delete pEntry;
		return;
	}

	if (pParent && (pParent->child == 0)) {
		pParent->child = pEntry->index;
		m_entryArray.AppendElement( pEntry);
		return;
	}
	
	if (!pSibling)
		pSibling = GetIndexEntry( pParent->child);

	while (pSibling && (pSibling->sibling != -1)) {
		pSibling = GetIndexEntry( pSibling->sibling);
	}

	if (!pSibling) {
		delete pEntry;
		return;
	}

	pSibling->sibling = pEntry->index;
	m_entryArray.AppendElement( pEntry);
}

PRBool nsOEScanBoxes::Scan50MailboxDir( nsIFileSpec * srcDir)
{
	Reset();

	MailboxEntry *	pEntry;	
	PRInt32			index = 1;
	char *			pLeaf;
	PRUint32		sLen;
	
	nsIFileSpec *			spec;
	nsIDirectoryIterator *	iter;
	
	if (NS_FAILED( NS_NewDirectoryIterator( &iter)))
		return( PR_FALSE);
	if (NS_FAILED( iter->Init( srcDir, PR_TRUE))) {
		iter->Release();
		return( PR_FALSE);
	}

	nsresult	rv;
	PRBool		exists = PR_FALSE;
	PRBool		isFile;
	
	rv = iter->Exists( &exists);
	while (NS_SUCCEEDED( rv) && exists) {
	    // do something with i.Spec()
	    rv = iter->GetCurrentSpec( &spec);
	    if (NS_SUCCEEDED( rv)) {
	    	isFile = PR_FALSE;
	    	rv = spec->IsFile( &isFile);
			if (NS_SUCCEEDED( rv) && isFile) {
				pLeaf = nsnull;
				rv = spec->GetLeafName( &pLeaf);
				if (NS_SUCCEEDED( rv) && pLeaf && 
					((sLen = nsCRT::strlen( pLeaf)) > 4) && 
					(!nsCRT::strcasecmp( pLeaf + sLen - 3, "dbx"))) {
					// This is a *.dbx file in the mail directory
					if (nsOE5File::IsLocalMailFile( spec)) {
						pEntry = new MailboxEntry;
						pEntry->index = index;
						index++;
						pEntry->parent = 0;
						pEntry->child = 0;
						pEntry->sibling = index;
						pEntry->type = -1;
						pEntry->fileName = pLeaf;
						pLeaf[sLen - 4] = 0;
						ConvertToUnicode(pLeaf, pEntry->mailName);
						m_entryArray.AppendElement( pEntry);				
					}
				}
				if (pLeaf)
					nsCRT::free( pLeaf);
			}
		}
		rv = iter->Next();    
		exists = PR_FALSE;
		rv = iter->Exists( &exists);
	}
	
	if (m_entryArray.Count() > 0) {
		pEntry = (MailboxEntry *)m_entryArray.ElementAt( m_entryArray.Count() - 1);
		pEntry->sibling = -1;
		return( PR_TRUE);
	}

	return( PR_FALSE);
}


void nsOEScanBoxes::ScanMailboxDir( nsIFileSpec * srcDir)
{
	if (Scan50MailboxDir( srcDir))
		return;

	Reset();

	MailboxEntry *	pEntry;	
	PRInt32			index = 1;
	char *			pLeaf;
	PRUint32		sLen;
	
	nsIFileSpec *			spec;
	nsIDirectoryIterator *	iter;
	
	if (NS_FAILED( NS_NewDirectoryIterator( &iter)))
		return;
	if (NS_FAILED( iter->Init( srcDir, PR_TRUE))) {
		iter->Release();
		return;
	}
	nsresult	rv;
	PRBool		exists = PR_FALSE;
	PRBool		isFile;
	
	rv = iter->Exists( &exists);
	while (NS_SUCCEEDED( rv) && exists) {
	    // do something with i.Spec()
	    rv = iter->GetCurrentSpec( &spec);
	    if (NS_SUCCEEDED( rv)) {
	    	isFile = PR_FALSE;
	    	rv = spec->IsFile( &isFile);
			if (NS_SUCCEEDED( rv) && isFile) {
				pLeaf = nsnull;
				rv = spec->GetLeafName( &pLeaf);
				if (NS_SUCCEEDED( rv) && pLeaf && 
					((sLen = nsCRT::strlen( pLeaf)) > 4) && 
					(!nsCRT::strcasecmp( pLeaf + sLen - 3, "mbx"))) {
					// This is a *.mbx file in the mail directory
					pEntry = new MailboxEntry;
					pEntry->index = index;
					index++;
					pEntry->parent = 0;
					pEntry->child = 0;
					pEntry->sibling = index;
					pEntry->type = -1;
					pEntry->fileName = pLeaf;
					pLeaf[sLen - 4] = 0;
					ConvertToUnicode(pLeaf, pEntry->mailName);
					m_entryArray.AppendElement( pEntry);				
				}
				if (pLeaf)
					nsCRT::free( pLeaf);
			}
		}
		rv = iter->Next();    
		exists = PR_FALSE;
		rv = iter->Exists( &exists);
	}
	
	if (m_entryArray.Count() > 0) {
		pEntry = (MailboxEntry *)m_entryArray.ElementAt( m_entryArray.Count() - 1);
		pEntry->sibling = -1;
	}
}


PRUint32 nsOEScanBoxes::CountMailboxes( MailboxEntry *pBox)
{
	if (pBox == nsnull) {
		if (m_pFirst != nsnull)
			pBox = m_pFirst;
		else {
			if (m_entryArray.Count() > 0)
				pBox = (MailboxEntry *) m_entryArray.ElementAt( 0);
		}
	}
	PRUint32		count = 0;
	
	MailboxEntry *	pChild;
	while (pBox) {
		count++;
		if (pBox->child) {
			pChild = GetIndexEntry( pBox->child);
			if (pChild != nsnull)
				count += CountMailboxes( pChild);
		}
		if (pBox->sibling != -1) {
			pBox = GetIndexEntry( pBox->sibling);
		}
		else
			pBox = nsnull;
	}
	
	return( count);
}

PRBool nsOEScanBoxes::GetMailboxList( nsIFileSpec * root, nsISupportsArray **pArray)
{	
	nsresult rv = NS_NewISupportsArray( pArray);
	if (NS_FAILED( rv)) {
		IMPORT_LOG0( "FAILED to allocate the nsISupportsArray\n");
		return( PR_FALSE);
	}
	
	BuildMailboxList( nsnull, root, 1, *pArray);
	
	return( PR_TRUE);
}

void nsOEScanBoxes::BuildMailboxList( MailboxEntry *pBox, nsIFileSpec * root, PRInt32 depth, nsISupportsArray *pArray)
{
	if (pBox == nsnull) {
		if (m_pFirst != nsnull) {
			pBox = m_pFirst;
			
			IMPORT_LOG0( "Assigning start of mailbox list to m_pFirst\n");
		}
		else {
			if (m_entryArray.Count() > 0) {
				pBox = (MailboxEntry *) m_entryArray.ElementAt( 0);
	
				IMPORT_LOG0( "Assigning start of mailbox list to entry at index 0\n");
			}
		}
		
		if (pBox == nsnull) {
			IMPORT_LOG0( "ERROR ASSIGNING STARTING MAILBOX\n");
		}
		
	}
	
	nsresult						rv;
	nsIFileSpec *					file;
	MailboxEntry *					pChild;
	nsIImportMailboxDescriptor *	pID;
	nsISupports *					pInterface;
	PRUint32						size;
	
	nsCOMPtr<nsIImportService> impSvc(do_GetService(kImportServiceCID, &rv));
	if (NS_FAILED( rv))
		return;
	
	while (pBox) {
		rv = impSvc->CreateNewMailboxDescriptor( &pID);
		if (NS_SUCCEEDED( rv)) {
			pID->SetDepth( depth);
			pID->SetIdentifier( pBox->index);
			pID->SetDisplayName( (PRUnichar *)pBox->mailName.get());
			if (pBox->fileName.Length() > 0) {
				pID->GetFileSpec( &file);
				file->FromFileSpec( root);
				file->AppendRelativeUnixPath( pBox->fileName.get());
				size = 0;
				file->GetFileSize( &size);
				pID->SetSize( size);
				file->Release();
			}
			rv = pID->QueryInterface( kISupportsIID, (void **) &pInterface);
			pArray->AppendElement( pInterface);
			pInterface->Release();
			pID->Release();
		}
		
		if (pBox->child) {
			pChild = GetIndexEntry( pBox->child);
			if (pChild != nsnull)
				BuildMailboxList( pChild, root, depth + 1, pArray);
		}
		if (pBox->sibling != -1) {
			pBox = GetIndexEntry( pBox->sibling);
		}
		else
			pBox = nsnull;
	}
	
}



nsOEScanBoxes::MailboxEntry * nsOEScanBoxes::GetIndexEntry( PRUint32 index)
{
	PRInt32 max = m_entryArray.Count();
	for (PRInt32 i = 0; i < max; i++) {
		MailboxEntry *pEntry = (MailboxEntry *) m_entryArray.ElementAt( i);
		if (pEntry->index == index)
			return( pEntry);
	}

	return( nsnull);
}


// -------------------------------------------------------
// File utility routines
// -------------------------------------------------------

PRBool nsOEScanBoxes::ReadLong( nsIFileSpec * stream, PRInt32& val, PRUint32 offset)
{
	nsresult	rv;
	rv = stream->Seek( offset);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	PRInt32	cntRead;
	char * pReadTo = (char *)&val;
	rv = stream->Read( &pReadTo, sizeof( val), &cntRead);
	
	if (NS_FAILED( rv) || (cntRead != sizeof( val)))
		return( PR_FALSE);
	return( PR_TRUE);
}

PRBool nsOEScanBoxes::ReadLong( nsIFileSpec * stream, PRUint32& val, PRUint32 offset)
{
	nsresult	rv;
	rv = stream->Seek( offset);
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	PRInt32	cntRead;
	char * pReadTo = (char *)&val;
	rv = stream->Read( &pReadTo, sizeof( val), &cntRead);
	
	if (NS_FAILED( rv) || (cntRead != sizeof( val)))
		return( PR_FALSE);
	return( PR_TRUE);
}

// It appears as though the strings for file name and mailbox
// name are at least 254 chars - verified - they are probably 255
// but why bother going that far!  If a file name is that long then 
// the heck with it.
#define	kOutlookExpressStringLength	252
PRBool nsOEScanBoxes::ReadString( nsIFileSpec * stream, nsString& str, PRUint32 offset)
{
	
	nsresult	rv;
	rv = stream->Seek( offset);
	if (NS_FAILED( rv))
		return( PR_FALSE);
		

	PRInt32	cntRead;
	char	buffer[kOutlookExpressStringLength];
	char *	pReadTo = buffer;
	rv = stream->Read( &pReadTo, kOutlookExpressStringLength, &cntRead);
	
	if (NS_FAILED( rv) || (cntRead != kOutlookExpressStringLength))
		return( PR_FALSE);
	buffer[kOutlookExpressStringLength - 1] = 0;
	str.AssignWithConversion(buffer);
	return( PR_TRUE);
}

PRBool nsOEScanBoxes::ReadString( nsIFileSpec * stream, nsCString& str, PRUint32 offset)
{
	
	nsresult	rv;
	rv = stream->Seek( offset);
	if (NS_FAILED( rv))
		return( PR_FALSE);
		

	PRInt32	cntRead;
	char	buffer[kOutlookExpressStringLength];
	char *	pReadTo = buffer;
	rv = stream->Read( &pReadTo, kOutlookExpressStringLength, &cntRead);
	
	if (NS_FAILED( rv) || (cntRead != kOutlookExpressStringLength))
		return( PR_FALSE);
	buffer[kOutlookExpressStringLength - 1] = 0;
	str = buffer;
	return( PR_TRUE);
}

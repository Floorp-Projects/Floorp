/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 
#include "MapiDbgLog.h"
#include "MapiApi.h"

#include "nsCRT.h"
#include "prprf.h"


int			CMapiApi::m_clients = 0;
BOOL		CMapiApi::m_initialized = PR_FALSE;
nsVoidArray	*CMapiApi::m_pStores = NULL;
LPMAPISESSION CMapiApi::m_lpSession = NULL;
LPMDB		CMapiApi::m_lpMdb = NULL;
HRESULT		CMapiApi::m_lastError;
PRUnichar *	CMapiApi::m_pUniBuff = NULL;
int			CMapiApi::m_uniBuffLen = 0;
/*
Type: 1, name: Calendar, class: IPF.Appointment
Type: 1, name: Contacts, class: IPF.Contact
Type: 1, name: Journal, class: IPF.Journal
Type: 1, name: Notes, class: IPF.StickyNote
Type: 1, name: Tasks, class: IPF.Task
Type: 1, name: Drafts, class: IPF.Note
*/

HINSTANCE	CMapiApi::m_hMapi32 = NULL;

LPMAPIUNINITIALIZE		gpMapiUninitialize = NULL;
LPMAPIINITIALIZE		gpMapiInitialize = NULL;
LPMAPIALLOCATEBUFFER	gpMapiAllocateBuffer = NULL;
LPMAPIFREEBUFFER		gpMapiFreeBuffer = NULL;
LPMAPILOGONEX			gpMapiLogonEx = NULL;
LPOPENSTREAMONFILE		gpMapiOpenStreamOnFile = NULL;

BOOL CMapiApi::LoadMapiEntryPoints( void)
{
	if (!(gpMapiUninitialize = (LPMAPIUNINITIALIZE) GetProcAddress( m_hMapi32, "MAPIUninitialize")))
		return( FALSE);
	if (!(gpMapiInitialize = (LPMAPIINITIALIZE) GetProcAddress( m_hMapi32, "MAPIInitialize")))
		return( FALSE);
	if (!(gpMapiAllocateBuffer = (LPMAPIALLOCATEBUFFER) GetProcAddress( m_hMapi32, "MAPIAllocateBuffer")))
		return( FALSE);
	if (!(gpMapiFreeBuffer = (LPMAPIFREEBUFFER) GetProcAddress( m_hMapi32, "MAPIFreeBuffer")))
		return( FALSE);
	if (!(gpMapiLogonEx = (LPMAPILOGONEX) GetProcAddress( m_hMapi32, "MAPILogonEx")))
		return( FALSE);
	if (!(gpMapiOpenStreamOnFile = (LPOPENSTREAMONFILE) GetProcAddress( m_hMapi32, "OpenStreamOnFile")))
		return( FALSE);

	return( TRUE);
}

void CMapiApi::MAPIUninitialize( void)
{
	if (m_hMapi32 && gpMapiUninitialize)
		(*gpMapiUninitialize)();
}

HRESULT CMapiApi::MAPIInitialize( LPVOID lpInit)
{
	if (m_hMapi32 && gpMapiInitialize)
		return( (*gpMapiInitialize)( lpInit));
	return( MAPI_E_NOT_INITIALIZED);
}

SCODE CMapiApi::MAPIAllocateBuffer( ULONG cbSize, LPVOID FAR * lppBuffer)
{
	if (m_hMapi32 && gpMapiAllocateBuffer)
		return( (*gpMapiAllocateBuffer)( cbSize, lppBuffer));
	return( MAPI_E_NOT_INITIALIZED);
}
 
ULONG CMapiApi::MAPIFreeBuffer( LPVOID lpBuff)
{
	if (m_hMapi32 && gpMapiFreeBuffer)
		return( (*gpMapiFreeBuffer)( lpBuff));
	return( MAPI_E_NOT_INITIALIZED);
}

HRESULT CMapiApi::MAPILogonEx( ULONG ulUIParam, LPTSTR lpszProfileName, LPTSTR lpszPassword, FLAGS flFlags, LPMAPISESSION FAR * lppSession)
{
	if (m_hMapi32 && gpMapiLogonEx)
		return( (*gpMapiLogonEx)( ulUIParam, lpszProfileName, lpszPassword, flFlags, lppSession));
	return( MAPI_E_NOT_INITIALIZED);
}

HRESULT CMapiApi::OpenStreamOnFile( LPALLOCATEBUFFER lpAllocateBuffer, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, LPTSTR lpszFileName, LPTSTR lpszPrefix, LPSTREAM FAR * lppStream)
{
	if (m_hMapi32 && gpMapiOpenStreamOnFile)
		return( (*gpMapiOpenStreamOnFile)( lpAllocateBuffer, lpFreeBuffer, ulFlags, lpszFileName, lpszPrefix, lppStream));
	return( MAPI_E_NOT_INITIALIZED);
}

void CMapiApi::FreeProws( LPSRowSet prows)
{
	ULONG		irow;
	if (!prows)
		return;
	for (irow = 0; irow < prows->cRows; ++irow)
		MAPIFreeBuffer( prows->aRow[irow].lpProps);
	MAPIFreeBuffer( prows);
}




BOOL CMapiApi::LoadMapi( void)
{
	if (m_hMapi32)
		return( TRUE);
	
	HINSTANCE	hInst = ::LoadLibrary( "MAPI32.DLL");
	if (!hInst)
		return( FALSE);
	FARPROC pProc = GetProcAddress( hInst, "MAPIGetNetscapeVersion");
	if (pProc) {
		::FreeLibrary( hInst);
		hInst = ::LoadLibrary( "MAPI32BAK.DLL");
		if (!hInst)
			return( FALSE);
	}

	m_hMapi32 = hInst;
	return( LoadMapiEntryPoints());
}

void CMapiApi::UnloadMapi( void)
{
	if (m_hMapi32)
		::FreeLibrary( m_hMapi32);
	m_hMapi32 = NULL;
}

CMapiApi::CMapiApi()
{
	m_clients++;
	LoadMapi();
	if (!m_pStores)
		m_pStores = new nsVoidArray();
}

CMapiApi::~CMapiApi()
{
	m_clients--;
	if (!m_clients) {
		HRESULT	hr;
		
		ClearMessageStores();
		delete m_pStores;
		m_pStores = NULL;

		m_lpMdb = NULL;

		if (m_lpSession) {
			hr = m_lpSession->Logoff( NULL, 0, 0);
			if (FAILED(hr)) {
				MAPI_TRACE2( "Logoff failed: 0x%lx, %d\n", (long)hr, (int)hr);
			}
			m_lpSession->Release();
			m_lpSession = NULL;
		}

		if (m_initialized) {
			MAPIUninitialize();
			m_initialized = FALSE;
		}

		UnloadMapi();
		
		if (m_pUniBuff)
			delete [] m_pUniBuff;
		m_pUniBuff = NULL;
		m_uniBuffLen = 0;
	}
}

void CMapiApi::CStrToUnicode( const char *pStr, nsString& result)
{
	result.Truncate( 0);
	int wLen = MultiByteToWideChar( CP_ACP, 0, pStr, -1, m_pUniBuff, 0);
	if (wLen >= m_uniBuffLen) {
		if (m_pUniBuff)
			delete [] m_pUniBuff;
		m_pUniBuff = new PRUnichar[wLen + 64];
		m_uniBuffLen = wLen + 64;
	}
	if (wLen) {
		MultiByteToWideChar( CP_ACP, 0, pStr, -1, m_pUniBuff, m_uniBuffLen);
		result = m_pUniBuff;
	}
}

BOOL CMapiApi::Initialize( void)
{
	if (m_initialized)
		return( TRUE);

	HRESULT		hr;

	hr = MAPIInitialize( NULL);

	if (FAILED(hr)) {
		MAPI_TRACE2( "MAPI Initialize failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}
	
	m_initialized = TRUE;
	MAPI_TRACE0( "MAPI Initialized\n");

	return( TRUE);
}

BOOL CMapiApi::LogOn( void)
{
	if (!m_initialized) {
		MAPI_TRACE0( "Tried to LogOn before initializing MAPI\n");
		return( FALSE);
	}
	
	if (m_lpSession)
		return( TRUE);

	HRESULT hr;

	hr = MAPILogonEx(	0, // might need to be passed in HWND
						NULL, // profile name, 64 char max (LPTSTR)
						NULL, // profile password, 64 char max (LPTSTR)
						// MAPI_NEW_SESSION | MAPI_NO_MAIL | MAPI_LOGON_UI | MAPI_EXPLICIT_PROFILE,
						// MAPI_NEW_SESSION | MAPI_NO_MAIL | MAPI_LOGON_UI,
						// MAPI_NO_MAIL | MAPI_LOGON_UI,
						MAPI_NO_MAIL | MAPI_USE_DEFAULT | MAPI_EXTENDED | MAPI_NEW_SESSION,
						&m_lpSession);

	if (FAILED(hr)) {
		m_lpSession = NULL;
		MAPI_TRACE2( "LogOn failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	MAPI_TRACE0( "MAPI Logged on\n");


	return( TRUE);
}

class CGetStoreFoldersIter : public CMapiHierarchyIter {
public:
	CGetStoreFoldersIter( CMapiApi *pApi, CMapiFolderList& folders, int depth, BOOL isMail = TRUE);
	
	virtual BOOL HandleHierarchyItem( ULONG oType, ULONG cb, LPENTRYID pEntry);

protected:
	BOOL	ExcludeFolderClass( const PRUnichar *pName);

	BOOL				m_isMail;
	CMapiApi *			m_pApi;
	CMapiFolderList *	m_pList;
	int					m_depth;
};

CGetStoreFoldersIter::CGetStoreFoldersIter( CMapiApi *pApi, CMapiFolderList& folders, int depth, BOOL isMail)
{
	m_pApi = pApi;
	m_pList = &folders;
	m_depth = depth;
	m_isMail = isMail;
}

BOOL CGetStoreFoldersIter::ExcludeFolderClass( const PRUnichar *pName)
{
	BOOL bResult;
	if (m_isMail) {
		bResult = FALSE;
		if (!nsCRT::strcasecmp( pName, L"IPF.Appointment"))
			bResult = TRUE;
		else if (!nsCRT::strcasecmp( pName, L"IPF.Contact"))
			bResult = TRUE;
		else if (!nsCRT::strcasecmp( pName, L"IPF.Journal"))
			bResult = TRUE;
		else if (!nsCRT::strcasecmp( pName, L"IPF.StickyNote"))
			bResult = TRUE;
		else if (!nsCRT::strcasecmp( pName, L"IPF.Task"))
			bResult = TRUE;
		// else if (!stricmp( pName, "IPF.Note"))
		//	bResult = TRUE;
	}
	else {
		bResult = TRUE;
		if (!nsCRT::strcasecmp( pName, L"IPF.Contact"))
			bResult = FALSE;
	}

	return( bResult);
}

BOOL CGetStoreFoldersIter::HandleHierarchyItem( ULONG oType, ULONG cb, LPENTRYID pEntry)
{
	if (oType == MAPI_FOLDER) {
		LPMAPIFOLDER pFolder;
		if (m_pApi->OpenEntry( cb, pEntry, (LPUNKNOWN *) &pFolder)) {
			LPSPropValue		pVal;
			nsString			name;

			pVal = m_pApi->GetMapiProperty( pFolder, PR_CONTAINER_CLASS);
			if (pVal)
				m_pApi->GetStringFromProp( pVal, name);
			else
				name.Truncate();

			if ((name.IsEmpty() && m_isMail) || (!ExcludeFolderClass(name.get()))) {
				pVal = m_pApi->GetMapiProperty( pFolder, PR_DISPLAY_NAME);
				m_pApi->GetStringFromProp( pVal, name);
				CMapiFolder	*pNewFolder = new CMapiFolder(name.get(), cb, pEntry, m_depth);
				m_pList->AddItem( pNewFolder);
				
				pVal = m_pApi->GetMapiProperty( pFolder, PR_FOLDER_TYPE);
				MAPI_TRACE2( "Type: %d, name: %s\n", m_pApi->GetLongFromProp( pVal), (const char *)name);
				// m_pApi->ListProperties( pFolder);
			
				CGetStoreFoldersIter	nextIter( m_pApi, *m_pList, m_depth + 1, m_isMail);
				m_pApi->IterateHierarchy( &nextIter, pFolder);
			}
			pFolder->Release();
		}
		else {
			MAPI_TRACE0( "GetStoreFolders - HandleHierarchyItem: Error opening folder entry.\n");
			return( FALSE);
		}
	}
	else {
		MAPI_TRACE1( "GetStoreFolders - HandleHierarchyItem: Unhandled ObjectType: %ld\n", oType);
	}

	return( TRUE);
}


BOOL CMapiApi::GetStoreFolders( ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders, int startDepth)
{
	// Fill in the array with the folders in the given store
	if (!m_initialized || !m_lpSession) {
		MAPI_TRACE0( "MAPI not initialized for GetStoreFolders\n");
		return( FALSE);
	}
	
	m_lpMdb = NULL;

	CMsgStore *		pStore = FindMessageStore( cbEid, lpEid);
	BOOL			bResult = FALSE;
	LPSPropValue	pVal;

	if (pStore && pStore->Open( m_lpSession, &m_lpMdb)) {
		// Successful open, do the iteration of the store
		pVal = GetMapiProperty( m_lpMdb, PR_IPM_SUBTREE_ENTRYID);
		if (pVal) {
			ULONG			cbEntry;
			LPENTRYID		pEntry;
			LPMAPIFOLDER	lpSubTree = NULL;

			if (GetEntryIdFromProp( pVal, cbEntry, pEntry)) {
				// Open up the folder!
				bResult = OpenEntry( cbEntry, pEntry, (LPUNKNOWN *)&lpSubTree);
				MAPIFreeBuffer( pEntry);
				if (bResult && lpSubTree) {
					// Iterate the subtree with the results going into the folder list
					CGetStoreFoldersIter iterHandler( this, folders, startDepth);
					bResult = IterateHierarchy( &iterHandler, lpSubTree);
					lpSubTree->Release();
				}
				else {
					MAPI_TRACE0( "GetStoreFolders: Error opening sub tree.\n");
				}
			}
			else {
				MAPI_TRACE0( "GetStoreFolders: Error getting entryID from sub tree property val.\n");
			}
		}
		else {
			MAPI_TRACE0( "GetStoreFolders: Error getting sub tree property.\n");
		}
	}
	else {
		MAPI_TRACE0( "GetStoreFolders: Error opening message store.\n");
	}

	return( bResult);
}

BOOL CMapiApi::GetStoreAddressFolders( ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders)
{
	// Fill in the array with the folders in the given store
	if (!m_initialized || !m_lpSession) {
		MAPI_TRACE0( "MAPI not initialized for GetStoreAddressFolders\n");
		return( FALSE);
	}
	
	m_lpMdb = NULL;

	CMsgStore *		pStore = FindMessageStore( cbEid, lpEid);
	BOOL			bResult = FALSE;
	LPSPropValue	pVal;

	if (pStore && pStore->Open( m_lpSession, &m_lpMdb)) {
		// Successful open, do the iteration of the store
		pVal = GetMapiProperty( m_lpMdb, PR_IPM_SUBTREE_ENTRYID);
		if (pVal) {
			ULONG			cbEntry;
			LPENTRYID		pEntry;
			LPMAPIFOLDER	lpSubTree = NULL;

			if (GetEntryIdFromProp( pVal, cbEntry, pEntry)) {
				// Open up the folder!
				bResult = OpenEntry( cbEntry, pEntry, (LPUNKNOWN *)&lpSubTree);
				MAPIFreeBuffer( pEntry);
				if (bResult && lpSubTree) {
					// Iterate the subtree with the results going into the folder list
					CGetStoreFoldersIter iterHandler( this, folders, 1, FALSE);
					bResult = IterateHierarchy( &iterHandler, lpSubTree);
					lpSubTree->Release();
				}
				else {
					MAPI_TRACE0( "GetStoreAddressFolders: Error opening sub tree.\n");
				}
			}
			else {
				MAPI_TRACE0( "GetStoreAddressFolders: Error getting entryID from sub tree property val.\n");
			}
		}
		else {
			MAPI_TRACE0( "GetStoreAddressFolders: Error getting sub tree property.\n");
		}
	}
	else {
		MAPI_TRACE0( "GetStoreAddressFolders: Error opening message store.\n");
	}

	return( bResult);
}


BOOL CMapiApi::OpenStore( ULONG cbEid, LPENTRYID lpEid, LPMDB *ppMdb)
{
	if (!m_lpSession) {
		MAPI_TRACE0( "OpenStore called before a session was opened\n");
		return( FALSE);
	}

	CMsgStore *		pStore = FindMessageStore( cbEid, lpEid);
	if (pStore && pStore->Open( m_lpSession, ppMdb))
		return( TRUE);
	return( FALSE);
}


BOOL CMapiApi::OpenEntry( ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen)
{
	if (!m_lpMdb) {
		MAPI_TRACE0( "OpenEntry called before the message store is open\n");
		return( FALSE);
	}
	
	return( OpenMdbEntry( m_lpMdb, cbEntry, pEntryId, ppOpen));
}

BOOL CMapiApi::OpenMdbEntry( LPMDB lpMdb, ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen)
{	
	ULONG		ulObjType;
	HRESULT		hr;
	hr =	lpMdb->OpenEntry(	cbEntry,
								pEntryId,
								NULL,
								0,
								&ulObjType,
								(LPUNKNOWN *) ppOpen);

	if (FAILED(hr)) {
		MAPI_TRACE2( "OpenMdbEntry failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}
	return( TRUE);
}

enum {
    ieidPR_ENTRYID = 0,
	ieidPR_OBJECT_TYPE,
    ieidMax
};

static const SizedSPropTagArray(ieidMax, ptaEid)=
{
    ieidMax,
    {
        PR_ENTRYID,
		PR_OBJECT_TYPE,
    }
};

BOOL CMapiApi::IterateContents( CMapiContentIter *pIter, LPMAPIFOLDER pFolder, ULONG flags)
{
	// flags can be 0 or MAPI_ASSOCIATED
	// MAPI_ASSOCIATED is usually used for forms and views

	HRESULT		hr;
	LPMAPITABLE	lpTable;
	hr = pFolder->GetContentsTable( flags, &lpTable);
	if (FAILED(hr)) {
		MAPI_TRACE2( "GetContentsTable failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	ULONG rowCount;
	hr = lpTable->GetRowCount( 0, &rowCount);
	if (!rowCount) {
		MAPI_TRACE0( "  Empty Table\n");
	}

	hr = lpTable->SetColumns( (LPSPropTagArray)&ptaEid, 0);
	if (FAILED(hr)) {
		lpTable->Release();
		MAPI_TRACE2( "SetColumns failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	hr = lpTable->SeekRow( BOOKMARK_BEGINNING, 0, NULL);
	if (FAILED(hr)) {
		lpTable->Release();
		MAPI_TRACE2( "SeekRow failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	int			cNumRows = 0;
	LPSRowSet	lpRow;
	BOOL		keepGoing = TRUE;
	BOOL		bResult = TRUE;
	do {
		
		lpRow = NULL;
		hr = lpTable->QueryRows( 1, 0, &lpRow);

        if(HR_FAILED(hr)) {
			MAPI_TRACE2( "QueryRows failed: 0x%lx, %d\n", (long)hr, (int)hr);
            bResult = FALSE;
			break;
		}

        if(lpRow) {
            cNumRows = lpRow->cRows;

		    if (cNumRows) {
                LPENTRYID	lpEID = (LPENTRYID) lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
                ULONG		cbEID = lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;
				ULONG		oType = lpRow->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.ul;

                keepGoing = HandleContentsItem( oType, cbEID, lpEID);
				MAPI_TRACE1( "    ObjectType: %ld\n", oType);
		    }
			FreeProws( lpRow);		
        }

	} while ( SUCCEEDED(hr) && cNumRows && lpRow && keepGoing);

	lpTable->Release();

	return( bResult);
}

BOOL CMapiApi::HandleContentsItem( ULONG oType, ULONG cb, LPENTRYID pEntry)
{
	if (oType == MAPI_MESSAGE) {
		LPMESSAGE pMsg;
		if (OpenEntry( cb, pEntry, (LPUNKNOWN *) &pMsg)) {
			LPSPropValue pVal;
			pVal = GetMapiProperty( pMsg, PR_SUBJECT);
			ReportStringProp( "PR_SUBJECT:", pVal);
			pVal = GetMapiProperty( pMsg, PR_DISPLAY_BCC);
			ReportStringProp( "PR_DISPLAY_BCC:", pVal);
			pVal = GetMapiProperty( pMsg, PR_DISPLAY_CC);
			ReportStringProp( "PR_DISPLAY_CC:", pVal);
			pVal = GetMapiProperty( pMsg, PR_DISPLAY_TO);
			ReportStringProp( "PR_DISPLAY_TO:", pVal);
			pVal = GetMapiProperty( pMsg, PR_MESSAGE_CLASS);
			ReportStringProp( "PR_MESSAGE_CLASS:", pVal);
			ListProperties( pMsg);
			pMsg->Release();
		}
		else {
			MAPI_TRACE0( "    Folder type - error opening\n");
		}
	}
	else
		MAPI_TRACE1( "    ObjectType: %ld\n", oType);

	return( TRUE);
}

void CMapiApi::ListProperties( LPMAPIPROP lpProp, BOOL getValues)
{
	LPSPropTagArray		pArray;
	HRESULT				hr = lpProp->GetPropList( 0, &pArray);
	if (FAILED(hr)) {
		MAPI_TRACE0( "    Unable to retrieve property list\n");
		return;
	}
  ULONG count = 0;
  LPMAPINAMEID FAR * lppPropNames;
  SPropTagArray		tagArray;
  LPSPropTagArray lpTagArray = &tagArray;
  tagArray.cValues = (ULONG)1;
	nsCString	desc;
	for (ULONG i = 0; i < pArray->cValues; i++) {
		GetPropTagName( pArray->aulPropTag[i], desc);
		if (getValues) {
      tagArray.aulPropTag[0] = pArray->aulPropTag[i];
      hr = lpProp->GetNamesFromIDs(&lpTagArray, nsnull, 0, &count, &lppPropNames); 
      if (hr == S_OK)
        MAPIFreeBuffer(lppPropNames);

			LPSPropValue pVal = GetMapiProperty( lpProp, pArray->aulPropTag[i]);
			if (pVal) {
				desc += ", ";
				ListPropertyValue( pVal, desc);
				MAPIFreeBuffer( pVal);
			}
		}
		MAPI_TRACE2( "    Tag #%d: %s\n", (int)i, (const char *)desc);
	}

  MAPIFreeBuffer( pArray);
}

ULONG CMapiApi::GetEmailPropertyTag(LPMAPIPROP lpProp, LONG nameID)
{
static GUID emailGUID = {
   0x00062004, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46
};

  MAPINAMEID mapiNameID;
  mapiNameID.lpguid = &emailGUID;
  mapiNameID.ulKind = MNID_ID;
  mapiNameID.Kind.lID = nameID;

  LPMAPINAMEID lpMapiNames = &mapiNameID;
  LPSPropTagArray lpMailTagArray = nsnull;

  HRESULT result = lpProp->GetIDsFromNames(1L, &lpMapiNames, 0, &lpMailTagArray);
  if (result == S_OK)
  {
    ULONG lTag = lpMailTagArray->aulPropTag[0];
    MAPIFreeBuffer(lpMailTagArray);
    return lTag;
  }
  else
    return 0L;
}

BOOL CMapiApi::HandleHierarchyItem( ULONG oType, ULONG cb, LPENTRYID pEntry)
{
	if (oType == MAPI_FOLDER) {
		LPMAPIFOLDER pFolder;
		if (OpenEntry( cb, pEntry, (LPUNKNOWN *) &pFolder)) {
			LPSPropValue pVal;
			pVal = GetMapiProperty( pFolder, PR_DISPLAY_NAME);
			ReportStringProp( "Folder name:", pVal);
			IterateContents( NULL, pFolder);
			IterateHierarchy( NULL, pFolder);
			pFolder->Release();
		}
		else {
			MAPI_TRACE0( "    Folder type - error opening\n");
		}
	}
	else
		MAPI_TRACE1( "    ObjectType: %ld\n", oType);

	return( TRUE);
}

BOOL CMapiApi::IterateHierarchy( CMapiHierarchyIter *pIter, LPMAPIFOLDER pFolder, ULONG flags)
{
	// flags can be CONVENIENT_DEPTH or 0
	// CONVENIENT_DEPTH will return all depths I believe instead
	// of just children
	HRESULT		hr;
	LPMAPITABLE	lpTable;
	hr = pFolder->GetHierarchyTable( flags, &lpTable);
	if (HR_FAILED(hr)) {
		m_lastError = hr;
		MAPI_TRACE2( "IterateHierarchy: GetContentsTable failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	ULONG rowCount;
	hr = lpTable->GetRowCount( 0, &rowCount);
	if (!rowCount) {
		lpTable->Release();
		return( TRUE);
	}

	hr = lpTable->SetColumns( (LPSPropTagArray)&ptaEid, 0);
	if (HR_FAILED(hr)) {
		m_lastError = hr;
		lpTable->Release();
		MAPI_TRACE2( "IterateHierarchy: SetColumns failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	hr = lpTable->SeekRow( BOOKMARK_BEGINNING, 0, NULL);
	if (HR_FAILED(hr)) {
		m_lastError = hr;
		lpTable->Release();
		MAPI_TRACE2( "IterateHierarchy: SeekRow failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	int			cNumRows = 0;
	LPSRowSet	lpRow;
	BOOL		keepGoing = TRUE;
	BOOL		bResult = TRUE;
	do {
		
		lpRow = NULL;
		hr = lpTable->QueryRows( 1, 0, &lpRow);

        if(HR_FAILED(hr)) {
			MAPI_TRACE2( "QueryRows failed: 0x%lx, %d\n", (long)hr, (int)hr);
			m_lastError = hr;
            bResult = FALSE;
			break;
		}

        if(lpRow) {
            cNumRows = lpRow->cRows;

		    if (cNumRows) {
                LPENTRYID	lpEntry = (LPENTRYID) lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
                ULONG		cb = lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;
				ULONG		oType = lpRow->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.ul;
				
				if (pIter)
					keepGoing = pIter->HandleHierarchyItem( oType, cb, lpEntry);
				else
					keepGoing = HandleHierarchyItem( oType, cb, lpEntry);

		    }
			FreeProws( lpRow);		
        }

	} while ( SUCCEEDED(hr) && cNumRows && lpRow && keepGoing);

	lpTable->Release();
	
	if (bResult && !keepGoing)
		bResult = FALSE;

	return( bResult);
}


enum {
	itblPR_DISPLAY_NAME,
    itblPR_ENTRYID,
    itblMax
};

static const SizedSPropTagArray(itblMax, ptaTbl)=
{
    itblMax,
    {
		PR_DISPLAY_NAME,
        PR_ENTRYID,
    }
};

BOOL CMapiApi::IterateStores( CMapiFolderList& stores)
{
	stores.ClearAll();

	if (!m_lpSession) {
		MAPI_TRACE0( "IterateStores called before session is open\n");
		m_lastError = -1;
		return( FALSE);
	}


	HRESULT		hr;
	
	/* -- Some Microsoft sample code just to see if things are working --- *//*

	ULONG		cbEIDStore;
	LPENTRYID	lpEIDStore;

    hr = HrMAPIFindDefaultMsgStore( m_lpSession, &cbEIDStore, &lpEIDStore);
    if (HR_FAILED(hr)) {
        MAPI_TRACE0( "Default message store not found\n");
		// MessageBox(NULL,"Message Store Not Found",NULL,MB_OK);
    }
	else {
		LPMDB	lpStore;
		MAPI_TRACE0( "Default Message store FOUND\n");
		hr = m_lpSession->OpenMsgStore( NULL, cbEIDStore,
                                  lpEIDStore, NULL,
                                  MDB_NO_MAIL | MDB_NO_DIALOG, &lpStore);
		if (HR_FAILED(hr)) {
			MAPI_TRACE1( "Unable to open default message store: 0x%lx\n", hr);
		}
		else {
			MAPI_TRACE0( "Default message store OPENED\n");
			lpStore->Release();
		}
	 }
     */



	LPMAPITABLE	lpTable;

	hr = m_lpSession->GetMsgStoresTable( 0, &lpTable);
	if (FAILED(hr)) {
		MAPI_TRACE0( "GetMsgStoresTable failed\n");
		m_lastError = hr;
		return( FALSE);
	}


	ULONG rowCount;
	hr = lpTable->GetRowCount( 0, &rowCount);
	MAPI_TRACE1( "MsgStores Table rowCount: %ld\n", rowCount);

	hr = lpTable->SetColumns( (LPSPropTagArray)&ptaTbl, 0);
	if (FAILED(hr)) {
		lpTable->Release();
		MAPI_TRACE2( "SetColumns failed: 0x%lx, %d\n", (long)hr, (int)hr);
		m_lastError = hr;
		return( FALSE);
	}

	hr = lpTable->SeekRow( BOOKMARK_BEGINNING, 0, NULL);
	if (FAILED(hr)) {
		lpTable->Release();
		MAPI_TRACE2( "SeekRow failed: 0x%lx, %d\n", (long)hr, (int)hr);
		m_lastError = hr;
		return( FALSE);
	}

	int			cNumRows = 0;
	LPSRowSet	lpRow;
	BOOL		keepGoing = TRUE;
	BOOL		bResult = TRUE;
	do {
		
		lpRow = NULL;
		hr = lpTable->QueryRows( 1, 0, &lpRow);

        if(HR_FAILED(hr)) {
			MAPI_TRACE2( "QueryRows failed: 0x%lx, %d\n", (long)hr, (int)hr);
            bResult = FALSE;
			m_lastError = hr;
			break;
		}

        if(lpRow) {
            cNumRows = lpRow->cRows;

		    if (cNumRows) {
				LPCTSTR		lpStr = (LPCTSTR) lpRow->aRow[0].lpProps[itblPR_DISPLAY_NAME].Value.LPSZ;
                LPENTRYID	lpEID = (LPENTRYID) lpRow->aRow[0].lpProps[itblPR_ENTRYID].Value.bin.lpb;
                ULONG		cbEID = lpRow->aRow[0].lpProps[itblPR_ENTRYID].Value.bin.cb;

				// In the future, GetStoreInfo needs to somehow return
				// whether or not the store is from an IMAP server.
				// Currently, GetStoreInfo opens the store and attempts
				// to get the hierarchy tree.  If the tree is empty or
				// does not exist, then szContents will be zero.  We'll
				// assume that any store that doesn't have anything in
				// it's hierarchy tree is not a store we want to import - 
				// there would be nothing to import from anyway!
				// Currently, this does exclude IMAP server accounts
				// which is the desired behaviour.

                int         strLen = nsCRT::strlen(lpStr);
                PRUnichar * pwszStr = (PRUnichar *) nsMemory::Alloc((strLen + 1) * sizeof(WCHAR));
                if (!pwszStr) {
                    // out of memory
                    FreeProws( lpRow);
                    lpTable->Release();
                    return FALSE;
                }
                ::MultiByteToWideChar(CP_ACP, 0, lpStr, nsCRT::strlen(lpStr) + 1, pwszStr, (strLen + 1) * sizeof(WCHAR));
				CMapiFolder *pFolder = new CMapiFolder( pwszStr, cbEID, lpEID, 0, MAPI_STORE);
                nsMemory::Free(pwszStr);

				long szContents = 1;
 				GetStoreInfo( pFolder, &szContents);

				MAPI_TRACE1( "    DisplayName: %s\n", lpStr);
				if (szContents) {
					stores.AddItem( pFolder);
				}
				else {
					delete pFolder;
					MAPI_TRACE0( "    ^^^^^ Not added to store list\n");
				}

				keepGoing = TRUE;
		    }
			FreeProws( lpRow);		
        }

	} while ( SUCCEEDED(hr) && cNumRows && lpRow && keepGoing);

	lpTable->Release();

	return( bResult);
}

void CMapiApi::GetStoreInfo( CMapiFolder *pFolder, long *pSzContents)
{
	HRESULT	hr;
	LPMDB	lpMdb;

	if (pSzContents)
		*pSzContents = 0;

	if (!OpenStore( pFolder->GetCBEntryID(), pFolder->GetEntryID(), &lpMdb))
		return;
	
	LPSPropValue pVal;
	/*
	pVal = GetMapiProperty( lpMdb, PR_DISPLAY_NAME);
	ReportStringProp( "    Message store name:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_MDB_PROVIDER);
	ReportUIDProp( "    Message store provider:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_COMMENT);
	ReportStringProp( "    Message comment:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_ACCESS_LEVEL);
	ReportLongProp( "    Message store Access Level:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_STORE_SUPPORT_MASK);
	ReportLongProp( "    Message store support mask:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_STORE_STATE);
	ReportLongProp( "    Message store state:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_OBJECT_TYPE);
	ReportLongProp( "    Message store object type:", pVal);
	pVal = GetMapiProperty( lpMdb, PR_VALID_FOLDER_MASK);
	ReportLongProp( "    Message store valid folder mask:", pVal);
	
	pVal = GetMapiProperty( lpMdb, 0x8001001e);
	ReportStringProp( "    Message prop 0x8001001e:", pVal);

	// This key appears to be the OMI Account Manager account that corresponds
	// to this message store.  This is important for IMAP accounts
	// since we may not want to import messages from an IMAP store!
	// Seems silly if you ask me!
	// In order to test this, we'll need the registry key to look under to determine
	// if it contains the "IMAP Server" value, if it does then we are an
	// IMAP store, if not, then we are a non-IMAP store - which may always mean
	// a regular store that should be imported.

	pVal = GetMapiProperty( lpMdb, 0x80000003);
	ReportLongProp( "    Message prop 0x80000003:", pVal);

	// ListProperties( lpMdb);
	*/

	pVal = GetMapiProperty( lpMdb, PR_IPM_SUBTREE_ENTRYID);
	if (pVal) {
		ULONG			cbEntry;
		LPENTRYID		pEntry;
		LPMAPIFOLDER	lpSubTree = NULL;

		if (GetEntryIdFromProp( pVal, cbEntry, pEntry)) {
			// Open up the folder!
			ULONG		ulObjType;
			hr = lpMdb->OpenEntry( cbEntry, pEntry, NULL, 0, &ulObjType, (LPUNKNOWN *) &lpSubTree);
			MAPIFreeBuffer( pEntry);
			if (SUCCEEDED( hr) && lpSubTree) {
				// Find out if there are any contents in the 
				// tree.
				LPMAPITABLE	lpTable;
				hr = lpSubTree->GetHierarchyTable( 0, &lpTable);
				if (HR_FAILED(hr)) {
					MAPI_TRACE2( "GetStoreInfo: GetHierarchyTable failed: 0x%lx, %d\n", (long)hr, (int)hr);
				}
				else {
					ULONG rowCount;
					hr = lpTable->GetRowCount( 0, &rowCount);
					lpTable->Release();
					if (SUCCEEDED( hr) && pSzContents)
						*pSzContents = (long) rowCount;
				}

				lpSubTree->Release();
			}
		}
	}
}


void CMapiApi::ClearMessageStores( void)
{
	if (m_pStores) {
		CMsgStore *	pStore;
		for (int i = 0; i < m_pStores->Count(); i++) {
			pStore = (CMsgStore *) m_pStores->ElementAt( i);
			delete pStore;
		}
		m_pStores->Clear();
	}
}

void CMapiApi::AddMessageStore( CMsgStore *pStore)
{
	if (m_pStores)
		m_pStores->AppendElement( pStore);
}

CMsgStore *	CMapiApi::FindMessageStore( ULONG cbEid, LPENTRYID lpEid)
{
	if (!m_lpSession) {
		MAPI_TRACE0( "FindMessageStore called before session is open\n");
		m_lastError = -1;
		return( NULL);
	}

	ULONG		result;
	HRESULT		hr;
	CMsgStore *	pStore;
	for (int i = 0; i < m_pStores->Count(); i++) {
		pStore = (CMsgStore *) m_pStores->ElementAt( i);
		hr = m_lpSession->CompareEntryIDs( cbEid, lpEid, pStore->GetCBEntryID(), pStore->GetLPEntryID(),
											0, &result);
		if (HR_FAILED( hr)) {
			MAPI_TRACE2( "CompareEntryIDs failed: 0x%lx, %d\n", (long)hr, (int)hr);
			m_lastError = hr;
			return( NULL);
		}
		if ( result) {
			return( pStore);
		}
	}
	
	pStore = new CMsgStore( cbEid, lpEid);
	AddMessageStore( pStore);
	return( pStore);
}

// --------------------------------------------------------------------
// Utility stuff
// --------------------------------------------------------------------

LPSPropValue CMapiApi::GetMapiProperty( LPMAPIPROP pProp, ULONG tag)
{
	if (!pProp)
		return( NULL);

	int	sz = CbNewSPropTagArray( 1);
	SPropTagArray *pTag = (SPropTagArray *) new char[sz];
	pTag->cValues = 1;
	pTag->aulPropTag[0] = tag;
	LPSPropValue	lpProp = NULL;
	ULONG	cValues = 0;
	HRESULT hr = pProp->GetProps( pTag, 0, &cValues, &lpProp);
	delete pTag;
	if (HR_FAILED( hr) || (cValues != 1)) {
		if (lpProp)
			MAPIFreeBuffer( lpProp);
		return( NULL);
	}
	else {
		if (PROP_TYPE( lpProp->ulPropTag) == PT_ERROR) {
			if (lpProp->Value.l == MAPI_E_NOT_FOUND) {
				MAPIFreeBuffer( lpProp);
				lpProp = NULL;
			}
		}
	}

	return( lpProp);
}

BOOL CMapiApi::IsLargeProperty( LPSPropValue pVal)
{
	if ((PROP_TYPE( pVal->ulPropTag) == PT_ERROR) && (pVal->Value.l == E_OUTOFMEMORY)) {
		return( TRUE);
	}
	return( FALSE);
}

BOOL CMapiApi::GetLargeStringProperty( LPMAPIPROP pProp, ULONG tag, nsCString& val)
{
	LPSTREAM	lpStream;
	HRESULT		hr = pProp->OpenProperty( tag, &IID_IStream, 0, 0, (LPUNKNOWN *)&lpStream);
	if (HR_FAILED( hr))
		return( FALSE);
	STATSTG		st;
	BOOL bResult = TRUE;
	hr = lpStream->Stat( &st, STATFLAG_NONAME);
	if (HR_FAILED( hr))
		bResult = FALSE;
	else {
		if (!st.cbSize.QuadPart)
			st.cbSize.QuadPart = 1;
		char *pVal = new char[ (int) st.cbSize.QuadPart + 1];
		// val.SetCapacity( (int) st.cbSize.QuadPart);
		if (pVal) {
			ULONG	sz;
			hr = lpStream->Read( pVal, (ULONG) st.cbSize.QuadPart, &sz);
			if (HR_FAILED( hr)) {
				bResult = FALSE;
				*pVal = 0;
				sz = 0;
			}
			else
				pVal[(int) st.cbSize.QuadPart] = 0;
			val = pVal;
			delete [] pVal;
		}
		else
			bResult = FALSE;
	}
	
	lpStream->Release();

	return( bResult);
}

BOOL CMapiApi::GetLargeStringProperty( LPMAPIPROP pProp, ULONG tag, nsString& val)
{
	nsCString	result;
	if (GetLargeStringProperty( pProp, tag, result)) {
		CStrToUnicode( result, val);
		return( TRUE);
	}

	return( FALSE);
}
// If the value is a string, get it...
BOOL CMapiApi::GetEntryIdFromProp( LPSPropValue pVal, ULONG& cbEntryId, LPENTRYID& lpEntryId, BOOL delVal)
{
	if (!pVal)
		return( FALSE);
	
	BOOL bResult = TRUE;
    switch( PROP_TYPE( pVal->ulPropTag)) {
		case PT_BINARY:
			cbEntryId = pVal->Value.bin.cb;
			MAPIAllocateBuffer( cbEntryId, (LPVOID *) &lpEntryId);
			nsCRT::memcpy( lpEntryId, pVal->Value.bin.lpb, cbEntryId);
		break;

		default:
			MAPI_TRACE0( "EntryId not in BINARY prop value\n");
			bResult = FALSE;
        break;
    }

	if (pVal && delVal)
		MAPIFreeBuffer( pVal);

	return( bResult);
}

BOOL CMapiApi::GetStringFromProp( LPSPropValue pVal, nsCString& val, BOOL delVal)
{
	BOOL bResult = TRUE;
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_STRING8)) {
		val = pVal->Value.lpszA;
	}
	else if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_UNICODE)) {
		val.AssignWithConversion((PRUnichar *) pVal->Value.lpszW);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		val.Truncate();
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		val.Truncate();
		bResult = FALSE;
	}
	else {
		if (pVal) {
			MAPI_TRACE1( "GetStringFromProp: invalid value, expecting string - %d\n", (int) PROP_TYPE( pVal->ulPropTag));
		}
		else {
			MAPI_TRACE0( "GetStringFromProp: invalid value, expecting string, got null pointer\n");
		}
		val.Truncate();
		bResult = FALSE;
	}
	if (pVal && delVal)
		MAPIFreeBuffer( pVal);

	return( bResult);
}

BOOL CMapiApi::GetStringFromProp( LPSPropValue pVal, nsString& val, BOOL delVal)
{
	BOOL bResult = TRUE;
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_STRING8)) {
		CStrToUnicode( (const char *)pVal->Value.lpszA, val);
	}
	else if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_UNICODE)) {
		val = (PRUnichar *) pVal->Value.lpszW;
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		val.Truncate();
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		val.Truncate();
		bResult = FALSE;
	}
	else {
		if (pVal) {
			MAPI_TRACE1( "GetStringFromProp: invalid value, expecting string - %d\n", (int) PROP_TYPE( pVal->ulPropTag));
		}
		else {
			MAPI_TRACE0( "GetStringFromProp: invalid value, expecting string, got null pointer\n");
		}
		val.Truncate();
		bResult = FALSE;
	}
	if (pVal && delVal)
		MAPIFreeBuffer( pVal);

	return( bResult);
}


LONG CMapiApi::GetLongFromProp( LPSPropValue pVal, BOOL delVal)
{
	LONG val = 0;
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_LONG)) {
		val = pVal->Value.l;
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		val = 0;
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		val = 0;
		MAPI_TRACE0( "GetLongFromProp: Error retrieving property\n");
	}
	else {
		MAPI_TRACE0( "GetLongFromProp: invalid value, expecting long\n");
	}
	if (pVal && delVal)
		MAPIFreeBuffer( pVal);

	return( val);
}


void CMapiApi::ReportUIDProp( const char *pTag, LPSPropValue pVal)
{
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_BINARY)) {
		if (pVal->Value.bin.cb != 16) {
			MAPI_TRACE1( "%s - INVALID, expecting 16 bytes of binary data for UID\n", pTag);
		}
		else {
			nsIID	uid;
			nsCRT::memcpy( &uid, pVal->Value.bin.lpb, 16);
			char *	pStr = uid.ToString();
			if (pStr) {
				MAPI_TRACE2( "%s %s\n", pTag, (const char *)pStr);
				nsCRT::free( pStr);
			}
		}
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		MAPI_TRACE1( "%s {NULL}\n", pTag);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		MAPI_TRACE1( "%s {Error retrieving property}\n", pTag);
	}
	else {
		MAPI_TRACE1( "%s invalid value, expecting binary\n", pTag);
	}
	if (pVal)
		MAPIFreeBuffer( pVal);
}

void CMapiApi::ReportLongProp( const char *pTag, LPSPropValue pVal)
{
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_LONG)) {
		nsCString	num;
		nsCString	num2;

		num.AppendInt( (PRInt32) pVal->Value.l);
		num2.AppendInt( (PRInt32) pVal->Value.l, 16);
		MAPI_TRACE3( "%s %s, 0x%s\n", pTag, num, num2);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		MAPI_TRACE1( "%s {NULL}\n", pTag);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		MAPI_TRACE1( "%s {Error retrieving property}\n", pTag);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		MAPI_TRACE1( "%s {Error retrieving property}\n", pTag);
	}
	else {
		MAPI_TRACE1( "%s invalid value, expecting long\n", pTag);
	}
	if (pVal)
		MAPIFreeBuffer( pVal);
}

void CMapiApi::ReportStringProp( const char *pTag, LPSPropValue pVal)
{
	if ( pVal && (PROP_TYPE( pVal->ulPropTag) == PT_TSTRING)) {
		nsCString val((LPCTSTR) (pVal->Value.LPSZ));
		MAPI_TRACE2( "%s %s\n", pTag, (const char *)val);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_NULL)) {
		MAPI_TRACE1( "%s {NULL}\n", pTag);
	}
	else if (pVal && (PROP_TYPE( pVal->ulPropTag) == PT_ERROR)) {
		MAPI_TRACE1( "%s {Error retrieving property}\n", pTag);
	}
	else {
		MAPI_TRACE1( "%s invalid value, expecting string\n", pTag);
	}
	if (pVal)
		MAPIFreeBuffer( pVal);
}

void CMapiApi::GetPropTagName( ULONG tag, nsCString& s)
{
	char numStr[256];
	PR_snprintf( numStr, 256, "0x%lx, %ld", tag, tag);
	s = numStr;
	switch( tag) {
#include "mapitagstrs.cpp"
	}
	s += ", data: ";
	switch( PROP_TYPE( tag)) {
		case PT_UNSPECIFIED: s += "PT_UNSPECIFIED"; break;
		case PT_NULL: s += "PT_NULL"; break;
		case PT_I2: s += "PT_I2"; break;
		case PT_LONG: s += "PT_LONG"; break;
		case PT_R4: s += "PT_R4"; break;
		case PT_DOUBLE: s += "PT_DOUBLE"; break;
		case PT_CURRENCY: s += "PT_CURRENCY"; break;
		case PT_APPTIME: s += "PT_APPTIME"; break;
		case PT_ERROR: s += "PT_ERROR"; break;
		case PT_BOOLEAN: s += "PT_BOOLEAN"; break;
		case PT_OBJECT: s += "PT_OBJECT"; break;
		case PT_I8: s += "PT_I8"; break;
		case PT_STRING8: s += "PT_STRING8"; break;
		case PT_UNICODE: s += "PT_UNICODE"; break;
		case PT_SYSTIME: s += "PT_SYSTIME"; break;
		case PT_CLSID: s += "PT_CLSID"; break;
		case PT_BINARY: s += "PT_BINARY"; break;
		case PT_MV_I2: s += "PT_MV_I2"; break;
		case PT_MV_LONG: s += "PT_MV_LONG"; break;
		case PT_MV_R4: s += "PT_MV_R4"; break;
		case PT_MV_DOUBLE: s += "PT_MV_DOUBLE"; break;
		case PT_MV_CURRENCY: s += "PT_MV_CURRENCY"; break;
		case PT_MV_APPTIME: s += "PT_MV_APPTIME"; break;
		case PT_MV_SYSTIME: s += "PT_MV_SYSTIME"; break;
		case PT_MV_STRING8: s += "PT_MV_STRING8"; break;
		case PT_MV_BINARY: s += "PT_MV_BINARY"; break;
		case PT_MV_UNICODE: s += "PT_MV_UNICODE"; break;
		case PT_MV_CLSID: s += "PT_MV_CLSID"; break;
		case PT_MV_I8: s += "PT_MV_I8"; break;
		default:
			s += "Unknown";
	}
}

void ReplaceEolChars( nsCString& s)
{
	int				idx;
	nsCString		t;
	nsCString		rt;

	while ((idx = s.Find( "\x0D")) != -1) {
		s.Left( t, idx);
		t += "\\n";
		s.Right( rt, s.Length() - idx - 1);
		t += rt;
		s = t;
	}
	while ((idx = s.Find( "\x0A")) != -1) {
		s.Left( t, idx);
		t += "\\r";
		s.Right( rt, s.Length() - idx - 1);
		t += rt;
		s = t;
	}
}

void CMapiApi::ListPropertyValue( LPSPropValue pVal, nsCString& s)
{
	nsCString		strVal;
	char			nBuff[64];
	nsCString		t;

	s += "value: ";
	switch (PROP_TYPE( pVal->ulPropTag)) {
		case PT_STRING8:
			GetStringFromProp( pVal, strVal, FALSE);
			if (strVal.Length() > 60) {
				strVal.Left( t, 60);
				strVal = t;
				strVal += "...";
			}
			ReplaceEolChars( strVal);
			s += strVal;
		break;
		case PT_LONG:
			s.AppendInt( (PRInt32) pVal->Value.l);
			s += ", 0x";
			s.AppendInt( (PRInt32) pVal->Value.l, 16);
			s += nBuff;
		break;
		case PT_BOOLEAN:
			if (pVal->Value.b)
				s += "True";
			else
				s += "False";
		break;
		case PT_NULL:
			s += "--NULL--";
		break;
		case PT_SYSTIME: {
			/*
			COleDateTime	tm( pVal->Value.ft);
			s += tm.Format();
			*/
			s += "-- Figure out how to format time in mozilla, PT_SYSTIME --";
		}
		break;
		default:
		 s += "?";
	}
}



// -------------------------------------------------------------------
// Folder list stuff
// -------------------------------------------------------------------
CMapiFolderList::CMapiFolderList()
{
}

CMapiFolderList::~CMapiFolderList()
{
	ClearAll();
}

void CMapiFolderList::AddItem( CMapiFolder *pFolder)
{
	EnsureUniqueName( pFolder);
	GenerateFilePath( pFolder);
	m_array.AppendElement( pFolder);
}

void CMapiFolderList::ChangeName( nsString& name)
{
	if (name.IsEmpty()) {
		name.AssignWithConversion("1");
		return;
	}
	PRUnichar lastC = name.Last();
	if ((lastC >= '0') && (lastC <= '9')) {
		lastC++;
		if (lastC > '9') {
			lastC = '1';
			name.SetCharAt( lastC, name.Length() - 1);
			name.AppendWithConversion("0");
		}
		else {
			name.SetCharAt( lastC, name.Length() - 1);
		}
	}
	else {
		name.AppendWithConversion(" 2");
	}
}

void CMapiFolderList::EnsureUniqueName( CMapiFolder *pFolder)
{
	// For everybody in the array before me with the SAME
	// depth, my name must be unique
	CMapiFolder *	pCurrent;
	int				i;
	BOOL			done;
	nsString		name;
	nsString		cName;

	pFolder->GetDisplayName( name);
	do {
		done = TRUE;
		i = m_array.Count() - 1;
		while (i >= 0) {
			pCurrent = (CMapiFolder *)GetAt( i);
			if (pCurrent->GetDepth() == pFolder->GetDepth()) {
				pCurrent->GetDisplayName( cName);
				if (!cName.CompareWithConversion( name, PR_TRUE)) {
					ChangeName( name);
					pFolder->SetDisplayName(name.get());
					done = FALSE;
					break;
				}
			}
			else if (pCurrent->GetDepth() < pFolder->GetDepth())
				break;
			i--;
		}
	} while (!done);
}

void CMapiFolderList::GenerateFilePath( CMapiFolder *pFolder)
{
	// A file path, includes all of my parent's path, plus mine
	nsString		name;
	nsString		path;
	if (!pFolder->GetDepth()) {
		pFolder->GetDisplayName( name);
		pFolder->SetFilePath(name.get());
		return;
	}

	CMapiFolder *	pCurrent;
	int				i = m_array.Count() - 1;
	while (i >= 0) {
		pCurrent = (CMapiFolder *) GetAt( i);
		if (pCurrent->GetDepth() == (pFolder->GetDepth() - 1)) {
			pCurrent->GetFilePath( path);
			path.AppendWithConversion(".sbd\\");
			pFolder->GetDisplayName( name);
			path += name;
			pFolder->SetFilePath(path.get());
			return;
		}
		i--;
	}
	pFolder->GetDisplayName( name);
	pFolder->SetFilePath(name.get());
}

void CMapiFolderList::ClearAll( void)
{
	CMapiFolder *pFolder;
	for (int i = 0; i < m_array.Count(); i++) {
		pFolder = (CMapiFolder *)GetAt( i);
		delete pFolder;
	}
	m_array.Clear();
}

void CMapiFolderList::DumpList( void)
{
	CMapiFolder *pFolder;
	nsString	str;
	int			depth;
	char		prefix[256];

	MAPI_TRACE0( "Folder List ---------------------------------\n");
	for (int i = 0; i < m_array.Count(); i++) {
		pFolder = (CMapiFolder *)GetAt( i);
		depth = pFolder->GetDepth();
		pFolder->GetDisplayName( str);
		depth *= 2;
		if (depth > 255)
			depth = 255;
		nsCRT::memset( prefix, ' ', depth);
		prefix[depth] = 0;
#ifdef MAPI_DEBUG
        char *ansiStr = str.ToNewCString();
		MAPI_TRACE2( "%s%s: ", prefix, ansiStr);
		nsCRT::free(ansiStr);
#endif
		pFolder->GetFilePath( str);
#ifdef MAPI_DEBUG
        ansiStr = str.ToNewCString();
		MAPI_TRACE2( "depth=%d, filePath=%s\n", pFolder->GetDepth(), ansiStr);
		nsCRT::free(ansiStr);
#endif
	}
	MAPI_TRACE0( "---------------------------------------------\n");
}


CMapiFolder::CMapiFolder()
{
	m_objectType = MAPI_FOLDER;
	m_cbEid = 0;
	m_lpEid = NULL;
	m_depth = 0;
	m_doImport = TRUE;
}

CMapiFolder::CMapiFolder( const PRUnichar *pDisplayName, ULONG cbEid, LPENTRYID lpEid, int depth, LONG oType)
{
	m_cbEid = 0;
	m_lpEid = NULL;
	SetDisplayName( pDisplayName);
	SetEntryID( cbEid, lpEid);
	SetDepth( depth);
	SetObjectType( oType);
	SetDoImport( TRUE);
}

CMapiFolder::CMapiFolder( const CMapiFolder *pCopyFrom)
{
	m_lpEid = NULL;
	m_cbEid = 0;
	SetDoImport( pCopyFrom->GetDoImport());
	SetDisplayName(pCopyFrom->m_displayName.get());
	SetObjectType( pCopyFrom->GetObjectType());
	SetEntryID( pCopyFrom->GetCBEntryID(), pCopyFrom->GetEntryID());
	SetDepth( pCopyFrom->GetDepth());
	SetFilePath(pCopyFrom->m_mailFilePath.get());
}

CMapiFolder::~CMapiFolder()
{
	if (m_lpEid)
		delete m_lpEid;
}

void CMapiFolder::SetEntryID( ULONG cbEid, LPENTRYID lpEid)
{
	if (m_lpEid)
		delete m_lpEid;
	m_lpEid = NULL;
	m_cbEid = cbEid;
	if (cbEid) {
		m_lpEid = new BYTE[cbEid];
		nsCRT::memcpy( m_lpEid, lpEid, cbEid);
	}
}

// ---------------------------------------------------------------------
// Message store stuff
// ---------------------------------------------------------------------


CMsgStore::CMsgStore( ULONG cbEid, LPENTRYID lpEid)
{
	m_lpEid = NULL;
	m_lpMdb = NULL;
	SetEntryID( cbEid, lpEid);
}

CMsgStore::~CMsgStore()
{
	if (m_lpEid)
		delete m_lpEid;

	if (m_lpMdb) {
		ULONG flags = LOGOFF_NO_WAIT;
		HRESULT hr = m_lpMdb->StoreLogoff( &flags);
		m_lpMdb->Release();
		m_lpMdb = NULL;
	}
}

void CMsgStore::SetEntryID( ULONG cbEid, LPENTRYID lpEid)
{
	HRESULT		hr;

	if (m_lpEid)
		delete m_lpEid;

	m_lpEid = NULL;
	if (cbEid) {
		m_lpEid = new BYTE[cbEid];
		nsCRT::memcpy( m_lpEid, lpEid, cbEid);
	}
	m_cbEid = cbEid;

	if (m_lpMdb) {
		ULONG flags = LOGOFF_NO_WAIT;
		hr = m_lpMdb->StoreLogoff( &flags);
		m_lpMdb->Release();
		m_lpMdb = NULL;
	}
}

BOOL CMsgStore::Open( LPMAPISESSION pSession, LPMDB *ppMdb)
{
	if ( m_lpMdb) {
		if (ppMdb)
			*ppMdb = m_lpMdb;
		return( TRUE);
	}

	BOOL bResult = TRUE;
	HRESULT hr = pSession->OpenMsgStore( NULL, m_cbEid, (LPENTRYID)m_lpEid, NULL, MDB_NO_MAIL, &m_lpMdb);    // MDB pointer
	if (HR_FAILED( hr)) {
		m_lpMdb = NULL;
		MAPI_TRACE2( "OpenMsgStore failed: 0x%lx, %d\n", (long)hr, (int)hr);
		bResult = FALSE;
	}

	if (ppMdb)
		*ppMdb = m_lpMdb;
	return( bResult);
}



// ------------------------------------------------------------
// Contents Iterator
// -----------------------------------------------------------


CMapiFolderContents::CMapiFolderContents( LPMDB lpMdb, ULONG cbEid, LPENTRYID lpEid)
{
	m_lpMdb = lpMdb;
	m_fCbEid = cbEid;
	m_fLpEid = new BYTE[cbEid];
	nsCRT::memcpy( m_fLpEid, lpEid, cbEid);
	m_count = 0;
	m_iterCount = 0;
	m_failure = FALSE;
	m_lastError = 0;
	m_lpFolder = NULL;
	m_lpTable = NULL;
	m_lastLpEid = NULL;
	m_lastCbEid = 0;
}

CMapiFolderContents::~CMapiFolderContents()
{
	if (m_lastLpEid)
		delete m_lastLpEid;
	delete m_fLpEid;
	if (m_lpTable)
		m_lpTable->Release();
	if (m_lpFolder)
		m_lpFolder->Release();
}


BOOL CMapiFolderContents::SetUpIter( void)
{
	// First, open up the MAPIFOLDER object
	ULONG		ulObjType;
	HRESULT		hr;
	hr = m_lpMdb->OpenEntry( m_fCbEid, (LPENTRYID) m_fLpEid, NULL, 0, &ulObjType, (LPUNKNOWN *) &m_lpFolder);

	if (FAILED(hr) || !m_lpFolder) {
		m_lpFolder = NULL;
		m_lastError = hr;
		MAPI_TRACE2( "CMapiFolderContents OpenEntry failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	if (ulObjType != MAPI_FOLDER) {
		m_lastError = -1;
		MAPI_TRACE0( "CMapiFolderContents - bad object type, not a folder.\n");
		return( FALSE);
	}


	hr = m_lpFolder->GetContentsTable( 0, &m_lpTable);
	if (FAILED(hr) || !m_lpTable) {
		m_lastError = hr;
		m_lpTable = NULL;
		MAPI_TRACE2( "CMapiFolderContents - GetContentsTable failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	hr = m_lpTable->GetRowCount( 0, &m_count);
	if (FAILED(hr)) {
		m_lastError = hr;
		MAPI_TRACE0( "CMapiFolderContents - GetRowCount failed\n");
		return( FALSE);
	}

	hr = m_lpTable->SetColumns( (LPSPropTagArray)&ptaEid, 0);
	if (FAILED(hr)) {
		m_lastError = hr;
		MAPI_TRACE2( "CMapiFolderContents - SetColumns failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	hr = m_lpTable->SeekRow( BOOKMARK_BEGINNING, 0, NULL);
	if (FAILED(hr)) {
		m_lastError = hr;
		MAPI_TRACE2( "CMapiFolderContents - SeekRow failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}
	
	return( TRUE);
}


BOOL CMapiFolderContents::GetNext( ULONG *pcbEid, LPENTRYID *ppEid, ULONG *poType, BOOL *pDone)
{
	*pDone = FALSE;
	if (m_failure)
		return( FALSE);
	if (!m_lpFolder) {
		if (!SetUpIter()) {
			m_failure = TRUE;
			return( FALSE);
		}
		if (!m_count) {
			*pDone = TRUE;
			return( TRUE);
		}
	}

	int			cNumRows = 0;		
	LPSRowSet	lpRow = NULL;
	HRESULT		hr = m_lpTable->QueryRows( 1, 0, &lpRow);

	if(HR_FAILED( hr)) {
		m_lastError = hr;
		m_failure = TRUE;
		MAPI_TRACE2( "CMapiFolderContents - QueryRows failed: 0x%lx, %d\n", (long)hr, (int)hr);
		return( FALSE);
	}

	if(lpRow) {
		cNumRows = lpRow->cRows;
		if (cNumRows) {
            LPENTRYID	lpEID = (LPENTRYID) lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.lpb;
            ULONG		cbEID = lpRow->aRow[0].lpProps[ieidPR_ENTRYID].Value.bin.cb;
			ULONG		oType = lpRow->aRow[0].lpProps[ieidPR_OBJECT_TYPE].Value.ul;
			
			if (m_lastCbEid != cbEID) {
				if (m_lastLpEid)
					delete m_lastLpEid;
				m_lastLpEid = new BYTE[cbEID];
				m_lastCbEid = cbEID;
			}
			nsCRT::memcpy( m_lastLpEid, lpEID, cbEID);

			*ppEid = (LPENTRYID) m_lastLpEid;
			*pcbEid = cbEID;
			*poType = oType;
		}
		else
			*pDone = TRUE;
		CMapiApi::FreeProws( lpRow);		
    }
	else
		*pDone = TRUE;

	return( TRUE);
}


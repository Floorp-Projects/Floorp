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
#ifndef MapiApi_h___
#define MapiApi_h___

#include "nscore.h"
#include "nsString.h"
#include "nsVoidArray.h"

#include <stdio.h>

#include <windows.h>
#include <mapi.h>
#include <mapix.h>
#include <mapidefs.h>
#include <mapicode.h>
#include <mapitags.h>
#include <mapiutil.h>
// wabutil.h expects mapiutil to define _MAPIUTIL_H but it actually
// defines _MAPIUTIL_H_
#define _MAPIUTIL_H

class CMapiFolderList;
class CMsgStore;
class CMapiFolder;

class CMapiContentIter {
public:
	virtual BOOL HandleContentItem( ULONG oType, ULONG cb, LPENTRYID pEntry) = 0;
};

class CMapiHierarchyIter {
public:
	virtual BOOL HandleHierarchyItem( ULONG oType, ULONG cb, LPENTRYID pEntry) = 0;
};

class CMapiApi {
public:
	CMapiApi();
	~CMapiApi();

	static BOOL		LoadMapi( void);
	static BOOL		LoadMapiEntryPoints( void);
	static void		UnloadMapi( void);

	static HINSTANCE	m_hMapi32;

	static void		MAPIUninitialize( void);
	static HRESULT	MAPIInitialize( LPVOID lpInit);
	static SCODE	MAPIAllocateBuffer( ULONG cbSize, LPVOID FAR * lppBuffer);
	static ULONG	MAPIFreeBuffer( LPVOID lpBuff);
	static HRESULT	MAPILogonEx( ULONG ulUIParam, LPTSTR lpszProfileName, LPTSTR lpszPassword, FLAGS flFlags, LPMAPISESSION FAR * lppSession);
	static HRESULT	OpenStreamOnFile( LPALLOCATEBUFFER lpAllocateBuffer, LPFREEBUFFER lpFreeBuffer, ULONG ulFlags, LPTSTR lpszFileName, LPTSTR lpszPrefix, LPSTREAM FAR * lppStream);
	static void		FreeProws( LPSRowSet prows);


	BOOL	Initialize( void);
	BOOL	LogOn( void);

	void	AddMessageStore( CMsgStore *pStore);
	void	SetCurrentMsgStore( LPMDB lpMdb) { m_lpMdb = lpMdb;}

	// Open any given entry from the current Message Store	
	BOOL	OpenEntry( ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen);
	static BOOL OpenMdbEntry( LPMDB lpMdb, ULONG cbEntry, LPENTRYID pEntryId, LPUNKNOWN *ppOpen);

	// Fill in the folders list with the heirarchy from the given
	// message store.
	BOOL	GetStoreFolders( ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders, int startDepth);
	BOOL	GetStoreAddressFolders( ULONG cbEid, LPENTRYID lpEid, CMapiFolderList& folders);
	BOOL	OpenStore( ULONG cbEid, LPENTRYID lpEid, LPMDB *ppMdb);

	// Iteration
	BOOL	IterateStores( CMapiFolderList& list);
	BOOL	IterateContents( CMapiContentIter *pIter, LPMAPIFOLDER pFolder, ULONG flags = 0);
	BOOL	IterateHierarchy( CMapiHierarchyIter *pIter, LPMAPIFOLDER pFolder, ULONG flags = 0);
	
	// Properties
	static LPSPropValue	GetMapiProperty( LPMAPIPROP pProp, ULONG tag);
	static BOOL			GetEntryIdFromProp( LPSPropValue pVal, ULONG& cbEntryId, LPENTRYID& lpEntryId, BOOL delVal = TRUE);
	static BOOL			GetStringFromProp( LPSPropValue pVal, nsCString& val, BOOL delVal = TRUE);
	static BOOL			GetStringFromProp( LPSPropValue pVal, nsString& val, BOOL delVal = TRUE);
	static LONG			GetLongFromProp( LPSPropValue pVal, BOOL delVal = TRUE);
	static BOOL			GetLargeStringProperty( LPMAPIPROP pProp, ULONG tag, nsCString& val);
	static BOOL			GetLargeStringProperty( LPMAPIPROP pProp, ULONG tag, nsString& val);
	static BOOL			IsLargeProperty( LPSPropValue pVal);
	static ULONG    GetEmailPropertyTag(LPMAPIPROP lpProp, LONG nameID);

	// Debugging & reporting stuff
	static void			ListProperties( LPMAPIPROP lpProp, BOOL getValues = TRUE);
	static void			ListPropertyValue( LPSPropValue pVal, nsCString& s);

protected:
	BOOL			HandleHierarchyItem( ULONG oType, ULONG cb, LPENTRYID pEntry);
	BOOL			HandleContentsItem( ULONG oType, ULONG cb, LPENTRYID pEntry);
	void			GetStoreInfo( CMapiFolder *pFolder, long *pSzContents);

	// array of available message stores, cached so that
	// message stores are only opened once, preventing multiple
	// logon's by the user if the store requires a logon.
	CMsgStore *		FindMessageStore( ULONG cbEid, LPENTRYID lpEid);
	void			ClearMessageStores( void);

	static void			CStrToUnicode( const char *pStr, nsString& result);

	// Debugging & reporting stuff
	static void			GetPropTagName( ULONG tag, nsCString& s);
	static void			ReportStringProp( const char *pTag, LPSPropValue pVal);
	static void			ReportUIDProp( const char *pTag, LPSPropValue pVal);
	static void			ReportLongProp( const char *pTag, LPSPropValue pVal);


private:
	static int				m_clients;
	static BOOL				m_initialized;
	static nsVoidArray *	m_pStores;
	static LPMAPISESSION	m_lpSession;
	static LPMDB			m_lpMdb;
	static HRESULT			m_lastError;
	static PRUnichar *		m_pUniBuff;
	static int				m_uniBuffLen;
};



class CMapiFolder {
public:
	CMapiFolder();
	CMapiFolder( const CMapiFolder *pCopyFrom);
	CMapiFolder( const PRUnichar *pDisplayName, ULONG cbEid, LPENTRYID lpEid, int depth, LONG oType = MAPI_FOLDER);
	~CMapiFolder();

	void	SetDoImport( BOOL doIt) { m_doImport = doIt;}
	void	SetObjectType( long oType) { m_objectType = oType;}
	void	SetDisplayName( const PRUnichar *pDisplayName) { m_displayName = pDisplayName;}
	void	SetEntryID( ULONG cbEid, LPENTRYID lpEid);
	void	SetDepth( int depth) { m_depth = depth;}
	void	SetFilePath( const PRUnichar *pFilePath) { m_mailFilePath = pFilePath;}
	
	BOOL	GetDoImport( void) const { return( m_doImport);}
	LONG	GetObjectType( void) const { return( m_objectType);}
	void	GetDisplayName( nsString& name) const { name = m_displayName;}
	void	GetFilePath( nsString& path) const { path = m_mailFilePath;}
	BOOL	IsStore( void) const { return( m_objectType == MAPI_STORE);}
	BOOL	IsFolder( void) const { return( m_objectType == MAPI_FOLDER);}
	int		GetDepth( void) const { return( m_depth);}

	LPENTRYID	GetEntryID( ULONG *pCb = NULL) const { if (pCb) *pCb = m_cbEid; return( (LPENTRYID) m_lpEid);}
	ULONG		GetCBEntryID( void) const { return( m_cbEid);}

private:
	LONG		m_objectType;
	ULONG		m_cbEid;
	BYTE *		m_lpEid;
	nsString	m_displayName;
	int			m_depth;
	nsString	m_mailFilePath;
	BOOL		m_doImport;

};

class CMapiFolderList {
public:
	CMapiFolderList();
	~CMapiFolderList();

	void			AddItem( CMapiFolder *pFolder);
	CMapiFolder *	GetItem( int index) { if ((index >= 0) && (index < m_array.Count())) return( (CMapiFolder *)GetAt( index)); else return( NULL);}
	void			ClearAll( void);

	// Debugging and reporting
	void			DumpList( void);

	void *			GetAt( int index) { return( m_array.ElementAt( index));}
	int				GetSize( void) { return( m_array.Count());}

protected:
	void	EnsureUniqueName( CMapiFolder *pFolder);
	void	GenerateFilePath( CMapiFolder *pFolder);
	void	ChangeName( nsString& name);

private:
	nsVoidArray		m_array;
};


class CMsgStore {
public:
	CMsgStore( ULONG cbEid = 0, LPENTRYID lpEid = NULL);
	~CMsgStore();

	void		SetEntryID( ULONG cbEid, LPENTRYID lpEid);
	BOOL		Open( LPMAPISESSION pSession, LPMDB *ppMdb);
	
	ULONG		GetCBEntryID( void) { return( m_cbEid);}
	LPENTRYID	GetLPEntryID( void) { return( (LPENTRYID) m_lpEid);}

private:
	ULONG		m_cbEid;
	BYTE *		m_lpEid;
	LPMDB		m_lpMdb;
};


class CMapiFolderContents {
public:
	CMapiFolderContents( LPMDB lpMdb, ULONG cbEID, LPENTRYID lpEid);
	~CMapiFolderContents();

	BOOL	GetNext( ULONG *pcbEid, LPENTRYID *ppEid, ULONG *poType, BOOL *pDone);

	ULONG	GetCount( void) { return( m_count);}

protected:
	BOOL SetUpIter( void);

private:
	HRESULT			m_lastError;
	BOOL			m_failure;
	LPMDB			m_lpMdb;
	LPMAPIFOLDER	m_lpFolder;
	LPMAPITABLE		m_lpTable;
	ULONG			m_fCbEid;
	BYTE *			m_fLpEid;
	ULONG			m_count;
	ULONG			m_iterCount;
	BYTE *			m_lastLpEid;
	ULONG			m_lastCbEid;
};


#endif /* MapiApi_h__ */

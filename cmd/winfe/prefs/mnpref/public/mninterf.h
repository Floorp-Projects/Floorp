/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __MNINTERF_H_
#define __MNINTERF_H_

/////////////////////////////////////////////////////////////////////////////
// IMailNewsInterface interface

#ifdef __cplusplus
interface IMailNewsInterface;
#else
typedef interface IMailNewsInterface IMailNewsInterface;
#endif

typedef IMailNewsInterface FAR* LPMAILNEWSINTERFACE;

#undef  INTERFACE
#define INTERFACE IMailNewsInterface

#ifdef XP_WIN16
#ifndef LPOLESTR

typedef char OLECHAR;
typedef OLECHAR FAR* LPOLESTR;
typedef const OLECHAR FAR* LPCOLESTR;

#endif
#endif

// IMailNewsInterface provides the Mail News preference UI to call back to .Exe
DECLARE_INTERFACE_(IMailNewsInterface, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IMailNewsPrefs methods
	STDMETHOD_(BOOL, CreateAddressBookCard)(HWND hParent) PURE;
	STDMETHOD(EditAddressBookCard)(HWND hParent) PURE;

	STDMETHOD(GetSigFile)(THIS_ HWND hParent, LPOLESTR *pszSigFile) PURE;
	STDMETHOD(ShowDirectoryPicker)(THIS_ HWND hParent, LPOLESTR lpIniDir, LPOLESTR *pszNewDir) PURE;

	STDMETHOD(ShowChooseFolder)(THIS_ HWND hParent, LPOLESTR lpFolderPath,
		DWORD dwType, LPOLESTR *lpFolder, LPOLESTR *lpServer, LPOLESTR *lpPref) PURE;
	STDMETHOD(GetFolderServer)(LPOLESTR lpFolderPath, DWORD dwType, 
		LPOLESTR *lpFolder, LPOLESTR *lpServer) PURE;

	STDMETHOD(FillNewsServerList)(THIS_ HWND hControl, LPOLESTR lpServerName, LPVOID FAR* ppServer) PURE;
	STDMETHOD_(BOOL, AddNewsServer)(THIS_ HWND hParent, HWND hControl) PURE;
	STDMETHOD_(BOOL, EditNewsServer)(THIS_ HWND hParent, HWND hControl, BOOL *pChanged) PURE;
	STDMETHOD(DeleteNewsServer)(THIS_ HWND hParent, HWND hControl, LPVOID FAR* lpDefault) PURE;
	
	STDMETHOD_(BOOL, AddMailServer)(THIS_ HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType) PURE;
	STDMETHOD_(BOOL, EditMailServer)(THIS_ HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType) PURE;
	STDMETHOD_(BOOL, DeleteMailServer)(THIS_ HWND hParent, HWND hControl, DWORD dwType) PURE;
	STDMETHOD(SetImapDefaultServer)(THIS_ HWND hControl) PURE;

	STDMETHOD(FillLdapDirList)(THIS_ HWND hControl, LPOLESTR lpServerName) PURE;
	STDMETHOD(SetLdapDirAutoComplete)(THIS_ HWND hControl, BOOL bOnOrOff) PURE;
	STDMETHOD(SaveLdapDirAutoComplete)(THIS) PURE;

};

/////////////////////////////////////////////////////////////////////////////
// IOfflineInterface interface

#ifdef __cplusplus
interface IOfflineInterface;
#else
typedef interface IOfflineInterface IOfflineInterface;
#endif

typedef IOfflineInterface FAR* LPOFFLINEINTERFACE;

#undef  INTERFACE
#define INTERFACE IOfflineInterface


// IMailNewsInterface provides the Mail News preference UI to call back to .Exe
DECLARE_INTERFACE_(IOfflineInterface, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IOfflineInterface methods
	STDMETHOD(DoSelectDiscussion)(THIS_ HWND hParent) PURE;
};

typedef enum
{
  TYPE_SENTMAIL,
  TYPE_SENTNEWS,
  TYPE_DRAFT,
  TYPE_TEMPLATE
}FolderType;

typedef enum
{
  TYPE_POP,
  TYPE_IMAP,
}MailServerType;

#endif /* __MNINTERF_H_ */


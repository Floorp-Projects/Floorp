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

#include "stdafx.h"
#ifndef _WIN32
#include <ole2ui.h>
#include <dispatch.h>
#endif
#include "prefs.h"
#include <olectl.h>
#include "winprefs/isppageo.h"
#include "winprefs/brprefid.h"
#ifdef MOZ_LOC_INDEP
#include "winprefs/liprefid.h"
#include "winprefs/iliprefs.h"
#endif // MOZ_LOC_INDEP
#ifdef EDITOR
#include "winprefs/edprefid.h"
#endif /* EDITOR */
#ifdef MOZ_MAIL_NEWS
#include "winprefs/mnprefid.h"
#include "winprefs/mninterf.h"
#endif /* MOZ_MAIL_NEWS */
#include "winprefs/intlfont.h"
#include "winprefs/ibrprefs.h"
#include "winprefs/prefuiid.h"
#include "winprefs/prefui.h"
#include "cvffc.h"
#ifdef EDITOR
#include "edt.h"
#endif // EDITOR
#include "prefapi.h"
#include "helper.h"
#ifdef MOZ_MAIL_NEWS
#include "mnprefs.h"
#include "mailmisc.h"
#include "wfemsg.h"
#endif /* MOZ_MAIL_NEWS */
#include "dlgseldg.h"
#ifdef MOZ_MAIL_NEWS
#include "addrbook.h"
#include "addrfrm.h"	// for creating vcards
#ifdef FEATURE_ADDRPROP
#include "addrprop.h"
#endif
#endif /* MOZ_MAIL_NEWS */

extern "C" {
#include "xpgetstr.h"
#ifdef MOZ_MAIL_NEWS
extern int MK_ADDR_BOOK_CARD;
extern int MK_ADDR_NEW_CARD;
extern int MK_MSG_SENT_L10N_NAME;  //Sent folder
extern int MK_MSG_TEMPLATES_L10N_NAME;
extern int MK_MSG_DRAFTS_L10N_NAME;
extern int MK_MSG_REMOVE_HOST_CONFIRM;
extern int MK_POP3_ONLY_ONE;
extern int MK_MSG_REMOVE_MAILHOST_CONFIRM;
#endif /* MOZ_MAIL_NEWS */
};
#include "nethelp.h"
#include "ngdwtrst.h"

BOOL	g_bReloadAllWindows;

extern "C" void GetFolderServerNames
(char* lpName, int nDefaultID, CString& folder, CString& server);
extern "C" MSG_Host *DoAddNewsServer(CWnd* pParent, int nFromWhere);

/////////////////////////////////////////////////////////////////////////////
// Helper functions

#ifndef _WIN32
static LPVOID
CoTaskMemAlloc(ULONG cb)
{
	LPMALLOC	pMalloc;
	
	if (SUCCEEDED(CoGetMalloc(MEMCTX_TASK, &pMalloc))) {
		LPVOID	pv = pMalloc->Alloc(cb);

		pMalloc->Release();
		return pv;
	}

	return NULL;
}
#endif

// Convert an ANSI string to an OLE string (UNICODE string)
static LPOLESTR NEAR
AllocTaskOleString(LPCSTR lpszString)
{
	LPOLESTR	lpszResult;
	UINT		nLen;

	if (lpszString == NULL)
		return NULL;

	// Convert from ANSI to UNICODE
	nLen = lstrlen(lpszString) + 1;
	lpszResult = (LPOLESTR)CoTaskMemAlloc(nLen * sizeof(OLECHAR));
	if (lpszResult)	{
#ifdef _WIN32
		MultiByteToWideChar(CP_ACP, 0, lpszString, -1, lpszResult, nLen);
#else
		lstrcpy(lpszResult, lpszString);  // Win 16 doesn't use any UNICODE
#endif
	}

	return lpszResult;
}

/////////////////////////////////////////////////////////////////////////////
// CEnumHelpers

// Helper class to encapsulate enumeration of the helper apps
class CEnumHelpers : public IEnumHelpers {
	public:
		CEnumHelpers();
		
		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();

		// IEnumHelpers methods
		STDMETHODIMP		 Next(NET_cdataStruct **ppcdata);
		STDMETHODIMP		 Reset();

	private:
		ULONG		m_uRef;
		XP_List	   *m_pInfoList;
};

CEnumHelpers::CEnumHelpers()
{
	m_uRef = 0;
	m_pInfoList = cinfo_MasterListPointer();
}

STDMETHODIMP
CEnumHelpers::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IEnumHelpers) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CEnumHelpers::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CEnumHelpers::Release()
{
	if (--m_uRef == 0) {
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CEnumHelpers::Next(NET_cdataStruct **ppcdata)
{
	CHelperApp	*pHelperApp;

	while ((*ppcdata = (NET_cdataStruct *)XP_ListNextObject(m_pInfoList))) {
		// Ignore items that don't have a MIME type
		if (!(*ppcdata)->ci.type)
			continue;

		// Don't give the user an opportunity to change application/octet-stream
		// or proxy auto-config
		if (strcmp((*ppcdata)->ci.type, APPLICATION_OCTET_STREAM) == 0 ||
			strcmp((*ppcdata)->ci.type, APPLICATION_NS_PROXY_AUTOCONFIG) == 0) {
			continue;
		}

		// Ignore items that don't have a description
		if (!(*ppcdata)->ci.desc) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}
		
		// Make sure there's a CHelperApp associated with the item
		pHelperApp = (CHelperApp *)(*ppcdata)->ci.fe_data;

		if (!pHelperApp) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}

		// Ignore items that have HANDLE_UNKNOWN or HANDLE_MOREINFO as how to handle
		if (pHelperApp->how_handle == HANDLE_UNKNOWN || pHelperApp->how_handle == HANDLE_MOREINFO) {
#ifdef DEBUG
			TRACE1("PREFS: Ignoring MIME type %s\n", (*ppcdata)->ci.type);
#endif
			continue;
		}

		return NOERROR;
	}

	return ResultFromScode(S_FALSE);
}

STDMETHODIMP
CEnumHelpers::Reset()
{
	m_pInfoList = cinfo_MasterListPointer();
	return NOERROR;
}

/////////////////////////////////////////////////////////////////////////////
// CBrowserPrefs

class CBrowserPrefs : public IBrowserPrefs {
	public:
		CBrowserPrefs(LPCSTR lpszCurrentPage);

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IBrowserPrefs methods
		STDMETHODIMP GetCurrentPage(LPOLESTR *lpoleStr);

		STDMETHODIMP EnumHelpers(LPENUMHELPERS *ppenumHelpers);
		STDMETHODIMP GetHelperInfo(NET_cdataStruct *, LPHELPERINFO);
		STDMETHODIMP SetHelperInfo(NET_cdataStruct *, LPHELPERINFO);
		STDMETHODIMP NewFileType(LPCSTR lpszDescription,
								 LPCSTR lpszExtension,
								 LPCSTR lpszMimeType,
								 LPCSTR lpszOpenCmd,
								 NET_cdataStruct **ppcdata);
		STDMETHODIMP RemoveFileType(NET_cdataStruct *);

		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:
		ULONG	  	m_uRef;
		LPUNKNOWN 	m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
		LPCSTR		m_lpszCurrentPage;
};

CBrowserPrefs::CBrowserPrefs(LPCSTR lpszCurrentPage)
{
	m_uRef = 0;
	m_pCategory = NULL;
	m_lpszCurrentPage = lpszCurrentPage ? strdup(lpszCurrentPage) : NULL;
}

HRESULT
CBrowserPrefs::Init()
{
	// Create the object as part of an aggregate
	return CoCreateInstance(CLSID_BrowserPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CBrowserPrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IBrowserPrefs) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CBrowserPrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CBrowserPrefs::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		if (m_lpszCurrentPage)
			XP_FREE((void *)m_lpszCurrentPage);
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CBrowserPrefs::GetCurrentPage(LPOLESTR *lpoleStr)
{
	if (!lpoleStr)
		return ResultFromScode(E_INVALIDARG);
	
	*lpoleStr = 0;

	if (m_lpszCurrentPage) {
		*lpoleStr = AllocTaskOleString(m_lpszCurrentPage);
		
		if (!*lpoleStr)
			return ResultFromScode(E_OUTOFMEMORY);
	}

	return NOERROR;
}

STDMETHODIMP
CBrowserPrefs::EnumHelpers(LPENUMHELPERS *ppEnumHelpers)
{
	CEnumHelpers   *pEnumHelpers = new CEnumHelpers;
	HRESULT			hres;
	
	if (!pEnumHelpers)
		return ResultFromScode(E_OUTOFMEMORY);

	pEnumHelpers->AddRef();
	hres = pEnumHelpers->QueryInterface(IID_IEnumHelpers, (LPVOID *)ppEnumHelpers);
	pEnumHelpers->Release();
	return hres;
}

// Returns the command string value (path and filename for the application plus any
// command line options) associated with the given file extension
static BOOL
GetOpenCommandForExt(LPCSTR lpszExt, LPSTR lpszCmdString, DWORD cbCmdString)
{
	char    szBuf[_MAX_PATH + 32];
	char	szFileClass[60];
	LONG	lResult;
	LONG	lcb;
#ifdef _WIN32
	DWORD	cbData;
	DWORD	dwType;
#else
	LONG	cbData;
#endif

	*lpszCmdString = '\0';
	
	// Look up the file association key which maps a file extension
	// to an application identifier (also called the file type class
	// identifier or just file class)
	PR_snprintf(szBuf, sizeof(szBuf), ".%s", lpszExt);
	lcb = sizeof(szFileClass);
	lResult = RegQueryValue(HKEY_CLASSES_ROOT, szBuf, szFileClass, &lcb);

#ifdef _WIN32
	ASSERT(lResult != ERROR_MORE_DATA);
#endif
	if (lResult != ERROR_SUCCESS)
		return FALSE;

	// Get the key for shell\open\command
	HKEY	hKey;
	
#ifdef _WIN32
	PR_snprintf(szBuf, sizeof(szBuf), "%s\\shell\\open\\command", szFileClass);
	lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szBuf, 0, KEY_QUERY_VALUE, &hKey);
#else
	PR_snprintf(szBuf, sizeof(szBuf), "%s\\shell\\open\\command", szFileClass);
	lResult = RegOpenKey(HKEY_CLASSES_ROOT, szBuf, &hKey);
#endif
	if (lResult != ERROR_SUCCESS)
		return FALSE;

	// Get the value of the key
	cbData = sizeof(szBuf);
#ifdef _WIN32
	lResult = RegQueryValueEx(hKey, NULL, NULL, &dwType, (LPBYTE)szBuf, &cbData);
#else
	lResult = RegQueryValue(hKey, NULL, (LPSTR)szBuf, &cbData);
#endif
	RegCloseKey(hKey);
	if (lResult != ERROR_SUCCESS)
		return FALSE;

#ifdef _WIN32
	// Win32 doesn't expand automatically environment variables (for value
	// data of type REG_EXPAND_SZ). We need this for things like %SystemRoot%
	if (dwType == REG_EXPAND_SZ)
		ExpandEnvironmentStrings(szBuf, lpszCmdString, cbCmdString);
	else
		lstrcpy(lpszCmdString, szBuf);
#else
	lstrcpy(lpszCmdString, szBuf);
#endif

    return TRUE;
}

// Returns the path of the helper application associated with the
// given pcdata
static BOOL
GetApplication(NET_cdataStruct *pcdata, LPSTR lpszApp)
{
	CHelperApp *pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	char   	   *lpszFile;
	char		szExtension[_MAX_EXT];
	LPSTR		lpszStrip;

	if (!pHelperApp)
		return FALSE;

	// How we do this depends on how it's handled
	switch (pHelperApp->how_handle) {
		case HANDLE_EXTERNAL:
			lstrcpy(lpszApp, (LPCSTR)pHelperApp->csCmd);

			// XXX - this is what the code in display.cpp does, but it's really
			// wrong...
			lpszStrip = strrchr(lpszApp, '%');

			if (lpszStrip)
				*lpszStrip = '\0';
			return TRUE;

		case HANDLE_SHELLEXECUTE:
		case HANDLE_BY_OLE:
			// Have FindExecutable() tell us what the executable name is
			wsprintf(szExtension, ".%s", pcdata->exts[0]);
			lpszFile = WH_TempFileName(xpTemporary, "M", szExtension);
			if (lpszFile) {
				BOOL	bResult = FEU_FindExecutable(lpszFile, lpszApp, TRUE);

				XP_FREE(lpszFile);
				return bResult;
			}
			return FALSE;

		default:
			ASSERT(FALSE);
			return FALSE;
	}
}

//Begin CRN_MIME
static BOOL
IsMimeTypeLocked(const char *pPrefix)
{
	BOOL bRet = FALSE;

	//???????Any holes here??????
	//I'm looking at just the ".type" field to see if a pref is locked. 
	char* setup_buf = PR_smprintf("%s.%s", pPrefix, "mimetype");

	bRet = PREF_PrefIsLocked(setup_buf);

	if(setup_buf && setup_buf[0])
		XP_FREEIF(setup_buf);

	return bRet;
}

static int
GetPrefLoadAction(int how_handle)
{
	switch(how_handle) {
	case HANDLE_SAVE:
			return 1;

		case HANDLE_SHELLEXECUTE: 
			return 2;

		case HANDLE_UNKNOWN:
			return 3;

		case HANDLE_VIA_PLUGIN:
			return 4;

		default:
			return 2;
	}
}
//End CRN_MIME

STDMETHODIMP
CBrowserPrefs::GetHelperInfo(NET_cdataStruct *pcdata, LPHELPERINFO lpInfo)
{
	CHelperApp	*pHelperApp;
    char         szApp[_MAX_PATH];
	
	if (!pcdata || !lpInfo)
		return ResultFromScode(E_INVALIDARG);

	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if (!pHelperApp)
		return ResultFromScode(E_UNEXPECTED);

	//Begin CRN_MIME
	if(! pHelperApp->csMimePrefPrefix.IsEmpty())
	{
		//This is a mime type associcated with a helper for a type specified thru' prefs.
		//Use the csMimePrefPrefix to generate the pref and see if it's locked.
		lpInfo->bIsLocked = IsMimeTypeLocked((LPCSTR)pHelperApp->csMimePrefPrefix);
		
	}
	else
		lpInfo->bIsLocked = FALSE;
	//End CRN_MIME

	lpInfo->nHowToHandle = pHelperApp->how_handle;
	lpInfo->szOpenCmd[0] = '\0';

	switch (pHelperApp->how_handle) {
		case HANDLE_EXTERNAL:
			lstrcpy(lpInfo->szOpenCmd, (LPCSTR)pHelperApp->csCmd);
            if (GetApplication(pcdata, szApp))
			    lpInfo->bAskBeforeOpening = theApp.m_pSpawn->PromptBeforeOpening(szApp);
			break;

		case HANDLE_SHELLEXECUTE:
		case HANDLE_BY_OLE:
			ASSERT(pcdata->num_exts > 0);
			if (pcdata->num_exts > 0) {
				GetOpenCommandForExt(pcdata->exts[0], lpInfo->szOpenCmd, sizeof(lpInfo->szOpenCmd));
                if (GetApplication(pcdata, szApp))
				    lpInfo->bAskBeforeOpening = theApp.m_pSpawn->PromptBeforeOpening(szApp);
			}
			break;

		case HANDLE_SAVE:
			if (pcdata->num_exts > 0) {
				// Even though it's marked as Save to Disk get the shell open command anyway. This
				// way if the user changes the association to launch the application they'll already
				// have it
				GetOpenCommandForExt(pcdata->exts[0], lpInfo->szOpenCmd, sizeof(lpInfo->szOpenCmd));
				lpInfo->bAskBeforeOpening = TRUE;
			}
			break;

		default:
			break;
	}
	
	return NOERROR;
}

STDMETHODIMP
CBrowserPrefs::SetHelperInfo(NET_cdataStruct *pcdata, LPHELPERINFO lpInfo)
{
    fe_ChangeFileType(pcdata, lpInfo->lpszMimeType, lpInfo->nHowToHandle, lpInfo->szOpenCmd);

//Begin CRN_MIME
	//Set user pref values only for mime types setup from the pref file
	CHelperApp	*pHelperApp;
	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if(pHelperApp)
	{
		if(! pHelperApp->csMimePrefPrefix.IsEmpty())
		{
			char* setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "mimetype");
			PREF_SetCharPref(setup_buf, lpInfo->lpszMimeType);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "win_appname");
			PREF_SetCharPref(setup_buf, lpInfo->szOpenCmd);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "load_action");
			PREF_SetIntPref(setup_buf, GetPrefLoadAction(lpInfo->nHowToHandle));
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);
		}
	}

//End CRN_MIME

	if (lpInfo->nHowToHandle == HANDLE_EXTERNAL ||
		lpInfo->nHowToHandle == HANDLE_SHELLEXECUTE ||
		lpInfo->nHowToHandle == HANDLE_BY_OLE) {

		// Update whether we should prompt before opening the file
        char    szApp[_MAX_PATH];

        if (GetApplication(pcdata, szApp))
		    theApp.m_pSpawn->SetPromptBeforeOpening(szApp, lpInfo->bAskBeforeOpening);
	}
	return NOERROR;
}

STDMETHODIMP
CBrowserPrefs::NewFileType(LPCSTR lpszDescription,
						   LPCSTR lpszExtension,
						   LPCSTR lpszMimeType,
						   LPCSTR lpszOpenCmd,
						   NET_cdataStruct **ppcdata)
{
	*ppcdata = fe_NewFileType(lpszDescription, lpszExtension, lpszMimeType, lpszOpenCmd);
	return *ppcdata ? NOERROR : ResultFromScode(S_FALSE);
}

STDMETHODIMP
CBrowserPrefs::RemoveFileType(NET_cdataStruct *pcdata)
{
//Begin CRN_MIME
	//Clear user pref values only for mime types setup from the pref file
	CHelperApp	*pHelperApp;
	pHelperApp = (CHelperApp *)pcdata->ci.fe_data;
	if(pHelperApp)
	{
		if(! pHelperApp->csMimePrefPrefix.IsEmpty())
		{
			char* setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "mimetype");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "win_appname");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "load_action");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "extension");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);

			setup_buf = PR_smprintf("%s.%s", (LPCSTR)pHelperApp->csMimePrefPrefix, "description");
			PREF_ClearUserPref(setup_buf);
			if(setup_buf && setup_buf[0])
				XP_FREEIF(setup_buf);
		}
	}
//End CRN_MIME

	return fe_RemoveFileType(pcdata) ? NOERROR : ResultFromScode(S_FALSE);
}

/////////////////////////////////////////////////////////////////////////////
// CAppearancePreferences

class CAppearancePrefs : public IIntlFont {
	public:
		CAppearancePrefs(int nCharsetId);

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IIntlFont methods
		STDMETHODIMP GetNumEncodings(LPDWORD pdwEncodings);
		STDMETHODIMP GetEncodingName(DWORD dwCharsetNum, LPOLESTR *pszName);
		STDMETHODIMP GetEncodingInfo(DWORD dwCharsetNum, LPENCODINGINFO lpInfo);
		STDMETHODIMP SetEncodingFonts(DWORD dwCharsetNum, LPENCODINGINFO lpInfo);
		STDMETHODIMP GetCurrentCharset(LPDWORD pdwCharsetNum);

		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:
		ULONG	  m_uRef;
		int		  m_nCharsetId;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
};

CAppearancePrefs::CAppearancePrefs(int nCharsetId)
{
	m_uRef = 0;
	m_nCharsetId = nCharsetId;
	m_pCategory = NULL;
}

HRESULT
CAppearancePrefs::Init()
{
	// Create the object as part of an aggregate
	return CoCreateInstance(CLSID_AppearancePrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CAppearancePrefs::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IIntlFont) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CAppearancePrefs::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CAppearancePrefs::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
CAppearancePrefs::GetNumEncodings(LPDWORD pdwEncodings)
{
	if (!pdwEncodings)
		return ResultFromScode(E_INVALIDARG);
	
	*pdwEncodings = (DWORD)MAXLANGNUM;
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::GetEncodingName(DWORD dwCharsetNum, LPOLESTR *lpoleName)
{
	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpoleName)
		return ResultFromScode(E_INVALIDARG);

	LPCSTR	lpszName = theApp.m_pIntlFont->GetEncodingName((int)dwCharsetNum);

	if (lpszName) {
		*lpoleName = AllocTaskOleString(lpszName);
		return *lpoleName ? NOERROR : ResultFromScode(E_OUTOFMEMORY);
	}
	
	return ResultFromScode(E_UNEXPECTED);
}

STDMETHODIMP
CAppearancePrefs::GetEncodingInfo(DWORD dwCharsetNum, LPENCODINGINFO lpInfo)
{
	EncodingInfo	*lpEncoding;

	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpInfo)
		return ResultFromScode(E_INVALIDARG);
	
	lpEncoding = theApp.m_pIntlFont->GetEncodingInfo((int)dwCharsetNum);
	lpInfo->bIgnorePitch = ( CIntlWin::FontSelectIgnorePitch(lpEncoding->iCSID) );
	lstrcpy(lpInfo->szVariableWidthFont, lpEncoding->szPropName);
	lpInfo->nVariableWidthSize = lpEncoding->iPropSize;
	lstrcpy(lpInfo->szFixedWidthFont, lpEncoding->szFixName);
	lpInfo->nFixedWidthSize = lpEncoding->iFixSize;
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::SetEncodingFonts(DWORD dwCharsetNum, LPENCODINGINFO lpInfo)
{
	EncodingInfo   *lpEncoding;
	BOOL			bFixedWidthChanged;
	BOOL			bVariableWidthChanged;

	if (dwCharsetNum >= (DWORD)MAXLANGNUM || !lpInfo)
		return ResultFromScode(E_INVALIDARG);
	
	lpEncoding = theApp.m_pIntlFont->GetEncodingInfo((int)dwCharsetNum);

	// See if anything has changed. We don't want to reset everything
	// if nothing has changed
	bFixedWidthChanged = lstrcmp(lpInfo->szFixedWidthFont, lpEncoding->szFixName) != 0 ||
		lpInfo->nFixedWidthSize != lpEncoding->iFixSize;
	bVariableWidthChanged = lstrcmp(lpInfo->szVariableWidthFont, lpEncoding->szPropName) != 0 ||
		lpInfo->nVariableWidthSize != lpEncoding->iPropSize;

	if (bFixedWidthChanged || bVariableWidthChanged) {
		lstrcpy(lpEncoding->szFixName, lpInfo->szFixedWidthFont);
		lpEncoding->iFixSize = lpInfo->nFixedWidthSize;
		lstrcpy(lpEncoding->szPropName, lpInfo->szVariableWidthFont);
		lpEncoding->iPropSize = lpInfo->nVariableWidthSize;
    	
		// Reset the assorted font caches...
		CDCCX::ClearAllFontCaches();
		CVirtualFontFontCache::Reset();
		theApp.m_pIntlFont->WriteToIniFile();

		// Indicate we need to reload all of the windows
		g_bReloadAllWindows = TRUE;
	}
	
	return NOERROR;
}

STDMETHODIMP
CAppearancePrefs::GetCurrentCharset(LPDWORD pdwCharsetNum)
{
	if (!pdwCharsetNum)
		return ResultFromScode(E_INVALIDARG);

	*pdwCharsetNum = (DWORD)m_nCharsetId;
	return NOERROR;
}

#ifdef MOZ_MAIL_NEWS
/////////////////////////////////////////////////////////////////////////////
// CMailNewsPreferences

class CMailNewsPreferences : public IMailNewsInterface {
	public:
		CMailNewsPreferences();
		~CMailNewsPreferences();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IMailNewsInterface methods
		STDMETHODIMP_(BOOL) CreateAddressBookCard(HWND hParent);
		STDMETHODIMP EditAddressBookCard(HWND hParent);

		STDMETHODIMP GetSigFile(HWND hParent, LPOLESTR *pszSigFile);
		STDMETHODIMP ShowDirectoryPicker(HWND hParent, LPOLESTR lpIniDir, LPOLESTR *pszNewDir);

		STDMETHODIMP ShowChooseFolder(HWND hParent, LPOLESTR lpFolderPath, DWORD dwType, 
						LPOLESTR *lpFolder, LPOLESTR *lpServer, LPOLESTR *lpPref);
		STDMETHODIMP GetFolderServer(LPOLESTR lpFolderPath, DWORD dwType, 
						LPOLESTR *lpFolder, LPOLESTR *lpServer);
		
		STDMETHODIMP FillNewsServerList(HWND hControl, LPOLESTR lpServerName);
		STDMETHODIMP GetNewsServerName(HWND hControl, LPOLESTR *lpServerName);
		STDMETHODIMP_(BOOL) AddNewsServer(HWND hParent, HWND hControl);
		STDMETHODIMP_(BOOL) EditNewsServer(HWND hParent, HWND hControl, BOOL *pChanged);
		STDMETHODIMP DeleteNewsServer(HWND hParent, HWND hControl);

		STDMETHODIMP_(BOOL) AddMailServer(HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType);
		STDMETHODIMP_(BOOL) EditMailServer(HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType);
		STDMETHODIMP_(BOOL) DeleteMailServer(HWND hParent, HWND hControl, DWORD dwType);

		STDMETHODIMP FillLdapDirList(HWND hControl, LPOLESTR lpServerName);
		STDMETHODIMP SetLdapDirAutoComplete(HWND hControl, BOOL bOnOrOff);

		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:

		ULONG	  m_uRef;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects

		BOOL m_bStaticCtl;
		int m_iInitialDepth;
		LPIMAGEMAP m_pIImageMap;
		LPUNKNOWN m_pIImageUnk;
		HFONT m_hFont, m_hBoldFont;
		XP_List	*m_pLdapServers;
		MSG_NewsHost** m_hNewsHost;
};

/////////////////////////////////////////////////////////////////////////////
// CMailNewsPreferences

CMailNewsPreferences::CMailNewsPreferences()
{
	m_uRef = 0;
	m_pCategory = NULL;
	m_pIImageMap = NULL;
	m_pIImageUnk = NULL;
	m_hFont = NULL;
	m_hBoldFont = NULL;
	m_pLdapServers = NULL;
	m_hNewsHost = NULL;

	char location[256];
	int nLen = 255;

	m_pLdapServers = XP_ListNew();
	PREF_GetDefaultCharPref("browser.addressbook_location", location, &nLen);
	DIR_GetServerPreferences (&m_pLdapServers, location);
}

CMailNewsPreferences::~CMailNewsPreferences()
{
	if (m_pIImageUnk && m_pIImageMap ) {
		m_pIImageUnk->Release();
	}
	if (m_hFont)
		theApp.ReleaseAppFont(m_hFont);
	if (m_hBoldFont)
		theApp.ReleaseAppFont(m_hBoldFont);
	if (m_pLdapServers)
	{
		DIR_DeleteServerList(m_pLdapServers);
		m_pLdapServers = NULL;
	}
	if (m_hNewsHost)
		delete [] m_hNewsHost;
	m_hNewsHost = NULL;
}

HRESULT
CMailNewsPreferences::Init()
{

	// Create the object as part of an aggregate
	return CoCreateInstance(CLSID_MailNewsPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CMailNewsPreferences::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_MailNewsInterface) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CMailNewsPreferences::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CMailNewsPreferences::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}


STDMETHODIMP_(BOOL)
CMailNewsPreferences::CreateAddressBookCard(HWND hParent)
{
	char* email = NULL;
	char* name = NULL;
	PersonEntry person;
	person.Initialize();
	ABID entryID;
	DIR_Server *pab = NULL;
	CWnd parent;
	BOOL result = TRUE;

#ifdef FEATURE_ADDRPROP
	parent.Attach(hParent);

	if (FE_UsersMailAddress())
		email = XP_STRDUP (FE_UsersMailAddress());
	if (FE_UsersFullName())
		name = XP_STRDUP (FE_UsersFullName());
	person.pGivenName = name;
	person.pEmailAddress = email;
	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);

	if (pab) 
	{
		AB_GetEntryIDForPerson(pab, theApp.m_pABook, &entryID, &person);

		if (entryID == MSG_MESSAGEIDNONE) 
		{
			CDialog createCardDlg(IDD_PREF_MNCREATECARD, NULL);
			if (IDOK == createCardDlg.DoModal())
			{
				CString formattedString;
				if (name)
					formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), name);
				else
					formattedString = XP_GetString (MK_ADDR_NEW_CARD);
				CAddrEditProperties prop(NULL, pab,
					(const char *) formattedString, &parent, 0, &person);
				prop.SetCurrentPage(0);
				if (prop.DoModal() == IDCANCEL)
					result = FALSE;
			}
			else
			{
				result = FALSE;
			}
		}
	}
	parent.Detach();
#endif
	return result;
}

STDMETHODIMP
CMailNewsPreferences::EditAddressBookCard(HWND hParent)
{
#ifdef FEATURE_ADDRPROP
	char* email = NULL;
	char* name = NULL;
	PersonEntry person;
	person.Initialize();
	ABID entryID;
	DIR_Server *pab = NULL;
	CWnd parent;

	parent.Attach(hParent);

	if (FE_UsersMailAddress())
		email = XP_STRDUP (FE_UsersMailAddress());
	if (FE_UsersFullName())
		name = XP_STRDUP (FE_UsersFullName());
	person.pGivenName = name;
	person.pEmailAddress = email;
	DIR_GetPersonalAddressBook (theApp.m_directories, &pab);
	XP_ASSERT (pab);

	if (pab) 
	{
		AB_GetEntryIDForPerson(pab, theApp.m_pABook, &entryID, &person);

		if (entryID != MSG_MESSAGEIDNONE) 
		{
			CString formattedString;
			char fullname [kMaxFullNameLength];
			AB_GetFullName(pab, theApp.m_pABook, entryID, fullname);
			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), fullname);
			CAddrEditProperties prop(NULL, pab,
				(const char *) formattedString, &parent, entryID);
			prop.SetCurrentPage(0);
			prop.DoModal();
		}
		else 
		{
			CString formattedString;
			formattedString.Format(XP_GetString (MK_ADDR_BOOK_CARD), name);
			CAddrEditProperties prop(NULL, pab,
				(const char *) formattedString, &parent, 0, &person);
			prop.SetCurrentPage(0);
			prop.DoModal();
		}
	}

	if (email)
			XP_FREE (email);
	if (name)
			XP_FREE (name);
	parent.Detach();
#endif
	return NOERROR;
}

STDMETHODIMP
CMailNewsPreferences::GetSigFile(HWND hParent, LPOLESTR *lpoleStr)
{
    char* pName = NULL;

	if (!lpoleStr)
		return ResultFromScode(E_INVALIDARG);
	
	pName = wfe_GetExistingFileName(hParent, szLoadString(IDS_SIG_FILE), 
		                            ALL, TRUE);
	*lpoleStr = 0;

	if (pName)
	{
		*lpoleStr = AllocTaskOleString(pName);
		
		if (!*lpoleStr)
			return ResultFromScode(E_OUTOFMEMORY);
	}

	return NOERROR;
}

STDMETHODIMP CMailNewsPreferences::ShowDirectoryPicker
(HWND hParent, LPOLESTR lpIniDir, LPOLESTR *pszNewDir)
{
	CWnd parent;
	parent.Attach(hParent);

	*pszNewDir = 0;

	CDirDialog directory(&parent, (char*)lpIniDir);
	if (IDOK == directory.DoModal())
	{
        CString fileName = directory.GetPathName();
		int nPos = fileName.GetLength() - strlen("\\k5bg");
        CString pathName(fileName.Left(nPos));

		*pszNewDir = AllocTaskOleString(LPCTSTR(pathName));
		
		if (!*pszNewDir)
			return ResultFromScode(E_OUTOFMEMORY);
	}
	parent.Detach();
	return NOERROR;
}

STDMETHODIMP CMailNewsPreferences::ShowChooseFolder
(HWND hParent, LPOLESTR lpFolderPath, DWORD dwType, LPOLESTR *lpFolder, 
 LPOLESTR *lpServer, LPOLESTR *lpPref)
{
	char* lpPath = (char*)lpFolderPath;
	int nDefaultID;
	if (dwType == TYPE_SENT)
		nDefaultID = MK_MSG_SENT_L10N_NAME;
	else if (dwType == TYPE_DRAFT)
		nDefaultID = MK_MSG_DRAFTS_L10N_NAME;
	else if (dwType == TYPE_TEMPLATE)
		nDefaultID = MK_MSG_TEMPLATES_L10N_NAME;
	CWnd parent;
	parent.Attach(hParent);
	CChooseFolderDialog folderDialog(&parent, lpPath, nDefaultID);
	if (IDOK == folderDialog.DoModal())
	{
	 	*lpFolder = AllocTaskOleString(LPCTSTR(folderDialog.m_szFolder));
	 	*lpServer = AllocTaskOleString(LPCTSTR(folderDialog.m_szServer));
	 	*lpPref = AllocTaskOleString(LPCTSTR(folderDialog.m_szPrefUrl));
	}

	parent.Detach();
	return NOERROR;
}


STDMETHODIMP CMailNewsPreferences::GetFolderServer
(LPOLESTR lpFolderPath, DWORD dwType, LPOLESTR *lpFolder, LPOLESTR *lpServer)
{
	
	char* lpPath = (char*)lpFolderPath;
	int nDefaultID;
	if (dwType == TYPE_SENT)
		nDefaultID = MK_MSG_SENT_L10N_NAME;
	else if (dwType == TYPE_DRAFT)
		nDefaultID = MK_MSG_DRAFTS_L10N_NAME;
	else if (dwType == TYPE_TEMPLATE)
		nDefaultID = MK_MSG_TEMPLATES_L10N_NAME;

	CString folder;
	CString server;

	GetFolderServerNames(lpPath, nDefaultID, folder, server);

	if (folder.GetLength())
		*lpFolder = AllocTaskOleString(LPCTSTR(folder));
	if (server.GetLength())
		*lpServer = AllocTaskOleString(LPCTSTR(server));

	return NOERROR;
}

STDMETHODIMP CMailNewsPreferences::FillNewsServerList(HWND hControl, LPOLESTR lpServerName)
{
	int nDefaultHost = 0;
	int nSelectedIndex = -1;
	char* lpPrefName = (char*)lpServerName;

 	MSG_Master * pMaster = WFE_MSGGetMaster();
	int32 nTotal =  MSG_GetNewsHosts(pMaster, NULL, 0);
	if (nTotal)
	{
		m_hNewsHost = new MSG_NewsHost* [nTotal];
		MSG_GetNewsHosts(pMaster, m_hNewsHost, nTotal);
		for (int i = 0; i < nTotal; i++)
		{
			int nAddedIndex = (int)SendMessage(hControl, LB_ADDSTRING, 0, 
				           (LPARAM)(LPCTSTR)MSG_GetNewsHostUIName(m_hNewsHost[i]));
			if (nAddedIndex != CB_ERR)
			{
				SendMessage(hControl, LB_SETITEMDATA, (WPARAM)nAddedIndex, 
				            (LPARAM)(DWORD)m_hNewsHost[i]);
				const char *pName = MSG_GetNewsHostUIName(m_hNewsHost[i]);
				if (!XP_FILENAMECMP(lpPrefName, pName))
					nSelectedIndex = nAddedIndex;
			}
		}
		if (nSelectedIndex >= 0)
			SendMessage(hControl, LB_SETCURSEL, (WPARAM)nSelectedIndex, (LPARAM)0);
	}	
	return NOERROR;
}

STDMETHODIMP 
CMailNewsPreferences::GetNewsServerName(HWND hControl, LPOLESTR *lpServerName)
{
	int nIndex = (int)SendMessage(hControl, LB_GETCURSEL, 0,0);
	if (nIndex != CB_ERR)
	{
		MSG_NewsHost* pServer = (MSG_NewsHost*)SendMessage(hControl, LB_GETITEMDATA, 
											(WPARAM)nIndex, 0);
		*lpServerName = AllocTaskOleString(MSG_GetHostUIName((MSG_Host*)pServer));
	}
	return NOERROR;
}

STDMETHODIMP_(BOOL)
CMailNewsPreferences::AddNewsServer(HWND hParent, HWND hControl)
{
	BOOL result = FALSE;
	CWnd parent;
	parent.Attach(hParent);

	MSG_Host *pNewHost = DoAddNewsServer(&parent, FROM_PREFERENCE);
	if (pNewHost)
	{
		int nIndex = (int)SendMessage(hControl, LB_ADDSTRING, 0, 
				          (LPARAM)(LPCTSTR)MSG_GetHostUIName(pNewHost));
		SendMessage(hControl, LB_SETCURSEL, (WPARAM)nIndex, (LPARAM)0);
		SendMessage(hControl, LB_SETITEMDATA, (WPARAM)nIndex, 
				    (LPARAM)(DWORD)pNewHost);
		result = TRUE;
	}
	else
		result = FALSE;
	parent.Detach();
	return result;
}

STDMETHODIMP_(BOOL)
CMailNewsPreferences::EditNewsServer(HWND hParent, HWND hControl, BOOL *pChanged)
{
    CWnd parent;

	int nIndex = (int)SendMessage(hControl, LB_GETCURSEL, 0, 0);
	if (nIndex >= 0)
	{
		parent.Attach(hParent);
		MSG_Host* pServer = (MSG_Host*)SendMessage(hControl, LB_GETITEMDATA, 
													(WPARAM)nIndex, 0);

		CNewsServerDialog addServerDialog(&parent, MSG_GetHostUIName(pServer), 
										FROM_PREFERENCE, (MSG_NewsHost*)pServer);
		if (IDOK == addServerDialog.DoModal())
		{
			char* pName = addServerDialog.GetNewsHostName();
			XP_Bool bSecure = addServerDialog.GetSecure();
			XP_Bool bAuthentication = addServerDialog.GetAuthentication();
			int32 nPort = addServerDialog.GetNewsHostPort();

			// set the new value
			MSG_SetNewsHostPushAuth ((MSG_NewsHost*)pServer, bAuthentication);

			*pChanged = TRUE;
			parent.Detach();
			return TRUE;
		}
		else
		{
			parent.Detach();
			return FALSE;
		}
	}
	return FALSE;
}

STDMETHODIMP CMailNewsPreferences::DeleteNewsServer(HWND hParent, HWND hControl)
{
	
    CWnd parent;
	parent.Attach(hParent);
	int nIndex = (int)SendMessage(hControl, LB_GETCURSEL, 0, 0);
	if (nIndex >= 0)
	{
		MSG_Host* pServer = (MSG_Host*)SendMessage(hControl, LB_GETITEMDATA, 
													(WPARAM)nIndex, 0);
		char* tmp = PR_smprintf(XP_GetString(MK_MSG_REMOVE_HOST_CONFIRM),
								MSG_GetHostUIName(pServer));
		if (IDOK == parent.MessageBox(tmp, szLoadString(AFX_IDS_APP_TITLE), MB_OKCANCEL))
		{
 			MSG_Master * pMaster = WFE_MSGGetMaster();
			MSG_DeleteNewsHost(pMaster, (MSG_NewsHost*)pServer);
			SendMessage(hControl, LB_DELETESTRING, nIndex, 0);
		}
		if(tmp && tmp[0])
			XP_FREE(tmp);
	}
	parent.Detach();
	
	return NOERROR;
}

STDMETHODIMP_(BOOL) CMailNewsPreferences::AddMailServer
(HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType)
{
	CWnd parent;
	parent.Attach(hParent);
	int nType = (int)dwType;

	if (nType == TYPE_POP && !bAllowBoth)
	{
		parent.MessageBox(szLoadString(IDS_POP3_ONLY_ONE), 
										szLoadString(AFX_IDS_APP_TITLE), MB_OK);
		parent.Detach();
		return FALSE;
	}

	CString title;
	title.LoadString(IDS_ADD_MAIL_SERVER);
    CMailServerPropertySheet addMailServer(&parent, title, NULL, TYPE_IMAP, FALSE, bAllowBoth);
    if (IDOK == addMailServer.DoModal())
	{
		PREF_SavePrefFile();
		if (nType == TYPE_POP)
		{
			SendMessage(hControl, LB_ADDSTRING, 0, 
				(LPARAM) (LPCTSTR)addMailServer.GetMailHostName());
		}
		parent.Detach();
		return TRUE;
	}
	else
	{
		parent.Detach();
		return FALSE;
	}
}

STDMETHODIMP_(BOOL) CMailNewsPreferences::EditMailServer
(HWND hParent, HWND hControl, BOOL bAllowBoth, DWORD dwType)
{
    CWnd parent;
	parent.Attach(hParent);
	CString title;
	title.LoadString(IDS_MAIL_SERVER_PROPERTY);
	int nType = (int)dwType;

	char serverName[MSG_MAXGROUPNAMELENGTH];
	int nIndex = (int)SendMessage(hControl, LB_GETCURSEL, 0, 0);
	SendMessage(hControl, LB_GETTEXT, (WPARAM)nIndex, 
			(LPARAM)(DWORD)serverName);

    CMailServerPropertySheet editMailServer(&parent, title, serverName,
											nType, TRUE, bAllowBoth);
    if (IDOK == editMailServer.DoModal())
	{
		PREF_SavePrefFile();
		parent.Detach();
		return TRUE;
	}
	else
	{
		parent.Detach();
		return FALSE;
	}
}

//delete imap server
STDMETHODIMP_(BOOL) CMailNewsPreferences::DeleteMailServer
(HWND hParent, HWND hControl, DWORD dwType)
{
	CWnd parent;
	parent.Attach(hParent);
	if (IDCANCEL == parent.MessageBox(szLoadString(IDS_REMOVE_MAILHOST_CONFIRM),
									szLoadString(AFX_IDS_APP_TITLE), MB_OKCANCEL))
	{
		parent.Detach();
		return FALSE;
	}

	int nIndex = (int)SendMessage(hControl, LB_GETCURSEL, 0, 0);
	int nType = (int)dwType;
	if (nType = TYPE_IMAP)
	{
		char serverName[MSG_MAXGROUPNAMELENGTH];

		MSG_Master* pMaster = WFE_MSGGetMaster();

		SendMessage(hControl, LB_GETTEXT, (WPARAM)nIndex, 
				(LPARAM)(DWORD)serverName);

		int nTotal =  MSG_GetIMAPHosts(pMaster, NULL, 0);
		if (nTotal)
		{
 			MSG_IMAPHost** hImapHost = NULL;
			hImapHost = new MSG_IMAPHost* [nTotal];
			ASSERT(hImapHost != NULL);
			nTotal =  MSG_GetIMAPHosts(pMaster, hImapHost, nTotal);
			for (int i = 0; i < nTotal; i++)
			{
	            MSG_Host* pMsgHost = MSG_GetMSGHostFromIMAPHost(hImapHost[i]);
		        if (0 == lstrcmp(serverName, MSG_GetHostName(pMsgHost)))
		            MSG_DeleteIMAPHost(pMaster, hImapHost[i]);
			}
			if (hImapHost)
				delete [] hImapHost;
		}
	}
	else
		SendMessage(hControl, LB_DELETESTRING, (WPARAM)nIndex, 0);

	parent.Detach();
	return TRUE;
}

STDMETHODIMP CMailNewsPreferences::FillLdapDirList(HWND hControl, LPOLESTR lpServerName)
{
	if (0 == XP_ListCount(m_pLdapServers))
		return NOERROR;
	
	DIR_Server* pServer;
	//Keep m_pLdapServers, XP_ListNextObject will reset pointer
	XP_List	*pList = m_pLdapServers;
	int nSelectedIndex = 0;
	while (pServer = (DIR_Server*)XP_ListNextObject(pList))
	{
		if (pServer->description && pServer->dirType == LDAPDirectory)
		{	
			int nIndex = (int)SendMessage(hControl, CB_ADDSTRING, 0, 
				                          (LPARAM)(LPCTSTR)pServer->description);
			if (nIndex >= 0)
			{
				SendMessage(hControl, CB_SETITEMDATA, (WPARAM)nIndex, 
				            (LPARAM)(DWORD)pServer);
				if (DIR_TestFlag(pServer, DIR_AUTO_COMPLETE_ENABLED))
					nSelectedIndex = nIndex;
			}
		}	
	}	
	SendMessage(hControl, CB_SETCURSEL, nSelectedIndex, 0);
	return NOERROR;
}

STDMETHODIMP CMailNewsPreferences::SetLdapDirAutoComplete(HWND hControl, BOOL bOnOrOff)
{
	int nIndex = (int)SendMessage(hControl, CB_GETCURSEL, 0,0);
	DIR_Server* pServer = (DIR_Server*)SendMessage(hControl, CB_GETITEMDATA, 
										(WPARAM)nIndex, 0);
	DIR_SetAutoCompleteEnabled(m_pLdapServers, pServer, bOnOrOff);

	return NOERROR;
}
#endif /* MOZ_MAIL_NEWS */

#ifdef MOZ_OFFLINE
/////////////////////////////////////////////////////////////////////////////
// COfflinePreference

class COfflinePreference : public IOfflineInterface {
	public:
		COfflinePreference();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// IIntlFont methods
		STDMETHODIMP DoSelectDiscussion(HWND hParent);

		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:

		ULONG	  m_uRef;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
};

/////////////////////////////////////////////////////////////////////////////
// COfflinePreference

COfflinePreference::COfflinePreference()
{
	m_uRef = 0;
	m_pCategory = NULL;
}

HRESULT
COfflinePreference::Init()
{
	// Create the object as part of an aggregate
	return CoCreateInstance(CLSID_OfflinePrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
COfflinePreference::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_OfflineInterface) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
COfflinePreference::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
COfflinePreference::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}

STDMETHODIMP
COfflinePreference::DoSelectDiscussion(HWND hParent)
{
	CWnd parent;

	parent.Attach(hParent);

	CDlgSelectGroups selDiscussionDlg(&parent);
	selDiscussionDlg.DoModal();

	parent.Detach();
	return NOERROR;
}
#endif // MOZ_OFFLINE

#ifdef MOZ_MAIL_NEWS
static BOOL
CreateMailNewsCategory(LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	BOOL	bResult = FALSE;

	// Create the property page providers
	CMailNewsPreferences *pMailNews = new CMailNewsPreferences;
	pMailNews->AddRef();

	// Initialize the mail news object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pMailNews->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pMailNews->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	
	// We're all done with the object
	pMailNews->Release();
	return bResult;
}
#endif /* MOZ_MAIL_NEWS */


#ifdef MOZ_LOC_INDEP
/////////////////////////////////////////////////////////////////////////////
// CLIPreference

class CLIPreference : public ILIPrefs {
	public:
		CLIPreference();

		// IUnknown methods
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR* ppvObj);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		
		// Initialization routine to create contained and aggregated objects
		HRESULT		 Init();

	private:

		ULONG	  m_uRef;
		LPUNKNOWN m_pCategory;  // inner object supporting ISpecifyPropertyPageObjects
};

/////////////////////////////////////////////////////////////////////////////
// CLIPreference

CLIPreference::CLIPreference()
{
	m_uRef = 0;
	m_pCategory = NULL;
}

HRESULT
CLIPreference::Init()
{
	// Create the object as part of an aggregate
	return CoCreateInstance(CLSID_LIPrefs, (LPUNKNOWN)this,
		CLSCTX_INPROC_SERVER, IID_IUnknown, (LPVOID *)&m_pCategory);
}

STDMETHODIMP
CLIPreference::QueryInterface(REFIID riid, LPVOID FAR* ppvObj)
{
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_ILIlogin) {
		*ppvObj = (LPVOID)this;
		AddRef();
		return NOERROR;

	} else if (riid == IID_ISpecifyPropertyPageObjects) {
		assert(m_pCategory);
		return m_pCategory->QueryInterface(riid, ppvObj);
	}

	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG)
CLIPreference::AddRef()
{
	return ++m_uRef;
}

STDMETHODIMP_(ULONG)
CLIPreference::Release()
{
	if (--m_uRef == 0) {
		if (m_pCategory)
			m_pCategory->Release();
		delete this;
		return 0;
	}

	return m_uRef;
}
#endif /* MOZ_LOC_INDEP */


static void
ReloadAllWindows()
{
	for (CGenericFrame *f = theApp.m_pFrameList; f; f = f->m_pNext) {
		CWinCX *pContext = f->GetMainWinContext();

		if (pContext && pContext->GetContext()) {
#ifdef EDITOR 
			if (EDT_IS_EDITOR(pContext->GetContext())) {
				// Edit can relayout page without having to do NET_GetURL
				EDT_RefreshLayout(pContext->GetContext());

			} else 
#endif // EDITOR
            {
				pContext->NiceReload();
			}
		}
	}
}

static BOOL
CreateAppearancesCategory(int nCharsetId, LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	CAppearancePrefs   *pAppearance = new CAppearancePrefs(nCharsetId);
	BOOL		   		bResult = FALSE;
	
	pAppearance->AddRef();

	// Initialize the appearances object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pAppearance->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pAppearance->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}

	// We're all done with the object
	pAppearance->Release();
	return bResult;
}

static BOOL
CreateBrowserCategory(MWContext *pContext, LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	CBrowserPrefs *pBrowser;
	History_entry *pEntry;
	BOOL		   bResult = FALSE;

	// If this is the browser then use the URL for the current page
	pEntry = pContext->type == MWContextBrowser ? pContext->hist.cur_doc_ptr : NULL;
	pBrowser = new CBrowserPrefs(pEntry ? pEntry->address : NULL);
	pBrowser->AddRef();
	
	// Initialize the browser pref object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pBrowser->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pBrowser->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}

	// We're all done with the object
	pBrowser->Release();
	return bResult;
}


#ifdef MOZ_OFFLINE		
static BOOL
CreateOfflineCategory(LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	BOOL	bResult = FALSE;
		
	// Create the property page providers
	COfflinePreference *pOffline = new COfflinePreference;
	pOffline->AddRef();

	// Initialize the offline object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pOffline->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pOffline->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	
	// We're all done with the object
	pOffline->Release();
	return bResult;
}
#endif /* MOZ_OFFLINE */


#ifdef MOZ_LOC_INDEP
static BOOL
CreateLICategory(LPSPECIFYPROPERTYPAGEOBJECTS *pCategory)
{
	BOOL	bResult = FALSE;
		
	// Create the property page providers
	CLIPreference *pLI = new CLIPreference;
	pLI->AddRef();

	// Initialize the offline object. This allows it to load any objects that are
	// contained or aggregated
	if (SUCCEEDED(pLI->Init())) {
		// Get the interface pointer for ISpecifyPropertyPageObjects
		if (SUCCEEDED(pLI->QueryInterface(IID_ISpecifyPropertyPageObjects, (LPVOID *)pCategory)))
			bResult = TRUE;
	}
	
	// We're all done with the object
	pLI->Release();
	return bResult;
}
#endif // MOZ_LOC_INDEP

typedef HRESULT (STDAPICALLTYPE *PFNPREFS)(HWND, int, int, LPCSTR, ULONG, LPSPECIFYPROPERTYPAGEOBJECTS *, ULONG, NETHELPFUNC);

// Callback for prefs to display a NetHelp window
void CALLBACK
#ifndef _WIN32
__export
#endif
NetHelpWrapper(LPCSTR lpszHelpTopic)
{
    // Call internal NetHelp routine
    NetHelp(lpszHelpTopic);
}

void
wfe_DisplayPreferences(CGenericFrame *pFrame)
{
	HINSTANCE					 	hPrefDll;
	LPSPECIFYPROPERTYPAGEOBJECTS	categories[6];
	PFNPREFS					 	pfnCreatePropertyFrame;
	ULONG						 	nCategories = 0;
	ULONG						 	nInitialCategory;
	MWContext	  				   *pContext = pFrame->GetMainContext()->GetContext();
	static CGenericFrame		   *pCurrentFrame = NULL;

	// We only allow one pref UI window up at a time
	if (pCurrentFrame) {
		HWND	hwnd = ::GetLastActivePopup(pCurrentFrame->m_hWnd);

#ifdef _WIN32
		::SetForegroundWindow(hwnd);
#else
		::BringWindowToTop(hwnd);
#endif
		return;
	}

	// Load the preferences UI DLL
#ifdef _WIN32
	hPrefDll = LoadLibrary("prefui32.dll");
	if (hPrefDll) {
#else
	hPrefDll = LoadLibrary("prefui16.dll");
	if ((UINT)hPrefDll > 32) {
#endif

		// Get the address of the routine to create the property frame
		pfnCreatePropertyFrame = (PFNPREFS)GetProcAddress(hPrefDll, "NS_CreatePropertyFrame");
		if (!pfnCreatePropertyFrame) {
#ifdef _DEBUG
			pFrame->MessageBox("Unable to get proc address for NS_CreatePropertyFrame",
				NULL, MB_OK | MB_ICONEXCLAMATION);
#endif
			FreeLibrary(hPrefDll);
			return;
		}
		
		// Remember that this window is displaying the pref UI
		pCurrentFrame = pFrame;

		// Appearances category
		if (CreateAppearancesCategory(theApp.m_pIntlFont->DocCSIDtoID(pFrame->m_iCSID), &categories[nCategories]))
			nCategories++;

		// Browser category
		if (CreateBrowserCategory(pContext, &categories[nCategories]))
			nCategories++;

#ifdef MOZ_MAIL_NEWS
		// Mail and News category
		if (CreateMailNewsCategory(&categories[nCategories]))
			nCategories++;
#endif // MOZ_MAIL_NEWS

#ifdef MOZ_LOC_INDEP
		// LI category
		if (CreateLICategory(&categories[nCategories]))
			nCategories++;
#endif // MOZ_LOC_INDEP		

#ifdef EDITOR		
		// Editor category
		if (SUCCEEDED(CoCreateInstance(CLSID_EditorPrefs,
			                           NULL,
									   CLSCTX_INPROC_SERVER,
									   IID_ISpecifyPropertyPageObjects,
									   (LPVOID *)&categories[nCategories]))) {
			nCategories++;
		}
#endif EDITOR

#ifdef MOZ_OFFLINE
		// Offline category
		if (CreateOfflineCategory(&categories[nCategories]))
			nCategories++;
#endif // MOZ_OFFLINE		

		// Advanced category
		if (SUCCEEDED(CoCreateInstance(CLSID_AdvancedPrefs,
			                           NULL,
									   CLSCTX_INPROC_SERVER,
									   IID_ISpecifyPropertyPageObjects,
									   (LPVOID *)&categories[nCategories]))) {
			nCategories++;
		}

        // Make sure we have at least one category
		if (nCategories == 0) {
            pFrame->MessageBox(szLoadString(IDS_CANT_LOAD_PREFS), NULL, MB_OK | MB_ICONEXCLAMATION);
			FreeLibrary(hPrefDll);
			return;
		}
		
        // Hack to indicate whether we need to reload the windows because something
		// like a color or a font changed
		//
		// We do have code that observes the XP prefs, but if more than one preference
		// changes we'll get several callbacks (we don't have a way to do batch changes)
		// and we'll reload the windows multiple times which is very sloppy...
		g_bReloadAllWindows = FALSE;

		// Figure out what the initial category we display should be
		if (pFrame->IsEditFrame()) {
			nInitialCategory = 3;

		} else if (pContext->type == MWContextMail ||
				 pContext->type == MWContextMailMsg ||
				 pContext->type == MWContextMessageComposition ||
				 pContext->type == MWContextAddressBook) {
			nInitialCategory = 2;

		} else {
			nInitialCategory = 1;
		}
		
		// Display the preferences
		pfnCreatePropertyFrame(pFrame->m_hWnd, -1, -1, "Preferences", nCategories, categories,
            nInitialCategory, NetHelpWrapper);
		pCurrentFrame = NULL;

		// Release the interface pointers for the preference categories
		for (ULONG i = 0; i < nCategories; i++)
			categories[i]->Release();

		// Unload the library
		FreeLibrary(hPrefDll);

		// See if we need to reload the windows
		if (g_bReloadAllWindows) {
			ReloadAllWindows();
			g_bReloadAllWindows = FALSE;
		}

		// Save out the preferences
		PREF_SavePrefFile();

	} else {
#ifdef _DEBUG
		MessageBox(NULL, "Unable to load NSPREFUI.DLL", "Navigator",
			MB_OK | MB_ICONEXCLAMATION);
#endif
	}
}


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
#ifndef __IBRPREFS_H_
#define __IBRPREFS_H_

#include "net.h"

/////////////////////////////////////////////////////////////////////////////
// IBrowserPrefs::EnumHelpers member returns an IEnumHelpers object.

#ifdef __cplusplus
interface IEnumHelpers;
#else
typedef interface IEnumHelpers IEnumHelpers;
#endif

typedef IEnumHelpers FAR* LPENUMHELPERS;

#undef  INTERFACE
#define INTERFACE       IEnumHelpers

DECLARE_INTERFACE_(IEnumHelpers, IUnknown)
{
    // *** IUnknown methods ***
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;

    // *** IEnumHelpers methods ***
    STDMETHOD(Next)  (THIS_ NET_cdataStruct **ppcdata) PURE;
    STDMETHOD(Reset) (THIS) PURE;
};

/////////////////////////////////////////////////////////////////////////////
// IBrowserPrefs interface

#ifdef __cplusplus
interface IBrowserPrefs;
#else
typedef interface IBrowserPrefs IBrowserPrefs;
#endif

typedef IBrowserPrefs FAR* LPBROWSERPREFS;

#undef  INTERFACE
#define INTERFACE IBrowserPrefs

typedef struct _HELPERINFO {
	int		nHowToHandle;  // one of HANDLE_VIA_NETSCAPE .. HANDLE_SHELLEXECUTE
	char	szOpenCmd[_MAX_PATH + 32];
	BOOL	bAskBeforeOpening;
	LPCSTR	lpszMimeType;  // not used for GetHelperInfo (only for SetHelperInfo)
	BOOL	bIsLocked; //TRUE if this helper is associated with a mime type from the prefs file and it's locked....CRN_MIME
} HELPERINFO, FAR *LPHELPERINFO;

// IBrowserPrefs provides the preference UI code with a way to get at
// various state maintained by the Navigator
DECLARE_INTERFACE_(IBrowserPrefs, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IBrowserPrefs methods
	STDMETHOD(GetCurrentPage)(THIS_ LPOLESTR *pszPage) PURE;

	STDMETHOD(EnumHelpers)(THIS_ LPENUMHELPERS *ppEnumHelpers) PURE;
	STDMETHOD(GetHelperInfo)(THIS_ NET_cdataStruct *, LPHELPERINFO) PURE;
	STDMETHOD(SetHelperInfo)(THIS_ NET_cdataStruct *, LPHELPERINFO) PURE;
	STDMETHOD(NewFileType)(THIS_
						   LPCSTR lpszDescription,
						   LPCSTR lpszExtension,
						   LPCSTR lpszMimeType,
						   LPCSTR lpszOpenCmd,
						   NET_cdataStruct **ppcdata) PURE;
	STDMETHOD(RemoveFileType)(THIS_ NET_cdataStruct *) PURE;
};


/////////////////////////////////////////////////////////////////////////////
// IAdvancedPrefs interface

#ifdef __cplusplus
interface IAdvancedPrefs;
#else
typedef interface IAdvancedPrefs IAdvancedPrefs;
#endif

typedef IAdvancedPrefs FAR* LPADVANCEDPREFS;

#undef  INTERFACE
#define INTERFACE IAdvancedPrefs

DECLARE_INTERFACE_(IAdvancedPrefs, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;
};

#ifdef MOZ_SMARTUPDATE

/////////////////////////////////////////////////////////////////////////////
// ISmartUpdatePrefs interface

#ifdef __cplusplus
interface ISmartUpdatePrefs;
#else
typedef interface ISmartUpdatePrefs ISmartUpdatePrefs;
#endif

typedef ISmartUpdatePrefs FAR* LPSMARTUPDATEPREFS;

#undef  INTERFACE
#define INTERFACE ISmartUpdatePrefs

typedef struct _PACKAGEINFO {
	char	userPackageName[MAX_PATH];  
    char	regPackageName[MAX_PATH];
} PACKAGEINFO, FAR *LPPACKAGEINFO;

// IBrowserPrefs provides the preference UI code with a way to get at
// various state maintained by the Navigator
DECLARE_INTERFACE_(ISmartUpdatePrefs, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IBrowserPrefs methods
	STDMETHOD_(LONG,RegPack)(THIS) PURE;
    STDMETHOD_(LONG,Uninstall)(THIS_ char* regPackageName) PURE;
    STDMETHOD_(LONG,EnumUninstall)(THIS_ void** context, char* packageName,
                                    LONG len1, char*regPackageName, LONG len2) PURE;

};

#endif /* MOZ_SMARTUPDATE */

#endif /* __IBRPREFS_H_ */


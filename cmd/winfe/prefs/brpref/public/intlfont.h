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

#ifndef __INTLFONT_H_
#define __INTLFONT_H_

/////////////////////////////////////////////////////////////////////////////
// IIntlFont interface

#ifdef __cplusplus
interface IIntlFont;
#else
typedef interface IIntlFont IIntlFont;
#endif

typedef IIntlFont FAR* LPINTLFONT;

#undef  INTERFACE
#define INTERFACE IIntlFont

typedef struct _ENCODINGINFO {
	char	szVariableWidthFont[LF_FACESIZE];
	int		nVariableWidthSize;
	char	szFixedWidthFont[LF_FACESIZE];
	int		nFixedWidthSize;
	BOOL	bIgnorePitch;
} ENCODINGINFO, FAR *LPENCODINGINFO;

// IIntlFont provides the preference UI code with a way to get the number
// of encodings, and for each encoding the encoding name and encoding info
DECLARE_INTERFACE_(IIntlFont, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IIntlFont methods
	STDMETHOD(GetNumEncodings)(THIS_ LPDWORD pdwEncodings) PURE;
	STDMETHOD(GetEncodingName)(THIS_ DWORD dwCharsetNum, LPOLESTR *pszName) PURE;
	STDMETHOD(GetEncodingInfo)(THIS_ DWORD dwCharsetNum, LPENCODINGINFO lpInfo) PURE;
	STDMETHOD(SetEncodingFonts)(THIS_ DWORD dwCharsetNum, LPENCODINGINFO lpInfo) PURE;
	STDMETHOD(GetCurrentCharset)(THIS_ LPDWORD pdwCharsetNum) PURE;
};

#endif /* __INTLFONT_H_ */


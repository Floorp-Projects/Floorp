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

#ifndef __CSysInfo_H
#define __CSysInfo_H

class CSysInfo	{
//	Classification of the operating system.
public:
	BOOL m_bWin16;	//	3.1, WFW
	BOOL m_bWin32;	//	Win32s, WinNT, Win95
	BOOL m_bWin32s;
	BOOL m_bWinNT;
	BOOL m_bWin4;
	BOOL m_bDBCS;   // double byte enable

//	Need an easy way to determine if under 4.0 and
//		if a 32 bit app.
public:
	BOOL IsWin4_32() const	{
		return(m_bWin32 == TRUE && m_bWin4 == TRUE);
	}
	BOOL IsWin4_16() const	{
		return(m_bWin16 == TRUE && m_bWin4 == TRUE);
	}
	BOOL IsWin4() const	{
		TRACE("Consider using IsWin4_32.\n");
		return(m_bWin4 == TRUE);
	}

//	Versioning information.
//	These are (DWORD)-1 if not initalized.
public:
	DWORD m_dwMajor;
	DWORD m_dwMinor;
	DWORD m_dwBuild;

//	Cached colors, brushes, whatever.
//	Usually defined by the OS.
public:
	HBRUSH	m_hbrBtnFace;
	HBRUSH	m_hbrMenu;
	HBRUSH  m_hbrHighlight;

	COLORREF m_clrBtnFace;
	COLORREF m_clrBtnShadow;
	COLORREF m_clrBtnHilite;
	COLORREF m_clrBtnText;
	COLORREF m_clrMenu;
	COLORREF m_clrHighlight;

// Bits per pixel
	int m_iBitsPerPixel;

//	Winsock information.
public:
	WSADATA m_wsaData;
    int m_iMaxSockets;

//	Various window attributes.
//	Such as the border size (WS_BORDER).
public:
	int m_iBorderWidth;
	int m_iBorderHeight;
	int m_iScrollWidth;
	int m_iScrollHeight;

//  Screen size information.
public:
    int m_iScreenWidth;
    int m_iScreenHeight;

//  Is a printer installed?
public:
    BOOL m_bPrinter;

// Do we need to override size for Win95 tooltips?
public:
	BOOL m_bOverrideWin95Tooltips;

//	Init/Reinit/Destroy...
public:
	CSysInfo();
	~CSysInfo();
	void UpdateInfo();

//  Watchers.
private:
    void *m_pWatchSettingChanges;
    void *m_pWatchDeviceModeChanges;
    void *m_pWatchDeviceChanges;

protected:
	BOOL OverrideWin95ToolTips(void);
};

extern CSysInfo sysInfo;

#endif // __CSysInfo_H

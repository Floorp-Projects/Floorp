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
#include "slavewnd.h"

//  setting/winini/device change callback.
void UpdateSystemInfo(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //  Update any info we might have.
    sysInfo.UpdateInfo();
}

//  Global must be present early, as other globals may want
//      to register early on.
#pragma warning(disable:4073)
#pragma init_seg(lib)
CSysInfo sysInfo;
#pragma warning(default:4073)

CSysInfo::CSysInfo()	{
	//	Flag the method in which we'll attempt to determine
	//		the operating system.
	BOOL bUseGetversion = FALSE;

	//	Init variants we'd like to detect.
	m_bWin32s = FALSE;
	m_bWinNT = FALSE;
	m_bWin4 = FALSE;

	//	Init os version major, minor, build.
	m_dwMajor = (DWORD)-1;
	m_dwMinor = (DWORD)-1;
	m_dwBuild = (DWORD)-1;

#ifdef XP_WIN16
	m_bWin16 = TRUE;
	m_bWin32 = FALSE;

	//	Use GetVersion.
	bUseGetversion = TRUE;
#else
	m_bWin16 = FALSE;
	m_bWin32 = TRUE;

	OSVERSIONINFO osVer;
	memset(&osVer, 0, sizeof(osVer));
	osVer.dwOSVersionInfoSize = sizeof(osVer);
	if(GetVersionEx(&osVer) == TRUE)	{
		switch(osVer.dwPlatformId)	{
		case VER_PLATFORM_WIN32s:
			m_bWin32s = TRUE;
			break;
		case VER_PLATFORM_WIN32_NT:
			m_bWinNT = TRUE;
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			m_bWin4 = TRUE;
			break;
		default:
			//	Hmm.....
			bUseGetversion = TRUE;
			break;
		}

		if ( osVer.dwMajorVersion >= 4)
			m_bWin4 = TRUE;

		//	Get even more information.
		//	Only take the lower word of the version info.
		//		Top also seems to be os version....
		m_dwMajor = osVer.dwMajorVersion;
		m_dwMinor = osVer.dwMinorVersion;
		m_dwBuild = (WORD)osVer.dwBuildNumber;
	} else {
		//	Use getversion to attempt to determine
		//		the OS flavor.
		bUseGetversion = TRUE;
	}
#endif

	if(bUseGetversion == TRUE)	{
		//	Attempt to determine exact OS flavor.
		DWORD dwVersion = ::GetVersion();

		//	Win95 or what?
		m_bWin4 = LOBYTE(dwVersion) >= 4;

		//	Clear 32s if Win4.
		m_bWin32s = (dwVersion & 0x80000000UL) != 0;
		if(m_bWin4 == TRUE)	{
			m_bWin32s = FALSE;
		}

#ifdef XP_WIN32
		//	If neither 32s or 95 are set, then assume NT.
		if(m_bWin4 == FALSE && m_bWin32s == FALSE)	{
			m_bWinNT = TRUE;
		}
#endif

		//	Attempt to get version information if not already set.
		if(m_dwMajor == (DWORD)-1)	{
			m_dwMajor = LOBYTE(dwVersion);
		}
		if(m_dwMinor == (DWORD)-1)	{
			m_dwMinor = HIBYTE(dwVersion);
		}

		//	Don't care about build number in the get version stone age.

#ifdef XP_WIN16
		//	However, if we aren't already set for 95, and our minor version
		//		number is 95, then set us to be 95.
		if(m_dwMinor >= 95)	{
			m_bWin4 = TRUE;
			m_bWin32s = FALSE;
		}
#endif
	}

	m_bDBCS = FALSE;	
	if (GetSystemMetrics(SM_DBCSENABLED))
		m_bDBCS = TRUE;	


	//	Clear out the color information.
	m_hbrBtnFace = NULL;

	m_clrBtnFace = RGB(0,0,0);
	m_clrBtnShadow = RGB(0,0,0);
	m_clrBtnHilite = RGB(0,0,0);
	m_clrBtnText = RGB(0,0,0);

	// Bits per pixel info.
	m_iBitsPerPixel = 0;

	//	Here's the current window border sizes.
	m_iBorderHeight = 0;
	m_iBorderWidth = 0;

	//	The current scroll bar sizes.
	m_iScrollWidth = 0;
	m_iScrollHeight = 0;

	//	Zero out the winsock information.
	//	Should be set later on.
	memset(&m_wsaData, 0, sizeof(m_wsaData));
    m_iMaxSockets = 0;

    //  Current screen dimensions.
    m_iScreenWidth = 0;
    m_iScreenHeight = 0;
    
    //  No printer yet.
    m_bPrinter = FALSE;

	//	Load up color information, etc....
	UpdateInfo();

    //  Let the slave window know that on any change
    //      to the system, we want to be updated.
    //  Note that WM_WININICHANGE is equal to WM_SETTINGCHANGE.
    m_pWatchSettingChanges = slavewnd.Register(WM_WININICHANGE, UpdateSystemInfo);
    m_pWatchDeviceModeChanges = slavewnd.Register(WM_DEVMODECHANGE, UpdateSystemInfo);
#ifdef _WIN32
    //  We'll want to update on this too (change of resolution).
    //  However, I've never seen this work!!!!
    m_pWatchDeviceChanges = slavewnd.Register(WM_DISPLAYCHANGE, UpdateSystemInfo);
#else
    m_pWatchDeviceChanges = NULL;
#endif

	m_bOverrideWin95Tooltips = OverrideWin95ToolTips();
}

CSysInfo::~CSysInfo()	{
	//	Get rid of any brushes we have allocated.
	if(m_hbrBtnFace != NULL)	{
		::DeleteObject(m_hbrBtnFace);
	}

	if(m_hbrMenu != NULL) {
		::DeleteObject(m_hbrMenu);
	}

	if(m_hbrHighlight != NULL){
		::DeleteObject(m_hbrHighlight);
	}

    if(m_pWatchSettingChanges)  {
        slavewnd.UnRegister(m_pWatchSettingChanges);
        m_pWatchSettingChanges = NULL;
    }
    if(m_pWatchDeviceModeChanges)  {
        slavewnd.UnRegister(m_pWatchDeviceModeChanges);
        m_pWatchDeviceModeChanges = NULL;
    }
    if(m_pWatchDeviceChanges)  {
        slavewnd.UnRegister(m_pWatchDeviceChanges);
        m_pWatchDeviceChanges = NULL;
    }
}

void CSysInfo::UpdateInfo()	{
	//	Set up all the known colors we'd like to cache...
	//	There are lots of other possible colors we could get
	//		here also.  Feel free to add.
	m_clrBtnFace = ::GetSysColor(COLOR_BTNFACE);
	m_clrBtnShadow = ::GetSysColor(COLOR_BTNSHADOW);
	m_clrBtnHilite = ::GetSysColor(COLOR_BTNHIGHLIGHT);
	m_clrBtnText = ::GetSysColor(COLOR_BTNTEXT);
	m_clrMenu = ::GetSysColor(COLOR_MENU);
	m_clrHighlight = ::GetSysColor(COLOR_HIGHLIGHT);

	// Bits per pixel
	HWND hTmp = ::GetDesktopWindow();
	HDC hScreen = ::GetDC(hTmp);
	m_iBitsPerPixel = ::GetDeviceCaps(hScreen, BITSPIXEL);
	::ReleaseDC(hTmp, hScreen);
	hScreen = NULL;
	hTmp = NULL;

	//	Determine window border sizes.
	m_iBorderWidth = ::GetSystemMetrics(SM_CXBORDER);
	m_iBorderHeight = ::GetSystemMetrics(SM_CYBORDER);

	//	On windows 95, NT4, 32 bit, we double the border size.
	if(m_dwMajor >= 4 && m_bWin32)	{
		m_iBorderWidth *= 2;
		m_iBorderHeight *= 2;
	}

	//	Determine scroll bar dimensions.
	m_iScrollWidth = ::GetSystemMetrics(SM_CXVSCROLL);
	m_iScrollHeight = ::GetSystemMetrics(SM_CYHSCROLL);

    //  Determine screen dimensions
#ifdef XP_WIN32
    // This excludes the Windows shell area on the desktop
    m_iScreenWidth = ::GetSystemMetrics(SM_CXFULLSCREEN);
    m_iScreenHeight = ::GetSystemMetrics(SM_CYFULLSCREEN);
#else
    m_iScreenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    m_iScreenHeight = ::GetSystemMetrics(SM_CYSCREEN);
#endif

	//	Free off brushes if previously allocated.
	if(m_hbrBtnFace != NULL)	{
		::DeleteObject(m_hbrBtnFace);
	}

	if(m_hbrMenu != NULL) {
		::DeleteObject(m_hbrMenu);
	}

	if(m_hbrHighlight != NULL){
		::DeleteObject(m_hbrHighlight);
	}

	//	Allocate brushes according to color.
	m_hbrBtnFace = ::CreateSolidBrush(m_clrBtnFace);
	m_hbrMenu = ::CreateSolidBrush(m_clrMenu);
	m_hbrHighlight = ::CreateSolidBrush(m_clrHighlight);
	
    //  Attempt to detect printer, and cleanup.
    m_bPrinter = FALSE;
    PRINTDLG pd;
    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.Flags = PD_RETURNDEFAULT;
    ::PrintDlg(&pd);
    if(pd.hDevMode) {
        m_bPrinter = TRUE;
        ::GlobalFree(pd.hDevMode);
        pd.hDevMode = NULL;
    }
    if(pd.hDevNames) {
        m_bPrinter = TRUE;
        ::GlobalFree(pd.hDevNames);
        pd.hDevNames = NULL;
    }
}

BOOL CSysInfo::OverrideWin95ToolTips(void)
{
	char comctrl32Path[MAX_PATH + 1];
	CString csComCtrl32;
	BOOL bResult = FALSE;
#ifdef XP_WIN32
	if(IsWin4_32() && !m_bWinNT) 
	{
		HMODULE hComCtl32 = GetModuleHandle("comctl32.dll");

		if(hComCtl32)
		{
			if(GetModuleFileName(hComCtl32, comctrl32Path, MAX_PATH) > 0)
			{
				csComCtrl32 = comctrl32Path;

				LPTSTR pComCtrl32 = csComCtrl32.LockBuffer();
				DWORD dwZero;
				DWORD dwSize = GetFileVersionInfoSize(pComCtrl32,&dwZero);

				if(dwSize > 0)
				{
					BYTE *data = new BYTE[dwSize];
					if(data && GetFileVersionInfo(pComCtrl32, 0, dwSize, data));
					{
						VS_FIXEDFILEINFO *fileInfo = NULL;
						UINT size;

						if (VerQueryValue(data, TEXT("\\"), (LPVOID*)&fileInfo, &size) && fileInfo)
						{
							WORD major = HIWORD(fileInfo->dwFileVersionMS);
							WORD minor = LOWORD(fileInfo->dwFileVersionMS);
							
							if(major == 4 && minor == 0)
								bResult = TRUE;
						}
  
					}
					if(data)
						delete data;

				}

				csComCtrl32.UnlockBuffer();
			}
		}
	}
#endif
	return bResult;
}

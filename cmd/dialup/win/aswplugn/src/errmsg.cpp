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
///////////////////////////////////////////////////////////////////////////////
//
// Errmsg.cpp
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
//             xxxxxxxxxxxxxx    Define routines
///////////////////////////////////////////////////////////////////////////////

#include <windows.h>

// resource include
#ifdef WIN32 // **************************** WIN32 *****************************
#include "resource.h"
#else        // **************************** WIN16 *****************************
#include "asw16res.h"
#endif // !WIN32

extern HINSTANCE DLLinstance; // dll instance


//********************************************************************************
// 
// getMsgString
//
// loads a Message String from the string table
//********************************************************************************
BOOL getMsgString(char *buf, UINT uID)
{
	if (!buf)
		return FALSE;
		
	HINSTANCE hInstance = DLLinstance;
	int ret = LoadString(hInstance, uID, buf, 255);

	return ret;
}


//********************************************************************************
// 
// DisplayErrMsg
//
// display error messages in a standard windows message box
//********************************************************************************
int DisplayErrMsg(char *text,
                  int style)					
{
	char title[50];
	getMsgString((char *)&title, IDS_APP_NAME);
	
	// try to get navigator window handle here
	HWND hwnd = GetActiveWindow();

	return MessageBox(hwnd, text, title, style);
}


//********************************************************************************
// 
// DisplayErrMsgWnd
//
// display error messages in a standard windows message box
// this is the err msg box call if we for sure can get a navigator window handle
//
// mostly used for displaying RAS connection errs
//********************************************************************************
int DisplayErrMsgWnd(char *text,
					 int style,
					 HWND hwnd)

{
	char title[50];
	getMsgString((char *)&title, IDS_APP_NAME);

	int ret = MessageBox(hwnd, text, title, style);

	return ret;
}

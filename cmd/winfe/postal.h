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

/******************************************************************************
                                    POSTAL
 ******************************************************************************
 Here is the defintion of Netscape's Postal Services API.  This interface 
 provides a service abstraction that allows a client to send and receive mail
 messages.  The fact that Extended MAPI (ie., Exchange) is being used is
 entirely encapsulated and transparent to the caller.  The implementation
 could be extended to support other messaging technologies without a change 
 to this API.  Thus, Netscape's native (Internet) messaging code could be
 integrated into this implementation to establish a single mail-enabling API.

 The following functions comprise the API:
	
     RegisterMailClient     - This function establishes the caller as a client 
                              of the POSTAL services.
 
     UnRegisterMailClient   - This function discontinues the caller's use of
                              the postal services.  Any active session is
                              closed automatically, prior to termination.
 
     OpenMailSession        - Creates a POSTAL session.
 
     CloseMailSession       - Destroys a POSTAL session.
 
     ComposeMailMessage     - Composes and sends a mail message.

	 ShowMailBox			- Shows the applications mailbox i.e. InBox
	 
	 ShowMessageCenter		- Shows the applications message center(if any)

	 GetMenuItemString		- Called to retrieve the menu item string to be
							  in Netscape Navigator's menu.

 The rules of engagement are simple.  You must register yourself as a client
 before you can establish a session.  You must establish a session before you
 can send a message.  Disengagement is just the reverse.  

 ******************************************************************************/

#ifndef _POSTAL_H
#define _POSTAL_H

#ifdef WIN32
#  define EXPORT16 PASCAL
#  ifdef __cplusplus
#    define EXPORT32 extern "C" __declspec(dllexport)
#  else
#    define EXPORT32 extern __declspec(dllexport)
#  endif
#else
#  define EXPORT16 FAR PASCAL _export
#  ifdef __cplusplus
#    define EXPORT32 extern "C"
#  else
#    define EXPORT32 extern
#  endif
#endif


//----------------------------------------------------------
//					Menu String Types
//----------------------------------------------------------
typedef enum {
	ALTMAIL_MenuMailBox,
	ALTMAIL_MenuMessageCenter
} ALTMAIL_MENU_ID;

//----------------------------------------------------------
//					Return codes						   
//----------------------------------------------------------
typedef LONG POSTCODE;

#define POST_W_ALREADY_OPEN         1
#define POST_OK                     0
#define POST_E_FAILED              -1
#define POST_E_BAD_HWND            -2
#define POST_E_UNREGISTERED        -3
#define POST_E_MAPI_INIT           -4
#define POST_E_OLE_INIT            -5
#define POST_E_USER_CANCEL         -6
#define POST_E_NO_MSG_STORE        -7
#define POST_E_CREATE_MSG_FAILED   -8
#define POST_E_FORM_FAILED         -9
#define POST_E_NO_MEMORY           -10
#define POST_E_BAD_SZMSG           -11
#define POST_E_BAD_MSGID           -12
#define POST_E_NO_SESSION          -13
#define	POST_E_CANT_LAUNCH_UI	   -14
#define	POST_E_BAD_BUFFER		   -15
#define	POST_E_BAD_LEN			   -16

#define POST_SUCCEEDED(pc) (pc >= 0)
#define POST_FALIED(pc) (pc < 0)

//---------------------------------------------------------------------
//An external mail client can notify Netscape Navigator of its inbox
//status by sending a message to it by using the following values in the
//wParam
//
//Netscape Navigator updates its status line to indicate the mail status
//
//Ex: SendMessage(hWnd, uMessageID, INBOX_STATE_NEWMAIL, 0L);
//	Where
//		 hWnd = Window handle passed via RegisterMailClient
//		 uMessageID = RegisterWindowMessage(szMessage);
//					  (szMessage is passed via RegisterMailClient)
//----------------------------------------------------------------------
#define	INBOX_STATE_NOMAIL	0
#define	INBOX_STATE_UNKNOWN	1
#define	INBOX_STATE_NEWMAIL	2

//---------------------------------------------------------------------
// Interface definition.
//---------------------------------------------------------------------

EXPORT32 POSTCODE EXPORT16 RegisterMailClient (HWND hwnd, LPSTR szMessage);
 
EXPORT32 POSTCODE EXPORT16 UnRegisterMailClient ();
 
EXPORT32 POSTCODE EXPORT16 OpenMailSession (LPSTR pszProfile, LPSTR pszPassword);
 
EXPORT32 POSTCODE EXPORT16 CloseMailSession ();
 
EXPORT32 POSTCODE EXPORT16 ComposeMailMessage (LPCSTR pszTo,
                                     LPCSTR pszReference,
                                     LPCSTR pszOrganization,
                                     LPCSTR pszXURL,
                                     LPCSTR pszSubject,
                                     LPCSTR pszPage,
                                     LPCSTR pszCc, 
                                     LPCSTR pszBcc);

EXPORT32 POSTCODE EXPORT16 ShowMailBox();

EXPORT32 POSTCODE EXPORT16 ShowMessageCenter();
																					 
EXPORT32 POSTCODE EXPORT16 GetMenuItemString(ALTMAIL_MENU_ID menuID, LPSTR lpszReturnBuffer, int cbBufLen); 
#endif //_POSTAL_H

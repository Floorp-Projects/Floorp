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

#ifndef __FEEMBED_H
//	Avoid include redundancy
//
#define __FEEMBED_H

//	Purpose:    Structure for embedded items and handling
//	Comments:
//	Revision History:
//      02-04-95    created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//

//	Global variables
//

//	Macros
//

//	Function declarations
//
extern "C"	{
#ifndef MOZ_NGLAYOUT
void EmbedUrlExit(URL_Struct *pUrl, int iStatus, MWContext *pContext);
#endif

NET_StreamClass *EmbedStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
int EmbedWrite(NET_StreamClass *stream, const char *pWriteData, int32 lLength);
void EmbedComplete(NET_StreamClass *stream);
void EmbedAbort(NET_StreamClass *stream, int iStatus);
unsigned int EmbedReady(NET_StreamClass *stream);

BOOL wfe_IsTypePlugin(NPEmbeddedApp* pEmbeddedApp); 
};

BOOL wfe_IsExtensionRegistrationValid(CString& csExtension, CWnd *pWnd, BOOL bAskDialog);

#endif // __FEEMBED_H

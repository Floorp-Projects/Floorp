/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* reserr.h
 * defines for error dialog boxes
 */
#ifndef __RESERR__
#define __RESERR__

// ParamText: ^0 application name, ^1 file name, ^2 error code
// Returns: 1 save file, 2 delete file
#define ALRT_ODOCFailed 1000

// ParamText: ^0 application name, ^1 file name
// Returns: 1 save file, 2 delete file
#define ALRT_AppNotFound 1001

// ParamText: ^0 application name, ^1 file name
// Returns: 1 save file, 2 delete file, 3 try again
#define ALRT_AppMemFull 1002

// ParamText: ^0 application name, ^1 file name, ^2 error code
// Returns: 1 save file, 2 delete file
#define ALRT_AppMiscError 1003

// Clear. Use ParamText ^0^1^2^3
#define ALRT_PlainAlert 1004

// resources between 1005 and 1007 are taken by password dialogs

// Your last command could not be completed because ^0.  Error number ^1.
#define ALRT_ErrorOccurred	1008

// Clear. Yes or no response. Use ParamText ^0^1^2^3
#define ALRT_YorNAlert	1010

// Unknown MIME type alert ^0 is file name, ^1 is document type
#define ALRT_UnknownMimeType 1011
#define ALRT_UnknownMimeType_Cancel 1
#define ALRT_UnknownMimeType_Save 2
#define ALRT_UnknownMimeType_PickApp 3
#define ALRT_UnknownMimeType_MoreInfo 4

// 
#define ALRT_BookmarkOutDrag	1013

#endif




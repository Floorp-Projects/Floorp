/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
///////////////////////////////////////////////////////////////////////////////
//
// Errmsg.h
// Revision History:
// Date        Author            Reason
// ----------------------------------------------------------------------------
//             xxxxxxxxxxxxxx    Define routines
///////////////////////////////////////////////////////////////////////////////

#ifndef _INC_ERRMSG_H_
#define _INC_ERRMSG_H_

//********************************************************************************
// 
// getMsgString()
//
// loads a Message String from the string table
//********************************************************************************
BOOL getMsgString(char *buf, UINT uID);


//********************************************************************************
// 
// DisplayErrMsg()
//
// display error messages in a standard windows message box
//********************************************************************************
int DisplayErrMsg(char *text, int style = MB_OK);

#endif

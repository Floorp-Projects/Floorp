/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 */

/*
 * Declares application dependent IDs for resources, messages, on so on
 * It is meant to be replaced or added to by any application using the
 * classes in this project. The file name name should not be changed though or
 * source files that include it will have to be changed.
 */
 
#ifndef __ApplIDs__
#define __ApplIDs__
 
//*****************************************************************************
//***    Resource IDs
//*****************************************************************************

// Windows
const PP_PowerPlant::ResIDT	wind_BrowserWindow = 129;

// Dialogs
const PP_PowerPlant::ResIDT	dlog_FindDialog = 1281;

// nsIPrompt
const PP_PowerPlant::ResIDT dlog_Alert = 1282;
const PP_PowerPlant::ResIDT dlog_Confirm = 1283;
const PP_PowerPlant::ResIDT dlog_ConfirmCheck = 1284;
const PP_PowerPlant::ResIDT dlog_Prompt = 1285;
const PP_PowerPlant::ResIDT dlog_PromptNameAndPass = 1286;
const PP_PowerPlant::ResIDT dlog_PromptPassword = 1287;

//*****************************************************************************
//***    Message IDs
//*****************************************************************************

const MessageT    msg_OnStartLoadDocument 	= 1000;
const MessageT    msg_OnEndLoadDocument 		= 1001;

#endif // __ApplIDs__

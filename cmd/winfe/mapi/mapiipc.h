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
#ifndef __MAPIIPC_HPP__
#define __MAPIIPC_HPP__

#include "port.h"
#include <nscpmapi.h>

//********************************************************
// Open and close functions for API
//********************************************************
// Open the API
// Return: 1 on success, 0 on failure
//
DWORD nsMAPI_OpenAPI(void);

//
// Purpose: Close the API
//
void  nsMAPI_CloseAPI(void);

//
// Send the actual request to Communicator
//
LRESULT   SendMAPIRequest(HWND hWnd, 
                DWORD mapiRequestID, 
                MAPIIPCType *ipcInfo);

#endif // __MAPIIPC_HPP__

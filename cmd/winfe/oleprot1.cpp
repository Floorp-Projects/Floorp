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

// Machine generated IDispatch wrapper class(es) created with ClassWizard

#include "stdafx.h"

#include "oleprot1.h"

/////////////////////////////////////////////////////////////////////////////
// IIProtocol1 properties

/////////////////////////////////////////////////////////////////////////////
// IIProtocol1 operations

short IIProtocol1::Initialize(LPCTSTR pProtocol, LPCTSTR pUrl)
{
	short result;
	static BYTE BASED_CODE parms[] =
		VTS_BSTR VTS_BSTR;
	InvokeHelper(0x1, DISPATCH_METHOD, VT_I2, (void*)&result, parms,
		pProtocol, pUrl);
	return result;
}

void IIProtocol1::Open(LPCTSTR pUrl)
{
	static BYTE BASED_CODE parms[] =
		VTS_BSTR;
	InvokeHelper(0x2, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 pUrl);
}

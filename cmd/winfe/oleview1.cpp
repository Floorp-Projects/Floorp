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

#include "oleview1.h"

/////////////////////////////////////////////////////////////////////////////
// IIViewer1 properties

/////////////////////////////////////////////////////////////////////////////
// IIViewer1 operations

void IIViewer1::Close(short iStatus)
{
	static BYTE BASED_CODE parms[] =
		VTS_I2;
	InvokeHelper(0x1, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		 iStatus);
}

short IIViewer1::Initialize(LPCTSTR pMimeType, LPCTSTR pUrl)
{
	short result;
	static BYTE BASED_CODE parms[] =
		VTS_BSTR VTS_BSTR;
	InvokeHelper(0x2, DISPATCH_METHOD, VT_I2, (void*)&result, parms,
		pMimeType, pUrl);
	return result;
}

long IIViewer1::Ready()
{
    long result;
    InvokeHelper(0x3, DISPATCH_METHOD, VT_I4, (void *)&result, NULL);
    return(result);
}

void IIViewer1::Write(BSTR* pData, long lBytes)
{
	static BYTE BASED_CODE parms[] =
		VTS_PBSTR VTS_I4;
	InvokeHelper(0x4, DISPATCH_METHOD, VT_EMPTY, NULL, parms,
		pData, lBytes);
}

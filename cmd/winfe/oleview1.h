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

#ifndef __OLE_VIEWER_1_H
#define __OLE_VIEWER_1_H

// Machine generated IDispatch driver class(es) created with MFCDSPWZ tool.
/////////////////////////////////////////////////////////////////////////////
// IIViewer1 wrapper class

class IIViewer1 : public COleDispatchDriver
{
// Attributes
public:

// Operations
public:
	void Close(short iStatus);
	short Initialize(LPCTSTR pMimeType, LPCTSTR pUrl);
    long Ready();
	void Write(BSTR* pData, long lBytes);
};

#endif  //  __OLE_VIEWER_1_H

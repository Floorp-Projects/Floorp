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

#ifndef __WPREFID_H_
#define __WPREFID_H_

// {464DC700-2727-11d2-8043-00600811A9C3}
DEFINE_GUID(IID_IWindowsPrefs, 
0x464dc700, 0x2727, 0x11d2, 0x80, 0x43, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3);

// {464DC701-2727-11d2-8043-00600811A9C3}
DEFINE_GUID(CLSID_WindowsPrefs, 
0x464dc701, 0x2727, 0x11d2, 0x80, 0x43, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3);

class nsIDefaultBrowser;

// IWindowsPrefs
DECLARE_INTERFACE_(IWindowsPrefs, IUnknown)
{
	// IUnknown methods
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID FAR* ppvObj) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;

	// IWindowsPrefs methods
	STDMETHOD(SetDefaultBrowser)(THIS_ nsIDefaultBrowser* pDefaultBrowser) PURE;
};

#endif /* __WPREFID_H_ */

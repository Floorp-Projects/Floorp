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

#include "stdafx.h"
#include "xpstrsw.h"

HINSTANCE ResourceSwitcher::m_oldresourcehandle;
HINSTANCE CXPResSwitcher::s_dllinstance;
unsigned int CXPResSwitcher::s_refcount=0;

CXPResSwitcher::CXPResSwitcher()
{
	s_dllinstance = LoadLibrary("XPSTRDLL.dll");
	s_refcount++;
}

CXPResSwitcher::~CXPResSwitcher()
{
	FreeLibrary(s_dllinstance);
	s_refcount--;
}
void CXPResSwitcher::Switch()
{
	if((UINT)s_dllinstance > 32)
	{
		m_oldresourcehandle = AfxGetResourceHandle();
		AfxSetResourceHandle(s_dllinstance);
	}
}
void CXPResSwitcher::Reset()
{
	if((UINT)s_dllinstance > 32)
		AfxSetResourceHandle(m_oldresourcehandle);
}

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

#ifndef XPSTRSW_H
#define XPSTRSW_H

#include "stdafx.h"

class ResourceSwitcher
{
protected:
    static HINSTANCE m_oldresourcehandle;
public:
    ~ResourceSwitcher(){}
    virtual void Reset()=0;
    virtual void Switch()=0;
	virtual HINSTANCE GetResourceHandle()=0;
};

class CXPResSwitcher :public ResourceSwitcher
{
    static HINSTANCE s_dllinstance;
    static unsigned int s_refcount;
public:
    CXPResSwitcher();
    ~CXPResSwitcher();
    void Switch();
    void Reset();
	HINSTANCE GetResourceHandle(){return s_dllinstance;}
};

#endif

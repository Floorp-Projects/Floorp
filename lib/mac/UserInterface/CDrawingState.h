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

#ifndef CDrawingState_H
#define CDrawingState_H
#pragma once

#include <PP_Prefix.h>

#ifndef __QUICKDRAW__
#include <QuickDraw.h>
#endif

class	StMercutioMDEFTextState
{
public:
				StMercutioMDEFTextState();
				~StMercutioMDEFTextState();
				
	void		SetUpForMercurtioMDEF();
	void		Restore();

private:
    GrafPtr     mPort;
    GrafPtr		mWMgrPort;
    CGrafPtr	mWMgrCPort;
    Int16		mWMgrFont;
    Int16		mWMgrSize;
    Int16		mCWMgrFont;
    Int16		mCWMgrSize;
};

class	StSysFontState
{
public:
				StSysFontState();
				~StSysFontState();
				
	void		Save();
	void		Restore();
	
	void		SetTextTraits(ResIDT inTextTraitsID);

private:
	Int16		mFont;
	Int16		mSize;
};

#endif
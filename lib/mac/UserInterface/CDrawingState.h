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
   // CGrafPtr	mWMgrCPort;
    Int16		mWMgrFont;
    Int16		mWMgrSize;
    //Int16		mCWMgrFont;
    //Int16		mCWMgrSize;
};

class	StSysFontState
{
public:
				StSysFontState();
				~StSysFontState();
				
	void		Save();
	void		Restore();
	
	void		SetTextTraits(ResIDT inTextTraitsID = 130);

private:
	Int16		mFont;
	Int16		mSize;
};

#endif
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

#include "CDrawingState.h"

#include <UTextTraits.h>

// ---------------------------------------------------------------------------
//		¥ StMercutioMDEFTextState
// ---------------------------------------------------------------------------

StMercutioMDEFTextState::StMercutioMDEFTextState()
{
	SetUpForMercurtioMDEF();
}

// ---------------------------------------------------------------------------
//		¥ StMercutioMDEFTextState
// ---------------------------------------------------------------------------

StMercutioMDEFTextState::~StMercutioMDEFTextState()
{
	Restore();
}	

// ---------------------------------------------------------------------------
//		¥ SetUpForMercurtioMDEF
// ---------------------------------------------------------------------------

void
StMercutioMDEFTextState::SetUpForMercurtioMDEF()
{
	mPort = UQDGlobals::GetCurrentPort();

    ::GetWMgrPort(&mWMgrPort);
    ::SetPort(mWMgrPort);
    mWMgrFont = mWMgrPort->txFont;
    mWMgrSize = mWMgrPort->txSize;
    ::TextFont(systemFont);
    ::TextSize(0);

	if (UEnvironment::HasFeature(env_SupportsColor))
	{
        ::GetCWMgrPort(&mWMgrCPort);
        ::SetPort((GrafPtr)mWMgrCPort);
        mCWMgrFont = mWMgrCPort->txFont;
        mCWMgrSize = mWMgrCPort->txSize;
        ::TextFont(systemFont);
        ::TextSize(0);
	}
	
	::SetPort(mPort);
}

// ---------------------------------------------------------------------------
//		¥ Restore
// ---------------------------------------------------------------------------

void
StMercutioMDEFTextState::Restore()
{
    ::SetPort(mWMgrPort);
    ::TextFont(mWMgrFont);
    ::TextSize(mWMgrSize);

	if (UEnvironment::HasFeature(env_SupportsColor))
	{
        ::SetPort((GrafPtr)mWMgrCPort);
        ::TextFont(mCWMgrFont);
        ::TextSize(mCWMgrSize);
	}

    ::SetPort(mPort);
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ StSysFontState
// ---------------------------------------------------------------------------

StSysFontState::StSysFontState()
{
	Save();
}

// ---------------------------------------------------------------------------
//		¥ StSysFontState
// ---------------------------------------------------------------------------

StSysFontState::~StSysFontState()
{
	Restore();
}	

// ---------------------------------------------------------------------------
//		¥ Save
// ---------------------------------------------------------------------------

void
StSysFontState::Save()
{
	mFont = ::LMGetSysFontFam();
	mSize = ::LMGetSysFontSize();
}

// ---------------------------------------------------------------------------
//		¥ Restore
// ---------------------------------------------------------------------------

void
StSysFontState::Restore()
{
	::LMSetSysFontFam(mFont);
	::LMSetSysFontSize(mSize);
	::LMSetLastSPExtra(-1L);
}

// ---------------------------------------------------------------------------
//		¥ SetTextTraits
// ---------------------------------------------------------------------------
//	Make sure port has been set (via FocusDraw()) before calling SetTextTraits.

void
StSysFontState::SetTextTraits(
	ResIDT inTextTraitsID)
{
	TextTraitsH traitsH = UTextTraits::LoadTextTraits ( 130 );
	if ( traitsH ) 
	{
		// Bug #64133 kellys
		// If setting to application font, get the application font for current script
		if((**traitsH).fontNumber == 1)
			::LMSetSysFontFam ( ::GetScriptVariable(::FontToScript(1), smScriptAppFond) );
		else
			::LMSetSysFontFam ( (**traitsH).fontNumber );
		::LMSetSysFontSize ( (**traitsH).size );
		::LMSetLastSPExtra ( -1L );
	}
}

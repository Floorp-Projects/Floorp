/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

/*	duh. this does exactly what we just did.

	if (UEnvironment::HasFeature(env_SupportsColor))
	{
        ::GetCWMgrPort(&mWMgrCPort);
        ::SetPort((GrafPtr)mWMgrCPort);
        mCWMgrFont = mWMgrCPort->txFont;
        mCWMgrSize = mWMgrCPort->txSize;
        ::TextFont(systemFont);
        ::TextSize(0);
	}
*/	
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

/*	duh. this does exactly what we just did.
	if (UEnvironment::HasFeature(env_SupportsColor))
	{
        ::SetPort((GrafPtr)mWMgrCPort);
        ::TextFont(mCWMgrFont);
        ::TextSize(mCWMgrSize);
	}
*/

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
	ResIDT inTextTraitsID )
{
	TextTraitsH traitsH = UTextTraits::LoadTextTraits ( inTextTraitsID );
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

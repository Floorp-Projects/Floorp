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

#include "CPatternTabControl.h"
#include "CSharedPatternWorld.h"
#include "UGraphicGizmos.h"

//-----------------------------------
CPatternTabControl::CPatternTabControl(LStream* inStream)
//-----------------------------------
	:	Inherited(inStream)
{
	ResIDT theBevelTraitsID;
	*inStream >> theBevelTraitsID;
	UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mArithBevelColors);
	
	ResIDT thePatternResID;
	*inStream >> thePatternResID;
	
	mPatternWorld = CSharedPatternWorld::CreateSharedPatternWorld(thePatternResID);
	ThrowIfNULL_(mPatternWorld);
	mPatternWorld->AddUser(this);
	
	*inStream >> mPatternOrientation;
} // CPatternTabControl::CPatternTabControl

//-----------------------------------
CPatternTabControl::~CPatternTabControl()
//-----------------------------------
{
	mPatternWorld->RemoveUser(this);
} // CPatternTabControl::~CPatternTabControl()

//-----------------------------------
void CPatternTabControl::DrawOneTabBackground(
	RgnHandle inRegion,
	Boolean /* inCurrentTab */)
//-----------------------------------
{
	Point theAlignment;
	CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
	GrafPtr thePort;
	::GetPort(&thePort);
	mPatternWorld->Fill((CGrafPtr)thePort, inRegion, theAlignment);
} // CPatternTabControl::DrawOneTabBackground

//-----------------------------------
void CPatternTabControl::DrawOneTabFrame(RgnHandle inRegion, Boolean /* inCurrentTab */)
//-----------------------------------
{
	::FrameRgn(inRegion);
} // CPatternTabControl::DrawOneTabFrame

//-----------------------------------
void CPatternTabControl::DrawCurrentTabSideClip(RgnHandle inRegion)
//-----------------------------------
{
	::SetClip(inRegion);
	Point theAlignment;
	CSharedPatternWorld::CalcRelativePoint(this, CSharedPatternWorld::eOrientation_Port, theAlignment);
	GrafPtr thePort;
	::GetPort(&thePort);
	mPatternWorld->Fill((CGrafPtr)thePort, mSideClipFrame, theAlignment);
} // CPatternTabControl::DrawOneTabSideClip


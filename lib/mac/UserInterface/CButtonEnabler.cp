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

// Written by John R. McMullen

#include "CButtonEnabler.h"
#include "LSharable.h"

#include "MButtonEnablerPolicy.h"

//======================================
#pragma mark --------- class CButtonMaster
//======================================

//======================================
class CButtonMaster
//======================================
	:	public LAttachable // CButtonEnablers all attach to this
	,	public LSharable // shared by all CButtonEnablers.
{
public:
	CButtonMaster();
	virtual ~CButtonMaster();
	static CButtonMaster* sButtonMaster;
}; // class CButtonMaster

CButtonMaster* CButtonMaster::sButtonMaster = nil;

//-----------------------------------
CButtonMaster::CButtonMaster()
//-----------------------------------
{
	sButtonMaster = this;
} // CButtonMaster::CButtonMaster()

//-----------------------------------
CButtonMaster::~CButtonMaster()
//-----------------------------------
{
	sButtonMaster = nil;
} // CButtonMaster::~CButtonMaster()

//======================================
#pragma mark --------- class CButtonEnabler
//======================================

//-----------------------------------
CButtonEnabler* CButtonEnabler::CreateButtonEnablerStream(LStream* inStream)
//-----------------------------------
{
	return new CButtonEnabler(inStream);
}

//-----------------------------------
CButtonEnabler::CButtonEnabler(LStream* inStream)
//-----------------------------------
:	LAttachment(inStream)
,	mControl(nil)
{
	SetMessage(msg_Event); // execute me only for event messages.
	LAttachable	*host = LAttachable::GetDefaultAttachable();
	mControl = dynamic_cast<LControl*>(host);
	Assert_(mControl);
	Try_
	{
		if (!CButtonMaster::sButtonMaster)
			new CButtonMaster;
		CButtonMaster::sButtonMaster->AddUser(this);
		CButtonMaster::sButtonMaster->AddAttachment(this, nil, false);
			// don't care where, master is not owner.
	}
	Catch_ (inErr)
	{
	}
	EndCatch_
	LAttachable::SetDefaultAttachable(host);
		// Restore, because LAttachable's constructor (called when we made
		// the new button master) clobbers the default attachable
} // CButtonEnabler::CButtonEnabler

//-----------------------------------
CButtonEnabler::~CButtonEnabler()
//-----------------------------------
{
	if (CButtonMaster::sButtonMaster)
	{
		CButtonMaster::sButtonMaster->RemoveAttachment(this);
		CButtonMaster::sButtonMaster->RemoveUser(this);
	}
} // CButtonEnabler::~CButtonEnabler

//-----------------------------------
void CButtonEnabler::UpdateButtons()
//-----------------------------------
{
	if (CButtonMaster::sButtonMaster)
		CButtonMaster::sButtonMaster->ExecuteAttachments(msg_Event, nil);	
} // CButtonEnabler::UpdateButtons

//-----------------------------------
void CButtonEnabler::ExecuteSelf(MessageT inMessage, void*)
//-----------------------------------
{
	MButtonEnablerPolicy*	thePolicy = dynamic_cast<MButtonEnablerPolicy*>(mControl);

	if (thePolicy)
	{
		// Delegate the policy
		
		thePolicy->HandleButtonEnabling();
	}
	else
	{
		// Default enabling policy for LControl
		
		LCommander* theTarget		= LCommander::GetTarget();
		MessageT	command			= mControl->GetValueMessage();
		Boolean 	enabled			= false;
		Boolean		usesMark		= false;
		Str255		outName;
		Char16		outMark;

		if (!mControl->IsActive() || !mControl->IsVisible())
			return;
			
		if (!theTarget)
			return;
		
		theTarget->ProcessCommandStatus(command, enabled, usesMark, outMark, outName);
		
		if (enabled)
		{
			mControl->Enable();
		}
		else
		{
			mControl->Disable();
		}
	}
} // CButtonEnabler::ExecuteSelf

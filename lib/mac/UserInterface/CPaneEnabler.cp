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

// CPaneEnabler is a more general form of CButtonEnabler, originally written by John R. McMullen.
// The generalization was done by Mark Young.

// CSlaveEnabler was written by John R. McMullen

#include "CPaneEnabler.h"
#include "LSharable.h"

#include "MPaneEnablerPolicy.h"
#include "CTargetedUpdateMenuRegistry.h"

#include <LControl.h>

//======================================
#pragma mark --------- class CPaneMaster
//======================================

//======================================
class CPaneMaster
//======================================
	:	public LAttachable // CPaneEnablers all attach to this
	,	public LSharable // shared by all CPaneEnablers.
{
public:
	CPaneMaster();
	virtual ~CPaneMaster();
	static CPaneMaster* sPaneMaster;
}; // class CPaneMaster

CPaneMaster* CPaneMaster::sPaneMaster = nil;

//-----------------------------------
CPaneMaster::CPaneMaster()
//-----------------------------------
{
	sPaneMaster = this;
} // CPaneMaster::CPaneMaster()

//-----------------------------------
CPaneMaster::~CPaneMaster()
//-----------------------------------
{
	sPaneMaster = nil;
} // CPaneMaster::~CPaneMaster()

//======================================
#pragma mark --------- class CPaneEnabler
//======================================

//-----------------------------------
CPaneEnabler::CPaneEnabler(LStream* inStream)
//-----------------------------------
:	LAttachment(inStream)
,	mPane(nil)
{
	SetMessage(msg_Event); // execute me only for event messages.
	LAttachable	*host = LAttachable::GetDefaultAttachable();
	mPane = dynamic_cast<LPane*>(host);
	Assert_(mPane);
	try
	{
		if (!CPaneMaster::sPaneMaster)
			new CPaneMaster;
		CPaneMaster::sPaneMaster->AddUser(this);
		CPaneMaster::sPaneMaster->AddAttachment(this, nil, false);
			// don't care where, master is not owner.
	}
	catch (...)
	{
	}
	LAttachable::SetDefaultAttachable(host);
		// Restore, because LAttachable's constructor (called when we made
		// the new pane master) clobbers the default attachable
} // CPaneEnabler::CPaneEnabler

//-----------------------------------
CPaneEnabler::~CPaneEnabler()
//-----------------------------------
{
	if (CPaneMaster::sPaneMaster)
	{
		CPaneMaster::sPaneMaster->RemoveAttachment(this);
		CPaneMaster::sPaneMaster->RemoveUser(this);
	}
} // CPaneEnabler::~CPaneEnabler

//-----------------------------------
void CPaneEnabler::UpdatePanes()
//-----------------------------------
{
	if (CPaneMaster::sPaneMaster)
		CPaneMaster::sPaneMaster->ExecuteAttachments(msg_Event, nil);	
} // CPaneEnabler::UpdatePanes

//-----------------------------------
void CPaneEnabler::ExecuteSelf(MessageT inMessage, void*)
//-----------------------------------
{
	MPaneEnablerPolicy*	thePolicy	= dynamic_cast<MPaneEnablerPolicy*>(mPane);
	LPane*				thePane		= dynamic_cast<LPane*>(mPane);
	LControl*			theControl	= dynamic_cast<LControl*>(mPane);

	if (thePolicy)
	{
		// Delegate the policy
		
		thePolicy->HandleEnablingPolicy();
	}
	else if (theControl)
	{
		// Default enabling policy for LControl
		
		LCommander* theTarget		= LCommander::GetTarget();
		MessageT	command			= theControl->GetValueMessage();
		Boolean 	enabled			= false;
		Boolean		usesMark		= false;
		Str255		outName;
		Char16		outMark;
		
		if (!CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus() ||
				CTargetedUpdateMenuRegistry::CommandInRegistry(command))
		{
			if (!mPane->IsActive() || !mPane->IsVisible())
				return;
				
			if (!theTarget)
				return;
			
			theTarget->ProcessCommandStatus(command, enabled, usesMark, outMark, outName);			
			if (enabled)
				mPane->Enable();
			else
				mPane->Disable();
		}
	}
	else if (thePane)
	{
		// Default enabling policy for LPane		
		mPane->Enable();
	}
	else
		throw;
} // CPaneEnabler::ExecuteSelf

//======================================
#pragma mark --------- class CSlaveEnabler
//======================================

//-----------------------------------
CSlaveEnabler::CSlaveEnabler(LStream* inStream)
//-----------------------------------
:	LAttachment(inStream)
,	mControllingControl(nil)
,	mControllingID(0)
,	mPane(nil)
{
	*inStream >> mControllingID;
	LAttachable	*host = LAttachable::GetDefaultAttachable();
	mPane = dynamic_cast<LPane*>(host);
	Assert_(mPane);	
	SetMessage(msg_DrawOrPrint);
} // CSlaveEnabler::CSlaveEnabler

//-----------------------------------
void CSlaveEnabler::Update(SInt32 inValue)
//-----------------------------------
{
	if (inValue != 0)
		mPane->Enable();
	else
		mPane->Disable();
} // CSlaveEnabler::Update

//-----------------------------------
void CSlaveEnabler::ExecuteSelf(MessageT inMessage, void*)
// When our pane is to be drawn for the first time, find our controlling control
// and set the enabling accordingly.  Then turn off attachment messages.
//-----------------------------------
{
	if (inMessage == msg_DrawOrPrint && !mControllingControl)
	{				
		mControllingControl = dynamic_cast<LControl*>(
			mPane->GetSuperView()->FindPaneByID(mControllingID));
		Assert_(mControllingControl);
		if (mControllingControl)
		{
			mControllingControl->AddListener(this);
			if (mControllingControl->GetValueMessage() == 0)
				mControllingControl->SetValueMessage(msg_Click);
			Update(mControllingControl->GetValue());
		}
		SetMessage(msg_Nothing); // don't call me again
	}
} // CSlaveEnabler::ExecuteSelf

//-----------------------------------
void CSlaveEnabler::ListenToMessage(MessageT inMessage, void* ioParam)
// Listening to our controlling control for when it does a value broadcast
//-----------------------------------
{
	if (mControllingControl && inMessage == mControllingControl->GetValueMessage())
		Update(*(Int32*)ioParam);
} // CSlaveEnabler::ListenToMessage

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

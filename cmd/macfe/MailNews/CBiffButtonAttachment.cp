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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CBiffButtonAttachment.cp

#include "CBiffButtonAttachment.h"

#include "LSharable.h"
#include "CButton.h"
#include "CCheckMailContext.h"

#include <LGAIconSuiteControl.h>

void SetIconState(LControl* inControl, ResIDT inID);

//======================================
#pragma mark --------- class CBiffButtonAttachment
//======================================

//-----------------------------------
CBiffButtonAttachment::CBiffButtonAttachment(LStream* inStream)
//-----------------------------------
:	LAttachment(inStream)
,	mButton(nil)
{
	for (int i = MSG_BIFF_NewMail; i <= MSG_BIFF_Unknown; i++)
		*inStream >> mResIDList[i];
	SetMessage(msg_Nothing); // Don't bother calling me, ever.
	LAttachable	*host = LAttachable::GetDefaultAttachable();
	mButton = dynamic_cast<LControl*>(host);
	Assert_(mButton);
	if (!mButton) return;
#ifndef MOZ_LITE
	CCheckMailContext* theContext = CCheckMailContext::Get();
	theContext->AddUser(this);
	theContext->AddListener(this);
	// Initialize the icon according to the current state!
	SetIconState(mButton, mResIDList[theContext->GetState()]);
#endif // MOZ_LITE
} // CBiffButtonAttachment::CBiffButtonAttachment

//-----------------------------------
CBiffButtonAttachment::~CBiffButtonAttachment()
//-----------------------------------
{
#ifndef MOZ_LITE
	CCheckMailContext* theContext = CCheckMailContext::Get();
	theContext->RemoveListener(this);
	theContext->RemoveUser(this);
#endif // MOZ_LITE
} // CBiffButtonAttachment::~CBiffButtonAttachment

//-----------------------------------
void SetIconState(LControl* inControl, ResIDT inID)
// sets the icon id of the control according to this.
//-----------------------------------
{
	if (!inID)
		return;
	CButton* button = dynamic_cast<CButton*>(inControl);
	if (button)
	{
		// CButton case
		button->SetGraphicID(inID);
	}
	else
	{
		// LGAIconSuiteControl case (may need to support others later...)
		LGAIconSuiteControl* lgaButton = dynamic_cast<LGAIconSuiteControl*>(inControl);
		if (lgaButton)
			lgaButton->SetIconResourceID(inID);
	}
	inControl->Refresh();
}

//-----------------------------------
void CBiffButtonAttachment::ListenToMessage(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	if (inMessage != CCheckMailContext::msg_MailNotificationState)
		return;
	SetIconState(mButton, mResIDList[*(MSG_BIFF_STATE*)ioParam]);
} // CBiffButtonAttachment::ListenToMessage

//-----------------------------------
void CBiffButtonAttachment::ExecuteSelf(
	MessageT	/* inMessage */,
	void*		/* ioParam */)
//-----------------------------------
{
	// We have nothing to do, and because we set our message to msg_Nothing, we'll never
	// be called.  However, I don't want to export this empty method from PP just to satisfy
	// the linker.
}

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


// CProgressListener.cp

#include "CProgressListener.h"

#include "CProgressCaption.h"
#include "CNSContext.h"	// for progress messages
#include "xp.h"

#include <LCaption.h>

#define MIN_TICKS	(60/4)	// Don't refresh the progress bar more than 4x /sec.

//-----------------------------------
CProgressListener::CProgressListener(LView* super, LBroadcaster* context)
//-----------------------------------
{
	Assert_(super);
	Assert_(context);
	mProgressCaption = dynamic_cast<CProgressCaption*>(super->FindPaneByID('Stat'));
	mProgressLastTicks = 0;
	mMessageLastTicks = 0;
	mPercentLastTicks = 0;
	mLaziness = mPreviousLaziness = lazy_NotAtAll;
	Assert_(mProgressCaption);
	context->AddListener(this);
} // CProgressListener::CProgressListener

//-----------------------------------
CProgressListener::~CProgressListener()
//-----------------------------------
{
} // CProgressListener::~CProgressListener()

//-----------------------------------
void CProgressListener::SetLaziness(ProgressBarLaziness inLaziness)
//-----------------------------------
{
	mPreviousLaziness = mLaziness;
	mLaziness = inLaziness;
}

//-----------------------------------
void CProgressListener::ListenToMessage(MessageT inMessage, void *ioParam)
//-----------------------------------
{
	if (!mProgressCaption)
		return;

	switch (inMessage)
	{
		case msg_NSCProgressEnd:
		case msg_NSCAllConnectionsComplete:
		case msg_NSCFinishedLayout:
			if (mLaziness == lazy_VeryButForThisCommandOnly)
				mLaziness = mPreviousLaziness;
			mProgressCaption->SetValue(0);
			mProgressCaption->SetDescriptor("\p");
			break;

		case msg_NSCProgressUpdate:
			if (mLaziness != lazy_VeryButForThisCommandOnly)
			{
				if (::TickCount() - mProgressLastTicks >= MIN_TICKS)
				{
					mProgressLastTicks = ::TickCount(); 
					CContextProgress* progress = (CContextProgress*)ioParam;
					mProgressCaption->SetDescriptor(progress->mMessage);
				}
			}
			break;

		case msg_NSCProgressMessageChanged:
			Boolean displayIt = true;
			if (mLaziness == lazy_JustABit || mLaziness == lazy_VeryButForThisCommandOnly)
			{
				if (::TickCount() - mMessageLastTicks < MIN_TICKS)
					displayIt = false;
			}
			if (displayIt)
			{
				mMessageLastTicks = ::TickCount();
				if (ioParam)
					mProgressCaption->SetDescriptor(TString<Str255>((const char*)ioParam));
				else
					mProgressCaption->SetDescriptor("\p");
			}
			break;

		case msg_NSCProgressPercentChanged:
			if (::TickCount() - mPercentLastTicks >= MIN_TICKS)
			{
				mPercentLastTicks = ::TickCount();
				mProgressCaption->SetValue(*(Int32*)ioParam);
			}
			break;
	}
} // CProgressListener::ListenToMessage

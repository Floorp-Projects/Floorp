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
//		case msg_NSCStartLoadURL:
//		case msg_NSCProgressBegin:
//			// context is busy
//			break;

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

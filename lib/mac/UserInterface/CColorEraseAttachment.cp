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

//	CColorEraseAttachment exists solely to provide stream-based access
//	to LColorEraseAttachment.

#include "CColorEraseAttachment.h"

#include <LStream.h>
#include <UDrawingState.h>

// ---------------------------------------------------------------------------
//		[static data members
// ---------------------------------------------------------------------------

PenState	CColorEraseAttachment::sDummyPenState;

// ---------------------------------------------------------------------------
//		¥ CColorEraseAttachment
// --------------------------------------------------------------------------- 	

CColorEraseAttachment::CColorEraseAttachment(
	LStream*	inStream)
	:	super(&sDummyPenState, nil, nil, true)
{
	Boolean		hostOwnsMe;

	// Read the fore and back colors from the stream
	
	ThrowIfNil_(inStream);
	
	// Read the data from the stream
	
	inStream->ReadData(&mMessage,		sizeof(mMessage));
	inStream->ReadData(&mExecuteHost,	sizeof(mExecuteHost));
	inStream->ReadData(&hostOwnsMe,		sizeof(hostOwnsMe));
	inStream->ReadData(&mForeColor,		sizeof(mForeColor));
	inStream->ReadData(&mBackColor,		sizeof(mBackColor));
	
	mOwnerHost = nil;
	
	LAttachable	*host = LAttachable::GetDefaultAttachable();
	if (host != nil)
	{
		host->AddAttachment(this, nil, hostOwnsMe);
	}
	
	// We ignore the message in the stream. It must *always* be msg_DrawOrPrint
	
	mMessage = msg_DrawOrPrint;

	// Adjust the pen state
	
	LPane* theHostPane = dynamic_cast<LPane*>(host);
	ThrowIfNil_(theHostPane);
	
	GrafPtr thePort = theHostPane->GetMacPort();
	ThrowIfNil_(thePort);
	
	StColorPortState theColorPortState(thePort);
	StColorPortState::Normalize();
}
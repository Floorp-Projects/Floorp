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

#include "CSuspenderResumer.h"

#include <algorithm> // should be <algo>

// ---------------------------------------------------------------------------
//		[static member data]
// ---------------------------------------------------------------------------

list<CSuspenderResumer*>	CSuspenderResumer::sSuspendersAndResumers;// = nil;
Boolean						CSuspenderResumer::sIsSuspended = false;

// ---------------------------------------------------------------------------
//		¥ CSuspenderResumer
// ---------------------------------------------------------------------------

CSuspenderResumer::CSuspenderResumer()
{
	sSuspendersAndResumers.push_back(this);
}

// ---------------------------------------------------------------------------
//		¥ ~CSuspenderResumer
// ---------------------------------------------------------------------------

CSuspenderResumer::~CSuspenderResumer()
{
	CSuspenderResumer* theSuspenderResumer = nil;
	
	list<CSuspenderResumer*>::iterator theItem = find(
													sSuspendersAndResumers.begin(),
													sSuspendersAndResumers.end(),
													theSuspenderResumer);
	if (theItem != sSuspendersAndResumers.end())
	{
		sSuspendersAndResumers.erase(theItem);
	}
}

// ---------------------------------------------------------------------------
//		¥ Suspend
// ---------------------------------------------------------------------------

void
CSuspenderResumer::Suspend()
{
  	sIsSuspended = true;

	list<CSuspenderResumer*>::iterator first	= sSuspendersAndResumers.begin();
	list<CSuspenderResumer*>::iterator last		= sSuspendersAndResumers.end();

    while (first != last)
    {
    	(*first++)->SuspendSelf();
    }
}

// ---------------------------------------------------------------------------
//		¥ Resume
// ---------------------------------------------------------------------------

void
CSuspenderResumer::Resume()
{
  	sIsSuspended = false;

	list<CSuspenderResumer*>::iterator first	= sSuspendersAndResumers.begin();
	list<CSuspenderResumer*>::iterator last		= sSuspendersAndResumers.end();

    while (first != last)
    {
    	(*first++)->ResumeSelf();
    }
}

// ---------------------------------------------------------------------------
//		¥ SuspendSelf
// ---------------------------------------------------------------------------

void
CSuspenderResumer::SuspendSelf()
{
}

// ---------------------------------------------------------------------------
//		¥ ResumeSelf
// ---------------------------------------------------------------------------

void
CSuspenderResumer::ResumeSelf()
{
}
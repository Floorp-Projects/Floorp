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

#include "msg.h"
#include "chngntfy.h"

ChangeAnnouncer::ChangeAnnouncer()
{
}

ChangeAnnouncer::~ChangeAnnouncer()
{
	XP_ASSERT(GetSize() == 0); // better not be any listeners, because we're going away.
}

XP_Bool ChangeAnnouncer::AddListener(ChangeListener *listener)
{
	XPPtrArray::Add((void *) listener);
	return TRUE;
}

XP_Bool ChangeAnnouncer::RemoveListener(ChangeListener *listener)
{
	for (int i = 0; i < GetSize(); i++)
	{
		if ((ChangeListener *) GetAt(i) == listener)
		{
			RemoveAt(i);
			return TRUE;
		}
	}
	return FALSE;
}

// convenience routines to notify all our ChangeListeners
void ChangeAnnouncer::NotifyViewChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator)
{
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnViewChange(startIndex, numChanged, changeType, 
			instigator); 
	}
}

// convenience routines to notify all our ChangeListeners
void ChangeAnnouncer::NotifyViewStartChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator)
{
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnViewStartChange(startIndex, numChanged, changeType, 
			instigator); 
	}
}

// convenience routines to notify all our ChangeListeners
void ChangeAnnouncer::NotifyViewEndChangeAll(MSG_ViewIndex startIndex, int32 numChanged, 
		MSG_NOTIFY_CODE changeType, ChangeListener *instigator)
{
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnViewEndChange(startIndex, numChanged, changeType, 
			instigator); 
	}
}


void ChangeAnnouncer::NotifyKeyChangeAll(MessageKey keyChanged, int32 flags, 
	ChangeListener *instigator)
{
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnKeyChange(keyChanged, flags, instigator); 
	}
}

void ChangeAnnouncer::NotifyAnnouncerGoingAway(ChangeAnnouncer *instigator)
{
	if (instigator == NULL)
		instigator = this;	// not sure about this - should listeners even care?
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnAnnouncerGoingAway(instigator); 
	}
}

void ChangeAnnouncer::NotifyAnnouncerChangingView(ChangeAnnouncer *instigator, MessageDBView *newView)
{
	if (instigator == NULL)
		instigator = this;	// not sure about this - should listeners even care?
	for (int i = 0; i < GetSize(); i++)
	{
		ChangeListener *changeListener = (ChangeListener *) GetAt(i);

		changeListener->OnAnnouncerChangingView(instigator, newView); 
	}
}

ChangeListener::ChangeListener()
{
}

ChangeListener::~ChangeListener()
{
}

// virtual inlines...
void ChangeListener::OnViewChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/) {}
void ChangeListener::OnViewStartChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/) {}
void ChangeListener::OnViewEndChange(MSG_ViewIndex /*startInd*/, int32 /*numChanged*/, 
		MSG_NOTIFY_CODE /* changeType */, ChangeListener * /*instigator*/) {}
void ChangeListener::OnKeyChange(MessageKey /*keyChanged*/, int32 /*flags*/, 
		ChangeListener * /*instigator*/) {}
void ChangeListener::OnAnnouncerGoingAway(ChangeAnnouncer * /*instigator*/) {}
void ChangeListener::OnAnnouncerChangingView(ChangeAnnouncer * /* instigator */, MessageDBView * /* view */) {}




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

#include "xp.h"
#include "abntfy.h"

/***************************************************************************

  Generic AB_ChangeAnnouncer definitions

****************************************************************************/

AB_ChangeAnnouncer::AB_ChangeAnnouncer()
{
}

AB_ChangeAnnouncer::~AB_ChangeAnnouncer()
{
	XP_ASSERT(GetSize() == 0);  // we better be empty since we are going away!
}

XP_Bool AB_ChangeAnnouncer::AddListener(AB_ChangeListener * listener)
{
	XP_Bool found = FALSE;
	int i;
	for (i = 0; i < GetSize(); i++)
		if (GetAt(i) == listener)
			found = TRUE;

	if (!found)
		Add(listener);  // we should make sure we don't add a container twice!!

	return !found;  // if we found it then we didn't add it as a listener...
}

XP_Bool AB_ChangeAnnouncer::RemoveListener(AB_ChangeListener * listener)
{
	// add insures we never have any duplicates so we don't need to worry about that case here. 

	int i = 0;
	for (i = 0; i < GetSize(); i++)
	{
		if (GetAt(i) == listener)
		{
			RemoveAt(i);
			return TRUE;
		}
	}
	return FALSE;
}

/***************************************************************************

  Generic AB_ChangeListener definitions

****************************************************************************/

AB_ChangeListener::AB_ChangeListener()
{
}

AB_ChangeListener::~AB_ChangeListener()
{
}

/***************************************************************************

  AB_ChangeAnnouncer subclass: AB_ContainerAnnouncer definitions.

****************************************************************************/

AB_ContainerAnnouncer::AB_ContainerAnnouncer() : AB_ChangeAnnouncer()
{
}

AB_ContainerAnnouncer::~AB_ContainerAnnouncer()
{
}

// Announcer Notification methods
void AB_ContainerAnnouncer::NotifyContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator)
{
	int i = 0;
	for (i = 0; i < GetSize(); i++)
	{
		AB_ContainerListener * listener = GetAt(i);
		if (listener)
			listener->OnContainerAttribChange(ctr, code, instigator);
	}
}

void AB_ContainerAnnouncer::NotifyContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, MSG_ViewIndex index, ABID entryID, AB_ContainerListener * instigator)
{
	int i = 0;
	for (i = 0; i < GetSize(); i++)
	{
		AB_ContainerListener * listener = GetAt(i);
		if (listener) // we don't know if it is an index or entryID based listener so notify them both. Only one function is implemented per listener...
		{
			listener->OnContainerEntryChange(ctr, code, entryID, instigator);
			listener->OnContainerEntryChange(ctr, code, index, 1, instigator);
		}
	}
}

void AB_ContainerAnnouncer::NotifyContainerEntryRangeChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, MSG_ViewIndex index, int32 numChanged, AB_ContainerListener * instigator)
{
	int i = 0;
	for (i = 0; i < GetSize(); i++)
	{
		AB_ContainerListener * listener = GetAt(i);
		if (listener) // only view index based listeners get these notifications...
			listener->OnContainerEntryChange(ctr, code, index, numChanged, instigator);
	}

}

void AB_ContainerAnnouncer::NotifyAnnouncerGoingAway(AB_ContainerAnnouncer * instigator)
{
	if (instigator == NULL)
		instigator = this;
	
	int i;
	for (i = 0; i < GetSize(); i++)
	{
		AB_ContainerListener * listener = GetAt(i);
		if (listener)
			listener->OnAnnouncerGoingAway(instigator);
	}
}

/***************************************************************************

  AB_ChangeListener subclass: AB_ContainerListener definitions.

****************************************************************************/

AB_ContainerListener::AB_ContainerListener() : AB_ChangeListener()
{
}

AB_ContainerListener::~AB_ContainerListener()
{
}

// virtual inlines

void AB_ContainerListener::OnContainerAttribChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, AB_ContainerListener * /* instigator */)
{}

void AB_ContainerListener::OnContainerEntryChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, ABID /* entryID */, AB_ContainerListener * /* instigator */)
{}

void AB_ContainerListener::OnContainerEntryChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, MSG_ViewIndex /* index */, int32 /* numChanged */, AB_ContainerListener * /* instigator */)
{}

void AB_ContainerListener::OnAnnouncerGoingAway(AB_ContainerAnnouncer * /*instigator*/)
{}

AB_IndexBasedContainerListener::AB_IndexBasedContainerListener() : AB_ContainerListener()
{}

AB_IndexBasedContainerListener::~AB_IndexBasedContainerListener()
{}

AB_ABIDBasedContainerListener::AB_ABIDBasedContainerListener() : AB_ContainerListener()
{}

AB_ABIDBasedContainerListener::~AB_ABIDBasedContainerListener()
{}

/***************************************************************************

  AB_ChangeAnnouncer subclass: AB_PaneAnnouncer definitions.

****************************************************************************/

AB_PaneAnnouncer::AB_PaneAnnouncer() : AB_ChangeAnnouncer()
{
}

AB_PaneAnnouncer::~AB_PaneAnnouncer()
{
}

void AB_PaneAnnouncer::NotifyAnnouncerGoingAway(AB_PaneAnnouncer * instigator)
{
	if (instigator == NULL)
		instigator = this;
	
	int i;
	for (i = 0; i < GetSize(); i++)
	{
		AB_PaneListener * listener = GetAt(i);
		if (listener)
			listener->OnAnnouncerGoingAway(instigator);
	}
}

/***************************************************************************

  AB_ChangeListener subclass: AB_PaneListener definitions.

****************************************************************************/

AB_PaneListener::AB_PaneListener() : AB_ChangeListener()
{
}

AB_PaneListener::~AB_PaneListener()
{
}

void AB_PaneListener::OnAnnouncerGoingAway(AB_PaneAnnouncer * /*instigator*/)
{
}


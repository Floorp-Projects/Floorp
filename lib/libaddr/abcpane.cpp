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

#include "abcpane.h"
#include "abcinfo.h"
#include "abpane2.h"
#include "xp_qsort.h"

extern "C"
{
extern int MK_MSG_ADD_NAME;
extern int MK_MSG_ADD_LIST;
extern int XP_BKMKS_IMPORT_ADDRBOOK;
extern int XP_BKMKS_SAVE_ADDRBOOK;
extern int MK_MSG_CALL;
extern int MK_ADDR_NO_EMAIL_ADDRESS; 
extern int MK_ADDR_DEFAULT_EXPORT_FILENAME;
extern int MK_ADDR_ADD;
extern int MK_MSG_ADDRESS_BOOK;
extern int MK_ACCESS_NAME;
extern int MK_MSG_PROPERTIES;
extern int XP_FILTER_DELETE;
extern int MK_ADDR_DELETE_ALL;
}


/*********************************************************************************************
	 Notification handlers
**********************************************************************************************/

void AB_ContainerPane::OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator)
{
	// do we need to remove the container from our view? Probably not as this will generate needless notifications
	// since the container can only go away if we have released our reference count to it. Which in turn means that
	// we are going away. Thus, no need to remove the container from our view...but we do want to remove ourselves
	// as a listener to the announcer for clean up purposes..

	// determine if the ctr is in the ctr pane. If it is, remove and delete it
	MSG_ViewIndex index = m_containerView.FindIndex (0, (void *) instigator);
	if (index != MSG_VIEWINDEXNONE) // did it find a match?
	{
		AB_ContainerInfo * ctrToRemove = GetContainerForIndex((MSG_ViewIndex) index);
		if (ctrToRemove)
		{
			StartingUpdate(MSG_NotifyInsertOrDelete, (MSG_ViewIndex) index, -1);
			m_containerView.RemoveAt(index);
			EndingUpdate(MSG_NotifyInsertOrDelete, (MSG_ViewIndex) index, -1);
		
			ctrToRemove->RemoveListener(this);
			ctrToRemove->Release();
		}
	}
}

void AB_ContainerPane::OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator)
{
	if (instigator != this) // if we changed it then don't generate another update
	{
		// find the index for the ctr
		MSG_ViewIndex index = GetIndexForContainer(ctr);
		if (index != MSG_VIEWINDEXNONE)  // does it really exist?
		{
			if (code == AB_NotifyPropertyChanged)
			{
				// update the front ends for this view index
				StartingUpdate(MSG_NotifyChanged, index, 1);
				EndingUpdate(MSG_NotifyChanged, index, 1);
			}
		}

	}
	return;
}

void AB_ContainerPane::OnContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator)
{
	// do we have to do anything if the entry was a mailing list? It might effect something in the container pane....
	// need to find out if the effected entry is a mailing list currently in the container pane

	if (instigator != this)
	{
		MSG_ViewIndex index = GetIndexForContainer(ctr);  // make sure we care about this container...

		if (index != MSG_VIEWINDEXNONE /* && IsExpanded(index) */) // only time ctr is showing entry content: when it is expanded
		{
			XP_Bool isAList = FALSE;
			AB_AttributeValue * value = NULL;
			switch (code)
			{
				case AB_NotifyInserted:
					if (ctr && entryID != AB_ABIDUNKNOWN)
						ctr->GetEntryAttribute(this, entryID, AB_attribEntryType, & value);
					if (value && value->u.entryType == AB_MailingList)
					{
						isAList = TRUE;
						AB_FreeEntryAttributeValue(value);
					}
					if (isAList)
					{
						StartingUpdate(MSG_NotifyChanged, index, 1);
						// if entry was a list, we need to insert it into the view
						if (IsExpanded(index))
							InsertEntryIntoExpandedList(index, ctr, entryID);
						EndingUpdate(MSG_NotifyChanged, index, 1);
					}
					break;
				case AB_NotifyAll:
				case AB_NotifyDeleted: // if an entry was deleted from the ctr it may effect the flippy state of the ctr
					StartingUpdate(MSG_NotifyChanged, index, 1);
					EndingUpdate(MSG_NotifyChanged, index, 1);
					break;
				default:
					break;
			}
		}
	}
}

void AB_ContainerPane::DeleteEntryFromExpandedList(MSG_ViewIndex index, AB_ContainerInfo * ctr, ABID listID)
{
	// make sure listID is a list or else you will be wasting your time.
	if (index != MSG_VIEWINDEXNONE && ctr && listID != AB_ABIDUNKNOWN && IsExpanded(index))
	{
		uint32 numVisibleChildren = NumVisibleChildrenForIndex(index);

		// get array of child ctrs
		int32 numChildren = GetNumChildrenForIndex(index);
		AB_ContainerInfo ** arrayOfContainers = (AB_ContainerInfo **) XP_ALLOC(sizeof(AB_ContainerInfo *) * numChildren);
		if (arrayOfContainers)
		{
			ctr->AcquireChildContainers(arrayOfContainers, &numChildren); // acquire containers for the children
			uint32 cnt = 0;
			for (MSG_ViewIndex pos = index + 1; pos < (uint32) m_containerView.GetSize() && cnt < numVisibleChildren; cnt++, pos++)
			{
				AB_ContainerInfo * visibleCtr = GetContainerForIndex(pos);
				// search for ctr in child ctr array
				XP_Bool found = FALSE;
				for (int32 i = 0; i < numChildren; i++)
					if (arrayOfContainers[i] == visibleCtr)
						found = TRUE;

				if (!found && visibleCtr) // then list has gone away so remove it...
				{
					StartingUpdate(MSG_NotifyInsertOrDelete, pos, -1);
					m_containerView.RemoveAt(pos);
					visibleCtr->RemoveListener(this);  // register ourselves as a listener on the container...
					visibleCtr->Release();
					EndingUpdate(MSG_NotifyInsertOrDelete, pos, -1);
					pos--; // decrement position because we removed something from the view
				}
			}
			
			// release child ctrs
			for (int32 i = 0; i < numChildren; i++)
				if (arrayOfContainers[i])
					arrayOfContainers[i]->Release();
			XP_FREE(arrayOfContainers);
		}
			
	}
	
}

uint32 AB_ContainerPane::NumVisibleChildrenForIndex(MSG_ViewIndex index)
// a  helper function used by InsertEntryIntoExpandedList and DeleteEntryFromExpandedList to determine how many visible children
// are currently showing in the view. 
{
	// children are always mailing lists....
	uint32 numVisibleChildren = 0;
	XP_Bool finished = FALSE;
	for (MSG_ViewIndex pos = index+1; pos < (uint32) m_containerView.GetSize() && !finished; pos++)
	{
		AB_ContainerInfo * child = GetContainerForIndex(pos);
		if (child && child->IsDirectory())
			finished = TRUE;
		else numVisibleChildren++;
	}

	return numVisibleChildren;
}

void AB_ContainerPane::InsertEntryIntoExpandedList(MSG_ViewIndex index /* index for ctr */, AB_ContainerInfo * ctr, ABID listID)
{
	// you should make sure entryID is a mailing list inside of ctr b4 calling this.
	// you woon't get burned if you don't, but you will be wasting your time....
	// *sigh* I think this function is way too complicated for its function but cannot think of a 
	// faster way
	if (index != MSG_VIEWINDEXNONE && ctr && listID != AB_ABIDUNKNOWN)
	{
		// get child array for ctr, find ctr with our entryID. its position in the array 
		// determines where it should be inserted into the expanded list...

		// first, determine number of children currently in the view...
		uint32 numVisibleChildren = NumVisibleChildrenForIndex(index);

		// find position of inserted entry in the list
		AB_ContainerInfo * newCtr = NULL;
		AB_ContainerInfo::Create(m_context, ctr, listID, &newCtr);
		
		// make sure newCtr is not already in our container pane...this can happen when you add a new mailng list
		// to another mailing list. We get an insert from the address book and we get an insert from the mailing 
		// list ctr we are adding it to....
		MSG_ViewIndex currentIndex = GetIndexForContainer(newCtr);

		if (newCtr && currentIndex == MSG_VIEWINDEXNONE) // does the new mailing list really exist and is it not in our view alreaady
		{
			// now get the child ctrs for the ctr
			int32 numChildren = GetNumChildrenForIndex(index);
			AB_ContainerInfo ** arrayOfContainers = (AB_ContainerInfo **) XP_ALLOC(sizeof(AB_ContainerInfo *) * numChildren);
			if (arrayOfContainers)
			{
				ctr->AcquireChildContainers(arrayOfContainers, &numChildren);		   // acquire containers for the childre
				// find position of our new ctr in the child array...
				uint32 offset = 0;
				XP_Bool found = FALSE;
				for (offset = 0; offset < (uint32) numChildren && !found; offset++)
					if (arrayOfContainers[offset] == newCtr) // same object?
						found = TRUE;

				if (found) // if it wasn't in the list then something very bad happened
				{
					if (offset > numVisibleChildren)  // reset our offset if it is > than the # children currently showing.
						offset = numVisibleChildren;
					MSG_ViewIndex insertPosition = offset + index + 1; // start with index for first child ctr & add our offset to this
					StartingUpdate(MSG_NotifyInsertOrDelete, insertPosition, 1);
					m_containerView.InsertAt(insertPosition, (void *) newCtr);
					newCtr->AddListener(this);  // register ourselves as a listener on the container...
					EndingUpdate(MSG_NotifyInsertOrDelete, insertPosition,1);
				}
				else
					newCtr->Release();
				// now release all acquired children
				for (int32 i = 0; i < numChildren; i++)
					if (arrayOfContainers[i])
						arrayOfContainers[i]->Release();

				XP_FREE(arrayOfContainers);
			}
		}
		else
			if (newCtr)
				newCtr->Release();
	}
}


AB_ContainerPane::AB_ContainerPane(MWContext * context, MSG_Master * master) : MSG_LinedPane(context, master), AB_ABIDBasedContainerListener(), AB_PaneAnnouncer()
{
	m_context = context;
	m_master = master;
}

AB_ContainerPane::~AB_ContainerPane()
{
	NotifyAnnouncerGoingAway(this);  // let any pane listener's know we are going away! i.e. any property sheets opened from the ctr pane

	// we are no longer listeners on the containers since we are going away,
	// let's remove ourselves!

	for (int i = 0; i < m_containerView.GetSize(); i++)
	{
		AB_ContainerInfo * ctr = m_containerView.GetAt(i);
		if (ctr)
		{
			ctr->RemoveListener(this);
			ctr->Release();  // release our reference count to this container
		}
	}

	// make sure our new directory server cache is empty!
	for (int j = 0; j < m_newEntries.GetSize(); j++)
	{
		AB_NewDIREntry * entry = m_newEntries.GetAt(j);
		if (entry)
			XP_FREE(entry);
	}

}

int AB_ContainerPane::Close(AB_ContainerPane * pane)
{
	if (pane)
		delete pane;
	return AB_SUCCESS;
}


/* static */ void AB_ContainerPane::Create(MSG_Pane ** abcPane, MWContext * context, MSG_Master * master)
{
	AB_ContainerPane * newPane = new AB_ContainerPane(context, master);
	(*abcPane) = newPane;
}

int AB_ContainerPane::Initialize()
{
	// how do we get the global list of DIR_Servers?? 
	XP_List * dirServers = DIR_GetDirServers();  // get ref counted list of the dir servers...
	int errorCode = AB_SUCCESS;

	DIR_Server * server = NULL;

	do
	{
		server = (DIR_Server *) XP_ListNextObject(dirServers);
		if (server)
			errorCode = AddDirectory(server);
	} while (server);
	
	// if we had a memory problem or something, we'll catch the last one and return it. 
	return errorCode;  // will eventually load up the global containers from the database
}

int AB_ContainerPane::GetNumRootContainers(int32 * numRootContainers)
{
	// eventually we want to go through all containers in view and count all root ones...

	// for now, all ctrs in the pane are "root" container because I'm not supporting mailing lists
	// yet. 
	int numRootCtrs = 0;
	for (int i = 0; i < m_containerView.GetSize(); i++)
		if (m_containerView.GetAt(i)->IsDirectory())  // directory containers are root containers!
			numRootCtrs++;

	*numRootContainers = numRootCtrs;
	return AB_SUCCESS;
}

int AB_ContainerPane::GetOrderedRootContainers(AB_ContainerInfo ** ctrArray, int32 * numCtrs)
{
	// will eventually traverse all containers in the view and insert container info into array
	// if it is a root container...

	// FE has allocated this array so just fill it.
	int32 i;
	int32 numAdded = 0; 
	for (i = 0; i < m_containerView.GetSize() && numAdded < *numCtrs; i++)
		if (m_containerView.GetAt(i)->IsDirectory())
			ctrArray[numAdded++] = m_containerView.GetAt(i);

	*numCtrs = numAdded;
	return AB_SUCCESS;
}

void AB_ContainerPane::NotifyPrefsChange (NotifyCode /* code */)
{
	return;
}

MSG_ViewIndex AB_ContainerPane::GetIndexForContainer(AB_ContainerInfo * ctr)
{
	if (ctr)
	{
		// look for the pointer in our view
		for (int i = 0; i < m_containerView.GetSize(); i++)
			if (ctr == m_containerView.GetAt(i))
				return i;  // found the index!
	}
	return MSG_VIEWINDEXNONE;
}

AB_ContainerInfo * AB_ContainerPane::GetContainerForIndex(const MSG_ViewIndex index)
{
	// look for the index in our container view.
	if (m_containerView.IsValidIndex(index))
		return m_containerView.GetAt(index);
	else
		return NULL;
}

int AB_ContainerPane::UpdateDIRServer(DIR_Server * directory)
{
	AB_ContainerInfo * ctr;
	XP_Bool match = FALSE;
	int errorCode = AB_SUCCESS;

	// examine the DIR_Server for each container in the pane
	for (int i = 0; i < m_containerView.GetSize() && !match; i++)
	{
		ctr = m_containerView.GetAt(i);
		if (ctr->IsDirectory())  // PAB or LDAP
			if (directory == ctr->GetDIRServer())
			{
				match = TRUE;
				ctr->UpdateDIRServer();
			}
	}

	if (!match)  // if we didn't find it, we need to create a new container!
		errorCode = AddNewDirectory(directory);
	return errorCode;
}

// always use this routine when adding directories AFTER we have already built the container pane..
// that is any new directories that need turned into containers and added to the DIR_Server list.
int AB_ContainerPane::AddNewDirectory(DIR_Server * directory)  // add this to the end of the container pane
{
	AB_ContainerInfo * ctr = NULL;
	DIR_SetServerFileName(directory, directory->fileName); // generates a file name if there isn't one already
	int errorCode = AB_ContainerInfo::Create(m_context, directory, &ctr); // all ctr info's are referenced counted
	if (errorCode == AB_SUCCESS)
	{
		// look up dir server in our cache to get the positiion
		MSG_ViewIndex position = GetDesiredPositionForNewServer(directory);
		RemoveFromNewServerCache(directory);
		AddNewContainer(position, ctr);
	}

	return errorCode;
}

// Always use this routine when all we want to do is turn directory into a container and add it to the end of
// the view. It is only called during Initialization!!! DIR_Server list is untouched!! 
// USE AddNewDirectory to add directories not part of the initialization process!
int AB_ContainerPane::AddDirectory(DIR_Server * directory)
{
	AB_ContainerInfo * ctr = NULL;
	int errorCode = AB_ContainerInfo::Create(m_context, directory, &ctr); // all ctr info's are referenced counted
	int position = m_containerView.GetSize();
	if (ctr)
	{
		StartingUpdate(MSG_NotifyInsertOrDelete, position, 1);
		m_containerView.Add(ctr);
		ctr->AddListener(this);
		EndingUpdate(MSG_NotifyInsertOrDelete, position, 1);
	}

	return errorCode;
}

int AB_ContainerPane::GetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribute * attribsArray, AB_ContainerAttribValue ** valuesArray,
											 uint16 * numItems)
{
	AB_ContainerInfo * ctr = GetContainerForIndex(index);
	if (ctr)
		return ctr->GetAttributes(attribsArray, valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}

int AB_ContainerPane::SetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribValue * valuesArray, uint16 numItems)
{
	AB_ContainerInfo *ctr = GetContainerForIndex(index);
	if (ctr)
		return ctr->SetAttributes(valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}


MSG_PaneType AB_ContainerPane::GetPaneType()
{
	return AB_CONTAINERPANE;
}

int32 AB_ContainerPane::GetNumChildrenForIndex(MSG_ViewIndex line)
{
	AB_ContainerInfo * ctr = GetContainerForIndex(line);
	if (ctr)
		return ctr->GetNumChildContainers();  // we could go throug the attribute value methods but this is faster..
	else
		return 0;
}

void AB_ContainerPane::ToggleExpansion(MSG_ViewIndex line, int32 * numChanged)
{
	StartingUpdate(MSG_NotifyChanged, line, 1);

	if (IsExpanded(line))
		CollapseContainer(line, numChanged);
	else
		ExpandContainer(line, numChanged);

	// now do a notify changed on the modified line...
	EndingUpdate(MSG_NotifyChanged, line, 1);
}

// returns TRUE if the container is expanded. FALSE otherwise.
XP_Bool AB_ContainerPane::IsExpanded(MSG_ViewIndex line)
{
	AB_ContainerInfo * ctr = GetContainerForIndex(line);
	if (ctr)
	{
		AB_ContainerInfo * nextCtr = GetContainerForIndex(++line);
		// I'm going to try a quick hack..if the next ctr is a mailing list then it must be expanded already
		// Why? because non directory containers are the only ctrs which are not root containers.
		// If it doesn't work, we can always write a more complicated method here..
		if (nextCtr && !nextCtr->IsDirectory())
			return TRUE;  // next ctr is a non-directory ctr so we must be expanded.
	}
	
	return FALSE;
}

int32 AB_ContainerPane::ExpansionDelta(MSG_ViewIndex line)
{
	AB_ContainerInfo * ctr = GetContainerForIndex(line);
	int32 numChildren = 0;

	if (ctr) // does it exist?
	{
		// how many children does the container have....
		numChildren = GetNumChildrenForIndex(line);
		if (numChildren > 0)
		{
			if (IsExpanded(line))
				return - (numChildren);
			else
				return numChildren;
		}
	}
	return 0;  // if we fell out then assume 0 children
}

int32 AB_ContainerPane::GetNumLines()
{
	return m_containerView.GetSize();
}

int32 AB_ContainerPane::GetContainerMaxDepth()
{
	return 0;
}

void AB_ContainerPane::CollapseContainer(MSG_ViewIndex line, int32 * numChanged)
{
	int32 changedCount = 0; // # ctrs removed...

	// first, make sure it is expanded..redundant call I know, but you never know!
	if (IsExpanded(line))
	{
		// how many children does the container have....
		int32 numChildren = GetNumChildrenForIndex(line);
		if (numChildren > 0)
		{
			// okay, remove each ctr from the view and release it...
			StartingUpdate(MSG_NotifyInsertOrDelete, line+1, -1 * numChildren);
			MSG_ViewIndex removeLine = line+1;
			for (int32 i = 0; i < numChildren; i++)
			{
				AB_ContainerInfo * childCtr = GetContainerForIndex(removeLine); // remove line doesn't change...
				if (childCtr)
				{
					m_containerView.Remove(childCtr);
					childCtr->RemoveListener(this);
					childCtr->Release();  // we are done with it...release it
					changedCount++;
				}
			}
			EndingUpdate(MSG_NotifyInsertOrDelete, line+1, -1 * numChildren);
		}

	}
	
	if (numChanged)
		*numChanged = changedCount;
}

void AB_ContainerPane::ExpandContainer(MSG_ViewIndex line, int32 * /*numChanged*/)
{
	// we are going to write this method to assume that the line could already be expanded, 
	// but the expansion may not be up to date. (More or fewer). So we will generate 
	// individual notifications as lines are added or removed during the expansion. 
	AB_ContainerInfo * ctr = GetContainerForIndex(line);

	if (!IsExpanded(line) && ctr)  // make sure we aren't already expanded
	{
		int32 numChildren = GetNumChildrenForIndex(line);
		// we need to get ctr info's for all of the children!
		if (numChildren > 0)
		{
			AB_ContainerInfo ** arrayOfContainers = (AB_ContainerInfo **) XP_ALLOC(sizeof(AB_ContainerInfo *) * numChildren);
			if (arrayOfContainers)
			{
				MSG_ViewIndex startPosition = line+1;

				StartingUpdate(MSG_NotifyInsertOrDelete, startPosition, numChildren);  // generate notification for FEs
				ctr->AcquireChildContainers(arrayOfContainers, &numChildren);		   // acquire containers for the childre

				for (int i = 0; i < numChildren; i++)  // add each container to our pane
					if (arrayOfContainers[i])
					{
						m_containerView.InsertAt(++line, (void *) arrayOfContainers[i]);
						((AB_ContainerInfo *) arrayOfContainers[i])->AddListener(this); // add ourselves to the listener list
					}

				EndingUpdate(MSG_NotifyInsertOrDelete, startPosition, numChildren);
				XP_FREE(arrayOfContainers);
			}
		}
	}
}

XP_Bool AB_ContainerPane::AddToNewServerCache(DIR_Server * server, MSG_ViewIndex desiredPosition)
// we just created a new DIR_Server. It will be committed to the view at some later time, but for now
// we want to save the position where it should be inserted!
{
	AB_NewDIREntry * entry = (AB_NewDIREntry *) XP_ALLOC(sizeof(AB_NewDIREntry));
	if (entry)
	{
		entry->server = server;
		entry->desiredPosition = desiredPosition;

		m_newEntries.Add(entry);
		return TRUE;
	}

	return FALSE;
}

XP_Bool AB_ContainerPane::RemoveFromNewServerCache(DIR_Server * server)
{
	for (int i = 0; i < m_newEntries.GetSize(); i++)
	{
		AB_NewDIREntry * entry = m_newEntries.GetAt(i);
		if (entry)
			if (entry->server == server)
			{
				m_newEntries.RemoveAt(i);
				XP_FREE(entry);
				return TRUE;
			}
	}
	return FALSE;
}

MSG_ViewIndex AB_ContainerPane::GetDesiredPositionForNewServer(DIR_Server * server)
{
	if (server)
	{
		for (int i = 0; i < m_newEntries.GetSize(); i++)
		{
			AB_NewDIREntry * entry = m_newEntries.GetAt(i);
			if (entry)
				if (entry->server == server)
					return entry->desiredPosition;
		}
	}

	return MSG_VIEWINDEXNONE;
}

MSG_ViewIndex AB_ContainerPane::ExtractInsertPosition(MSG_ViewIndex desiredPosition)
{
	// now find out if known position is a directory or not...if not advance until we hit the end or we hit
	// a directory container (LDAP or PAB)
	XP_Bool found = FALSE;
	MSG_ViewIndex position = desiredPosition;
	position++; // we want to insert it after whatever the selected position was...
	AB_ContainerInfo * insertAfterCtr = NULL;
	while (m_containerView.IsValidIndex(position) && !found && position != MSG_VIEWINDEXNONE)
	{
		insertAfterCtr = GetContainerForIndex(position);
		if (insertAfterCtr && insertAfterCtr->IsDirectory())
			found = TRUE;
		else
			position++;
	}

	if (m_containerView.IsValidIndex(position) && position != MSG_VIEWINDEXNONE)  // are we still a valid insertion index?
		return position;
	else
		return MSG_VIEWINDEXNONE;
}

  DIR_Server * AB_ContainerPane::ExtractServerPosition(MSG_ViewIndex desiredPosition)
 {
	// okay, we need to find the directory container which is at or comes before the desired position
	XP_Bool found = FALSE;
	MSG_ViewIndex position = desiredPosition;
	AB_ContainerInfo * insertAfterCtr = NULL;
	while (m_containerView.IsValidIndex(position) && position != MSG_VIEWINDEXNONE && !found)
	{
		insertAfterCtr = GetContainerForIndex(position);
		if (insertAfterCtr && insertAfterCtr->IsDirectory())
			found = TRUE;
		else
			position--;  // go back to the previous container
	}

	if (found)
		return insertAfterCtr->GetDIRServer();
	else
		return NULL;  // will insert at bottom of the list
}


void AB_ContainerPane::AddNewContainer(MSG_ViewIndex position, AB_ContainerInfo * ctr)
// Call this when you want to insert a new PAB or LDAP directory into the container pane. We attempt to 
// insert the container in the position AFTER the one provided. This method 
// also modifies the DIR_Servers list, adding the new directory as well.Assumes ctr has already been ref counted!!!!!
{
	if (ctr)
	{
		if (ctr->IsDirectory())
		{
			// okay, finding the insertion point is complicated by the fact that we must insert after the last
			// directory container which comes after the position index and not just the container the position index
			// points too.

			// we have two positions: (1) position in the DIR_Server list (not the same as the ctr pane view which shows
			// mailing lists and (2) the index in the ctr pane to insert the new ctr into

			DIR_Server * insertAfter= ExtractServerPosition(position);
			XP_List * servers = DIR_GetDirServers();
			XP_ListInsertObjectAfter(servers, insertAfter, ctr->GetDIRServer());  // add to DIR_Server List
			// now save the list...
			DIR_SaveServerPreferences(servers);
		}
	
		
		MSG_ViewIndex insertPosition = ExtractInsertPosition(position);
		// now add it to our container view as well.
		insertPosition = insertPosition == MSG_VIEWINDEXNONE ? m_containerView.GetSize() : insertPosition;
		StartingUpdate(MSG_NotifyInsertOrDelete, insertPosition, 1);
		m_containerView.InsertAt(insertPosition, (void *) ctr);
		ctr->AddListener(this);  // register ourselves as a listener on the container...
		EndingUpdate(MSG_NotifyInsertOrDelete, insertPosition,1);
	}
}

int AB_ContainerPane::ShowProperties(MSG_ViewIndex * indices, int32 numIndices)
{
	for (int32 i = 0; i < numIndices; i++)
	{
		AB_ContainerInfo * ctr = GetContainerForIndex(indices[i]);
		if (ctr)
			if (ctr->GetType() == AB_MListContainer)
				ShowPropertySheet(&indices[i], 1);
			else
				ShowDirectoryProperties(&indices[i], 1);
	}
	return AB_SUCCESS;
}

int AB_ContainerPane::ShowDirectoryProperties(MSG_ViewIndex * indices, int32 numIndices)
{
	// general comment: we don't care if the directory is a PAB or LDAP directory. They all look the
	// same at this level. 
	if (m_DirectoryPropSheetFunc)
	{
		for (int i = 0; i < numIndices; i++)
		{
			AB_ContainerInfo * ctr = GetContainerForIndex(indices[i]);
			if (ctr)
				if (ctr->IsDirectory())
				{
					DIR_Server * server = ctr->GetDIRServer();
					if (server)
						m_DirectoryPropSheetFunc(server, m_context, this, FALSE);
				}
		}
	}

	return AB_SUCCESS;
}

int AB_ContainerPane::ShowNewDirectoryProperties(AB_CommandType cmd, MSG_ViewIndex * indices, int32 numIndices)
{
	// if numIndices > 1 then stick them at the end....
	// if we have zero indices, stick them at the end...
	// if we have just one index selected, insert it underneath the given index
	MSG_ViewIndex position = MSG_VIEWINDEXNONE;
	if (numIndices == 1)
		position = indices[0];

	// can only show one new directory at a time
	if (m_DirectoryPropSheetFunc)  // make sure we have a FE call back!
	{
		DIR_Server * server = (DIR_Server *) XP_ALLOC(sizeof(DIR_Server));
		if (server)
		{
			DIR_InitServer(server);
			server->description = NULL;  // FE will fill this field in
			
			// only valid commands coming in here are AB_NewLDAPDirectory or AB_NewPABDirectory
			server->dirType = (cmd == AB_NewLDAPDirectory) ? LDAPDirectory : PABDirectory;
			server->isOffline = (cmd == AB_NewLDAPDirectory) ? TRUE : FALSE; // set offline to false for PABs...
			// add the server and position pair to our cache, so we will remember when we go to really add it!!!
			AddToNewServerCache(server, position);
			m_DirectoryPropSheetFunc(server, m_context, this, TRUE /* we are a new server */);
		}
		
	}

	return AB_SUCCESS;
}

void AB_ContainerPane::RemoveAndCloseDirectory(DIR_Server * server)
{
	if (server)
	{
		XP_List * servers = DIR_GetDirServers();
		XP_ListRemoveObject(servers, server); // removes from original DIR_Server list
		// the question is....does anyone need to free the DIR_Server object now that we 
		// are all done with it? 
		DIR_DeleteServer(server);
		DIR_SaveServerPreferences(servers);      // make sure the removal is committed 
	}

}

int AB_ContainerPane::DeleteContainers(MSG_ViewIndex * indices, int32 numIndices)
{
	for (int32 i = 0; i < numIndices; i++)
	{
		AB_ContainerInfo * ctr = GetContainerForIndex(indices[i]);
		if (ctr) /* mlist qualifier just temporary */
		{
			// if the container is a directory container, we need to remove it from the DIR_Server list!
			if (ctr->IsDirectory())
			{
				// first, if the container is expanded, collapse it...
				if (IsExpanded(indices[i]))
					CollapseContainer(indices[i], NULL /* we don't care how many changed */);
				StartingUpdate(MSG_NotifyInsertOrDelete, indices[i], -1);
				m_containerView.Remove(ctr);
				RemoveAndCloseDirectory(ctr->GetDIRServer());
				ctr->RemoveListener(this);
				ctr->Release();  // we are done with this container
				EndingUpdate(MSG_NotifyInsertOrDelete, indices[i], -1);
			}
			else  // if it is a mailing list, delete it from the parent and we'll get our notification to 
				  // delete self that way...
				if (ctr->GetType() == AB_MListContainer)
				{
					AB_MListContainerInfo *mListCtr = ctr->GetMListContainerInfo();
					if (mListCtr)
						mListCtr->DeleteSelfFromParent(this); 
				}
		}
	}

	return AB_SUCCESS;
}

int AB_ContainerPane::ShowPropertySheetForNewType(AB_EntryType entryType, MSG_ViewIndex index /* index for ctr to show sheet for */)
{
	int status = AB_SUCCESS;
	MSG_Pane * pane = NULL;
	AB_ContainerInfo * ctr = GetContainerForIndex(index); // creating pane will ref count ctr
	if (ctr && (ctr->GetType() != AB_LDAPContainer)) // cannot add entries or lists to LDAP.
	{
		if (entryType == AB_MailingList)
			pane = new AB_MailingListPane(m_context, m_master, ctr, AB_ABIDUNKNOWN); // ref counts the ctr
		else 
			if (entryType == AB_Person)
				pane = new AB_PersonPane(m_context, m_master, ctr, AB_ABIDUNKNOWN, this);
	}

	if (pane && m_entryPropSheetFunc)
		m_entryPropSheetFunc(pane, m_context); // non-blocking FE call back 
	else
	{
		AB_ClosePane(pane);  // otherwise, close person pane because it is unused....
		status = AB_FAILURE;
	}

	return status;
}

int AB_ContainerPane::ShowPropertySheet(MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_SUCCESS;
	for (int i = 0; i < numIndices; i++)
	{
		// if we are  a picker pane, m_container is not necessarily the ctr to use...
		// could also have a mixture of property sheet types to be shown..
		AB_ContainerInfo * ctrToUse = GetContainerForIndex(indices[i]);
		MSG_Pane * pane = NULL;
		if (ctrToUse && ctrToUse->GetType() == AB_MListContainer)
		{
			pane = new AB_MailingListPane(m_context, m_master, ctrToUse->GetMListContainerInfo());
		}

		if (pane && m_entryPropSheetFunc)
			m_entryPropSheetFunc(pane, m_context); // non-blocking FE call back 
		else
		{
			AB_ClosePane(pane);  // otherwise, close person pane because it is unused....
			status = AB_FAILURE;
		}
		ctrToUse = NULL;
	} // for each index
	return status;
}

/*****************************************************************************

  Container Pane -> Do command and command status

******************************************************************************/

ABErr AB_ContainerPane::DoCommand(MSG_CommandType cmd, MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_SUCCESS;

	// what commands should the AB_Container Pane support? 
	switch ((AB_CommandType) cmd)
	{
		case AB_NewAddressBook:
		case AB_NewLDAPDirectory:
		case AB_PropertiesCmd:
		case AB_DeleteCmd:
			// we need to sort the indices for deleting to make sure we delete the correct indices first...
			if (numIndices > 1)
				XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);

			ApplyToIndices((AB_CommandType)cmd, indices, numIndices);
			break;
		case AB_AddMailingListCmd:
			if (numIndices == 1) // we only do this for one selection
				status = ShowPropertySheetForNewType(AB_MailingList, indices[0]);
			break;
		case AB_AddUserCmd:
			if (numIndices == 1) // we only do this for one selection
				status = ShowPropertySheetForNewType(AB_Person, indices[0]);
			break;
		default:
			status = AB_INVALID_COMMAND; // we do not recognize the command...
			break;
	}
	
	return status;
}

int AB_ContainerPane::ApplyToIndices(AB_CommandType cmd, MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_FAILURE;
	switch(cmd)
	{
		case AB_NewAddressBook:
		case AB_NewLDAPDirectory:
			status = ShowNewDirectoryProperties(cmd, indices, numIndices);
			break;
		case AB_PropertiesCmd:
			status = ShowProperties(indices, numIndices);
			break;
		case AB_DeleteCmd:
			StartingUpdate(MSG_NotifyNone, 0, 0);
			status = DeleteContainers(indices, numIndices);
			EndingUpdate(MSG_NotifyNone, 0, 0);
			break;
		default: 
			XP_ASSERT(0);
			break;
	}

	return status;
}

ABErr AB_ContainerPane::GetCommandStatus(MSG_CommandType  cmd,  const MSG_ViewIndex * indices,  int32 numIndices, XP_Bool * isSelectableState,
									  MSG_COMMAND_CHECK_STATE * checkedState , const char ** displayString,
									  XP_Bool * pluralState)
{
	int status = AB_SUCCESS;

	// for each cmd, we need to determine if it is (1) enabled, (2) its checked state and (3) display string to use
	MSG_COMMAND_CHECK_STATE state = MSG_NotUsed; // most of our pane commands do not have a check state
	const char * displayText = NULL;
	XP_Bool enabled = FALSE;
	XP_Bool selected = (numIndices > 0); // anything selected? 
	XP_Bool plural = FALSE;

	XP_Bool isLDAPContainer = FALSE;
	AB_ContainerInfo * ctr = NULL;  

	if (numIndices == 1)
	{
		ctr = GetContainerForIndex(indices[0]);
		if (ctr)
			isLDAPContainer = ctr->GetType() == AB_LDAPContainer;
		
	}

	switch ((AB_CommandType) cmd)
	{
	case AB_NewAddressBook:
		displayText = XP_GetString(MK_MSG_ADDRESS_BOOK);
		enabled = TRUE;
		break;
	case AB_NewLDAPDirectory:
		displayText = XP_GetString(MK_ADDR_ADD); // note to self: verify that this is the correct resource string
		enabled = TRUE;
		break;
	case AB_PropertiesCmd:
		displayText = XP_GetString(MK_MSG_PROPERTIES);
		enabled = (numIndices == 1);  // disable properties if multple selections
		break;
	case AB_DeleteCmd:
		displayText = XP_GetString(XP_FILTER_DELETE);
		enabled = selected;
		break;
	case AB_DeleteAllCmd:
		displayText = XP_GetString(MK_ADDR_DELETE_ALL);  
		enabled = selected;
		break;
	case AB_AddUserCmd:
		displayText = XP_GetString(MK_MSG_ADD_NAME);
		enabled = (numIndices == 1) && !isLDAPContainer && ctr;   // only enable if we have a non-LDAP directory container selected
		break;
	case AB_AddMailingListCmd:
		displayText = XP_GetString(MK_MSG_ADD_LIST);
		enabled = (numIndices == 1) && !isLDAPContainer && ctr;
		enabled = (numIndices == 1) && !isLDAPContainer && ctr;  // not implemented yet
		break;
	default:
		status = AB_INVALID_COMMAND; // It is not a matter of enabling, disabling this command...We just recognize it
		// we don't know anything about this cmd
		break;
	}

	if (isSelectableState)
		*isSelectableState = enabled;

	if (checkedState)
		*checkedState = state;

	if (displayString)
		*displayString = displayText;

	if (pluralState)
		*pluralState = plural;

	return status;
}

/*****************************************************************************
 
   Drag and Drop related APIs

*****************************************************************************/

int AB_ContainerPane::DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices,
												  AB_ContainerInfo * destContainer, AB_DragEffect /* request */)
{
	// we can really ignore the passed in request because all ofour actions are fixed based on the input ctr
	// types...That is...there is never a choice between copy, move etc.

	// some of this code was taken from the status method...
	if (destContainer && numIndices && indices)
	{
		XP_Bool destIsDirectory = destContainer->IsDirectory();
		XP_Bool srcCtrsAllLists = TRUE;
		XP_Bool srcCtrsAllDirectories = TRUE;
		// determine if srcs are all lists, all directories or neither...
		for (int32 i = 0; i < numIndices; i++)
		{
			AB_ContainerInfo * ctr = GetContainerForIndex(indices[i]);
			if (ctr)
			{
				if (ctr->IsDirectory())
					srcCtrsAllLists = FALSE;
				else
					srcCtrsAllDirectories = FALSE;
			}
		}

		// if all the src indices are directories, and the dest is a directory...then move all of the selected ctrs in front of the 
		// dest ctr
		if (srcCtrsAllDirectories && destIsDirectory)
			ReOrderContainers(indices, numIndices, destContainer);
		else // ignore case where we are copying lists to other ctrs
		{
			if (srcCtrsAllLists && destContainer->AcceptsNewEntries()) // copy lists to the destination ctr
			{
				// not immplemented yet
			}
		}
				

	}

	return AB_SUCCESS;
}

AB_DragEffect AB_ContainerPane::DragEntriesIntoContainerStatus(const MSG_ViewIndex * indices, int32 numIndices,
														AB_ContainerInfo * destContainer, AB_DragEffect request)
{

	AB_DragEffect effect = AB_Drag_Not_Allowed;
	if (destContainer && numIndices && indices)
	{
		XP_Bool destIsDirectory = destContainer->IsDirectory();
		XP_Bool srcCtrsAllLists = TRUE;
		XP_Bool srcCtrsAllDirectories = TRUE;
		// determine if srcs are all lists, all directories or neither...
		for (int32 i = 0; i < numIndices; i++)
		{
			AB_ContainerInfo * ctr = GetContainerForIndex(indices[i]);
			if (ctr)
			{
				if (ctr->IsDirectory())
					srcCtrsAllLists = FALSE;
				else
					srcCtrsAllDirectories = FALSE;
			}
		}
		
		if (srcCtrsAllLists)
			// you can only copy maiiling lists into others....
			effect = (AB_DragEffect) (request & AB_Require_Copy);
		else
			if (srcCtrsAllDirectories && destIsDirectory)
				effect = (AB_DragEffect) (request & AB_Require_Move);
	}

	return effect;
}

int AB_ContainerPane::ReOrderContainers(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer)
{
	// for right now, this method assumes each index is to a directory ctr. It ignores list ctrs...but it does move
	// all of the directory ctr's children when it reorders
	MSG_ViewIndex insertIndex = GetIndexForContainer(destContainer);
	if (insertIndex != MSG_VIEWINDEXNONE)
	{
		XP_List * servers = DIR_GetDirServers();
		DIR_Server * destServer = destContainer->GetDIRServer();

		for (int32 i = 0; i < numIndices; i++)
		{
			MSG_ViewIndex index = indices[i];
			AB_ContainerInfo * ctr = GetContainerForIndex(index);
			if (ctr && ctr != destContainer && ctr->IsDirectory()) // ignore mailing list ctrs for now...
			{
				// remove it and each of its children, inserting them in front of the src ctr
				AB_ContainerInfoArray arrayOfCtrs;
				arrayOfCtrs.Add(ctr);
				// if ctr is expanded, add each of the children...
				if (IsExpanded(index))
				{
					MSG_ViewIndex position = index+1; // first child index
					for (int32 i = 0; i < ctr->GetNumChildContainers(); i++, position++)
					{
						AB_ContainerInfo * ctr = GetContainerForIndex(position);
						if (ctr)
							arrayOfCtrs.Add(ctr);
					}
				}
					
				StartingUpdate(MSG_NotifyInsertOrDelete, index, -1*arrayOfCtrs.GetSize());  // generate notification for FEs
				m_containerView.RemoveAt(index, &arrayOfCtrs);
				EndingUpdate(MSG_NotifyInsertOrDelete, index, -1*arrayOfCtrs.GetSize());
				
				// now insert it before destCtr
				insertIndex = GetIndexForContainer(destContainer); // update index count because ctr position could have changed
				StartingUpdate(MSG_NotifyInsertOrDelete, insertIndex, arrayOfCtrs.GetSize());  // generate notification for FEs
				m_containerView.InsertAt(insertIndex, &arrayOfCtrs);
				EndingUpdate(MSG_NotifyInsertOrDelete, insertIndex, arrayOfCtrs.GetSize());

				// Don't forget to change its position in the DIR_Server list!!!
				DIR_Server * srcServer = ctr->GetDIRServer();
				if (srcServer && XP_ListRemoveObject(servers, srcServer)) // if the srcServer was in the list...
					XP_ListInsertObject(servers, destServer, srcServer);  // insert it before the destination server..

			}
		}
		DIR_SaveServerPreferences(servers);      // make sure the re-ordering is committed 
	}

	return AB_SUCCESS;	
}


//********************************************************************************
// Undo/Redo
//********************************************************************************
int AB_ContainerPane::Undo()
{
	return AB_SUCCESS;
}

int AB_ContainerPane::Redo()
{
	return AB_SUCCESS;
}

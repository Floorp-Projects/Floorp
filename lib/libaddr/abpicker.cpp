/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "abpicker.h"
#include "abcinfo.h"
#include "prefapi.h"

extern "C"
{
extern int MK_MSG_ADD_TO_ADDR_BOOK;  // only two supported command types
extern int MK_MSG_PROPERTIES;
}

/*********************************************************************************************
  Notification handlers

***********************************************************************************************/
void AB_PickerPane::OnAnnouncerGoingAway(AB_ContainerAnnouncer * /*instigator*/)
{
	// our container is going away...remove any entries which were depending on the ctr

}

void AB_PickerPane::OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * /*instigator*/)
{
	if (ctr && ctr->GetType() == AB_LDAPContainer)
	{
		switch (code)
		{
			case AB_NotifyStartSearching:
				m_IsSearching = TRUE;
				FE_PaneChanged(this, TRUE, MSG_PaneNotifyStartSearching, 0);
				break;
			case AB_NotifyStopSearching:
				m_IsSearching = FALSE;
				FE_PaneChanged(this, TRUE, MSG_PaneNotifyStopSearching, 0);
				break;
			default:
				break;
		}
	}
}


void AB_PickerPane::OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, ABID /* entryID */, AB_ContainerListener * /* instigator */)
{
	return;
#if 0
	// scan through list of entries till we find a matching entry (if any)
	if (ctr)
	{
		MSG_ViewIndex index = 0; 
		XP_Bool found = FALSE;
		AB_PickerEntry * entry = NULL;

		// start scanning through the list
		for (index = 0; index < m_entriesView.GetSize() && !found; index++)
		{
			entry = GetEntryForIndex(index);
			if (entry && entry->ctr == ctr && (entry->entryID == entryID))
				found = TRUE;
		}

		// if found, then entry is the one we want and it is at index...now process notification.
		switch (code)
		{
			case AB_NotifyTotalLDAPContentChanged:
				StartingUpdate(MSG_NotifyLDAPTotalContentChanged, index, numChanged);
				EndingUpdate(MSG_NotifyLDAPTotalContentChanged, index, numChanged);
				break;
			
			case AB_NotifyDeleted:
				StartingUpdate(MSG_NotifyInsertOrDelete, index, -1);
				m_entriesView.Remove(entry);

				// free the entry...

				EndingUpdate(MSG_NotifyInsertOrDelete, index, -1);
				break;

			case AB_NotifyInserted: // i don't think so!!! can't insert into a list of what are basically search results!!
				break;

			case AB_NotifyPropertyChanged:
				if (index != MSG_VIEWINDEXNONE)
				{
					StartingUpdate(MSG_NotifyChanged, index, 1);
					EndingUpdate(MSG_NotifyChanged, index, 1);
				}
				break;
			case AB_NotifyAll:  // could be expensive!!!!
				StartingUpdate(MSG_NotifyAll, 0, 0);
				EndingUpdate(MSG_NotifyAll, 0, 0);
				break;
			default: // we don't know how to handle the error...
				break;
		}
#endif
}


/**********************************************************************************************

  Opening/Closing/Deleting

***********************************************************************************************/

AB_PickerPane::AB_PickerPane(MWContext * context, MSG_Master * master, uint32 pageSize) : AB_Pane(context, master, pageSize)
{
	// initialize name completion stuff
	m_currentSearchCtr = NULL;	
	m_exitFunction = NULL;
	m_FECookie = NULL;
	m_completionValue = NULL;
	m_paneVisible = TRUE;

	m_ABIDCtrView = new AB_ABIDContainerView(this);
}

/* static */ int AB_PickerPane::Create(MSG_Pane ** pane, MWContext * context, MSG_Master * master, uint32 pageSize)
{
	*pane = new AB_PickerPane(context, master, pageSize);
	if (*pane)
		return AB_SUCCESS;
	else
		return AB_OUT_OF_MEMORY;
}

AB_PickerPane::~AB_PickerPane()
{
	XP_ASSERT(m_entriesView.GetSize() == 0);
	XP_ASSERT(m_searchQueue.GetSize() == 0);
	XP_ASSERT(m_ABIDCtrView == NULL);
}

void AB_PickerPane::CloseSelf()
{
	TerminateSearch(); // clean up any in progress searching...
	ClearResults();
	delete m_ABIDCtrView;
	m_ABIDCtrView = NULL;
}

/* static */ int AB_PickerPane::Close(AB_PickerPane * pane)
{
	if (pane)
	{
		pane->CloseSelf();
		delete pane;

	}

	return AB_SUCCESS;
}

int AB_PickerPane::GetEntryAttributes(MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry)
	{
		if (IsNakedAddress(entry))
			return AB_GetAttributesForNakedAddress(entry->nakedAddress, attribs, values, numItems);
		else
			if (entry->ctr)
				return entry->ctr->GetEntryAttributes(this, entry->entryID /* our view indices are meaningless to the FEs */, attribs, values, numItems);
	}

	return AB_FAILURE;
}

int AB_PickerPane::GetEntryAttribute(MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue ** value)
{
	uint16 numItems = 1;
	return GetEntryAttributes(index, &attrib, value, &numItems);
}

int AB_PickerPane::SetEntryAttribute(MSG_ViewIndex index, AB_AttributeValue * value)
{
	return SetEntryAttributes(index, value, 1);
}
	
int AB_PickerPane::SetEntryAttributes(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry)
	{
		if (IsNakedAddress(entry))
			return AB_FAILURE;
		else
			if (entry->ctr)
				return entry->ctr->SetEntryAttributes(entry->entryID /* our view indices are meaningless to the FEs */, valuesArray, numItems);
	}

	return AB_FAILURE;
}

XP_Bool AB_PickerPane::IsNakedAddress(AB_PickerEntry * entry)
{
	if (entry && entry->ctrType == AB_UnknownContainer)
		return TRUE;
	else
		return FALSE;
}

AB_ContainerInfo * AB_PickerPane::GetContainerForIndex(const MSG_ViewIndex index)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry)
	{
		if (!IsNakedAddress(entry))
			return entry->ctr;
	}

	return NULL;
}

AB_PickerEntry * AB_PickerPane::GetEntryForIndex(MSG_ViewIndex index)
{
	if (m_entriesView.IsValidIndex(index))
		return m_entriesView.GetAt(index);
	else
		return NULL;
}

ABID AB_PickerPane::GetABIDForIndex(const MSG_ViewIndex index)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry && entry->ctrType != AB_UnknownContainer)
		return entry->entryID;
	else
		return AB_ABIDUNKNOWN;
}

MSG_ViewIndex AB_PickerPane::GetEntryIndexForID(ABID /*id*/) // VIEWINDEXNONE if not in the picker pane
{
	// scan through all the 

	return MSG_VIEWINDEXNONE;
}

AB_ContainerType AB_PickerPane::GetEntryContainerType(MSG_ViewIndex index)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry)
		return entry->ctrType;
	else
		return AB_UnknownContainer; // do we have a more specific return value because an error occurred...hmmm
}

AB_NameCompletionCookie * AB_PickerPane::GetNameCompletionCookieForIndex(MSG_ViewIndex index)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	AB_NameCompletionCookie * cookie = NULL;
	
	if (entry)
	{
		if (IsNakedAddress(entry))
			cookie = new AB_NameCompletionCookie(entry->nakedAddress);
		else
			cookie = new AB_NameCompletionCookie(this, entry->ctr, entry->entryID);
	}

	return cookie;
}

/**********************************************************************************************

  The interesting stuff...actually performing the name completion searches, terminating searches, 
  firing off a NC on the next container, finishing a search, etc.

***********************************************************************************************/


int AB_PickerPane::NameCompletionSearch(const char * completionValue, AB_NameCompletionExitFunction * exitFunction, XP_Bool paneVisible, void * FECookie)
{
	TerminateSearch();  // terminates any previous searches, re-initializes search state variables
	
	m_paneVisible = paneVisible; 

	// set up search 
	if (completionValue && XP_STRLEN(completionValue) > 0) // make sure we weren't just terminating a search..
	{
		m_completionValue = XP_STRDUP(completionValue);
		m_exitFunction = exitFunction;
		m_FECookie = FECookie;

		
		// perform a name completion search...
		ClearResults(); //if we are actually beginning a new search, clean out the old results.
		LoadNakedAddress(completionValue); // the string we are name completing against is always a valid entry to select
		LoadSearchQueue();
		StartNextSearch(); // fire off next search
	}

	return AB_SUCCESS;
}

void AB_PickerPane::ClearResults()
{
	StartingUpdate(MSG_NotifyAll, 0, 0);
	for (int32 i = 0; i < m_entriesView.GetSize(); i++)
	{
		AB_PickerEntry * entry = m_entriesView.GetAt(i);
		if (entry)
		{
			if (!IsNakedAddress(entry) && entry->ctr)
			{
				entry->ctr->RemoveListener(m_ABIDCtrView); // in case we haven't removed already
				entry->ctr->Release(); // release our ref count on the ctr
			}
		else
			if (entry->nakedAddress)
				XP_FREE(entry->nakedAddress);
			
			XP_FREE(entry);
		}
	}
	m_entriesView.RemoveAll();
	EndingUpdate(MSG_NotifyAll, 0, 0);
}

void AB_PickerPane::LoadNakedAddress(const char * nakedAddress) // the string we are name completing against is always a valid entry to select
{
	if (nakedAddress) // only create one if we actually have a naked address
	{
		AB_PickerEntry * entry = (AB_PickerEntry *) XP_ALLOC(sizeof(AB_PickerEntry));
		if (entry)
		{
			entry->entryID = AB_ABIDUNKNOWN;
			entry->ctr = NULL;
			entry->ctrType = AB_UnknownContainer;
			entry->nakedAddress = XP_STRDUP(nakedAddress);

			//now add it to the view
			MSG_ViewIndex index = m_entriesView.Add(entry);
			StartingUpdate(MSG_NotifyInsertOrDelete, index, 1);
			EndingUpdate(MSG_NotifyInsertOrDelete, index, 1);
		}
	}
}

XP_Bool AB_PickerPane::ValidServerToSearch(DIR_Server * server)
{
	// abstract away checking all the preferences for auto completion,
	// so we can determine if the server should be added to the search queue or not
	XP_Bool status = FALSE;
	if (server)
	{
		if (server->dirType == PABDirectory)
		{
			XP_Bool searchAddressBooks = FALSE;
			PREF_GetBoolPref("ldap_1.autoComplete.useAddressBooks", &searchAddressBooks);
			if (searchAddressBooks)
				status = TRUE;
		}
		else
			if (server->dirType == LDAPDirectory)
			{
				XP_Bool searchLDAPDirectories = FALSE;
				PREF_GetBoolPref("ldap_1.autoComplete.useDirectory", &searchLDAPDirectories);
				// don't add the ldap server unless we are supposed to search a directory,
				// this is the directory to search and we are not offline...eventually need
				// to use the replica if it exists...
				if (searchLDAPDirectories && DIR_TestFlag(server, DIR_AUTO_COMPLETE_ENABLED) && !NET_IsOffline())
					status = TRUE;
			}
	}

	return status;
}


void AB_PickerPane::LoadSearchQueue()
{
	// make sure the search queue is empty...otherwise someone did something wrong!
	XP_ASSERT(m_searchQueue.GetSize() == 0);
	if (m_searchQueue.GetSize() == 0) // load new search queue
	{
		XP_List * dirServers = DIR_GetDirServers();  // acquire lists of servers
		DIR_Server * server = NULL;
		do
		{
			server = (DIR_Server *) XP_ListNextObject(dirServers);
			// eventually, if we are in offline mode, and the LDAP directory is a replica, we
			// want to use it. But for now, we don't have replicated LDAP directories yet so we
			// just won't add the LDAP directory to our search queue
			if (ValidServerToSearch(server))
			{
				AB_ContainerInfo * ctr = NULL;
				AB_ContainerInfo::Create(m_context, server, & ctr); // create acquires ref count
				if (ctr)
				{
					AB_LDAPContainerInfo * LDAPCtr = ctr->GetLDAPContainerInfo();
					if (LDAPCtr)
					{
						ctr = LDAPCtr->AcquireLDAPResultsContainer(m_context);
						LDAPCtr->Release();  // we are done with it...
						LDAPCtr = NULL;
					}
					if (ctr) // make sure we still have one
						m_searchQueue.Add(ctr);
				}
			}
		} while (server);

		// we want to put the LDAP container at the end of the list..so make a pass find it, and move it to the end of the queue...
		for (int32 index = 0; index < m_searchQueue.GetSize(); index++)
		{
			AB_ContainerInfo * ctr = m_searchQueue.GetAt(index);
			if (ctr && ctr->GetType() == AB_LDAPContainer)
			{
				m_searchQueue.RemoveAt(index);
				m_searchQueue.InsertAt(m_searchQueue.GetSize(), (void *) ctr); // insert the LDAP ctr at the end of the list
			}
		}
		// once all the ctrs have been loaded, we need to sort them by type...we want the LDAP directory to be searched last...
//		m_searchQueue.QuickSort(AB_ContainerInfo::CompareByType);
	}  
}

void AB_PickerPane::ProcessSearchResult(AB_ContainerInfo * ctr, ABID resultID)
{
	// create a new entry for this result and add the entry to the view, notifying the FE.
	// Note: we must ref count the container. Each picker entry has its own refcounted container.

	AB_PickerEntry * entry = (AB_PickerEntry *) XP_ALLOC(sizeof(AB_PickerEntry));
	if (entry && ctr)
	{
		entry->ctr = ctr;
		entry->ctr->AddListener(m_ABIDCtrView); // we want to listen to this ctr now 
		entry->ctr->Acquire(); // obtain a ref count.
		entry->ctrType = ctr->GetType();
		entry->entryID = resultID;
		
		// now update FE because we just inserted a new entry (assuming we are not LDAP searching!!
		MSG_ViewIndex index = m_entriesView.Add(entry);
		if (!m_IsSearching)
		{
			StartingUpdate(MSG_NotifyInsertOrDelete, index, 1);
			EndingUpdate(MSG_NotifyInsertOrDelete, index, 1);
		}
	}
}

void AB_PickerPane::TerminateSearch() // halts current name completion searches but doesn't remove any results.
{
	if (m_currentSearchCtr)
	{
		m_currentSearchCtr->NameCompletionSearch(this, NULL /* signal to terminate search */);
		if (m_currentSearchCtr->GetType() == AB_LDAPContainer)
			m_currentSearchCtr->RemoveListener(m_ABIDCtrView);
		m_currentSearchCtr = NULL; // don't worry about ref count. ctr still in queue
	}

	for (int32 i = 0; i < m_searchQueue.GetSize(); i++)
	{
		AB_ContainerInfo * ctr = m_searchQueue.GetAt(i);
		if (ctr)
			ctr->Release(); // release our search ref count
	}

	m_searchQueue.RemoveAll();

	if (m_completionValue)
		XP_FREE(m_completionValue);

	m_completionValue = NULL;
	m_exitFunction = NULL;
	m_FECookie = NULL;
}

void AB_PickerPane::StartNextSearch()
{
	// get first container in the search queue and fire off a name completion search
	if (m_searchQueue.GetSize() > 0) // at least one more container to search.
	{
		m_currentSearchCtr = m_searchQueue.GetAt(0); // get first container in queue
		if (m_currentSearchCtr)
		{
			if (m_currentSearchCtr->GetType() == AB_LDAPContainer)
			{
				m_currentSearchCtr->AddListener(m_ABIDCtrView);
				XP_Bool stopIfLocalMatch = FALSE;
				PREF_GetBoolPref("ldap_1.autoComplete.skipDirectoryIfLocalMatchFound", &stopIfLocalMatch);
				// if we have 1 match (+ 1 because naked address is there) and we are supposed to stop if we have one local
				// match, then announce the LDAP search as complete because we don't want to search it. 
				if (stopIfLocalMatch && m_entriesView.GetSize() == 2) /* only listen to the pref if pane is not visible */
					SearchComplete(m_currentSearchCtr);
				else
					m_currentSearchCtr->NameCompletionSearch(this, m_completionValue);
			}
			else  // if it is a PAB, we always search it...
				m_currentSearchCtr->NameCompletionSearch(this, m_completionValue);
		}
	}
	else  // no more search queues to run...
		AllSearchesComplete();
}
		
void AB_PickerPane::SearchComplete(AB_ContainerInfo * ctr)
{
	if (m_searchQueue.GetSize() > 0 && ctr)
	{
		if (m_searchQueue.Remove(ctr))  // was it in our search queue  ??
		{
			XP_ASSERT(m_currentSearchCtr == ctr);
			if (m_currentSearchCtr->GetType() == AB_LDAPContainer)
				m_currentSearchCtr->RemoveListener(m_ABIDCtrView);
			if (ctr == m_currentSearchCtr) // if it is the one we are now searching
				m_currentSearchCtr = NULL; // clear it outs
			ctr->Release();  // release the NC search's ref count on the container
		}
		// fire off the next search
		StartNextSearch();
	}
}

void AB_PickerPane::AllSearchesComplete() // notify FE that we are done
{
	// update FE with name completion cookie for first match using the exit fuction if it exists
	if (m_exitFunction)
	{
		AB_NameCompletionCookie * cookie = NULL;
		if (m_entriesView.GetSize() > 1) // do we have any matches?? Remember, we always have the naked address..don't count it as a result.
			cookie = GetNameCompletionCookieForIndex(1);

		m_exitFunction(cookie, m_entriesView.GetSize()-1 /* -1 for naked address */, m_FECookie);
	}
}

int AB_PickerPane::LDAPSearchResults(MSG_ViewIndex index, int32 num)
// we got burned here because I made the NC picker inherit from ab_pane...don't use m_container, 
// use the ctr for the index
{
	if (m_currentSearchCtr)
	{
		AB_LDAPContainerInfo * ldapCtr = m_currentSearchCtr->GetLDAPContainerInfo();
		if (ldapCtr)
			return ldapCtr->LDAPSearchResults(this, index, num);
	}

	return AB_FAILURE;
}

int AB_PickerPane::FinishSearch()
{
	if (m_IsSearching)
	{
		MSG_InterruptSearch(m_context);
		if (m_currentSearchCtr)
		{
			AB_LDAPContainerInfo * ldapCtr = m_currentSearchCtr->GetLDAPContainerInfo();
			if (ldapCtr)
				return ldapCtr->FinishSearch(this);
		}
	}
	return AB_SUCCESS;
}

/**********************************************************************************************

  Pane command methods - doing, command status, etc.

***********************************************************************************************/

ABErr AB_PickerPane::DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_FAILURE;

	// if you later decide to add more commands here and are implementing the handler in the base class
	// (i.e. like Show property sheet), make sure you modufy the base class function to get the right
	// ctr(s) for the operation instead of m_container

	switch(command)
	{
		case AB_PropertiesCmd:
			status = ShowPropertySheet(indices, numIndices);
			break;
			// note: we don't support add to address book here...FEs should be doing that through the drag and drop APIs...
		default:
			break;
	}

	return status;
}

ABErr AB_PickerPane::GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * indices, int32 numIndices, XP_Bool * IsSelectableState, 
						MSG_COMMAND_CHECK_STATE * checkedState, const char ** displayString, XP_Bool * pluralState)
{

	// for each command, we need to determine (1) is it enabled? (2) its check state: unused, checked, unchecked
	// and (3) the display string to use. 

	// Our defaults are going to be state = MSG_NotUsed, enabled = FALSE, and displayText = 0;
	const char * displayText = 0;
	XP_Bool enabled = FALSE;
	MSG_COMMAND_CHECK_STATE state = MSG_NotUsed;
	XP_Bool selected = (numIndices > 0);
	XP_Bool plural = FALSE;

	// if one of the selections is a naked address, note it. if all are LDAP note it..
	XP_Bool anyNakedAddresses = FALSE;
	XP_Bool allLDAPSelections = TRUE;

	for (int32 i = 0; i < numIndices && indices; i++)
	{
		AB_PickerEntry * entry = GetEntryForIndex(indices[i]);
		if (entry)
		{
			if (entry->ctrType == AB_UnknownContainer)
				anyNakedAddresses = TRUE;
			if (entry->ctrType != AB_LDAPContainer)
				allLDAPSelections = FALSE;
		}
	}

	// right now properties is the only supported command..eventually need an add to address book for LDAP.
	switch ((AB_CommandType) command) 
	{
		case AB_PropertiesCmd:
			displayText = XP_GetString(MK_MSG_PROPERTIES);
			enabled = selected && !anyNakedAddresses;
			break;

		case AB_ImportLdapEntriesCmd:
			displayText = XP_GetString(MK_MSG_ADD_TO_ADDR_BOOK);
			enabled = selected && allLDAPSelections;
			enabled = FALSE;
			break;

		default: // we don't support any other commands
			break;
	}

	if (IsSelectableState)
		*IsSelectableState = enabled;

	if (checkedState)
		*checkedState = state;

	if (displayString)
		*displayString = displayText;

	if (pluralState)
		*pluralState = plural;
	
	return AB_SUCCESS;
}


/**********************************************************************************************

  Getting container attributes. Unlike an AB_Pane, picker pane entries belong to different entries. 
  Sometimes the FE wants to find things out about the container for an index. I'm going to disallow setting
  for now because I don't think it is a good idea to be able to set ctr attributes from the completion
  pane. 

***********************************************************************************************/
	
int AB_PickerPane::GetContainerAttribute (MSG_ViewIndex index, AB_ContainerAttribute attrib, AB_ContainerAttribValue ** value)
{
	uint16 numItems = 1;
	return GetContainerAttributes(index, &attrib, value, &numItems);
}

int AB_PickerPane::GetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribute * attribArray, AB_ContainerAttribValue ** values, uint16 * numItems)
{
	AB_PickerEntry * entry = GetEntryForIndex(index);
	if (entry && entry->ctr)
		return entry->ctr->GetAttributes(attribArray, values, numItems);
	else
	{
		if (values)
			*values = NULL;

		return AB_FAILURE;
	}

}

int AB_PickerPane::SetContainerAttribute (MSG_ViewIndex /*index*/, AB_ContainerAttribValue * /*value*/)
{
	XP_ASSERT(0);  // why are you trying to set ctr attribs in the picker pane??

	return AB_FAILURE;
}
	
int AB_PickerPane::SetContainerAttributes(MSG_ViewIndex /*index*/, AB_ContainerAttribValue * /*valuesArray*/, uint16 /*numItems*/)
{
	XP_ASSERT(0);  // why are you trying to set ctr attribs in the picker pane??
	return AB_FAILURE;
}


/**********************************************************************************************

Loading the column attributes. FEs need to know what data columns should appear in the picker 
pane. We do that here. Eventually we should load the column attributes from the LDAP ctr being 
searched. But for now, we'll just hard code them.

***********************************************************************************************/

int AB_PickerPane::GetNumColumnsForPicker()
{
	// for right now, we have a fixed number of columns...0 based...
	return AB_NumberOfColumns;   // enumerated columnsID type
}

int AB_PickerPane::GetColumnAttribIDs(AB_AttribID * attribIDs, int * numAttribs)
{
	// this is just for starters...eventually we want to hook into the DIR_Server and update ourselves
	// with any changes....

	// okay this is hacky and not good since I fail completely if the array isn't big enough...
	// but the code should only be temporary =)

	if (*numAttribs == AB_NumberOfColumns)
	{
		attribIDs[0] = AB_attribEntryType;
		attribIDs[1] = AB_attribDisplayName;
		attribIDs[2] = AB_attribEmailAddress;
		attribIDs[3] = AB_attribCompanyName;
		attribIDs[4] = AB_attribWorkPhone;
		attribIDs[5] = AB_attribLocality;
		attribIDs[6] = AB_attribNickName;
	}

	else
		*numAttribs = 0;

	return AB_SUCCESS;
}


AB_ColumnInfo * AB_PickerPane::GetColumnInfo(AB_ColumnID columnID)
{
	AB_ColumnInfo * columnInfo = (AB_ColumnInfo *) XP_ALLOC(sizeof(AB_ColumnInfo));
	if (columnInfo)
	{
		switch (columnID)
		{
			case AB_ColumnID0:
				columnInfo->attribID = AB_attribEntryType;
				columnInfo->displayString = NULL;
				columnInfo->sortable = FALSE;
				break;
			case AB_ColumnID1:
				columnInfo->attribID= AB_attribDisplayName;
				columnInfo->displayString = XP_STRDUP("Name");
				columnInfo->sortable = TRUE;
				break;
			case AB_ColumnID2:
				columnInfo->attribID = AB_attribEmailAddress;
				columnInfo->displayString = XP_STRDUP("Email Address");
				columnInfo->sortable = TRUE;
				break;
			case AB_ColumnID3:
				columnInfo->attribID = AB_attribCompanyName;
				columnInfo->displayString = XP_STRDUP("Organization");
				columnInfo->sortable = TRUE;
				break;
			case AB_ColumnID4:
				columnInfo->attribID = AB_attribWorkPhone;
				columnInfo->displayString = XP_STRDUP("Phone");
				columnInfo->sortable = FALSE;
				break;
			case AB_ColumnID5:
				columnInfo->attribID = AB_attribLocality;
				columnInfo->displayString = XP_STRDUP("City");
				columnInfo->sortable = TRUE;
				break;
			case AB_ColumnID6:
				columnInfo->attribID = AB_attribNickName;
				columnInfo->displayString = XP_STRDUP("NickName");
				columnInfo->sortable = TRUE;
				break;
			default:
				XP_ASSERT(0);
				XP_FREE(columnInfo);
				columnInfo = NULL;
		}
	}

	return columnInfo;
}

//******************************************************************
// Drag and Drop - (also use these for Add To Address Book) to take NC results and add
// them to address book containers....
//******************************************************************

int AB_PickerPane::DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, AB_DragEffect /* request */)
{
	// we can ignore the drag effect because you can only COPY from the NC resolution pane!
	int status = AB_SUCCESS;
	if (destContainer && numIndices && indices)
	{
		// we are going to be inefficient at first and process them one at a time....i'll
		// come back later and batch them together....
		for (int32 i = 0; i < numIndices; i++)
		{
			int loopStatus = AB_SUCCESS;
			AB_PickerEntry * entry = GetEntryForIndex(indices[i]);
			if (entry && entry->ctr && entry->entryID != AB_ABIDUNKNOWN)
			{
				if (entry->ctr->GetType() == AB_LDAPContainer)
					loopStatus = AB_ImportLDAPEntriesIntoContainer(this, &indices[i], 1, destContainer);
				else
					loopStatus = entry->ctr->CopyEntriesTo(destContainer, &entry->entryID, 1);
			}
			if (loopStatus != AB_SUCCESS)
				status = loopStatus; // insures that we'll catch at least the last error....
		}
	}

	return status;
}

AB_DragEffect AB_PickerPane::DragEntriesIntoContainerStatus(const MSG_ViewIndex * /*indices*/, int32 /*numIndices*/, AB_ContainerInfo * destContainer,
								   AB_DragEffect request)
{
	// ask dest ctr if it accepts entries...LDAP doesn't..MList and PAB should...
	// You can only COPY entries from the resolution pane.
	if (destContainer) // make sure both ctrs are  around...
		if (destContainer->AcceptsNewEntries())
			return (AB_DragEffect) (request & AB_Require_Copy);

	return AB_Drag_Not_Allowed;
}


/*************************************************************************************
  Implemenation of the ABID container view which forwards all ABID based notifications
  on to the picker pane
**************************************************************************************/

AB_ABIDContainerView::AB_ABIDContainerView(AB_PickerPane * pane)
{
	m_pickerPane = pane;
}


AB_ABIDContainerView::~AB_ABIDContainerView()
{
	m_pickerPane = NULL;
}


void AB_ABIDContainerView::OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator)
{
	if (m_pickerPane)
		m_pickerPane->OnContainerAttribChange(ctr, code, instigator);

}

void AB_ABIDContainerView::OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator)
{
	if (m_pickerPane)
		m_pickerPane->OnContainerEntryChange(ctr, code, entryID, instigator);
}

void AB_ABIDContainerView::OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator)
{
	if (m_pickerPane)
		m_pickerPane->OnAnnouncerGoingAway(instigator);

}

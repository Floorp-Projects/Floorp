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

#include "abpane2.h"
#include "xp_qsort.h"
#include "abpicker.h"
#include "msgurlq.h"

extern "C"
{
extern int MK_MSG_NEW_MAIL_MESSAGE;
extern int MK_MSG_UNDO;
extern int MK_MSG_REDO;
extern int MK_ADDR_DELETE_ALL; 
extern int XP_FILTER_DELETE;
extern int MK_MSG_SORT_BACKWARD;
extern int MK_MSG_BY_TYPE;
extern int MK_MSG_BY_NAME;
extern int MK_MSG_BY_NICKNAME;
extern int MK_MSG_BY_EMAILADDRESS;
extern int MK_MSG_BY_COMPANY;
extern int MK_MSG_BY_LOCALITY;
extern int MK_MSG_SORT_DESCENDING;
extern int MK_MSG_ADD_NAME;
extern int MK_MSG_ADD_LIST;
extern int MK_MSG_PROPERTIES;
extern int MK_MSG_SEARCH_STATUS;
extern int XP_BKMKS_IMPORT_ADDRBOOK;
extern int XP_BKMKS_SAVE_ADDRBOOK;
extern int MK_MSG_CALL;
extern int MK_ADDR_NO_EMAIL_ADDRESS; 
extern int MK_ADDR_DEFAULT_EXPORT_FILENAME;
extern int MK_ADDR_ADD;
extern int MK_MSG_ADDRESS_BOOK;
extern int MK_ACCESS_NAME;
}

/**********************************************************************

  AB_Pane is a container listener. Here are the notification handlers....

************************************************************************/

void AB_Pane::OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * /* instigator */)
{
	// unless the container is going away, this does not effect us at all
	if (ctr == m_container)
	{
		switch (code)
		{
			case AB_NotifyScramble:
				StartingUpdate(MSG_NotifyAll, 0, 0);
				EndingUpdate(MSG_NotifyAll, 0, 0);
				break;
			case AB_NotifyStartSearching:
				m_IsSearching = TRUE;
				FE_PaneChanged(this, TRUE, MSG_PaneNotifyStartSearching, 0);
				break;
			case AB_NotifyStopSearching:
				m_IsSearching = FALSE;
				FE_PaneChanged(this, TRUE, MSG_PaneNotifyStopSearching, 0);
				break;
			default:
				// do nothing....
				break;
		}
	}
}

void AB_Pane::OnContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, MSG_ViewIndex index, int32 numChanged, AB_ContainerListener * /*instigator*/)
{
	if (ctr == m_container)
	{
		switch (code)
		{
			case AB_NotifyNewTopIndex:
				StartingUpdate(MSG_NotifyNewTopIndex, index, 0);
				EndingUpdate(MSG_NotifyNewTopIndex, index, 0);
				break;
			case AB_NotifyLDAPTotalContentChanged:
				StartingUpdate(MSG_NotifyLDAPTotalContentChanged, index, numChanged);
				EndingUpdate(MSG_NotifyLDAPTotalContentChanged, index, numChanged);
				break;
			case AB_NotifyDeleted:
				StartingUpdate(MSG_NotifyInsertOrDelete, index, -1*numChanged);
				EndingUpdate(MSG_NotifyInsertOrDelete, index, -1*numChanged);
				break;
			case AB_NotifyInserted:
				// if we are LDAP searching, we need to supress this notification...FEs know already that they need
				// to update themselves. Generating a notification here only confuses them as they are already getting
				// these from the LDAP Search code.
				if (!m_IsSearching)
				{
					StartingUpdate(MSG_NotifyInsertOrDelete, index, numChanged);
					EndingUpdate(MSG_NotifyInsertOrDelete, index, numChanged);
				}
				break;
			case AB_NotifyPropertyChanged:
				if (index != MSG_VIEWINDEXNONE)
				{
					StartingUpdate(MSG_NotifyChanged, index, numChanged);
					EndingUpdate(MSG_NotifyChanged, index, numChanged);
				}
				break;
			case AB_NotifyAll:
				StartingUpdate(MSG_NotifyAll, 0, 0);
				EndingUpdate(MSG_NotifyAll, 0, 0);
				break;
			default: // we don't know how to handle the error...
				break;
		}
	}
}

void AB_Pane::OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator)
{
	// our container is going away!!!
	if (instigator == m_container)
	{
		instigator->RemoveListener(this);
		// now, to be safe, we should probably set our container to NULL. Since it is now invalid!
		m_container->Release(); // release our ref on the ctr
		m_container = NULL;
	}
}

/***********************************************************************

  Rest of the AB_Pane definitions....

************************************************************************/

AB_Pane::AB_Pane(MWContext * context, MSG_Master * master, uint32 /*pageSize*/) : MSG_LinedPane(context, master), AB_IndexBasedContainerListener(), AB_PaneAnnouncer()
{
	m_context = context;
	m_master = master;
	m_IsSearching = FALSE;
	m_container = NULL;
 	m_selectionList = NULL;
}

AB_Pane::~AB_Pane()
{
	if (m_selectionList)
		delete m_selectionList;

	NotifyAnnouncerGoingAway(this);  // let any pane listener's know we are going away! 

	if (m_container)
	{
		m_container->RemoveListener(this);
		// now, we have obtained a reference for our container, we must dereference it...
		m_container->Release();
		m_container = NULL; // our handle is now invalid! 
	}
}

int AB_Pane::SetPageSize(uint32 pageSize)
{
	// right now, all we need to do is save the size....
	if (pageSize > 0)
		m_pageSize = pageSize;
	else
		m_pageSize = AB_k_DefaultPageSize;

	// notify our container that our page size has changed!!
	if (m_container)
		m_container->UpdatePageSize(pageSize);

	return AB_SUCCESS;
}

MSG_PaneType AB_Pane::GetPaneType()
{
	return AB_ABPANE;
}

/* static */ int AB_Pane::Create(MSG_Pane ** pane, MWContext * context, MSG_Master * master, uint32 pageSize)
{
	*pane = new AB_Pane(context, master, pageSize);
	if (*pane)
		return AB_SUCCESS;
	else
		return AB_OUT_OF_MEMORY;
}

/* static */ int AB_Pane::Close(AB_Pane * pane)
{
	if (pane)
		delete pane;
	return AB_SUCCESS;
}

ABID AB_Pane::GetABIDForIndex(const MSG_ViewIndex index)
{
	if (m_container)
		return m_container->GetABIDForIndex(index);
	else
		return AB_ABIDUNKNOWN;
}

MSG_ViewIndex AB_Pane::GetEntryIndexForID(ABID id)
{
	if (m_container)
		return m_container->GetIndexForABID(id);
	else
		return MSG_VIEWINDEXNONE;
}

AB_ContainerInfo * AB_Pane::GetContainerForIndex(const MSG_ViewIndex /* index */)
{
	// will eventually detect if index is a mailing list, if so, create a mailing list container info
	// for the item. Will need to be careful with the reference counting issue. We might need to store away
	// mailing list containerInfo's in a pane cache so we can release ref counts when the pane goes away.

	return m_container;
}

// if you ever want to load from a container, make sure to reference count the container
// otherwise you WILL have problems!!!
int AB_Pane::LoadContainer(AB_ContainerInfo * container)
{
	// do we need to do anything to close out the old container???
	if (m_container != container) // if they are different, do something....
	{
		StartingUpdate(MSG_NotifyAll, 0, 0);
		if (m_container)
		{
			m_container->RemoveListener(this);
			m_container->Release();  // handles reference count
			m_container = NULL;
		}
	
		if (container)
		{
			m_container = container;
			m_container->AddListener(this);
			m_container->UpdatePageSize(m_pageSize);
			// adjust the reference count
			m_container->Acquire();
		}
		
		EndingUpdate(MSG_NotifyAll, 0, 0);

		// Load the initial entries into the view
		m_container->PreloadSearch(this);
	}

	return AB_SUCCESS;  // but what if the create failed?? hmmmm
}

int AB_Pane::TypedownSearch(const char * typedownValue, MSG_ViewIndex startIndex)
{
	if (m_container)
	{
		// pass the buck to the container...
		return m_container->TypedownSearch(this, typedownValue, startIndex);
	}
	else
		return AB_INVALID_CONTAINER;
}

int AB_Pane::SearchDirectory(char * searchString)
{
	// make sure the container is an LDAP container
	if (m_container)
	{
		AB_LDAPContainerInfo * ldapCtr = m_container->GetLDAPContainerInfo();
		// push details of performing search into the LDAP container level
		if (ldapCtr)
		{
			m_IsSearching = TRUE;
			return ldapCtr->SearchDirectory(this, searchString);
		}
	}
	return AB_INVALID_CONTAINER;
}

int AB_Pane::LDAPSearchResults(MSG_ViewIndex index, int32 num)
// we got burned here because I made the NC picker inherit from ab_pane...don't use m_container, 
// use the ctr for the index
{
	if (m_container)
	{
		AB_LDAPContainerInfo * ldapCtr = m_container->GetLDAPContainerInfo();
		if (ldapCtr)
			return ldapCtr->LDAPSearchResults(this, index, num);
	}

	return AB_FAILURE;
}

int AB_Pane::FinishSearch()
{
	if (m_IsSearching)
	{
		MSG_InterruptSearch(m_context);
//		m_IsSearching = FALSE; // event
		if (m_container)
		{
			AB_LDAPContainerInfo * ldapCtr = m_container->GetLDAPContainerInfo();
			if (ldapCtr)
				return ldapCtr->FinishSearch(this);
		}
	}
	return AB_SUCCESS;
}

void AB_Pane::ToggleExpansion(MSG_ViewIndex /* line */, int32 * /* numChanged */)
{
	return;
}

int32 AB_Pane::ExpansionDelta (MSG_ViewIndex /* line */)
{
	return 0; 
}

int32 AB_Pane::GetNumLines()
{
	if (m_container)
		return m_container->GetNumEntries();
	else
		return 0;
}

void AB_Pane::NotifyPrefsChange(NotifyCode /* code */)
{
	// not sure if we need to do anything here yet 
	
}

AB_AttribID AB_Pane::ConvertCommandToAttribID(AB_CommandType command)
{
	// Why do we need this function when we can hard code it in AB_CommandType switch statements you ask?
	// Good question. Because the mappings shown here are subject to change. In other words, AB_sortByEmailAddress
	// command may not always mean sort by email address. It could mean, sort by whatever attribute is in the email
	// address column. More details to come on this. 

	switch(command)
	{
	case AB_SortByColumnID0:
		return AB_attribEntryType;
	case AB_SortByColumnID6:
		return AB_attribNickName;
	case AB_SortByColumnID5: /* Locality */
		return AB_attribLocality;
	case AB_SortByColumnID2: /* EmailAddress */
		return AB_attribEmailAddress;
	case AB_SortByColumnID3: /* CompanyName */
		return AB_attribCompanyName;
	case AB_SortByColumnID1: /* FullNameCmd */
		return AB_attribDisplayName;
	default:
		return AB_attribUnknown;
	}

}


ABErr AB_Pane::DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_SUCCESS;

	if (!m_container)
		return AB_FAILURE;

	XP_Bool sortedAscending = m_container->GetSortAscending();
	AB_AttribID sortedBy;   // what we are currently sorted by
	AB_AttribID sortBy;		// what (if any) we want to sort by
	m_container->GetSortedBy(&sortedBy);

	// indices could be in any order so let's sort them.
	if (numIndices > 1)
		XP_QSORT(indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);
	FEStart();  // why do we call this??
	switch(command)
	{
		case AB_UndoCmd:
		case AB_RedoCmd:
			break;

		case AB_ImportCmd:
			m_container->ImportData(this, NULL, 0, AB_PromptForFileName); 
			break;

		case AB_SaveCmd:
		case AB_ExportCmd:
			m_container->ExportData(this, NULL, 0, AB_PromptForFileName);
			break;
		
		case AB_NewMessageCmd:
			ApplyCommandToIndices((AB_CommandType)command, indices, numIndices);
			break;
		// For sorting commands, if we are already sorted by this type, then toggle ascending/descending order
		case AB_SortByColumnID0:
		case AB_SortByColumnID1:
		case AB_SortByColumnID2:
		case AB_SortByColumnID3:
		case AB_SortByColumnID4:
		case AB_SortByColumnID5:
		case AB_SortByColumnID6:
			// handle all the sorting commands in one set of statements..
			sortBy = ConvertCommandToAttribID((AB_CommandType)command);
			if (sortBy == AB_attribUnknown)   // if given an invalid attribute, use the current...
				sortBy = sortedBy;
			
			StartingUpdate(MSG_NotifyScramble, 0, 0);
			if (sortedBy == sortBy)  // are they the same?
				m_container->SetSortAscending(!sortedAscending);
			else
				m_container->SortByAttribute(sortBy);
			EndingUpdate(MSG_NotifyScramble, 0, 0);
			break;
		case AB_DeleteAllCmd:	/* delete all occurrences...*/
		case AB_DeleteCmd:
			// selected indices could be in any
			StartingUpdate(MSG_NotifyNone, 0, 0);
			ApplyCommandToIndices((AB_CommandType)command, indices, numIndices);
			EndingUpdate(MSG_NotifyNone, 0, 0);
			break;
		case AB_AddUserCmd:
		case AB_AddMailingListCmd:
		case AB_PropertiesCmd:
		case AB_ImportLdapEntriesCmd:
			ApplyCommandToIndices((AB_CommandType)command, indices, numIndices);
			break;
		default:
			status = AB_INVALID_COMMAND;
			break;
	}
	
	FEEnd();
	return status;
}

int AB_Pane::ApplyCommandToIndices(AB_CommandType command, MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_FAILURE;
	switch(command)
	{
	case AB_NewMessageCmd:
		status = ComposeMessages(indices, numIndices);
		break;
	case AB_AddMailingListCmd:
		status = ShowPropertySheetForNewType(AB_MailingList);
		break;
	case AB_AddUserCmd:
		status = ShowPropertySheetForNewType(AB_Person);
		break; // nothing to do yet....will eventually want to do a show property sheet...
	case AB_PropertiesCmd:
		status = ShowPropertySheet(indices, numIndices);
		break;
	case AB_DeleteAllCmd:
	case AB_DeleteCmd:
		// we need to sort the indices for deleting to make sure we delete the correct indices first...
		if (numIndices > 1)
				XP_QSORT (indices, numIndices, sizeof(MSG_ViewIndex), CompareViewIndices);
		status = DeleteIndices(indices, numIndices);
		break;
	default:
		XP_ASSERT(0);
	}
	return AB_SUCCESS;
}

int AB_Pane::ComposeMessages (MSG_ViewIndex *indices, int32 numIndices)
{
	char* buf = NULL;
	char* tmp = NULL;
	URL_Struct *url_struct;

	if (numIndices == 0) 
	{
		// just open a compose window
		url_struct = NET_CreateURLStruct ("mailto:", NET_NORMAL_RELOAD);
		if (!url_struct) 
			return eOutOfMemory;
		url_struct->internal_url = TRUE;
		GetURL (url_struct, FALSE);
		return AB_SUCCESS;
	}

	else
	{
		// get the addresses for the entries 
		for (int32 i = numIndices - 1; i > -1; i--) 
		{
			char * address = NULL;
			// okay, convert MSG_ViewIndex into an ABID. Then extract the displayable name completion string for the ABID
			ABID entryID = GetABIDForIndex (indices[i]);
			if (entryID != AB_ABIDUNKNOWN)  // valid entry?
			{
				address = m_container->GetFullAddress(this, entryID);
				if (address)
				{
					if ((numIndices > 1) && (i != 0))
						NET_SACat(&address, ", ");  
					NET_SACat(&buf, address);
				}
				else
				{
					XP_ASSERT(0);  // HACK ALERT!!! PROBABLY NEED TO ADD AN ERROR PRINTING DISPLAY MESSAGE HERE
				}

				XP_FREEIF(address);  // we are done with the address...it has been copied into buf
			}
		}

		if (buf)  // do we have at least one address?
		{
			tmp = NET_Escape(buf, URL_PATH);
			XP_FREEIF(buf); 
			buf = tmp;

			tmp = XP_Cat("mailto:?to=", buf, (char *)/*For Win16*/ NULL);
			XP_FREEIF(buf);
			buf = tmp;
			
			if (buf) // have the XP_Cats succeeded? 
			{
				url_struct = NET_CreateURLStruct (buf, NET_NORMAL_RELOAD);
				if (url_struct) 
				{
					url_struct->internal_url = TRUE;
					FE_GetURL(m_context, url_struct);
				}

				XP_FREEIF(buf);
			}
		}
	}

	return AB_SUCCESS;
}

int AB_Pane::DeleteIndices(MSG_ViewIndex * indices, int32 numIndices)
{
	if (m_container)
	{
		// convert the indices to ABIDs and hand the container an array of ABIDs.
		ABID * ids = (ABID *) XP_ALLOC( sizeof(ABID) * numIndices);
		if (ids)
		{
			for (int i =0; i < numIndices; i++)
				ids[i] = GetABIDForIndex(indices[i]);

			AB_CreateAndRunCopyInfoUrl(this, m_container, ids, numIndices, NULL, AB_CopyInfoDelete);
			// url will delete id array when url is finished...
			return AB_SUCCESS;
		}
	}
	return AB_INVALID_CONTAINER;
}

int AB_Pane::ShowPropertySheetForNewType(AB_EntryType entryType)
{
	int status = AB_SUCCESS;
	MSG_Pane * pane = NULL;
	if (m_container)
	{
		if (entryType == AB_MailingList)
			pane = new AB_MailingListPane(m_context, m_master, m_container, AB_ABIDUNKNOWN);
		else 
			if (entryType == AB_Person)
				pane = new AB_PersonPane(m_context, m_master, m_container, AB_ABIDUNKNOWN, this);
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

int AB_Pane::ShowLDAPPropertySheet(MSG_ViewIndex * indices, int32 numIndices)  // fire off HTML window
{
	int status = AB_SUCCESS;
	char * ldapURL = NULL;
	// open a window for each entry that was selected
	for (int32 i = 0; i < numIndices; i++)
	{
		AB_ContainerInfo * ctrToUse = GetContainerForIndex(indices[i]);
		if (ctrToUse && ctrToUse->GetType() == AB_LDAPContainer)
		{
			AB_LDAPContainerInfo * ldapCtr = ctrToUse->GetLDAPContainerInfo();
			if (ldapCtr)
			{
				AB_AttributeValue * value = NULL;
				AB_AttribID attrib = AB_attribDistName;
				uint16 numItems = 1;
				if (GetEntryAttributes(indices[i], &attrib, &value, &numItems ) == AB_SUCCESS)
					if (value->u.string && XP_STRLEN(value->u.string) > 0)  // make sure the string is really there
					{
						ldapURL = ldapCtr->BuildLDAPUrl("ldap", value->u.string);
						if (ldapURL)  /* then we need to create a struct for the url to run in */
						{
							URL_Struct *url_s = NET_CreateURLStruct(ldapURL, NET_DONT_RELOAD);
							FE_CreateNewDocWindow(m_context /* hang it off our context */, url_s); 
							XP_FREE(ldapURL);
						}
						
						AB_FreeEntryAttributeValues(value, numItems);
					} // getting entry attribute
			} // for loop
		} // server check
	}
	return status;
}

int AB_Pane::ShowPropertySheet(MSG_ViewIndex * indices, int32 numIndices)
{
	int status = AB_SUCCESS;

	for (int i = 0; i < numIndices; i++)
	{
		// if we are  a picker pane, m_container is not necessarily the ctr to use...
		// could also have a mixture of property sheet types to be shown..
		AB_ContainerInfo * ctrToUse = GetContainerForIndex(indices[i]);
		if (ctrToUse && ctrToUse->GetType() == AB_LDAPContainer)
			ShowLDAPPropertySheet(&indices[i], 1);
		else
			if (ctrToUse)
			{
				// ref count ctr!
				ctrToUse->Acquire();

				// turn the view index into an ABID
				ABID entryID = GetABIDForIndex(indices[i]);
			
				// determine if we are a person or mailing list and create an appropriate pane
				AB_AttributeValue * value = NULL;
				AB_EntryType entryType;
				MSG_Pane * pane = NULL;

				if (ctrToUse->GetEntryAttribute(this, entryID, AB_attribEntryType, &value) == AB_SUCCESS)
				{
					entryType = value->u.entryType;
					if (entryType == AB_Person)
						pane = new AB_PersonPane(m_context, m_master, ctrToUse, entryID, this);
					else
						if (entryType == AB_MailingList)
							pane = new AB_MailingListPane(m_context, m_master, ctrToUse, entryID, this);
				}

				if (value)
					AB_FreeEntryAttributeValue(value);
		
				if (pane && m_entryPropSheetFunc)
					m_entryPropSheetFunc(pane, m_context); // non-blocking FE call back 
				else
				{
					AB_ClosePane(pane);  // otherwise, close person pane because it is unused....
					status = AB_FAILURE;
				}
	
				ctrToUse->Release();
				ctrToUse = NULL;
			} // ctrToUse
	} // for each index

	return status;
}

ABErr AB_Pane::GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * /* indices */, int32 numIndices, XP_Bool * IsSelectableState, 
						MSG_COMMAND_CHECK_STATE * checkedState, const char ** displayString, XP_Bool * pluralState)
{
	int status = AB_FAILURE;
	XP_Bool isLDAPContainer = FALSE;
	XP_Bool isOfflineContainer = TRUE;
	AB_AttribID sortedBy = AB_attribGivenName;  // what about a default value...
	XP_Bool ascending = TRUE;
	if (m_container)
	{
		m_container->GetSortedBy(&sortedBy);
		ascending = m_container->GetSortAscending();
		isLDAPContainer = m_container->GetType() == AB_LDAPContainer;
//		isOfflineContainer = m_container->GetType() == AB_PABContainer;
	}

	// for each command, we need to determine (1) is it enabled? (2) it's check state: unused, checked, unchecked
	// and (3) the display string to use. 

	// Our defaults are going to be state = MSG_NotUsed, enabled = FALSE, and displayText = 0;
	const char * displayText = 0;
	XP_Bool enabled = FALSE;
	MSG_COMMAND_CHECK_STATE state = MSG_NotUsed;
	XP_Bool selected = (numIndices > 0);
	XP_Bool plural = FALSE;

	switch ((AB_CommandType) command) {

		/*	FILE MENU
			=========
		*/

		case AB_NewMessageCmd:
			displayText = XP_GetString(MK_MSG_NEW_MAIL_MESSAGE);  // who handles freeing memory for display string??
			enabled = TRUE;
			break;

		case AB_ImportCmd:
			if (m_container && !isLDAPContainer)
				enabled = !(m_container->IsExporting() || m_container->IsImporting());
			else
				enabled = FALSE;
			displayText = XP_GetString(XP_BKMKS_IMPORT_ADDRBOOK);
			break;

		case AB_SaveCmd:
		case AB_ExportCmd:
			if (m_container && !isLDAPContainer)
				enabled = !(m_container->IsExporting() || m_container->IsImporting());
			else
				enabled = FALSE;
			displayText = XP_GetString(XP_BKMKS_SAVE_ADDRBOOK);
			break;

		case AB_CloseCmd:
			enabled = TRUE;
			break;

		/*	EDIT MENU
			=========
		*/

		case AB_UndoCmd:
			displayText = XP_GetString(MK_MSG_UNDO);
			enabled = CanUndo();
			break;

		case AB_RedoCmd:
			displayText = XP_GetString(MK_MSG_REDO);
			enabled = CanRedo();
			break;

		case AB_DeleteAllCmd:
			displayText = XP_GetString(MK_ADDR_DELETE_ALL);  // HACK ONLY TEMPORARY!!! REALLY NEEDS TO BE A RESOURCE STRING!!!
			if (m_container)
				enabled = selected && (m_container->GetType() == AB_PABContainer);
			else
				enabled = FALSE;
			break;

		case AB_DeleteCmd:
			displayText = XP_GetString(XP_FILTER_DELETE);
			if (m_container)
				enabled = selected && (m_container->GetType() != AB_LDAPContainer );
			else
				enabled = FALSE;
//			enabled = selected && !isLDAPContainer && !isOfflineContainer;
			break;

		case AB_LDAPSearchCmd:
			displayText = XP_GetString(MK_MSG_SEARCH_STATUS);
			enabled = isLDAPContainer && !m_IsSearching;  // make sure we aren't currently searching!
			break;

		/*	VIEW/SORT MENUS  - disable them if we don't have a container loaded into the pane..
			===============
		*/

		// eventually these source strings are going to change to match the prefs.....but for now...
		case AB_SortByColumnID0: /* AB_SortByTypeCmd */
			state = (m_container && sortedBy == AB_attribEntryType) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_TYPE);
			// only enable sorting commands if a container has been loaded into the pane...
			enabled = m_container ? TRUE : FALSE;
			break;

		case AB_SortByColumnID1: /*AB_SortByFullNameCmd */
			state = (m_container && sortedBy == AB_attribDisplayName) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_NAME);
			enabled = m_container ? TRUE : FALSE;
			break;
			
		case AB_SortByColumnID2: /* AB_SortByEmailAddress */
			state = (m_container && sortedBy == AB_attribEmailAddress) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_EMAILADDRESS);
			enabled = m_container ? TRUE : FALSE;
			break;

		case AB_SortByColumnID3: /* AB_SortByCompanyName */
			state = (m_container && sortedBy == AB_attribCompanyName) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_COMPANY);
			enabled = m_container ? TRUE : FALSE;
			break;

		case AB_SortByColumnID4: /* Work Phone Number */
			state = MSG_NotUsed;
			displayText = "";
			enabled = m_container ? TRUE : FALSE;
			break;

		case AB_SortByColumnID5:  /* AB_SortByLocality */
			state = (m_container && sortedBy == AB_attribLocality) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_LOCALITY);
			enabled = m_container ? TRUE : FALSE;
			break;

		case  AB_SortByColumnID6: /* AB_SortByNickname */
			state = (m_container && sortedBy == AB_attribNickName) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_BY_NICKNAME);
			enabled = m_container ? TRUE : FALSE;
			break;

		case AB_SortAscending:
			state = (m_container && ascending) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_SORT_BACKWARD);
			enabled = m_container ? TRUE : FALSE;
			break;

		case  AB_SortDescending:
			state = (m_container && !ascending) ? MSG_Checked : MSG_Unchecked;
			displayText = XP_GetString(MK_MSG_SORT_DESCENDING);
			enabled = m_container ? TRUE : FALSE;
			break;
  
		/*	ITEM MENU - disable if we don't have a container loaded 
			============
		*/

		case AB_AddUserCmd:
			displayText = XP_GetString(MK_MSG_ADD_NAME);
			enabled = !isLDAPContainer && isOfflineContainer && m_container;
			break;

		case AB_AddMailingListCmd:
			displayText = XP_GetString(MK_MSG_ADD_LIST);
			enabled = !isLDAPContainer && m_container;
			break;

		case AB_PropertiesCmd:
			displayText = XP_GetString(MK_MSG_PROPERTIES);
			enabled = selected && m_container;
			break;

		case AB_CallCmd:
			displayText = XP_GetString(MK_MSG_CALL);
			enabled = (numIndices == 1) && !isLDAPContainer && m_container;
			break;

		case AB_ImportLdapEntriesCmd:
			displayText = XP_GetString(MK_ADDR_ADD);
/*			enabled = isLDAPContainer && selected && m_container;  */
			enabled = FALSE;
			break;
		default:
			status = AB_INVALID_COMMAND;  // we don't recognize this as a cmd we know hot to enable
			enabled = FALSE;
			displayText = NULL;
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
	
	return status;
}

//******************************************************************
// Drag and Drop 
//******************************************************************
int AB_Pane::DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, 
							 AB_DragEffect request)
{
	int status = AB_FAILURE;
	if (m_container && destContainer && numIndices && indices)
	{
		if (m_container->GetType() == AB_LDAPContainer) // if ldap, fire off an ldap url for each entry...
			return AB_ImportLDAPEntriesIntoContainer(this, indices, numIndices, destContainer);

		// build list of ABIDs...use them with src ctr to copy
		// baby steps first...ignore what they want and just try copying...
		ABID * idArray = (ABID *) XP_ALLOC(sizeof(ABID)*numIndices);
		if (idArray)
		{
			for (int32 i = 0; i < numIndices; i++)
				idArray[i] = GetABIDForIndex(indices[i]);


			// we have a request....run it through the status to make sure we get a valid request...default request: 
			request = DragEntriesIntoContainerStatus(indices, numIndices, destContainer, request);
			AB_AddressCopyInfoState state = AB_CopyInfoCopy;
			switch (request)
			{
				case AB_Require_Copy:
					state = AB_CopyInfoCopy;
					break;
				case AB_Require_Move:
				case AB_Default_Drag:
					state = AB_CopyInfoMove;
					break;
				default: /* including drag not allowed....do nothing */
					break;
			}

			AB_CreateAndRunCopyInfoUrl(this, m_container, idArray, numIndices, destContainer, state);
			// idArray will be freed when the url is finished...
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	return status;
}

AB_DragEffect AB_Pane::DragEntriesIntoContainerStatus(const MSG_ViewIndex * /*indices*/, int32 /*numIndices*/, AB_ContainerInfo * destContainer,
								   AB_DragEffect request)
{
	// ask dest ctr if it accepts entries...LDAP doesn't..MList and PAB should...
	
	// if we are an LDAP ctr...we only support copying to destination...
	
	XP_Bool mustCopy = FALSE;
	XP_Bool destAccepts = FALSE;

	if (m_container && destContainer) // make sure both ctrs are  around...
	{
		if (!m_container->CanDeleteEntries())
			mustCopy = TRUE;

		if (destContainer->AcceptsNewEntries())
			destAccepts = TRUE;

		if (destAccepts)
			if (mustCopy)
				return (AB_DragEffect) (request & AB_Require_Copy);
			else
				if ((request & AB_Require_Move) == AB_Require_Move)
					return AB_Require_Move;
			else // dest does accept and it doesn't have to be a copy so give them what they want
				return request;
		}

	return AB_Drag_Not_Allowed;
}

//******************************************************************
// Getting/Setting Entry attributes
//******************************************************************
int AB_Pane::GetEntryAttributes(MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems)
{
	// get the appropriate container...
	if (m_container)
		return m_container->GetEntryAttributesForIndex(this, index, attribs, values, numItems);
	else
		return AB_INVALID_CONTAINER;
}

int AB_Pane::SetEntryAttributes(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems)
{
	// get the appropriate container...
	if (m_container)
		return m_container->SetEntryAttributesForIndex(index, valuesArray, numItems);
	else
		return AB_INVALID_CONTAINER;
}

//******************************************************************
// Sorting
//******************************************************************
int AB_Pane::SortByAttribute(AB_AttribID attrib, XP_Bool sortAscending)
{
	if (m_container)
		return m_container->SortByAttribute(attrib, sortAscending);
	else
		return AB_INVALID_CONTAINER;
}

int AB_Pane::GetPaneSortedBy(AB_AttribID * attrib)
{
	if (m_container)
		return m_container->GetSortedBy(attrib);
	else
		return AB_INVALID_CONTAINER;
}

//******************************************************************
// Selection
//******************************************************************
XP_Bool AB_Pane::UseExtendedSelection()
{
	return m_container->UseExtendedSelection(this);
}

XP_Bool AB_Pane::AddSelection(MSG_ViewIndex index)
{
	void *entry = m_container->GetSelectionEntryForIndex(index);
	if (entry != NULL)
	{
		if (m_selectionList == NULL)
			m_selectionList = new XPSortedPtrArray(m_container->CompareSelections);

		if (   m_selectionList != NULL
			&& m_selectionList->Add(m_container->CopySelectionEntry(entry)) >= 0)
			return TRUE;
	}

	return FALSE;

}

XP_Bool AB_Pane::IsSelected(MSG_ViewIndex index)
{
	void *entry = m_container->GetSelectionEntryForIndex(index);

	if (entry != NULL && m_selectionList && m_selectionList->FindIndex(0, entry) >= 0)
		return TRUE;
	else
		return FALSE;
}

XP_Bool AB_Pane::RemoveSelection(MSG_ViewIndex index)
{
	if (m_selectionList)
	{
		if (index == MSG_VIEWINDEXNONE)
		{
			for (int i = 0; i < m_selectionList->GetSize(); i++)
				m_container->DeleteSelectionEntry(m_selectionList->GetAt(i));
			m_selectionList->RemoveAll();
			return TRUE;
		}
		else
		{
			void *entry = m_container->GetSelectionEntryForIndex(index);

			if (entry != NULL)
			{
				int i = m_selectionList->FindIndex(0, entry);
				if (i >= 0)
				{
					m_container->DeleteSelectionEntry(m_selectionList->GetAt(i));
					m_selectionList->RemoveAt(i);
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}


/****************************************************************************

  Mailing List Pane definitions here....

*****************************************************************************/

/*****************************************************************************

  Mailing List notification handlers

******************************************************************************/
MSG_ViewIndex AB_MailingListPane::GetIndexForABID(AB_ContainerInfo * ctr, ABID entryID)
{
	if (ctr)
	{
		for (int32 i = 0; i < m_entriesView.GetSize(); i++)
		{
			AB_MailingListEntry * entry = m_entriesView.GetAt(i);
			if ( (entry->ctr == ctr) && (entry->entryID == entryID) )
				return i;
		}
	}

	return MSG_VIEWINDEXNONE;
}

AB_MailingListEntry * AB_MailingListPane::GetEntryForIndex(MSG_ViewIndex index)
{
	if (m_entriesView.IsValidIndex(index))
		return m_entriesView.GetAt(index);
	else
		return NULL;
}

void AB_MailingListPane::OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * /* instigator */ )
{
	// find ctr, entryID in our list..
	MSG_ViewIndex index = GetIndexForABID(ctr, entryID);
	AB_MailingListEntry * entry = GetEntryForIndex(index);
	
	switch(code)
	{
		case AB_NotifyDeleted:
			if (index != MSG_VIEWINDEXNONE)
			{
				// now remove and free any attributes we may have set
				StartingUpdate(MSG_NotifyInsertOrDelete, index, -1);
				m_entriesView.Remove(entry);
				FreeMailingListEntry(entry);  // also releases ref count
				EndingUpdate(MSG_NotifyInsertOrDelete, index, -1);
			}
			break;
		
		case AB_NotifyPropertyChanged:
			if (index != MSG_VIEWINDEXNONE && entry /* make sure it is something in our view */)
			{
				StartingUpdate(MSG_NotifyChanged, index, 1);  // property change so notify FE 
				EndingUpdate(MSG_NotifyChanged, index, 1);
			}
				break;
		
		case AB_NotifyInserted:
			if (m_container && m_container == ctr && !m_commitingEntries) /* make sure we weren't the ones adding the entry... */
			{
				index = m_container->GetIndexForABID(entryID); // our index is bad because we don't have the entry...
				if (index != MSG_VIEWINDEXNONE)
					UpdateViewWithEntry(index, m_container, entryID); // insert the committed entry into our view and update FE
			}
			break;

		case AB_NotifyAll:
			if (ctr == m_container && !m_commitingEntries)
			{
				StartingUpdate(MSG_NotifyAll, 0, 0);
				// update our internal view in case items were inserted or deleted...could be an expensive operation!!!
				UpdateView();
				EndingUpdate(MSG_NotifyAll, 0, 0);
			}
			break;
		
		default:
			break;
			// do nothing
	}
}

void AB_MailingListPane::OnContainerAttribChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, AB_ContainerListener * /* instigator */)
{
	return;  // we don't show any container attribs!
}

void AB_MailingListPane::OnAnnouncerGoingAway(AB_ContainerAnnouncer * /* instigator */)
{
	FE_PaneChanged(this, FALSE, MSG_PaneClose, 0);  // we are losing our container!!!
}

// pane listener notifications!
void AB_MailingListPane::OnAnnouncerGoingAway(AB_PaneAnnouncer * /* announcer */)
{
		// since we were tied to a pane, we must now instruct the FEs to close us
		FE_PaneChanged(this, FALSE, MSG_PaneClose, 0);  // we are losing our container!!!
}

/*****************************************************************************

  Mailing List Pane -> Creating, Initializing, closing, destroying...

******************************************************************************/

AB_MailingListPane::AB_MailingListPane(MWContext * context, MSG_Master * master, AB_ContainerInfo * parentCtr, ABID listABID, 
									   AB_PaneAnnouncer * announcer) : MSG_LinedPane(context, master), AB_ABIDBasedContainerListener(), AB_PropertySheet(announcer)
{
	m_ListABID = listABID;

	m_parentCtr = parentCtr;
	m_parentCtr->Acquire(); // make sure it doesn't go away on us
	m_container = NULL;
	m_commitingEntries = FALSE;
	m_numEntriesAdded = 0;
}

AB_MailingListPane::AB_MailingListPane (MWContext * context,MSG_Master * master, AB_MListContainerInfo * mListCtr, 
										AB_PaneAnnouncer * announcer): MSG_LinedPane(context, master), AB_ABIDBasedContainerListener(), AB_PropertySheet(announcer)
{
	m_container  = NULL;
	m_parentCtr = NULL;
	m_ListABID = AB_ABIDUNKNOWN;
	if (mListCtr)
	{
		m_container = mListCtr;
		m_container->Acquire();  
		m_ListABID = mListCtr->GetContainerABID();

		m_parentCtr = mListCtr->GetParentContainer();
		if (m_parentCtr)
			m_parentCtr->Acquire();
	}
}

AB_MailingListPane::~AB_MailingListPane()
{
	XP_ASSERT(m_container == NULL);
	XP_ASSERT(m_entriesView.GetSize() == 0);
	XP_ASSERT(m_listAttributes.GetSize() == 0);
}

int AB_MailingListPane::Initialize()
{
	// FE calls this after creating and receiving the mlist pane. They are ready to handle notifications on the pane
	int status = AB_SUCCESS;

	StartingUpdate(MSG_NotifyAll, 0, 0);
	if (m_parentCtr)
	{
		m_parentCtr->AddListener(this); // we have already acquired a ref count
		if (m_ListABID != AB_ABIDUNKNOWN && !m_container)
			status = AB_ContainerInfo::Create(m_context, m_parentCtr, m_ListABID, &m_container);
	}

	if (m_container)
	{
		m_newMailingList = FALSE; // we must not be new...
		m_container->AddListener(this);  // we already got the ref count when we created it

		// now build up our entries view, getting an ABID for each entry
		for (uint32 index = 0; index < m_container->GetNumEntries(); index++)
		{
			AB_MailingListEntry * entry = new AB_MailingListEntry; // use new because it contains a class
			if (entry)
			{
				entry->newEntry = FALSE; // so we know to not commit it again

				// set up ctr related info
				entry->entryID = m_container->GetABIDForIndex(index);

				if (entry->entryID != AB_ABIDUNKNOWN)
				{
					entry->nakedAddress = NULL;
					entry->ctr = m_container;
					entry->ctr->Acquire();

					m_entriesView.Add(entry);
				}
				else
					delete entry; // not going to use it
			}
		}
	}
	else
		m_newMailingList = TRUE;

	EndingUpdate(MSG_NotifyAll, 0, 0);

	return AB_SUCCESS;
}

/* static */ int AB_MailingListPane::Close(AB_MailingListPane * pane)
{
	if (pane)
	{
		pane->CloseSelf();
		delete pane;
	}
	return AB_SUCCESS;
}

int AB_MailingListPane::CloseSelf()   // doesn't delete...just closes..
{
	// free any view entries
	for (int32 index = 0; index < m_entriesView.GetSize(); index++)
		FreeMailingListEntry(m_entriesView.GetAt(index));

	m_entriesView.RemoveAll();

	// free any mailing list attributes
	for (int j = 0; j < m_listAttributes.GetSize(); j++)
		AB_FreeEntryAttributeValue(m_listAttributes.GetAt(j));
	
	m_listAttributes.RemoveAll();

	if (m_parentCtr)
	{
		m_parentCtr->RemoveListener(this);
		m_parentCtr->Release();
		m_parentCtr = NULL;
	}

	if (m_container)
	{
		m_container->RemoveListener(this);
		m_container->Release();
		m_container = NULL;
	}

	return AB_SUCCESS;
}

AB_ContainerInfo * AB_MailingListPane::GetContainerForMailingList()
{
	return m_container;
}

MSG_PaneType AB_MailingListPane::GetPaneType()
{
	return AB_MAILINGLISTPANE;
}

void AB_MailingListPane::ToggleExpansion(MSG_ViewIndex /* line */, int32 * numChanged)
{
	// mailing list entries are not expandable/collapsable
	*numChanged = 0; 
}

int32 AB_MailingListPane::ExpansionDelta(MSG_ViewIndex /* line */)
{
	return 0;
}

int32 AB_MailingListPane::GetNumLines()
{
	return m_entriesView.GetSize();
}

void AB_MailingListPane::NotifyPrefsChange(NotifyCode /* code */)
{
	return;
}

ABID AB_MailingListPane::GetABIDForIndex(const MSG_ViewIndex index)
{
	if (m_entriesView.IsValidIndex(index))
	{
		AB_MailingListEntry * entry = GetEntryForIndex(index);
		if (entry)
			return entry->entryID;
	}

	return AB_ABIDUNKNOWN;  // new entry code...
}

MSG_ViewIndex AB_MailingListPane::GetIndexForABID(ABID /* entryID */)
{
	// do i really need to support this? 
	return MSG_VIEWINDEXNONE;
}

XP_Bool AB_MailingListPane::CachedMailingListAttribute(AB_AttribID attrib, int * position)
// returns the position in m_values if that attribute is stored there. returns TRUE if found, FALSE otherwise.
{	
	for (int i = 0; i < m_listAttributes.GetSize(); i++)
		if (m_listAttributes.GetAt(i)->attrib == attrib)
		{
			if (position)
				*position = i;
			return TRUE;
		}
	if (position)
		*position = 0;

	return FALSE;
}

// Getting / Setting mailing list properties

int AB_MailingListPane::GetAttributes(AB_AttribID * attribs, AB_AttributeValue ** valuesArray , uint16 * numItems)
{
	uint16 numItemsSet = 0;
	int position = 0; 
	AB_AttributeValue * values = NULL;

	if (*numItems > 0)
	{
		values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));
		if (values)
			for (uint16 i = 0; i < *numItems; i++)
			{
				if (CachedMailingListAttribute(attribs[i], &position))  // check if it is one of our cached attributes
					AB_CopyEntryAttributeValue(m_listAttributes.GetAt(position), &values[numItemsSet++]);
				else

				if (!m_newMailingList && m_ListABID && m_parentCtr) // is this not a new mailing list? 
				{
					if (m_parentCtr->GetEntryAttribute(this, m_ListABID, attribs[i], &values[numItemsSet]) == AB_SUCCESS)
						numItemsSet++;
				}
				else AB_CopyDefaultAttributeValue(attribs[i], &values[numItemsSet++]);
			}
	}

	*valuesArray = values;
	*numItems = numItemsSet;
	return AB_SUCCESS;
}

int AB_MailingListPane::SetAttributes(AB_AttributeValue * valuesArray, uint16 numItems)
{
	int position = 0;
	// step through the array of attribute values and add them to our pane
	for (uint16 i = 0; i < numItems; i++)
	{
		AB_AttributeValue * newValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (newValue)
		{
			AB_CopyEntryAttributeValue(&valuesArray[i], newValue); // copy new attribute
			
			// determine if it is in our list already...
			if (CachedMailingListAttribute(valuesArray[i].attrib, &position))
			{
				AB_FreeEntryAttributeValue(m_listAttributes.GetAt(position));
				m_listAttributes.RemoveAt(position);
				m_listAttributes.InsertAt(position, newValue);
			}
			else
				m_listAttributes.Add(newValue);
		}
	}
	return AB_SUCCESS;
}

// Getting / Setting mailing list entry properties

int AB_MailingListPane::GetEntryAttributes(const MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	// we have two types of entries we can fetch...entries with ctrs (they have valid ABIDs) or
	// entries which were formed from naked addresses

	if (m_entriesView.IsValidIndex(index))
	{
		AB_MailingListEntry * entry = m_entriesView.GetAt(index);
		if (entry)
			if (entry->entryID != AB_ABIDUNKNOWN && entry->ctr)
				return entry->ctr->GetEntryAttributes(this, entry->entryID, attribs, valuesArray, numItems);
			else  // attempt to fetch from the naked address...
				return GetEntryAttributesForNakedAddress(entry, attribs, valuesArray, numItems);
	}

	return AB_FAILURE;
}


int AB_MailingListPane::GetEntryAttributesForNakedAddress(AB_MailingListEntry * entry, AB_AttribID * attribs,AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	uint16 numItemsSet = 0; 
	AB_AttributeValue * values = NULL;
	int status = AB_SUCCESS;
	
	if (numItems && (*numItems > 0 && entry)) // want at least one item and we have an entry for that index
	{
		values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));
		if (values)
		{
			for (uint16 i = 0; i < *numItems; i++)  // for each requested attribute
			{
				values[i].attrib = attribs[i];
				if ((attribs[i] == AB_attribEmailAddress || attribs[i] == AB_attribDisplayName || attribs[i] == AB_attribFullAddress) && entry->nakedAddress)
					values[i].u.string = XP_STRDUP(entry->nakedAddress);
				else
					AB_CopyDefaultAttributeValue(attribs[i], &values[i]); //empty attribute
			}

			numItemsSet = *numItems;
		}
		else
			status = AB_OUT_OF_MEMORY;
	}
	else
		status = AB_FAILURE;

	if (numItems)
		*numItems = numItemsSet;

	if (valuesArray)
		*valuesArray = values;

	return AB_SUCCESS;
}

int AB_MailingListPane::SetEntryAttributes(const MSG_ViewIndex /*index*/, AB_AttributeValue * /*valuesArray*/, uint16 /*numItems*/)
{
	// no setting of attributes from mailing list pane
return AB_SUCCESS;
}

AB_AttributeValue * AB_MailingListPane::ConvertListAttributesToArray(AB_AttribValueArray * values, uint16 * numItems)
{
	uint16 numAttributes = 0;
	AB_AttributeValue * newValues = NULL;

	if (values)
	{
		numAttributes = values->GetSize();  
		if (numAttributes)
		{
			newValues = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
			if (newValues)
			for (int i = 0; i < numAttributes; i++)
				AB_CopyEntryAttributeValue(values->GetAt(i), &newValues[i]);
		}
	}

	if (numItems)
		*numItems = numAttributes;

	return newValues;
}

int AB_MailingListPane::SetEntryType()
{
	int position = 0;
	int status = AB_SUCCESS;

	if (!CachedMailingListAttribute(AB_attribEntryType, &position)) // if it is not set already....
	{
		AB_AttributeValue * newValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (newValue)
		{
			newValue->attrib = AB_attribEntryType;
			newValue->u.entryType = AB_MailingList;
			// now add the new value
			m_listAttributes.Add(newValue);
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	return AB_SUCCESS;
}

int AB_MailingListPane::CommitNewContainer()
{
	int status = AB_SUCCESS;
	// create an entry in the parent container for our mailing list, set our list attributes then open a container on it.
	if (m_newMailingList && m_parentCtr)
	{
		SetEntryType();
		// convert our mailing list attribute array object into an array of attributes
		uint16 numAttributes = 0;
		AB_AttributeValue * newValues = ConvertListAttributesToArray(&m_listAttributes, &numAttributes);
		if (newValues && numAttributes)
		{
			status = m_parentCtr->AddEntry(newValues, numAttributes, &m_ListABID);
			AB_FreeEntryAttributeValues(newValues, numAttributes);

			// now create a new container for the mailing list if adding entry succeeded
			if (m_ListABID != AB_ABIDUNKNOWN)
					status = AB_ContainerInfo::Create(m_context, m_parentCtr, m_ListABID, &m_container);

			m_newMailingList = FALSE;
		}
		else 
			status = AB_FAILURE; // in order to create a mailing list, we need to have at least one attribute!!!
	}
	else
		status = AB_FAILURE;

	return status;
}

int AB_MailingListPane::CommitNakedAddress(AB_MailingListEntry * entry)
{
	int status = AB_FAILURE;
	AB_AttributeValue  * values = NULL;
	uint16 numItems = 0;

	// winfe is currently passing in an empty naked address for empty mailing lists...filter that out
	if (m_container && entry->nakedAddress)
	{
		if (XP_STRLEN(entry->nakedAddress))
		{
			status = AB_CreateAttributeValuesForNakedAddress(entry->nakedAddress, &values, &numItems);
			if (status == AB_SUCCESS)
			{
				status = m_container->AddEntry(values, numItems, &entry->entryID);
				if (status == AB_SUCCESS)  // change entry from a naked address to a ctr entry
				{
					entry->ctr = m_container;
					entry->ctr->Acquire();
					XP_FREE(entry->nakedAddress);
					entry->nakedAddress = NULL;
				}
			}
		}

		AB_FreeEntryAttributeValues(values, numItems);
	}

	return status;
}

int AB_MailingListPane::CommitEntries() // commit entries to the container
{
	// commit mailing list entries by scanning through the view and committing any new entries we find
	int status = AB_SUCCESS;

	for (int32 index =  0; index < m_entriesView.GetSize(); index++)
	{
		status = AB_FAILURE;
		AB_MailingListEntry * entry = m_entriesView.GetAt(index);
		if (entry && entry->newEntry)
		{
			if (entry->ctr) // if we have a ctr
				status = entry->ctr->CopyEntriesTo(m_container, &entry->entryID, 1);
			else // it is a naked address
				status = CommitNakedAddress(entry);

			if (status == AB_SUCCESS)
				entry->newEntry = FALSE; // entry has now been committed
		} // if entry
		else
			status = AB_FAILURE;
	} // for loop through the view

	return status;
}

int AB_MailingListPane::RemoveDeletedEntriesFromContainer()
{
	int status = AB_FAILURE;

	int32 numEntries = m_deletedEntries.GetSize();
	if (numEntries && m_container)
	{
		ABID * idArray = (ABID *) XP_ALLOC(numEntries * sizeof(ABID));
		if (idArray)
		{
			for (int32 i = 0; i < numEntries; i++)
				idArray[i] = m_deletedEntries.GetAt(i);

			m_container->DeleteEntries(idArray,numEntries,this);

			XP_FREE(idArray);
			status = AB_SUCCESS;
		}
		else
			status =  AB_OUT_OF_MEMORY;
	}

	return status;
}

int AB_MailingListPane::CommitChanges()
{
	// this function is too big right now...unresolved commit issues: do we abort after the first error or keep going?

	// We have a four step commit process....
	// (1) If we are a new mailing list, create a mailing list container
	// (2) remove any ABIDs from the mailing list container that were deleted from the mailing list pane.
	// (3) commit mailing list attribute changes
	// (4) commit mailing list entry attributes (this will in turn generate new mailing list entries in the container if necessary)

	int status = AB_SUCCESS;
	m_commitingEntries = TRUE; // mark ourselves as committing

	if (m_newMailingList && m_parentCtr) // if we are new, then use parent ctr to create new entry
		status = CommitNewContainer();   // set up m_container in the parent ctr

	if (m_parentCtr && m_container && status == AB_SUCCESS)
	{
		// (2) delete any entries the user deleted from the mailing list pane!
		RemoveDeletedEntriesFromContainer();
		// (3) commit mailing list attribute changes...
		uint16 numAttributes = 0;
		AB_AttributeValue * newValues = ConvertListAttributesToArray(&m_listAttributes, &numAttributes);
		if (newValues && numAttributes > 0)
		{
			status = m_parentCtr->SetEntryAttributes(m_ListABID, newValues, numAttributes);
			AB_FreeEntryAttributeValues(newValues, numAttributes);
		}

		// (4) commit mailing list entries by scanning through the view and committing any new entry
		status = CommitEntries();
	} // m_container

	// remember...actually closing the pane will have the effect of releasing the containers
	m_commitingEntries = FALSE; // mark ourselves as finished
	return status;
}

void AB_MailingListPane::FreeMailingListEntry(AB_MailingListEntry * entry)
{
	if (entry)
	{
		if (entry->ctr)
		{
			entry->ctr->Release();
			entry->ctr = NULL;
		}

		if (entry->nakedAddress)
			XP_FREE(entry->nakedAddress);

		XP_FREE(entry);
	}
}

int AB_MailingListPane::DeleteEntries(const MSG_ViewIndex * indices, int32 numIndices)
{
	// do we need to record the ABIDs we are deleting after we take them out of the view? 
	// I think we do....then at commit time, we remove them from the mailing list container
	for (int32 i = 0; i < numIndices; i++)
		if (m_entriesView.IsValidIndex(indices[i]))
		{
			AB_MailingListEntry * entry = m_entriesView.GetAt(indices[i]);
			if (entry)
			{
				// we only need to add the entryID to our deleted list if it is in the list ctr already and it is not a newEntry...
				// if it is a new entry or lives in another ctr, then we don't have to keep track of its removal.
				if (entry->entryID != AB_ABIDUNKNOWN && entry->ctr == m_container && !entry->newEntry)  // does it really exist in the container?
					// add it to our delete cache so that on commit, we remove it from the container
					m_deletedEntries.Add((void *) entry->entryID);
							
				// now remove and free any attributes we may have set
				StartingUpdate(MSG_NotifyInsertOrDelete, indices[i], -1);
				m_entriesView.RemoveAt(indices[i]);
				FreeMailingListEntry(entry);
				EndingUpdate(MSG_NotifyInsertOrDelete, indices[i], -1);
			}
		}

	return AB_SUCCESS;
}

// inserting, replacing entries into the pane..
int AB_MailingListPane::AddEntry(const MSG_ViewIndex index, AB_MailingListEntry * entry, XP_Bool ReplaceCurrent)
{
	MSG_ViewIndex position = m_entriesView.GetSize();
	if (m_entriesView.IsValidIndex(index))
		position = index;

	if (ReplaceCurrent)
		DeleteEntries(&position, 1);

	StartingUpdate(MSG_NotifyInsertOrDelete, position, 1);
	m_entriesView.InsertAt(position, entry);
	EndingUpdate(MSG_NotifyInsertOrDelete, position, 1);

	return AB_SUCCESS;
}

int AB_MailingListPane::UpdateViewWithEntry(const MSG_ViewIndex /*index*/ /* to insert at */, AB_ContainerInfo * ctr, ABID entryID)
// this function creates a new entry in the view but treats it as an old entry...in other words it was added to the database by
// someone else, not the mailing list pane and as such is already committed to the database...so we don't want to commit it again.
// Do NOT use this function for everyday adding of entries to the miling list pane.
{
	if (ctr && entryID != AB_ABIDUNKNOWN)
	{
		// make sure the entry is not already in the view...
		MSG_ViewIndex index = GetIndexForABID(ctr, entryID);
		if (index == MSG_VIEWINDEXNONE)  // not found in our view already...
		{
			// make the entry then call insert or replace handler...
			AB_MailingListEntry * entry = (AB_MailingListEntry *) XP_ALLOC(sizeof(AB_MailingListEntry));
			if (entry)
			{
				entry->ctr = ctr;
				entry->ctr->Acquire();  // will release when view is closed...
				entry->entryID = entryID;
				entry->nakedAddress = NULL;
				entry->newEntry = FALSE; // because we are adding an entry that was already committed
				return AddEntry(index, entry, FALSE /* do not replace any existing entries */);
			}
		}
	}

	return AB_FAILURE;
}
	
	
int AB_MailingListPane::AddEntry(const MSG_ViewIndex index, AB_NameCompletionCookie * cookie, XP_Bool ReplaceCurrent)
{
	if (cookie)
	{
		if (cookie->GetEntryContainer())
		{
			// make the entry then call insert or replace handler...
			AB_MailingListEntry * entry = (AB_MailingListEntry *) XP_ALLOC(sizeof(AB_MailingListEntry));
			if (entry)
			{
				entry->ctr = cookie->GetEntryContainer();
				entry->ctr->Acquire();  // will release when view is closed...
				entry->entryID = cookie->GetEntryABID();
				entry->nakedAddress = NULL;
				entry->newEntry = TRUE;

				return AddEntry(index, entry, ReplaceCurrent);
			}
		}
		else  // otherwise, try it as a naked address...
			return AddEntry(index, cookie->GetNakedAddress(), ReplaceCurrent);
	}

	return AB_FAILURE;
}

int AB_MailingListPane::AddEntry(const MSG_ViewIndex index, const char * nakedAddress, XP_Bool ReplaceCurrent)
{
	int status = AB_SUCCESS;

	if (nakedAddress)
	{
		AB_MailingListEntry * entry = (AB_MailingListEntry *) XP_ALLOC(sizeof(AB_MailingListEntry));
		if (entry)
		{
			entry->entryID = AB_ABIDUNKNOWN;
			entry->ctr = NULL;
			entry->nakedAddress = XP_STRDUP(nakedAddress);
			entry->newEntry = TRUE;
			
			status = AddEntry(index, entry, ReplaceCurrent);
			if (status != AB_SUCCESS)
				FreeMailingListEntry(entry);
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	return status;
}


int AB_MailingListPane::InsertEntry  (const MSG_ViewIndex index)
{
	// okay, we want to insert an entry at the specified index
	AB_MailingListEntry * entry = new AB_MailingListEntry;
	if (entry)
	{
		entry->entryID = AB_ABIDUNKNOWN; // we are inserting a new (blank) mailing list entry
		MSG_ViewIndex position = m_entriesView.GetSize(); // default is always end of view....
		if (m_entriesView.IsValidIndex(index))
			position = index;

		// notify FE that we have inserted an entry
		StartingUpdate(MSG_NotifyInsertOrDelete, position, 1);
		m_entriesView.InsertAt(position, entry);
		EndingUpdate(MSG_NotifyInsertOrDelete, position, 1);
		return AB_SUCCESS;
	}

	return AB_FAILURE;
}

int AB_MailingListPane::ReplaceEntry (const MSG_ViewIndex /* index */)
{

	return AB_SUCCESS;
}

/*****************************************************************************

  Mailing List Pane -> Do command and command status

******************************************************************************/

ABErr AB_MailingListPane::DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices)
{

	switch ((AB_CommandType) command)
	{
		case AB_ReplaceLineCmd:  // we used to support these commands...but not anymore...they are phasing out
		case AB_InsertLineCmd:
			break;
		case AB_DeleteCmd:
			return DeleteEntries(indices, numIndices);

		default:
			break;
	}

	return AB_SUCCESS;
}

ABErr AB_MailingListPane::GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * /* indices */, int32 numIndices, XP_Bool * IsSelectableState, 
						MSG_COMMAND_CHECK_STATE * checkedState, const char ** displayString, XP_Bool * pluralState)
{
	// Our defaults are going to be state = MSG_NotUsed, enabled = FALSE, and displayText = 0;
	const char * displayText = 0;
	XP_Bool enabled = FALSE;
	MSG_COMMAND_CHECK_STATE state = MSG_NotUsed;
	XP_Bool selected = (numIndices > 0);
	XP_Bool plural = FALSE;

	switch( (AB_CommandType) command)
	{
		case AB_DeleteCmd:
			enabled = selected;  // if something is selected, then we can enable this command
			displayText = XP_GetString(XP_FILTER_DELETE);  // should verify this resource string...I got it from 4.0
			break;
		case AB_ReplaceLineCmd: // no longer supportng these cmds
		case AB_InsertLineCmd:
			enabled = FALSE;
			break;
		default:
			break;
	}

	if (IsSelectableState)
		*IsSelectableState = enabled;

	if (pluralState)
		*pluralState = plural;

	if (checkedState)
		*checkedState = state;

	if (displayString)
		*displayString = displayText;

	return AB_SUCCESS;
}

int AB_MailingListPane::UpdateView()
{
	// okay, we have no idea if things were added or deleted behind us...
	// expensive operation.......try to avoid having to call this. 
	// make one pass of m_container looking for additions that we don't have...

	if (m_container)
	{
		ABID entryID = AB_ABIDUNKNOWN;
		for (MSG_ViewIndex index = 0; index < m_container->GetNumEntries(); index++)
		{
			entryID = m_container->GetABIDForIndex(index);
			if (entryID != AB_ABIDUNKNOWN)
			{
				// scan through our view, making sure we have the entry
				XP_Bool found = FALSE;
				for (MSG_ViewIndex entriesIndex = 0; entriesIndex < (uint32) m_entriesView.GetSize() && !found; entriesIndex++)
				{
					AB_MailingListEntry * entry = GetEntryForIndex(entriesIndex);
					if (entry && entry->ctr == m_container && entry->entryID == entryID)  // match??
						found = TRUE;
				}

				if (!found) // our view doesn't have the entry so add it....
					UpdateViewWithEntry(index, m_container,entryID);
			}
		}

		entryID = AB_ABIDUNKNOWN;
		// make a pass removing any entries which were removed from us behind the scenes...
		for (MSG_ViewIndex entriesIndex = 0; entriesIndex < (uint32) m_entriesView.GetSize(); entriesIndex++)
		{
			AB_MailingListEntry * entry = GetEntryForIndex(entriesIndex);
			if (entry)
			{
				if (m_container == entry->ctr)
					DeleteEntries(&entriesIndex, 1);
			}
		}
	}

	return AB_SUCCESS;
}

/*****************************************************************************

  Mailing List Pane -> Drag and Drop 

******************************************************************************/

int AB_MailingListPane::DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, 
							 AB_DragEffect /*request*/)
{
	// you should be able to copy or move entries from the mailing list into other ctrs. This is a complicated operation.
	// Just allowing copy for now because moving is ugly. Remember, the entries in the mailing list pane do not all live in the
	// ctr unless they have been committed. So we really need to process each one individually. 

	int status = AB_FAILURE;

	if (destContainer && numIndices && indices)
	{
		// extract src ctr for each selected index and perform the action on it.
		for (int32 i = 0; i < numIndices; i++)
		{
			AB_MailingListEntry * entry = GetEntryForIndex(indices[i]);
			if (entry && entry->ctr && (entry->entryID != AB_ABIDUNKNOWN)) // ignore naked addresses for now
			{
				if (entry->ctr->GetType() == AB_LDAPContainer) // if src container is LDAP, build ldap url
					AB_ImportLDAPEntriesIntoContainer(this, &indices[i], 1, destContainer);
				else // always perform a copy for now...
				{
					AB_AddressCopyInfoState state = AB_CopyInfoCopy;
					// AB_CreateAndRunCopyInfoUrl wants its own copy of the IDArray...so let's give it to them...
					ABID * entryID = (ABID *) XP_ALLOC(sizeof(ABID));
					if (entryID)
					{
						*entryID = entry->entryID;
						AB_CreateAndRunCopyInfoUrl(this, entry->ctr, entryID, 1, destContainer, state);
					}
					else
						status = AB_OUT_OF_MEMORY;

				}
			}
		}

		status = AB_SUCCESS;
	}

	return status;
}

AB_DragEffect AB_MailingListPane::DragEntriesIntoContainerStatus(const MSG_ViewIndex * /*indices*/, int32 /*numIndices*/, AB_ContainerInfo * destContainer,
								   AB_DragEffect request)
{
	// ask dest ctr if it accepts entries...LDAP doesn't..MList and PAB should...
	
	// if we are an LDAP ctr...we only support copying to destination...
	
	XP_Bool mustCopy = FALSE;
	XP_Bool destAccepts = FALSE;

	if (m_container && destContainer) // make sure both ctrs are  around...
	{
		if (m_container->CanDeleteEntries())
			mustCopy = TRUE;

		if (destContainer->AcceptsNewEntries())
			destAccepts = TRUE;

		if (destAccepts)
			if (mustCopy)
				return (AB_DragEffect) (request & AB_Require_Copy);
			else
				if ((request & AB_Require_Move) == AB_Require_Move)
					return AB_Require_Move;
			else // dest does accept and it doesn't have to be a copy so give them what they want
				return request;
		}


	return AB_Drag_Not_Allowed;
}

/***************************************************************************

  AB_PropertySheet base class definitions. AB_PersonPane and AB_MailingListPane
  are both AB_PropertySheets. We use this so we can have a generic AB_ABPaneView
  class later on.

*****************************************************************************/

AB_PropertySheet::AB_PropertySheet(AB_PaneAnnouncer * announcer)
{
	m_abPaneView = new AB_ABPaneView(announcer, this);
}

AB_PropertySheet::~AB_PropertySheet()
{
	if (m_abPaneView)
		delete m_abPaneView;
}

/****************************************************************************

  Person Entry Pane definitions here....(and helper AB_ABPaneView class)

*****************************************************************************/

AB_ABPaneView::AB_ABPaneView(AB_PaneAnnouncer * announcer, AB_PropertySheet * propSheet)
{
	if (propSheet)
		m_propertySheet = propSheet;
	else
		m_propertySheet = NULL;

	if (announcer)
	{
		m_paneAnnouncer = announcer;
		m_paneAnnouncer->AddListener(this);
	}
	else
		m_paneAnnouncer = NULL;
}

AB_ABPaneView::~AB_ABPaneView()
{
	if (m_paneAnnouncer)
	{
		m_paneAnnouncer->RemoveListener(this);  // I'm going away!
		m_paneAnnouncer = NULL;
	}
}

void AB_ABPaneView::OnAnnouncerGoingAway(AB_PaneAnnouncer * instigator)
{
	if (instigator == m_paneAnnouncer)
	{
		m_paneAnnouncer->RemoveListener(this);
		m_paneAnnouncer = NULL;
		if (m_propertySheet)
			m_propertySheet->OnAnnouncerGoingAway(instigator);  // forward the notification onto the person pane
	}
}

/* AB_PersonPane */

AB_PersonPane::AB_PersonPane(MWContext * context, MSG_Master * master, AB_ContainerInfo * ctr, ABID entryID,
							 AB_PaneAnnouncer * announcer, XP_Bool identityPane) : MSG_Pane(context, master), AB_ABIDBasedContainerListener(), AB_PropertySheet(announcer)
{
	m_container = ctr;
	m_entryID = entryID;

	// look up ABID in container, if it exists, load attributes set. 
	if (m_container)
		m_newEntry = !m_container->IsInContainer(entryID);
	else
		m_newEntry = TRUE;

	m_identityPane = identityPane;
	if (m_identityPane)
		m_newEntry = FALSE;
	Initialize();
}

/* static */ int AB_PersonPane::CreateIdentityPane(MWContext * context, MSG_Master * master, AB_AttributeValue * identityValues, uint16 numItems, MSG_Pane ** pane)
{
	int status = AB_SUCCESS;
	AB_PersonPane * pPane = NULL;

	if (pane)
	{
		pPane = new AB_PersonPane(context, master, NULL,AB_ABIDUNKNOWN, NULL, TRUE);
		if (pPane)
			status = pPane->InitializeWithAttributes(identityValues, numItems);
		else
			status = AB_FAILURE;

		*pane = pPane;
	}
	
	return status; 
}

AB_PersonPane::~AB_PersonPane()
{
	XP_ASSERT(m_container == NULL);
	XP_ASSERT(m_values.GetSize() == 0);;
}

void AB_PersonPane::OnAnnouncerGoingAway(AB_PaneAnnouncer * /* announcer */)
{
		// since we were tied to a pane, we must now instruct the FEs to close us
		FE_PaneChanged(this, FALSE, MSG_PaneClose, 0);  // we are losing our container!!!
}

int AB_PersonPane::CloseSelf()  // non-static version
{
	if (m_container)
	{
		m_container->RemoveListener(this);
		m_container->Release();
		m_container = NULL;
	}
	
	for (int i = 0; i < m_values.GetSize(); i++)
		AB_FreeEntryAttributeValue(m_values.GetAt(i));
	
	m_values.RemoveAll();

	return AB_SUCCESS;
}

/* static */ int AB_PersonPane::Close(AB_PersonPane * pane)
{
	// eventually we will have to destroy our attribute values array if it is still around (i.e. user was never committed)
	if (pane)
	{
		pane->CloseSelf();
		delete pane;
	}

	return AB_SUCCESS;
}

void AB_PersonPane::Initialize()
{
	if (m_container)
	{
		m_container->AddListener(this);
		m_container->Acquire(); // ref count our container! 
	}
}

/* Use this function to load the property sheet with a set of attribute values. The property sheet makes copies of
	the attributes...you are still responsible for freeing your attrib value array */
int AB_PersonPane::InitializeWithAttributes(AB_AttributeValue * values, uint16 numItems)
{
	return SetAttributes(values, numItems);
}

MSG_PaneType AB_PersonPane::GetPaneType()
{
	return AB_PERSONENTRYPANE;
}

void AB_PersonPane::OnContainerAttribChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, AB_ContainerListener * /* instigator */)
{
	return;  // we don't show any container attribs!
}

void AB_PersonPane::OnContainerEntryChange (AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * /* instigator */ )
{
	if (entryID == m_entryID)  // are we effected?
	{
		switch(code)
		{
		case AB_NotifyDeleted:
			FE_PaneChanged(this, FALSE, MSG_PaneClose, 0);
			break;
		case AB_NotifyPropertyChanged:
			FE_PaneChanged(this, FALSE, MSG_PaneChanged, 0);
			break;
		default:
			break;
			// do nothing
		}
	}
}

void AB_PersonPane::OnAnnouncerGoingAway(AB_ContainerAnnouncer * /* instigator */)
{
	FE_PaneChanged(this, FALSE, MSG_PaneClose, 0);  // we are losing our container!!!
}


AB_ContainerInfo * AB_PersonPane::GetContainerForPerson()
{
	return m_container;  // should we pass back a const return type instead??
}

ABID AB_PersonPane::GetABIDForPerson()
{
	return m_entryID;
}

XP_Bool AB_PersonPane::CachedAttributePosition(AB_AttribID attrib, int * position)
// returns the position in m_values if that attribute is stored there. returns TRUE if found, FALSE otherwise.
{	
	for (int i = 0; i < m_values.GetSize(); i++)
		if (m_values.GetAt(i)->attrib == attrib)
		{
			*position = i;
			return TRUE;
		}
	*position = 0;
	return FALSE;
}

int AB_PersonPane::SetAttributes(AB_AttributeValue * valuesArray, uint16 numItems)
{
	int position = 0;
	// step through the array of attribute values and add them to our pane
	for (uint16 i = 0; i < numItems; i++)
	{
		AB_AttributeValue * newValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (newValue)
		{
			AB_CopyEntryAttributeValue(&valuesArray[i], newValue); // copy new attribute
			
			// determine if it is in our list already...
			if (CachedAttributePosition(valuesArray[i].attrib, &position))
			{
				AB_FreeEntryAttributeValue(m_values.GetAt(position));
				m_values.RemoveAt(position);
				m_values.InsertAt(position, newValue);
			}
			else
				m_values.Add(newValue);
		}
	}
	return AB_SUCCESS;
}

int AB_PersonPane::GetAttributes(AB_AttribID * attribs, AB_AttributeValue ** valuesArray, uint16 * numItems)
{
	// in the long run, we are either going to want to return attributes from the database (if it is not a new
	// entry and they have not set any attributes) OR return our cache of attributes that have previously been set.

	uint16 numItemsSet = 0;
	int position = 0; 
	AB_AttributeValue * values = NULL;

	if (*numItems > 0)
	{
		values = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * (*numItems));
		if (values)
			for (uint16 i = 0; i < *numItems; i++)
			{
				if (CachedAttributePosition(attribs[i], &position))  // check if it is one of our cached attributes
					AB_CopyEntryAttributeValue(m_values.GetAt(position), &values[numItemsSet++]);
				else
					if (!m_newEntry && m_container) // does it exist in the container?
						if (m_container->GetEntryAttribute(this, m_entryID, attribs[i], &values[numItemsSet]) == AB_SUCCESS)
							numItemsSet++;
			}
	}

	*valuesArray = values;
	*numItems = numItemsSet;
	return AB_SUCCESS;
}

int AB_PersonPane::SetEntryType()
{
		int position = 0;
	int status = AB_SUCCESS;

	if (!CachedAttributePosition(AB_attribEntryType, &position)) // if it is not set already....
	{
		AB_AttributeValue * newValue = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue));
		if (newValue)
		{
			newValue->attrib = AB_attribEntryType;
			newValue->u.entryType = AB_Person;
			// now add the new value
			m_values.Add(newValue);
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	return AB_SUCCESS;
}


int AB_PersonPane::CommitIdentityPane(AB_AttributeValue * values, uint16 numAttributes) // write any attributes out to the personal identity vcard which is stored in prefs.
{
	int status = AB_SUCCESS;
	if (values)
	{
		char * vCard = NULL;
		status = AB_ConvertAttribValuesToVCard(values, numAttributes, &vCard);
		if (vCard)
		{
			status = AB_ExportVCardToPrefs(vCard);
			XP_FREE(vCard);
		}
	}

	return status;
}

int AB_PersonPane::CommitChanges()
{
	int status = AB_SUCCESS;
	// write out any cached attributes to the container
	if (m_values.GetSize() > 0)
	{
		if (m_newEntry)
			SetEntryType();
		uint16 numAttributes = m_values.GetSize();  // we already know this is > 0
		AB_AttributeValue * newValues = (AB_AttributeValue *) XP_ALLOC(sizeof(AB_AttributeValue) * numAttributes);
		if (newValues)
		{
			for (int i = 0; i < m_values.GetSize(); i++)
				AB_CopyEntryAttributeValue(m_values.GetAt(i), &newValues[i]);

			if (m_identityPane) 
				status = CommitIdentityPane(newValues, numAttributes);
			else
				if (m_newEntry && m_container)	
					status = m_container->AddEntry(newValues, numAttributes, &m_entryID);
			else
				if (m_container)  // entry must already exist....
					status = m_container->SetEntryAttributes(m_entryID, newValues, numAttributes);

			AB_FreeEntryAttributeValues(newValues, numAttributes);
		}
		else
			status = AB_OUT_OF_MEMORY;
	}

	return status;
}

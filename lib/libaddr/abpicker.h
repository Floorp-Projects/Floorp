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

#ifndef _ABPickerPane_H_
#define _ABPickerPane_H_

#include "abpane2.h"
#include "abcinfo.h"
#include "namecomp.h"

class AB_ABIDContainerView;

/***********************************************************************************************************************
	The AB_PickerPane is used in conjunction with name completion for displaying matches to a name completion search.
	It also holds the code which actually organizes the name completion search. Each line in the picker pane corresponds
	to a name completion match. The entries are organized by container. The picker pane always shows the naked address 
	which was typed by the user to start the name completion. It is then followed by matches in all of the address books
	and at MOST one LDAP directory. You cannot sort the picker pane as it must be ordered by container. 

************************************************************************************************************************/

/* An AB_PickerEntry is either a ctr, entryID pair or a naked address. We could have used a union inside
	of this struct but I felt it wouldn't have been worth the complexity because the union would have needed
	another struct for the ctr, ABID pair. *sigh* */

typedef struct AB_PickerEntry
{
	AB_ContainerType ctrType;	/* used to determine value of the union */
	AB_ContainerInfo * ctr;
	ABID entryID;  /* unique ID used to look up entry in the table */
	char * nakedAddress;
} AB_PickerEntry;

class AB_PickerEntriesArray : public XPPtrArray
{
public:
	AB_PickerEntry * GetAt(int i) const { return (AB_PickerEntry *) XPPtrArray::GetAt(i);}
};

class AB_PickerPane : public AB_Pane
{
public:
	AB_PickerPane(MWContext * context, MSG_Master * master, uint32 pageSize = AB_k_DefaultPageSize);
	virtual ~AB_PickerPane();

	static int Create(MSG_Pane ** pickerPane, MWContext * context, MSG_Master * master, uint32 pageSize = AB_k_DefaultPageSize);
	static int Close(AB_PickerPane * pane);

	// getting an entry's container type
	AB_ContainerType GetEntryContainerType(MSG_ViewIndex index);

	// getting a name completion cookie for the pane
	AB_NameCompletionCookie * GetNameCompletionCookieForIndex(MSG_ViewIndex index);

	// executing a name completion seach + related callbacks from container
	int NameCompletionSearch(const char * completionValue, AB_NameCompletionExitFunction * exitFunction, XP_Bool paneVisible, void * FECookie);
	virtual void ProcessSearchResult(AB_ContainerInfo * ctr, ABID resultID);
	virtual void SearchComplete(AB_ContainerInfo * ctr);

	// converting FE view indices into the actual entries in a container and vice versa
	virtual ABID GetABIDForIndex(const MSG_ViewIndex index);
	virtual MSG_ViewIndex GetEntryIndexForID(ABID id); // VIEWINDEXNONE if not in the picker pane
	XP_Bool IsNakedAddress(AB_PickerEntry * entry);
	AB_ContainerInfo * GetContainerForIndex(const MSG_ViewIndex);

	// basic message pane stuff
	virtual void ToggleExpansion (MSG_ViewIndex /*line*/, int32 * /*numChanged*/) {return;}
	virtual int32 ExpansionDelta (MSG_ViewIndex /*line*/) {return 0;}
	virtual int32 GetNumLines () { return m_entriesView.GetSize();}
	virtual MSG_PaneType GetPaneType() { return AB_PICKERPANE;}

	virtual int LDAPSearchResults(MSG_ViewIndex index, int32 num); // process a search result
	virtual int FinishSearch();

	// command information...
	virtual ABErr DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices);
	virtual ABErr GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * indices, int32 numIndices,
						XP_Bool * selectable_p, MSG_COMMAND_CHECK_STATE * selected_p,
						const char ** displayString, XP_Bool * plural_p);

	// getting entry attributes
	virtual int GetEntryAttribute(MSG_ViewIndex index, AB_AttribID attrib, AB_AttributeValue ** value);
	virtual int GetEntryAttributes(MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems);
	virtual	int SetEntryAttribute(MSG_ViewIndex index, AB_AttributeValue * value);
	virtual int SetEntryAttributes(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems);

	// getting container attributes for an index
	int GetContainerAttribute (MSG_ViewIndex index, AB_ContainerAttribute attrib, AB_ContainerAttribValue ** value);
	int GetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribute * attribArray, AB_ContainerAttribValue ** value, uint16 * numItems);

	int SetContainerAttribute (MSG_ViewIndex index, AB_ContainerAttribValue * value);
	int SetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribValue * valuesArray, uint16 numItems);

	// entry column data methods
	AB_ColumnInfo * GetColumnInfo(AB_ColumnID columnID);
	int GetNumColumnsForPicker();
	int GetColumnAttribIDs(AB_AttribID * attribIDs, int * numAttribs);
	
	// drag and drop APIs.
	virtual int DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, AB_DragEffect request);
	virtual AB_DragEffect DragEntriesIntoContainerStatus(const MSG_ViewIndex * /*indices*/, int32 /*numIndices*/, AB_ContainerInfo * destContainer,AB_DragEffect request);

	// container notification handlers
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator);

	/* the following is a dummy function because we inherit it from AB_Pane. We are not a view index based ctr listener so we should always ignore this!! */
	virtual void OnContainerEntryChange(AB_ContainerInfo * /* ctr */, AB_NOTIFY_CODE /* code */, MSG_ViewIndex /* index */, int32 /* numChanged */, AB_ContainerListener * /* instigator */) {return;}

protected:
	AB_PickerEntriesArray m_entriesView;

	// state for a name completion search
	AB_NameCompletionExitFunction * m_exitFunction;
	XP_Bool m_paneVisible; // our search behavior is different if the pane is not visible to the user vs. when it is visible to the user
	void * m_FECookie;
	char * m_completionValue;

	AB_ContainerInfoArray m_searchQueue;  // queue of ctrs to be searched for the name completion. Add to end, remove from front
	AB_ContainerInfo * m_currentSearchCtr;
	
	AB_PickerEntry * GetEntryForIndex(MSG_ViewIndex index);
	
	void LoadNakedAddress(const char * completionValue);
	void ClearResults();
	void StartNextSearch();
	void TerminateSearch(); // halts current name completion searches but doesn't remove any results.
	void LoadSearchQueue(); // opens containers that need searching and adds them to the search queue...
	void AllSearchesComplete();

	void CloseSelf();

	XP_Bool ValidServerToSearch(DIR_Server * server);

	AB_ABIDContainerView * m_ABIDCtrView;
};



/***********************************************************************************
	When you create a pane which inherits from another pae in the address book, the 
	inherited listener types may not match. For example, consider the AB_PickerPane
	which inherits from AB_Pane. AB_Pane is a index based container listener. But
	the picker pane needs to be an ABID based container listener. In order to avoid
	multiple inheritence, we use this AB_ABIDContainerView class which is used to 
	act as the ABIDBasedContainerListener. It is a member of AB_PickerPane and 
	AB_PickerPane is a member of it. Thus, the pane view can forward any notifications
	on to the picker pane.
**************************************************************************************/

class AB_ABIDContainerView : public AB_ABIDBasedContainerListener
{
public:
	AB_ABIDContainerView(AB_PickerPane * pane);
	virtual ~AB_ABIDContainerView();

	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);
	
protected:
	AB_PickerPane * m_pickerPane;
};

#endif // _ABPickerPane_H_

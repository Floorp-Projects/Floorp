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

#ifndef _ABPane_H_
#define _ABPane_H_

#include "abcom.h"
#include "abcinfo.h"
#include "errcode.h"
#include "msglpane.h"
#include "msg.h"
#include "aberror.h"
#include "abntfy.h"

/* include abook database files here */


class AB_ABPaneView;
class AB_PropertySheet;

class ABIDArray : public XPPtrArray
{
public: 
	ABID GetAt(int i) const { return (ABID) XPPtrArray::GetAt(i);}
};

/*********************************************************************************************************

  The AB_Pane is the right hand pane in the address book. It acts like a "view" on a containerInfo in that
  it displays a range of the container's entries to the user. As such, the pane's view is a list of ABIDs
  which correspond to the entrid ABIDs in the container. Containers are loaded into the AB_Pane.

  The AB_Pane is a listener on the currently loaded container. Any changes to entries or attributes of
  the container cause the AB_Pane to be notified. The AB_Pane in turn produces pane and list change 
  notifications to the FEs to reflect changes. 

  Each line or MSG_ViewIndex in the AB_Pane corresonds to an entry in the container. You can Get and Set
  entry attributes for a given view index in the pane. The AB_Pane responds to commands for deleting entries,
  creating new entries (both mailing list and person entry card) among others. 

  When you are done with the AB_Pane you should call its Close method which will release its reference
  count on the currently loaded container, and remove itself as a listener from the loaded container.

**********************************************************************************************************/

const uint32 AB_k_DefaultPageSize = 24;

class AB_Pane : public MSG_LinedPane, public AB_IndexBasedContainerListener, public AB_PaneAnnouncer
{
public:
	AB_Pane(MWContext * context, MSG_Master * master, uint32 pageSize = AB_k_DefaultPageSize);
	virtual ~AB_Pane();

	static int Create(MSG_Pane ** abPane, MWContext * context, MSG_Master * master, uint32 pageSize = AB_k_DefaultPageSize);
	
	/* to load a new container into the pane call this */
	int LoadContainer(DIR_Server * server);
	int LoadContainer(AB_ContainerInfo * container); 
	static int Close(AB_Pane * abPane);

	virtual ABID GetABIDForIndex(const MSG_ViewIndex index);
	virtual MSG_ViewIndex GetEntryIndexForID(ABID id);
	virtual AB_ContainerInfo * GetContainerForIndex(const MSG_ViewIndex index);

	int TypedownSearch(const char * typedownValue, MSG_ViewIndex startIndex);
	// for now, these 2 methods reserved for name completion use. 
	virtual void ProcessSearchResult(AB_ContainerInfo * /*ctr*/, ABID /*resultID*/) { return;}
	virtual void SearchComplete(AB_ContainerInfo * /*ctr*/) {return;}

	virtual int SearchDirectory (char * searchString);
	virtual int LDAPSearchResults(MSG_ViewIndex index, int32 num);
	virtual int FinishSearch();

	// MSG_Pane methods
	virtual MSG_PaneType GetPaneType();

	// MSG_LinedPane methods
	virtual void ToggleExpansion (MSG_ViewIndex line, int32 *numChanged);
	virtual int32 ExpansionDelta (MSG_ViewIndex line);
	virtual int32 GetNumLines ();

	// MSG_PrefsNotify methods
	virtual void NotifyPrefsChange (NotifyCode code);

	virtual ABErr DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices);
	virtual ABErr GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * indices, int32 numIndices,
						XP_Bool * selectable_p, MSG_COMMAND_CHECK_STATE * selected_p,
						const char ** displayString, XP_Bool * plural_p);

	// Drag and Drop methods
	virtual int DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, 
								 AB_DragEffect request);
	virtual AB_DragEffect DragEntriesIntoContainerStatus(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer,
										AB_DragEffect request);

	// getting/setting entry attributes
	virtual int GetEntryAttributes(MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values, uint16 * numItems);
	virtual int SetEntryAttributes(MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems);

	// sorting
	int SortByAttribute(AB_AttribID, XP_Bool sortAscending);
	int GetPaneSortedBy(AB_AttribID * id);

	// notifications
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, MSG_ViewIndex index, int32 numChanged, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);
	
	// Undo & Redo 
	XP_Bool CanUndo() {return FALSE;}
	XP_Bool CanRedo() {return FALSE;}

	// Virtual List support methods
	int SetPageSize(uint32 pageSize);
	// Selection support methods
	XP_Bool UseExtendedSelection();
	XP_Bool AddSelection(MSG_ViewIndex index);
	XP_Bool IsSelected(MSG_ViewIndex index);
	XP_Bool RemoveSelection(MSG_ViewIndex index);

protected:
	XP_Bool m_IsSearching; // is the pane currently performing a search? 
	ABID m_paneID;  // do i still need this?
	AB_ContainerInfo * m_container;

	// virtual list support methods
	uint32 m_pageSize;
	XPSortedPtrArray *m_selectionList;

	// Do Command related helper routines
	int ShowPropertySheet(MSG_ViewIndex * indices, int32 numIndices);
	int ShowPropertySheetForNewType(AB_EntryType entryType); 
	int DeleteIndices(MSG_ViewIndex * indices, int32 numIndices);
	int ComposeMessages (MSG_ViewIndex *indices, int32 numIndices);
	int ApplyCommandToIndices(AB_CommandType command, MSG_ViewIndex * indices, int32 numIndices);
	AB_AttribID ConvertCommandToAttribID(AB_CommandType command);

	// building LDAP property sheet helper methods
	int ShowLDAPPropertySheet(MSG_ViewIndex * indices, int32 numIndices);
	int ImportLDAPEntries(MSG_Pane * pane, const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer);

};


/******************************************************************************************

  AB_PropertySheet and AB_ABPaneView are two classes introduced to avoid a multiple inheritence 
  problem. The MI problem arises from the fact that both the mailing list and person entry panes
  need to be container and pane listeners. Container listeners to receive notifications if the entries
  they are displaying change and pane listeners in case their source pane (typically an AB_Pane) is
  going away. AB_PropertySheet is inherited by both the mailing list and person entry panes. 

  AB_ABPaneView is a view class which has a property sheet. It registers itself as a listener to the 
  source pane (typically an AB_Pane). Any notifications it receives from the source pane, it "forwards"
  on to its property sheet member. Thus, the propery sheet must contain any notification APIs you want 
  to be able to handle. These APIs must then be supported in the peron and mailing list panes. 

  *********************************************************************************************/

class AB_PropertySheet  
{
public:
	AB_PropertySheet(AB_PaneAnnouncer * announcer);
	virtual ~AB_PropertySheet();

	// pane listener notification APIs a property sheet pane needs to be able to respond to
	virtual void OnAnnouncerGoingAway(AB_PaneAnnouncer * /* instigator */) { return;}
protected:
	AB_ABPaneView * m_abPaneView;
};

class AB_ABPaneView : public AB_PaneListener
{
public:
	AB_ABPaneView(AB_PaneAnnouncer * announcer, AB_PropertySheet * propSheet);
	virtual ~AB_ABPaneView();

	virtual void OnAnnouncerGoingAway(AB_PaneAnnouncer * instigator);
	
protected:
	AB_PaneAnnouncer * m_paneAnnouncer;
	AB_PropertySheet * m_propertySheet;
};

/******************************************************************************************

  AB_MailingListPane - 

*******************************************************************************************/

// The following two objects are used in the mailing list pane to support its caching of entries until commit time.

// a mailing list entry is either an ABID in the container representing an entry OR if that ABID is AB_ABIDUNKNOWN, then it
// is an array of attribute values representing the user. At commit, if the ABID is Unknown, then a new entry is created in the
// ctr for this entry and the attributes are stored.

// you cannot set attributes through the mailing list pane for entries already in the database so the attrib value array should
// always be zero for this index entry. 

typedef struct AB_MailingListEntry  // this structure is very similar to ab_pickerr entry...I should look into using same struct...
{
	AB_ContainerInfo * ctr;  // if this is NULL, assume entry is a naked address. Ctr will be ref counted
	ABID entryID;  
	char * nakedAddress; 
	XP_Bool newEntry; // i.e. an uncommitted entry
} AB_MailingListEntry;

class AB_MailingListEntries : public XPPtrArray
{
public:
	AB_MailingListEntry * GetAt(int i) const { return (AB_MailingListEntry *) XPPtrArray::GetAt(i);}
};

// I'm going to add the mailing list and person entry pane code here as well for lack of a better place to put them
class AB_MailingListPane : public MSG_LinedPane, public AB_ABIDBasedContainerListener, public AB_PropertySheet
{
public:
	AB_MailingListPane(MWContext * context,MSG_Master * master, AB_ContainerInfo * parentCtr, ABID listABID, AB_PaneAnnouncer * announcer = NULL /* our source pane if any */); 
	AB_MailingListPane (MWContext * context,MSG_Master * master,AB_MListContainerInfo * mListCtr, AB_PaneAnnouncer * announcer = NULL);
	int Initialize();
	
	// closing / destroying
	virtual ~AB_MailingListPane();
	static int Close(AB_MailingListPane * pane);
	int CloseSelf();   // doesn't delete...just closes..

	// besides drag and drop, these are the two main ways for adding entries to the mailing list. Caller is responsible for freeing
	// name completion cookie
	int AddEntry(const MSG_ViewIndex index, const char * nakedAddress, XP_Bool ReplaceExisting);
	int AddEntry(const MSG_ViewIndex index, AB_NameCompletionCookie * cookie, XP_Bool ReplaceExisting); 
	
	AB_ContainerInfo * GetContainerForMailingList();  // could be NULL if new mailing list 
	
	// MSG_LinedPane methods
	virtual MSG_PaneType GetPaneType();
	virtual void ToggleExpansion(MSG_ViewIndex line, int32 *numChanged);
	virtual int32 ExpansionDelta(MSG_ViewIndex line);
	virtual int32 GetNumLines();
	virtual void NotifyPrefsChange(NotifyCode code);

	// View Index manipulations. 
	ABID GetABIDForIndex(const MSG_ViewIndex);
	MSG_ViewIndex GetIndexForABID(ABID entryID); /* could return MSG_VIEWINDEXNONE */

	// Getting/Setting the mailing list properties
	int GetAttributes(AB_AttribID * attribs, AB_AttributeValue ** values /* we allocate */, uint16 * numItems);
	int SetAttributes(AB_AttributeValue * valuesArray /* caller allocated */, uint16 numItems);

	// Getting/Setting mailing list entry properties
	int GetEntryAttributes(const MSG_ViewIndex index, AB_AttribID * attribs, AB_AttributeValue ** values /* we allocate */, uint16 * numItems);
	int SetEntryAttributes(const MSG_ViewIndex index, AB_AttributeValue * valuesArray, uint16 numItems);

	// Commiting the changes to the data base
	int CommitChanges();

	virtual ABErr DoCommand(MSG_CommandType command, MSG_ViewIndex * indices, int32 numIndices);
	virtual ABErr GetCommandStatus(MSG_CommandType command, const MSG_ViewIndex * indices, int32 numIndices,
						XP_Bool * selectable_p, MSG_COMMAND_CHECK_STATE * selected_p,
						const char ** displayString, XP_Bool * plural_p);

	// Drag and Drop methods
	int DragEntriesIntoContainer(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer, 
								 AB_DragEffect request);
	AB_DragEffect DragEntriesIntoContainerStatus(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer,
										AB_DragEffect request);

	// Entry Manipulation Commands
	int DeleteEntries(const MSG_ViewIndex * indices, int32 numIndices);
	int InsertEntry  (const MSG_ViewIndex index);
	int ReplaceEntry (const MSG_ViewIndex index);

	// pane view notifications. Called by AB_ABPaneView
	void OnAnnouncerGoingAway(AB_PaneAnnouncer * instigator);

	// Container Listener notifications
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);

protected:
	AB_ContainerInfo * m_container;  // may be NULL if we are a new mailing list....
	XP_Bool m_newMailingList;  // are we a new mailing list? i.e. no mailing list container to back us up...
	XP_Bool m_commitingEntries;  // flag used so we can ignore insert notifications caused by our commits

	// view on all of the ABIDs 
	AB_MailingListEntries m_entriesView; // view of the mailing list

	AB_ContainerInfo * m_parentCtr;
	ABID m_ListABID;  // the ID for the mailing list inside its parent ctr

	AB_AttribValueArray m_listAttributes;  // the mailing list has a set of attributes separate from the entry attributes

	// we also need a list of ABIDs for deleted entries. That is, entries that are in the database, but have been
	// removed from the mailing list. When we commit the mailing list, we will need to remove these entries from
	// the mailing list container!
	ABIDArray m_deletedEntries;

	int32 m_numEntriesAdded; // # added entries which would not be in the container.

	AB_AttributeValue * ConvertListAttributesToArray(AB_AttribValueArray * values, uint16 * numItems);
	void FreeMailingListEntry(AB_MailingListEntry * entry);  // helper function to free entry structs
	XP_Bool CachedMailingListAttribute(AB_AttribID, int * position);

	int GetEntryAttributesForNakedAddress(AB_MailingListEntry * entry, AB_AttribID * attribs,AB_AttributeValue ** valuesArray, uint16 * numItems);

	// commiting the mailing list is involved...these functions help break the process down into components...
	int CommitNewContainer();
	int CommitEntries(); // commit entries to the container
	int CommitNakedAddress(AB_MailingListEntry * entry);

	int RemoveDeletedEntriesFromContainer();

	int AddEntry(const MSG_ViewIndex index, AB_MailingListEntry * entry, XP_Bool ReplaceExisting); // if true, replaces existing entry
	int SetEntryType();  // sets as a mailing list
	MSG_ViewIndex GetIndexForABID(AB_ContainerInfo * ctr, ABID entryID);
	AB_MailingListEntry * GetEntryForIndex(MSG_ViewIndex index);

	int UpdateViewWithEntry(const MSG_ViewIndex index /* to insert at */, AB_ContainerInfo * ctr, ABID entryID);
	int UpdateView();  // expensive operation that makes sure we are synched with all changes to the list ctr
};

/****************************************************************************************

  AB_PersonPane is a pane used to reflect a person's property sheet. Data can be changed through
  this pane and it will not actually be committed to the database until CommitChanges is called. 
  We also listen to the container holding the person so we can go generate appropriate notifications if
  the user is deleted or modified from underneath us.

  The parentPane used in the constructor should be used if the person pane should be tied to a
  parent container (i.e. the AB_Pane if you did an ShowProperties or NewCard cmd). The person pane
  registers itself as a listener on the parent pane. If the parent goes away, the person pane
  will generate a close message pane notification to the FE. The FE should then close us. 

  If user is delete, we generate a MSG_PaneClose.
  If the entry data is changed, we generate a MSG_PaneChanged.
  If the address book is being closed down, we issue a MSG_PaneClose.
*****************************************************************************************/

class AB_PersonPane : public MSG_Pane, public AB_ABIDBasedContainerListener, public AB_PropertySheet
{
public:
	AB_PersonPane(MWContext * context, MSG_Master * master, AB_ContainerInfo * ctr, ABID entryID /* could be 0 if new person */,
		AB_PaneAnnouncer * announcer = NULL /* this could be NULL if we are not tied to a parent pane announcer */, XP_Bool identityPane = FALSE);

	// it is possible to create a person pane which does not reflect an entry in the database, but instead, reflects
	// the user's identity (which is stored as a vcard in prefs. Simply pass in the attributes (if any). When the pane
	// is later committed, it will save the attributes to prefs. 
	static int CreateIdentityPane(MWContext * context, MSG_Master * master, AB_AttributeValue * identityValues, uint16 numItems, MSG_Pane ** pane);
	XP_Bool IsIdentityPane() { return m_identityPane;}

	virtual ~AB_PersonPane();

	void Initialize();
	int InitializeWithAttributes(AB_AttributeValue * valuesArray, uint16 numItems); /* caller must still free the attrib values in this array...pane copies them */

	static int Close(AB_PersonPane * pane);  // closes and deletes...
	int CloseSelf();   // doesn't delete...just closes..
	virtual MSG_PaneType GetPaneType();

	AB_ContainerInfo * GetContainerForPerson();
	ABID GetABIDForPerson();

	// getting/setting the attributes
	int SetAttributes(AB_AttributeValue * valuesArray, uint16 numItems);
	int GetAttributes(AB_AttribID * attribs /* caller allocated */, AB_AttributeValue ** values, uint16 * numItems);

	int CommitChanges(); // commiting changes to the database

	// container notifications we care about
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);
	
	// pane view notifications. Called by AB_ABPaneView
	void OnAnnouncerGoingAway(AB_PaneAnnouncer * announcer);  // public API called by AB_ABPaneView

protected:
	AB_ContainerInfo * m_container;
	ABID m_entryID; 
	AB_AttribValueArray m_values;
	XP_Bool m_newEntry;
	XP_Bool m_identityPane; // we are actually the identity pane..

	XP_Bool CachedAttributePosition(AB_AttribID attrib, int * position);
	int SetEntryType();  // sets an entry type attribute for person...

	int CommitIdentityPane(AB_AttributeValue * values, uint16 numAttributes); // write any attributes out to the personal identity vcard which is stored in prefs.
};

#endif /* _ABPANE_H */

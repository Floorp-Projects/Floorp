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
/*ABCPane - address book container pane
 */
/* -*- Mode: C++; tab-width: 4 -*- */

#ifndef _ABCPANE_H
#define _ABCPANE_H

#include "msg.h"
#include "abcom.h"
#include "aberror.h"
#include "abcinfo.h"
#include "abntfy.h"
#include "msglpane.h"
#include "ptrarray.h"

class AB_ContainerInfo;

typedef struct AB_NewDIREntry
{
	DIR_Server * server;
	MSG_ViewIndex desiredPosition;
} AB_NewDIREntry;

class AB_NewDIREntryArray : public XPPtrArray
{
public:
	AB_NewDIREntry * GetAt(int i) const { return (AB_NewDIREntry *) XPPtrArray::GetAt(i);}
};


/*******************************************************************************************************
   A few words about the container pane....We keep an array of container info's callled the m_containerView which
   corresponds to the view the front ends see for this pane. 

  In addition, we keep a new DirEntry cache which is an array of DIR_Server and desired position index pairs. 
  We need this because the DIR_Server is created with a MSG_ViewIndex in the DoCommand method. This in turn 
  generates an FE callback where they actually present the property sheet which is filled in at the user's leisure.
  The back end cannot block on this call. So creating a new DIR_Server (new PAB or LDAP) is now an asynch operation.
  We use the cache to remember where we should insert the DIR_Server when we are later informed by the FE
  through UpdateDirServer that it should be committed to the container pane.

 *******************************************************************************************************/

class AB_ContainerPane : public MSG_LinedPane, public AB_ABIDBasedContainerListener, public AB_PaneAnnouncer
{
public:
	AB_ContainerPane(MWContext * context, MSG_Master * master);
	virtual ~AB_ContainerPane();

	static void Create(MSG_Pane ** abcPane, MWContext * context, MSG_Master * master);
	int Initialize();  // uses global database table to load up global containers.
	static int Close(AB_ContainerPane * pane);

	int GetNumRootContainers(int32 * numRootContainers); 
	int GetOrderedRootContainers(AB_ContainerInfo ** ctrArray /* caller allocated */, int32 * numCtrs);
	
	MSG_ViewIndex GetIndexForContainer(AB_ContainerInfo * ctr);
	AB_ContainerInfo * GetContainerForIndex(const MSG_ViewIndex index);

	int UpdateDIRServer(DIR_Server * server); // add or update container for this dir server

	int GetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribute * attribArray, AB_ContainerAttribValue ** value, uint16 * numItems);
	int SetContainerAttributes(MSG_ViewIndex index, AB_ContainerAttribValue * valuesArray, uint16 numItems);

	// MSG_Pane methods
	virtual MSG_PaneType GetPaneType();

	// MSG_LinedPane methods
	virtual void ToggleExpansion (MSG_ViewIndex line, int32 *numChanged);
	virtual int32 ExpansionDelta (MSG_ViewIndex line);
	virtual int32 GetNumLines ();

	// MSG_PrefsNotify methods
	virtual void NotifyPrefsChange (NotifyCode code);

	int32 GetContainerMaxDepth(); // examine all containers in the pane and determine the maximum depth...

	XPPtrArray * GetABContainerView () { return &m_containerView;}

	virtual ABErr DoCommand(MSG_CommandType command, MSG_ViewIndex *indices, int32 numIndices);

	virtual ABErr GetCommandStatus(MSG_CommandType command,
							  const MSG_ViewIndex* indices, int32 numindices,
							  XP_Bool *selectable_p,
							  MSG_COMMAND_CHECK_STATE *selected_p,
							  const char ** displayString,
							  XP_Bool *plural_p);	



	// Drag & Drop related methods
	int DragEntriesIntoContainer(const MSG_ViewIndex * srcIndices, int32 numIndices, AB_ContainerInfo * destContainer, 
									AB_DragEffect request /* copy or move? */);
	AB_DragEffect DragEntriesIntoContainerStatus(const MSG_ViewIndex * srcIndices, int32 numIndices, AB_ContainerInfo * destContainer,
										AB_DragEffect request);
	int Undo();
	int Redo();

	// Notification methods
	virtual void OnContainerAttribChange(AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, AB_ContainerListener * instigator);
	virtual void OnContainerEntryChange (AB_ContainerInfo * ctr, AB_NOTIFY_CODE code, ABID entryID, AB_ContainerListener * instigator);
	virtual void OnAnnouncerGoingAway(AB_ContainerAnnouncer * instigator);

	// Geting and Setting Front End notification functions
	void SetShowPropSheetForDirFunc(AB_ShowPropertySheetForDirFunc * func) { m_DirectoryPropSheetFunc = func; }
	AB_ShowPropertySheetForDirFunc * GetShowPropSheetForDirFunc() { return m_DirectoryPropSheetFunc;}

	// Showing properties for new, existing directories
	int ShowDirectoryProperties(MSG_ViewIndex * indices, int32 numIndices); // bring up property sheets on the following indices...
	int ShowNewDirectoryProperties(AB_CommandType cmd, MSG_ViewIndex * indices, int32 numIndices);

	// deleting container entries from the pane...
	int DeleteContainers(MSG_ViewIndex * indices, int32 numIndices);

protected:
/*	int AddAddressBook(MSG_ViewIndex index);  // adds AB at index location */
	virtual int ApplyToIndices(AB_CommandType command, MSG_ViewIndex * indices, int32 numIndices);
	
	void AddNewContainer(MSG_ViewIndex position, AB_ContainerInfo * ctr);	
	int	 AddNewDirectory(DIR_Server * directory); // use this to turn a server into a ctr & add it to server list & container view

	int AddDirectory(DIR_Server * directory); // only turns into ctr & adds to view. ONLY call when initializing the pane!

	MSG_ViewIndex GetDesiredPositionForNewServer(DIR_Server * server);
	XP_Bool RemoveFromNewServerCache(DIR_Server * server);  // returns TRUE if found and removed...false otherwise
	XP_Bool AddToNewServerCache(DIR_Server * server, MSG_ViewIndex desiredPosition);

	void RemoveAndCloseDirectory(DIR_Server * server);  // removes directory from the dir_servers list
	int ReOrderContainers(const MSG_ViewIndex * indices, int32 numIndices, AB_ContainerInfo * destContainer);

	XP_Bool IsExpanded(MSG_ViewIndex line); // returns TRUE if ctr for line is already expanded in the pane. FALSE otherwise. Note: 0 children = FALSE.
	void CollapseContainer(MSG_ViewIndex line, int32 * numChanged);
	void ExpandContainer(MSG_ViewIndex line, int32 * numChanged);
	int32 GetNumChildrenForIndex(MSG_ViewIndex line);
	
	// these two commands are used to help us where to insert new directory containers into the container pane and DIR_Server List
	DIR_Server * ExtractServerPosition(MSG_ViewIndex desiredPosition);
	MSG_ViewIndex ExtractInsertPosition(MSG_ViewIndex desiredPosition);

	AB_ContainerInfoArray m_containerView;  // Array of displayed AB_ContainerInfo*
	AB_NewDIREntryArray m_newEntries;  // mapping of new servers which have not been committed to their desired insertion position when they are 
									   // eventually committed. 
	MWContext * m_context;
	MSG_Master * m_master;

	AB_ShowPropertySheetForDirFunc * m_DirectoryPropSheetFunc;

	int ShowPropertySheetForNewType(AB_EntryType entryType, MSG_ViewIndex index /* index to container the prop sheet should be in */); 
	int ShowPropertySheet(MSG_ViewIndex * indices, int32 numIndices); /* for mailing lists */
	int ShowProperties(MSG_ViewIndex * indices, int32 numIndices); // dispatcher to property sheet or directory properties based on ctr types

	void DeleteEntryFromExpandedList(MSG_ViewIndex index, AB_ContainerInfo * ctr, ABID listID);
	void InsertEntryIntoExpandedList(MSG_ViewIndex index /* index for ctr */, AB_ContainerInfo * ctr, ABID listID);
	uint32 NumVisibleChildrenForIndex(MSG_ViewIndex index); // helper function
};

#endif /* _ABCPANE_H */

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

#ifndef _SubPane_H_
#define _SubPane_H_

#include "msglpane.h"
#include "subline.h"
#include "idarray.h"

struct ChangeSubscribe;
typedef struct xp_HashTable *XP_HashTable;
class msg_GroupRecord;
class msg_IMAPGroupRecord;
class MSG_Host;
class XPSortedPtrArray;

class MSG_SubscribePane : public MSG_LinedPane {
public:
	static MSG_SubscribePane* Create(MWContext* context, MSG_Master* master,
									 MSG_Host* host = NULL);
    virtual ~MSG_SubscribePane();

    virtual MSG_PaneType GetPaneType();

    virtual void NotifyPrefsChange(NotifyCode code);

    virtual void ToggleExpansion(MSG_ViewIndex line, int32* numchanged);
    virtual int32 ExpansionDelta(MSG_ViewIndex line);
    virtual int32 GetNumLines();

	virtual MsgERR DoCommand(MSG_CommandType command,
							 MSG_ViewIndex* indices, int32 numindices);

	virtual MsgERR GetCommandStatus(MSG_CommandType command,
									const MSG_ViewIndex* indices,
									int32 numindices,
									XP_Bool *selectable_p,
									MSG_COMMAND_CHECK_STATE *selected_p,
									const char **display_string,
									XP_Bool *plural_p);

	int SetCallbacks(MSG_SubscribeCallbacks* callbacks, void* closure);
	int Cancel();
	int CommitSubscriptions();
	MSG_Host* GetHost();
	int SetHost(MSG_Host* host);
	MSG_SubscribeMode GetMode();
	int SetMode(MSG_SubscribeMode mode,
				XP_Bool force = FALSE, // If FALSE, then this is a no-op
									   // if already in this mode.  If
									   // TRUE, always recreate this mode
									   // from scratch.
				XP_Bool autofetch = TRUE // If TRUE, then if we know we only
										 // have partial data from this
										 // newsgroup, then start getting
										 // that data automatically or pop
										 // up a warning, depending on mode.
				);

	MSG_ViewIndex FindFirst(const char* str);
	int FindAll(const char* str);
	XP_Bool GetGroupNameLineByIndex(MSG_ViewIndex firstline, int32 numlines,
									MSG_GroupNameLine* data);

	virtual int32 GetNewsRCCount(MSG_NewsHost* host);
	virtual char* GetNewsRCGroup(MSG_NewsHost* host);
	virtual int DisplaySubscribedGroup(MSG_NewsHost* host,
									   const char *group,
									   int32 oldest_message,
									   int32 youngest_message,
									   int32 total_messages,
									   XP_Bool nowvisiting);

	// This should be used only by the internal msg_BackgroundGroupsSearch
	// class.
	int AddSearchResult(msg_GroupRecord* group);

	XP_Bool AddIMAPGroupToList(const char *folderName, char delimiter,
		XP_Bool isSubscribed, uint32 box_flags, XP_Bool filledInGroup = FALSE);
	void IMAPChildDiscoverySuccessful();
	void ReportIMAPFolderDiscoveryFinished();


	void UpdateCounts();		// Whenever new things get added to the list,
								// we should call this to kick off updating
								// the counts, if necessary.  For internal use
								// only.
protected:
    MSG_SubscribePane(MWContext* context, MSG_Master* master,
					  MSG_Host* host);
    void DoToggleExpansion(MSG_ViewIndex line, int32* numchanged);
	int32 GetKidsArray(msg_GroupRecord*, msg_SubscribeLineArray* kids,
					   int depth);
	msg_GroupRecord* FindRealDescendent(msg_GroupRecord* group);
	MsgERR ToggleSubscribed(MSG_ViewIndex* indices, int32 numIndices);
	MsgERR SetSubscribed(MSG_ViewIndex* indices, int32 numIndices, XP_Bool subscribed);
	MsgERR ExpandAll(MSG_ViewIndex* indices, int32 numIndices);
	MsgERR CollapseAll();
	MsgERR ClearNew();
	MsgERR FetchGroupList();
	MSG_ViewIndex FindGroupExpandIfNecessary(msg_GroupRecord* group);

	XP_Bool WasSubscribed(msg_GroupRecord* group);  // Whether the given group
													// was already subscribed
													// for the current host.
	virtual void UpdateNewsCountsDone(int status);
	virtual XP_Bool AddGroupsAsNew();

	virtual void CheckForNewDone(URL_Struct* url_struct, int status,
								 MWContext* context);

	msg_IMAPGroupRecord*	FindParentRecordOfIMAPGroup(const char *folderName, char delimiter);
	MSG_ViewIndex			FindGroupViewIndex(msg_GroupRecord* group);
	void					ClearAndFreeIMAPSubscribedList();
	void					ClearAndFreeIMAPGroupList();

	static MSG_SubscribePane* CurPane; // To ensure we never create more than
									   // one.


	MSG_Host* m_host;
	MSG_SubscribeMode m_mode;
	msg_SubscribeLineArray m_groupView;	// Array of displayed lines
	ChangeSubscribe* m_subscribeList; // List of items we want to subscribe
									  // or unsubscribe.
	ChangeSubscribe* m_endSubscribeList;// End of above list.
	XP_HashTable m_subscribeHash;	  // Hash into above.  Need both hashtable
									  // and linked list, because ordering
									  // and quick lookups are both needed.
	XP_HashTable m_imapGroupHash;	// hash table of all msg_IMAPGroupRecords that
									// exist.  This structure owns the storage
									// for the group records.
	XPSortedPtrArray *m_imapSubscribedList;	// list of all IMAP folder names that
										// we are subscribed to initially.
										// This is used for quick lookup to see if a
										// folder is subscribed or not when it is later added
										// to the view through expansion.
	char* m_lastSearch;			// String used for the last search.

	int32 m_curUpdate;			// Which line we think we're updating counts
								// on.

	MSG_SubscribeCallbacks m_callbacks;
	void* m_callbackclosure;

	time_t m_updateTime;

	msg_IMAPGroupRecord *m_imapTree;
	XP_Bool m_subscribeCommitted;
	msg_IMAPGroupRecord *m_expandingIMAPGroup;
};

#endif /* _SubPane_H_ */

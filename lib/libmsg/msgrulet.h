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
#ifndef _MSGRULET_H
#define _MSGRULET_H

#include "ptrarray.h"
#include "dwordarr.h"

class MSG_FolderInfo;
class MSG_Master;
struct MSG_FilterList;
struct MSG_Rule;

// MSG_RuleTrackAction comprises the set of possible resolutions when
// a change to a mail folder conflicts with a filter rule.

typedef enum MSG_RuleTrackAction
{
	disableRule,	// Allow the folder operation, but disable referring rules
	breakRule,		// Allow the rule to keep firing using old path
	trackChange,	// Change the rule to refer to the new path
	dontChange		// Abort the folder operation with no change
} MSG_RuleTrackAction;


typedef void (*MSG_RuleTrackCallback) (void*, XPPtrArray&, XPDWordArray&, XP_Bool isDelete);


class MSG_RuleTrackElement 
{
public:
	MSG_RuleTrackElement (MSG_Rule *rule, char *delta)
	{
		m_rule = rule;
		m_parentalPathDelta = delta;
	}

	~MSG_RuleTrackElement ()
	{
		m_rule = NULL;
		XP_FREEIF(m_parentalPathDelta);
	}

	MSG_Rule *m_rule;
	char *m_parentalPathDelta;
};


class MSG_RuleTrackArray : public XPPtrArray
{
public:

	MSG_RuleTrackElement *GetAt (int i)
	{
		return (MSG_RuleTrackElement*) XPPtrArray::GetAt(i);
	}

	void DeleteElements ()
	{
		for (int i = 0; i < GetSize(); i++)
			delete GetAt(i);
	}
};

// MSG_RuleTracker is responsible for watching when mail folder paths
// change, and if the changing folder is a target of a rule, ask the user how 
// to resolve the potential breakage given the options in TrackAction
//
// MSG_RuleTracker reads and (potentially) writes the rule list, so it 
// must be used atomically. i.e. it's not legal for a MSG_RuleTracker
// object to live across invocations of GetNewMail or the Rules dialog box.

class MSG_RuleTracker
{
public:
	MSG_RuleTracker (MSG_Master *, MSG_RuleTrackCallback, void *);
	~MSG_RuleTracker ();

	XP_Bool WatchMoveFolders (MSG_FolderInfo **list, int listCount);
	XP_Bool WatchDeleteFolders (MSG_FolderInfo **list, int listCount);
	XP_Bool AbortTracking ();

protected:
	XP_Bool WatchFolders (MSG_FolderInfo **list, int listCount);
	XP_Bool LoadRules (MSG_FolderInfo **list, int listCount);
	void MatchFolderToRules (MSG_FolderInfo *);
	void ApplyTrackActions (const char *newPath);

	MSG_Master *m_master;
	MSG_FilterList *m_list;
	XP_Bool m_listIsDirty;

	MSG_RuleTrackArray m_rulesToTrack;
	XPDWordArray m_actions;

	MSG_FolderInfo **m_folderList;
	int m_folderListCount;

	MSG_RuleTrackCallback m_callback;
	void *m_callbackCookie;

	XP_Bool m_isDelete;
};

#endif //_MSGRULET_H

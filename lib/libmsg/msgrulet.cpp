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
// MSG_RuleTracker is responsible for watching when mail folder paths
// change, and if the changing folder is a target of a rule, ask the user how 
// to resolve the potential breakage given the options in TrackAction
//
// MSG_RuleTracker reads and (potentially) writes the rule list, so it 
// must be used atomically. i.e. it's not legal for a MSG_RuleTracker
// object to live across invocations of GetNewMail or the Rules dialog box.

#include "msg.h"

#include "msgrulet.h"
#include "msgfinfo.h"
#include "msgmast.h"
#include "pmsgfilt.h"


MSG_RuleTracker::MSG_RuleTracker (MSG_Master *master, MSG_RuleTrackCallback cb, void *cookie)
{
	m_master = master;
	m_list = NULL;
	m_listIsDirty = FALSE;

	m_folderList = NULL;
	m_folderListCount = 0;

	m_callback = cb;
	m_callbackCookie = cookie;
}


XP_Bool MSG_RuleTracker::WatchMoveFolders (MSG_FolderInfo **list, int count)
{
	m_isDelete = FALSE;
	return WatchFolders (list, count);
}


XP_Bool MSG_RuleTracker::WatchDeleteFolders (MSG_FolderInfo **list, int count)
{
	m_isDelete = TRUE;
	return WatchFolders (list, count);
}


XP_Bool MSG_RuleTracker::WatchFolders (MSG_FolderInfo **folderList, int folderListCount)
{
	XP_Bool ret = TRUE;

	m_folderList = folderList;
	m_folderListCount = folderListCount;

	ret = LoadRules(folderList, folderListCount);
	if (ret)
	{
		for (int i = 0; i < m_folderListCount; i++)
		{
			// Find out if any rules refer to this folder before we change it
			MatchFolderToRules (m_folderList[i]);
			if (m_rulesToTrack.GetSize() > 0)
			{
				(*m_callback) (m_callbackCookie, m_rulesToTrack, m_actions, m_isDelete);
				for (uint32 j = 0; j < m_actions.GetSize(); j++)
					if (dontChange == (MSG_RuleTrackAction) m_actions.GetAt(j))
						return FALSE;
			}
		}
	}
	return ret;
}

XP_Bool MSG_RuleTracker::AbortTracking ()
{
	// Let go of allocated path deltas we're remembering
	m_folderListCount = 0;
	m_listIsDirty = FALSE;
//	m_rulesToTrack.DeleteElements ();

	return TRUE;
}

MSG_RuleTracker::~MSG_RuleTracker ()
{
	// Look through the list of folders to see if any of them need to have
	// their location updated in one or more rules
	for (int i = 0; i < m_folderListCount; i++)
	{
		// Apply the changes the user requested to the rule set
		MSG_FolderInfoMail *mailFolder = m_folderList[i]->GetMailFolderInfo();
		if (mailFolder)
			ApplyTrackActions (mailFolder->GetPathname());
	}

	// If the operation which used the rule tracker has decided to change a rule's
	// path, the list will be dirty, otherwise we don't need to write it out again
	if (m_listIsDirty)
		MSG_CloseFilterList (m_list);
	else
		MSG_CancelFilterList (m_list);

	// Let go of allocated path deltas we're remembering
	m_rulesToTrack.DeleteElements ();

	// Now that we're done, other customers of the rules list are allowed
	// to load it and fire the rules
	m_master->ReleaseRulesSemaphore(this);
}


XP_Bool MSG_RuleTracker::LoadRules (MSG_FolderInfo **folderList, int count)
{
	// Don't let anyone else change the rules list while we're running
	if (!m_master->AcquireRulesSemaphore(this))
		return FALSE;

	// Pull the rule list out of the disk file
	//
	// We don't really support opening the folder list for all the folders,
	// we only open the filters for the first folder.  This should be fine
	// since we should never be operating on folders that would have different
	// sets of filters.
	MSG_FolderInfo *folder = (count > 0) ? folderList[0] : (MSG_FolderInfo *)NULL;
	if (MSG_OpenFolderFilterListFromMaster(m_master, folder, filterInbox, &m_list) != FilterError_Success)
		return FALSE;

	m_listIsDirty = FALSE;

	return TRUE;
}


void MSG_RuleTracker::MatchFolderToRules (MSG_FolderInfo *folder)
{
	// Look through the rules list for any rules which refer to the pathname
	// and add them to the list of rules we should keep an eye on.

	m_rulesToTrack.RemoveAll();
	MSG_FolderInfoMail *mailFolder = folder->GetMailFolderInfo();
	if (mailFolder)
	{
		int32 filterCount = 0;
		m_list->GetFilterCount (&filterCount);
		for (MSG_FilterIndex i = 0; i < filterCount; i++)
		{
			MSG_Filter *filter = NULL;
			m_list->GetFilterAt (i, &filter);
			XP_ASSERT(filter);
			if (!filter)
				continue;

			MSG_FilterType filter_type = filter->GetType();
			if (filterInboxJavaScript == filter_type || filterNewsJavaScript == filter_type)
				continue;

			MSG_Rule *rule = NULL;
			filter->GetRule (&rule);
			XP_ASSERT(rule);
			if (!rule)
				continue;

			MSG_RuleActionType action;
			void *value = NULL;
			rule->GetAction (&action, &value);
			if (!value)
				continue;

			int mailFolderPathLen = XP_STRLEN(mailFolder->GetPathname());
			char *mailFolderPath = (char *)XP_ALLOC(mailFolderPathLen + 6);
			XP_STRCPY(mailFolderPath, mailFolder->GetPathname());
			XP_STRCAT(mailFolderPath, ".sbd/");
			if (   acMoveToFolder == action
			    && (   XP_STRSTR((char*) value, mailFolderPath)
			        || !XP_STRCMP((char*) value, mailFolder->GetPathname())))
			{
				char *delta = XP_STRDUP ((char*) value + mailFolderPathLen);
				XP_ASSERT(delta);
				MSG_RuleTrackElement *elem = new MSG_RuleTrackElement (rule, delta);
				m_rulesToTrack.Add (elem);
			}
			XP_FREE(mailFolderPath);
		}
	}
}


void MSG_RuleTracker::ApplyTrackActions (const char * newPath)
{
	// Now that we're done, look through the rules 
	for (int i = 0; i < m_rulesToTrack.GetSize(); i++)
	{
		MSG_RuleTrackElement *elem = m_rulesToTrack.GetAt(i);
		XP_ASSERT(elem && elem->m_rule->IsValid());

		MSG_RuleTrackAction a = (MSG_RuleTrackAction) m_actions.GetAt(i);
		switch (a)
		{
		case disableRule:
			elem->m_rule->GetFilter()->SetEnabled (FALSE);
			m_listIsDirty = TRUE;
			break;
		case breakRule:
			break;
		case trackChange:
		{
			char *childPath = PR_smprintf ("%s%s", newPath, elem->m_parentalPathDelta);
			if (childPath)
			{
				elem->m_rule->SetAction (acMoveToFolder, (void*) childPath);
				m_listIsDirty = TRUE;
				XP_FREE(childPath);
			}
			break;
		}
		case dontChange:
			break;
		default:
			XP_ASSERT(FALSE);
		}
	}
}



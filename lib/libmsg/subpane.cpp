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

#include "msg.h"
#include "errcode.h"

#include "subpane.h"
#include "hosttbl.h"
#include "msgmast.h"
#include "msgfinfo.h"
#include "xp_hash.h"
#include "newshost.h"
#include "imaphost.h"
#include "xpgetstr.h"
#include "grec.h"
#include "xp_qsort.h"
#include "msgurlq.h"

extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_SEARCH_FAILED;
	extern int MK_MSG_INCOMPLETE_NEWSGROUP_LIST;
	extern int XP_ALERT_OFFLINE_MODE_SELECTED;
	extern int MK_NNTP_SERVER_NOT_CONFIGURED;
}

// we are using the same ChangeSubscribe structure for both
// News and IMAP.  We are also keeping these structs in the same hash
struct ChangeSubscribe {
	MSG_Host* host;
	char* groupname;
	XP_Bool subscribe;			// Whether we're trying to subscribe or
								// unsubscribe to this group.
	ChangeSubscribe* next;
};

static XP_Bool imap_freehashkeys(XP_HashTable table, const void* key,
										void* value, void* closure);

MSG_SubscribePane* MSG_SubscribePane::CurPane = NULL;


MSG_SubscribePane*
MSG_SubscribePane::Create(MWContext* context, MSG_Master* master,
						  MSG_Host* host)
{
	XP_ASSERT(!CurPane);
	if (CurPane) return NULL;

	MSG_Host* defhost = NULL;

	// first try getting the default News (NNTP) host
	defhost = MSG_GetDefaultNewsHost(master);
	
	// if no NNTP host, try getting the default IMAP host
	if (!defhost)
	{
		defhost = MSG_GetDefaultIMAPHost(master);
	}

	// no hosts configured
	if (defhost == NULL)
	{
		FE_Alert(context, XP_GetString(MK_NNTP_SERVER_NOT_CONFIGURED));
	#ifdef XP_MAC
		/* AFTER the alert, not before! */
		FE_EditPreference(PREF_NewsHost);
	#endif
		return NULL;
	}

	MSG_SubscribePane* result = new MSG_SubscribePane(context, master, host);
	if (result && CurPane == NULL) {
		// We ran out of memory or something.  Abort.
		delete result;
		result = NULL;
	}
	return result;
}



static int
HashComp(const void *obj1, const void *obj2)
{
	XP_ASSERT(obj1 && obj2);
	ChangeSubscribe* c1 = (ChangeSubscribe*) obj1;
	ChangeSubscribe* c2 = (ChangeSubscribe*) obj2;
	int delta = XP_STRCMP(c1->groupname, c2->groupname);
	if (delta) return delta;
	if (c1->host < c2->host) return -1;
	if (c1->host == c2->host) return 0;
	return 1;
}

static int
msg_hash_strcmp (const void *a, const void *b)
{
  return XP_STRCMP ((const char *) a, (const char *) b);
}

static uint32
HashFunc(const void* obj)
{
	return XP_StringHash(((ChangeSubscribe*) obj)->groupname);
}

static int sortedptrarray_strcmp (const void* a, const void* b)
{
	return XP_STRCMP (*(char **)a, *(char **)b);
}


XP_Bool imap_freehashkeys(XP_HashTable /*table*/, const void* key,
										void* /*value*/, void* /*closure*/)
{
	XP_FREE((char*) key);
	return TRUE;
}


MSG_SubscribePane::MSG_SubscribePane(MWContext* context, MSG_Master* master,
									 MSG_Host* host)
	: MSG_LinedPane(context, master)
{
	m_host = NULL;
	XP_ASSERT(master);
	if (!master) return;
	msg_HostTable *newsHosts = master->GetHostTable();

	m_subscribeHash = XP_HashTableNew(20, (XP_HashingFunction) HashFunc, (XP_HashCompFunction) HashComp);
	if (!m_subscribeHash) return;

	m_imapGroupHash = XP_HashTableNew(20, XP_StringHash, msg_hash_strcmp);
	if (!m_imapGroupHash) return;

	m_imapSubscribedList = new XPSortedPtrArray(sortedptrarray_strcmp);

	m_imapTree = msg_IMAPGroupRecord::Create(NULL, NULL, 0, FALSE);
	XP_ASSERT(m_imapTree);
	if (!m_imapTree) return;

	m_subscribeCommitted = FALSE;
	m_expandingIMAPGroup = NULL;

	m_host = host;
	if (!m_host) {
		if (newsHosts) m_host = newsHosts->GetDefaultHost(TRUE);
	}
	if (!m_host)
	{
		m_host = MSG_GetDefaultIMAPHost(master);
	}
	int numNewsHosts = newsHosts ? newsHosts->getNumHosts() : 0;
	if (!m_host && numNewsHosts > 0) {
		m_host = newsHosts->getHost(0);
	}
	m_subscribeList = NULL;
	m_endSubscribeList = NULL;

	// Go through the news hosts and add each group to 
	// the hash table
	for (int i=0 ; i<numNewsHosts ; i++) {
		MSG_NewsHost* h = newsHosts->getHost(i);
		XP_ASSERT(h);
		if (!h) continue;
		MSG_FolderInfo* hostinfo = h->GetHostInfo();
		XP_ASSERT(hostinfo);
		if (!hostinfo) continue;
		int num = hostinfo->GetNumSubFolders();
		for (int j=0 ; j<num ; j++) {
			MSG_FolderInfoNews* newsinfo =
				(MSG_FolderInfoNews*) hostinfo->GetSubFolder(j);
			XP_ASSERT(newsinfo->IsNews());
			if (!newsinfo->IsNews()) continue;
			if (!newsinfo->IsSubscribed()) continue;
			const char* groupname = newsinfo->GetNewsgroupName();
			h->NoticeNewGroup(groupname);
			ChangeSubscribe* tmp = new ChangeSubscribe;
			if (!tmp) break;
			if (m_endSubscribeList) {
				m_endSubscribeList->next = tmp;
			} else {
				m_subscribeList = tmp;
			}
			m_endSubscribeList = tmp;
			tmp->host = h;
			tmp->groupname = new char [XP_STRLEN(groupname) + 1];
			if (!tmp->groupname) {
				m_host = NULL;
				return;
			}
			XP_STRCPY(tmp->groupname, groupname);
			tmp->next = NULL;
			tmp->subscribe = TRUE;
			XP_Puthash(m_subscribeHash, tmp, tmp);
		}
	}

	// We probably don't have to go through the IMAP hosts
	// and add each group to the hash table, since they will
	// be added on the fly

	if (m_host) {
		MSG_NewsHost *newsHost = m_host->GetNewsHost();
			if (newsHost)
				newsHost->Inhale();
		SetMode(MSG_SubscribeAll, TRUE);
	}
	CurPane = this;
}


MSG_SubscribePane::~MSG_SubscribePane()
{
	int32 i;

	//if (!m_subscribeCommitted)
	//	CommitSubscriptions();

	// Clean up the list of changed subscriptions.
	// We should do this here because we don't know
	// if the IMAP commits will succeed or not,
	// until now.
	ChangeSubscribe* tmp = m_subscribeList;
	while (tmp) {
		ChangeSubscribe* next = tmp->next;
		delete [] tmp->groupname;
		tmp->groupname = NULL;
		tmp->next = NULL;
		delete tmp;
		tmp = next;
	}

	// Force all the newsrc files to be updated now.  After all,
	// subscribing is a major operation we don't want to lose.
	msg_HostTable* hosts = GetMaster()->GetHostTable();
	for (i=0 ; i < (hosts ? hosts->getNumHosts() : 0) ; i++) {
		MSG_NewsHost* h = hosts->getHost(i);
		XP_ASSERT(h);
		if (!h) continue;
		h->WriteIfDirty();
	}

	if (m_host && m_host->IsNews())
	{
		MSG_NewsHost *newsHost = m_host->GetNewsHost();
		if (newsHost)
			newsHost->Exhale();
	}

	if (m_subscribeHash)
		XP_HashTableDestroy(m_subscribeHash);

	ClearAndFreeIMAPGroupList();
	if (m_imapGroupHash)
		XP_HashTableDestroy(m_imapGroupHash);

	ClearAndFreeIMAPSubscribedList();
	delete m_imapSubscribedList;

	for (i=0 ; i<m_groupView.GetSize() ; i++) {
		delete m_groupView[i];
		m_groupView[i] = NULL;
	}

	XP_ASSERT(CurPane == this);
	if (CurPane == this) CurPane = NULL;

	delete m_imapTree;
	
	MSG_RefreshFoldersForUpdatedIMAPHosts(GetContext());
}


MSG_PaneType
MSG_SubscribePane::GetPaneType() {
	return MSG_SUBSCRIBEPANE;
}

void
MSG_SubscribePane::NotifyPrefsChange(NotifyCode)
{
	// ###tw  Write me!
}


void
MSG_SubscribePane::ToggleExpansion(MSG_ViewIndex line, int32* numchanged)
{
	DoToggleExpansion(line, numchanged);
	UpdateCounts();
}

void
MSG_SubscribePane::DoToggleExpansion(MSG_ViewIndex line, int32* numchanged)
{
	if (numchanged) *numchanged = 0;
	XP_ASSERT(line != MSG_VIEWINDEXNONE && line < m_groupView.GetSize());
	if (line == MSG_VIEWINDEXNONE || line >= m_groupView.GetSize()) return;
	XP_ASSERT(m_mode == MSG_SubscribeAll);
	if (m_mode != MSG_SubscribeAll) return;
	msg_SubscribeLine* info = m_groupView[line];
	int depth = info->GetDepth();
	msg_GroupRecord* group = info->GetGroup();
	XP_ASSERT(group);
	if (!group) return;
	if (line < m_groupView.GetSize() - 1 &&
		(m_groupView[line+1]->GetDepth() > depth)) {
		// OK, we are already expanded.  So, collapse things, which means
		// nuke all lines after the given one that have a depth bigger than
		// the given line.
		XP_ASSERT(group->IsExpanded());
		int32 start = line + 1;
		int32 end;
		for (end=start ; end<m_groupView.GetSize() ; end++) {
			if (m_groupView[end]->GetDepth() <= depth) break;
		}
		StartingUpdate(MSG_NotifyInsertOrDelete, start, - (end - start));
		for (int32 i=start ; i<end ; i++) {
			delete m_groupView[i];
			m_groupView[i] = NULL;
		}
		m_groupView.RemoveAt(start, end - start);
		if (numchanged) *numchanged = - (end - start);
		EndingUpdate(MSG_NotifyInsertOrDelete, start, - (end - start));
		group->SetIsExpanded(FALSE);
	} 
	else
	{
		// We are not expanded, so expand, if possible.
		if (info->CanExpand()) {
			XP_ASSERT(!group->IsExpanded());
			msg_SubscribeLineArray kids;
			GetKidsArray(group, &kids, info->GetDepth() + 1);
			XP_ASSERT(kids.GetSize() > 0); // If not, why did we think we could
										   // expand?
			StartingUpdate(MSG_NotifyInsertOrDelete, line + 1, kids.GetSize());
			m_groupView.InsertAt(line + 1, &kids);
			EndingUpdate(MSG_NotifyInsertOrDelete, line + 1, kids.GetSize());
			if (numchanged) *numchanged = kids.GetSize();
			group->SetIsExpanded(TRUE);
		}

		if (m_host && m_host->IsIMAP())
		{
			// expanding an imap folder, search for new children
			msg_IMAPGroupRecord *imapGroup = group->GetIMAPGroupRecord();
			if (imapGroup && !imapGroup->GetAllGrandChildrenDiscovered())
			{
				m_expandingIMAPGroup = imapGroup;
				MSG_IMAPHost *imapHost = m_host->GetIMAPHost();
				if (imapHost)
				{
					char *groupname = imapGroup->GetFullName();
					if (groupname)
					{
						char * url = CreateImapLevelChildDiscoveryUrl(imapHost->GetHostName(), groupname, imapGroup->GetHierarchySeparator(), 2);
        				if (url)
        				{
							MSG_UrlQueue::AddUrlToPane(url, NULL, this);
	        				XP_FREE(url);
						}
						delete groupname;
					}
				}
			}
		}
	}
	// Tell the FE to redraw the expand/collapse widget of the parent group
	StartingUpdate(MSG_NotifyChanged, line, 1);
	EndingUpdate(MSG_NotifyChanged, line, 1);
}


void
MSG_SubscribePane::IMAPChildDiscoverySuccessful()
{
	if (m_expandingIMAPGroup)
	{
		m_expandingIMAPGroup->SetAllGrandChildrenDiscovered(TRUE);
		m_expandingIMAPGroup = NULL;
	}
}


int32
MSG_SubscribePane::ExpansionDelta(MSG_ViewIndex line)
{
	int32 result = 0;
	XP_ASSERT(line != MSG_VIEWINDEXNONE && line < m_groupView.GetSize());
	if (line == MSG_VIEWINDEXNONE || line >= m_groupView.GetSize()) return 0;
	XP_ASSERT(m_mode == MSG_SubscribeAll);
	if (m_mode != MSG_SubscribeAll) return 0;
	msg_SubscribeLine* info = m_groupView[line];
	int depth = info->GetDepth();
	if (line < m_groupView.GetSize() - 1 &&
		(m_groupView[line+1]->GetDepth() > depth)) {
		// OK, we are already expanded.  So, count how many lines after the
		// given one that have a depth bigger than the given line; that's how
		// many lines we would nuke.
		int32 start = line + 1;
		
		int32 end;
		for (end=start ; end<m_groupView.GetSize() ; end++) {
			if (m_groupView[end]->GetDepth() <= depth) break;
		}
		result = - (end - start);
	} else {
		// We are not expanded, so count how many to expand, if any.
		if (info->CanExpand()) {
			msg_GroupRecord* group = info->GetGroup();
			XP_ASSERT(group);
			if (group) {
				result = GetKidsArray(group, NULL, info->GetDepth() + 1);
			}
		}
	}
	return result;
}

int32
MSG_SubscribePane::GetNumLines()
{
	return m_groupView.GetSize();
}



msg_GroupRecord*
MSG_SubscribePane::FindRealDescendent(msg_GroupRecord* group)
{
	while (group && group->GetChildren()) {
		group = group->GetChildren();
	}
	return group;
}

		


int32
MSG_SubscribePane::GetKidsArray(msg_GroupRecord* group,
								msg_SubscribeLineArray* kids,
								int depth)
{
	msg_GroupRecord* child;
	int32 result = 0;
	int32 initsize = 0;
	if (kids) initsize = kids->GetSize();
	for (child = group->GetChildren(); child; child = child->GetSibling()) {
		msg_SubscribeLine* info = NULL;
		int32 numkids = child->GetNumKids();
		if (child->IsGroup() && (child->IsIMAPGroupRecord() ? 
									(numkids == 0) : TRUE))
		{
			if (kids) {
				info = new msg_SubscribeLine(child, depth,
											 WasSubscribed(child), 0);
				kids->Add(info);
			}
			result++;
		}
		if (numkids > 0 /*&& !child->IsCategoryContainer() */ ) {
			if ((numkids == 1) && !group->IsIMAPGroupRecord()) {
				// If there's only one real group inside, then just go grab
				// that one group, rather than wasting space by making a
				// container for it.
				msg_GroupRecord* tmp = FindRealDescendent(child);
				if (kids) {
					info = new
						msg_SubscribeLine(tmp, depth,
										  WasSubscribed(tmp), 0);
					kids->Add(info);
				}
				result++;
			} else {
				if (kids) {
					info =
						new msg_SubscribeLine(child, depth,
											  WasSubscribed(child),
											  child->GetNumKids());
					kids->Add(info);
				}
				result++;
				if (child->IsExpanded()) {
					result += GetKidsArray(child, kids, depth + 1);
				}
			}
				
		}
#ifdef NOTDEF
		// I wish the below worked, but right now it turns out we can have
		// some empty container groups.  Sigh.   ###
		XP_ASSERT(kids == NULL || info != NULL); // We had to have added
												 // something or another...
#endif
	}
	XP_ASSERT(depth == 0 || result > 0); // We should never try to get any
										 // subnewsgroups unless there actually
										 // are some.
	XP_ASSERT(kids == NULL || result + initsize == kids->GetSize());
	return result;
}


MsgERR
MSG_SubscribePane::ToggleSubscribed(MSG_ViewIndex* indices, int32 numIndices)
{
	if (!m_host) return eUNKNOWN;
	for (int32 i=0 ; i<numIndices ; i++) {
		MSG_ViewIndex line = indices[i];
		XP_ASSERT(line != MSG_VIEWINDEXNONE && line < m_groupView.GetSize());
		if (line == MSG_VIEWINDEXNONE || line >= m_groupView.GetSize()) {
			continue;
		}
		msg_SubscribeLine* info = m_groupView[line];
		msg_GroupRecord* group = info->GetGroup();
		char *fullname = group->GetFullName();
		if (!fullname)
		{
			return eOUT_OF_MEMORY;
		}
		ChangeSubscribe key;
		key.groupname = fullname;
		key.host = m_host;
		ChangeSubscribe* tmp = (ChangeSubscribe*)
			XP_Gethash(m_subscribeHash, &key, NULL);
		if (!tmp) {
			tmp = new ChangeSubscribe;
			if (!tmp) return eOUT_OF_MEMORY;
			if (m_endSubscribeList) {
				m_endSubscribeList->next = tmp;
			} else {
				m_subscribeList = tmp;
			}
			m_endSubscribeList = tmp;
			tmp->host = m_host;
			tmp->groupname = fullname;
			tmp->next = NULL;
			XP_Puthash(m_subscribeHash, tmp, tmp);
		} else {
			XP_ASSERT(XP_STRCMP(tmp->groupname, fullname) == 0);
			delete [] fullname;
		}
		XP_ASSERT(tmp->host == m_host);
		tmp->subscribe = !info->GetSubscribed();
		StartingUpdate(MSG_NotifyChanged, line, 1);
		info->SetSubscribed(tmp->subscribe);
		EndingUpdate(MSG_NotifyChanged, line, 1);
	}
	return eSUCCESS;
}


MsgERR
MSG_SubscribePane::SetSubscribed(MSG_ViewIndex* indices, int32 numIndices, XP_Bool subscribed)
{
	if (!m_host) return eUNKNOWN;
	for (int32 i=0 ; i<numIndices ; i++) {
		MSG_ViewIndex line = indices[i];
		XP_ASSERT(line != MSG_VIEWINDEXNONE && line < m_groupView.GetSize());
		if (line == MSG_VIEWINDEXNONE || line >= m_groupView.GetSize()) {
			continue;
		}
		msg_SubscribeLine* info = m_groupView[line];
		if (info->GetSubscribed() != subscribed)
		{
			msg_GroupRecord* group = info->GetGroup();
			char *fullname = group->GetFullName();
			if (!fullname)
			{
				return eOUT_OF_MEMORY;
			}
			ChangeSubscribe key;
			key.groupname = fullname;
			key.host = m_host;
			ChangeSubscribe* tmp = (ChangeSubscribe*)
				XP_Gethash(m_subscribeHash, &key, NULL);
			if (!tmp) {
				tmp = new ChangeSubscribe;
				if (!tmp) return eOUT_OF_MEMORY;
				if (m_endSubscribeList) {
					m_endSubscribeList->next = tmp;
				} else {
					m_subscribeList = tmp;
				}
				m_endSubscribeList = tmp;
				tmp->host = m_host;
				tmp->groupname = fullname;
				tmp->next = NULL;
				XP_Puthash(m_subscribeHash, tmp, tmp);
			} else {
				XP_ASSERT(XP_STRCMP(tmp->groupname, fullname) == 0);
				delete [] fullname;
			}
			XP_ASSERT(tmp->host == m_host);
			tmp->subscribe = subscribed;
			StartingUpdate(MSG_NotifyChanged, line, 1);
			info->SetSubscribed(tmp->subscribe);
			EndingUpdate(MSG_NotifyChanged, line, 1);
		}
	}
	return eSUCCESS;
}


MsgERR
MSG_SubscribePane::ExpandAll(MSG_ViewIndex* indices, int32 numIndices)
{
	XP_ASSERT(m_host->IsNews());
	if (!m_host->IsNews())
		return 0;

	// since the FE might have given us a pointer to their real selection (the
	// WinFE does this), make a copy of the array so the array doesn't change 
	// out from under us when we call EndingUpdate
	MSG_ViewIndex *tmpIndices = new MSG_ViewIndex[numIndices];
	if (NULL == tmpIndices) {
		return eOUT_OF_MEMORY;
	} 
	XP_MEMCPY(tmpIndices, indices, sizeof(MSG_ViewIndex) * numIndices);

	// since the FE could have constructed the list of indices in any order
	// (e.g. order of discontiguous selection), we have to sort the indices so
	// that we can expand the last one first (so we won't screw up any of the
	// other indices.
	if (numIndices > 1) {
		XP_QSORT(tmpIndices, numIndices, sizeof(MSG_ViewIndex),
			  MSG_LinedPane::CompareViewIndices);
	}
	for (int32 i=numIndices-1 ; i>=0 ; i--) {
		int32 n = ExpansionDelta(tmpIndices[i]);
		if (n > 0) {
			int32 m;
			DoToggleExpansion(tmpIndices[i], &m);
			XP_ASSERT(m == n);
			for (MSG_ViewIndex l = tmpIndices[i] + m ; l>tmpIndices[i] ; l--) {
				ExpandAll(&l, 1);
			}
		}
	}
	delete [] tmpIndices;
	return 0;
}


MsgERR
MSG_SubscribePane::CollapseAll()
{
	for (int32 i=m_groupView.GetSize() - 2 ; i>=0 ; i--) {
		if (m_groupView[i]->GetDepth() < m_groupView[i+1]->GetDepth()) {
			DoToggleExpansion(MSG_ViewIndex(i), NULL);
		}
	}
	return 0;
}


MsgERR
MSG_SubscribePane::ClearNew()
{
	if (!m_host) return eUNKNOWN;
	if (!m_host->IsNews()) return eUNKNOWN;	// only works for news
	XP_ASSERT(m_mode == MSG_SubscribeNew);
	MSG_NewsHost *newsHost = m_host->GetNewsHost();
	if (newsHost)
		newsHost->ClearNew();
	else
		return eUNKNOWN;
	SetMode(MSG_SubscribeNew, TRUE);
	return eSUCCESS;
}



void
MSG_SubscribePane::CheckForNewDone(URL_Struct* url_struct, int status,
								   MWContext* context)
{
	if (!m_host) return;
	if (!m_host->IsNews()) return;
	MSG_NewsHost *newsHost = (MSG_NewsHost *)m_host;
	MSG_Pane::CheckForNewDone(url_struct, status, context);
	if (m_mode == MSG_SubscribeAll) {
		if (status >= 0) {
			newsHost->ClearNew();
			newsHost->SaveHostInfo();
		} else {
			newsHost->Inhale(TRUE);
		}
	}
	SetMode(m_mode, TRUE, FALSE);
	if (m_callbacks.FetchCompleted) {
		(*m_callbacks.FetchCompleted)(this, m_callbackclosure);
	}
}

void
MSG_SubscribePane::ReportIMAPFolderDiscoveryFinished()
{
	if (m_callbacks.FetchCompleted) {
		(*m_callbacks.FetchCompleted)(this, m_callbackclosure);
	}
}


void MSG_SubscribePane::ClearAndFreeIMAPSubscribedList()
{
	char *subscribedName = NULL;
	for (int j = 0; j < m_imapSubscribedList->GetSize(); j++)
	{
		subscribedName = (char *)(m_imapSubscribedList->GetAt(j));
		XP_ASSERT (subscribedName);
		delete subscribedName;
		m_imapSubscribedList->RemoveAt(j);
	}
}

void MSG_SubscribePane::ClearAndFreeIMAPGroupList()
{
	delete m_imapTree;	// deleting the root will delete all its children
	XP_MapRemhash(m_imapGroupHash, imap_freehashkeys, NULL);	// free the keys in the hash table
	XP_Clrhash(m_imapGroupHash);	// clear all the entries from the hash table
	m_imapTree = msg_IMAPGroupRecord::Create(NULL, NULL, 0, FALSE);	// recreate a new root
}


msg_IMAPGroupRecord *MSG_SubscribePane::FindParentRecordOfIMAPGroup(const char *folderName, char delimiter)
{
	// look for this group's parent
	char *parentName = XP_STRDUP(folderName);
	if (!parentName)
		return 0;
	char *lastSlash = XP_STRRCHR(parentName, delimiter);
	msg_IMAPGroupRecord *parentRecord = NULL;
	if (lastSlash)
	{
		*lastSlash = 0;
		parentRecord = (msg_IMAPGroupRecord *)(XP_Gethash(m_imapGroupHash, parentName, NULL));
	}
	XP_FREEIF(parentName);
	return parentRecord;
}


// returns TRUE if we explicitly need to list this folder's children
// -- used only for top-level folders
// (NthLevelDiscovery is used for all other folders - easier and faster)
//
// filledInGroup is TRUE if we (the client) made up this group to act as a placeholder
// as a parent of a child which we have discovered first
XP_Bool MSG_SubscribePane::AddIMAPGroupToList(const char *folderName, char delimiter, XP_Bool isSubscribed,
											  uint32 folder_flags, XP_Bool filledInGroup /* = FALSE */)
{
	XP_Bool needChildrenListed = FALSE;
	// storage for ourFolderName is owned by the hash table m_imapGroupHash, as the key
	char *ourFolderName = XP_STRDUP(folderName);
	if (!ourFolderName)
		return 0;
	msg_IMAPGroupRecord *parentRecord = FindParentRecordOfIMAPGroup(ourFolderName, delimiter);
	if (!parentRecord)
	{
		char *maybeParentName = XP_STRDUP(ourFolderName);
		if (maybeParentName)
		{
			char *where = XP_STRRCHR(maybeParentName, delimiter);
			if (where)
				*where = 0;
			if (where && *maybeParentName)
			{
				AddIMAPGroupToList(maybeParentName, delimiter, FALSE, 0, TRUE);
				parentRecord = FindParentRecordOfIMAPGroup(ourFolderName, delimiter);
			}

			if (!parentRecord)
			{
				parentRecord = m_imapTree;
				needChildrenListed = TRUE;
			}
			XP_FREE(maybeParentName);
		}
	}

	if (isSubscribed)
	{
		// add it to the subscribe hash list - it should disallow duplicates automatically
		char *subscribeFolderName = new char[XP_STRLEN(ourFolderName) + 1];
		if (subscribeFolderName)
			XP_STRCPY(subscribeFolderName, ourFolderName);
		if (subscribeFolderName)
			m_imapSubscribedList->Add(subscribeFolderName);
	}

	// should return TRUE if we've ever put this into the list
	isSubscribed = (m_imapSubscribedList->FindIndex(0, ourFolderName) != -1);

	msg_IMAPGroupRecord *lookedUpRecord = NULL;
	if (!(lookedUpRecord = (msg_IMAPGroupRecord *)(XP_Gethash(m_imapGroupHash, ourFolderName, NULL))))
	{
		msg_IMAPGroupRecord *record = msg_IMAPGroupRecord::Create(parentRecord, ourFolderName, delimiter, filledInGroup);
		if (record)
		{
			// set flags for the record
			record->SetFlags(folder_flags);

			// add to the list of known IMAP groups
			XP_Puthash(m_imapGroupHash, ourFolderName, record);

			// there's a parent, so add as a child if it is visible
			// (or if the parent is the root)
			msg_SubscribeLine *parentLine = NULL;
			MSG_ViewIndex parentLineIndex = FindGroupViewIndex(parentRecord);
			if (parentLineIndex != MSG_VIEWINDEXNONE)
				parentLine = m_groupView[parentLineIndex];

			if (parentLine || (parentRecord == m_imapTree))
			{
				if (parentLine)
					parentLine->AddNewSubGroup();
				if ((parentRecord == m_imapTree) ||
					parentRecord->IsExpanded())
				{
					msg_IMAPGroupRecord *nextRecord = (msg_IMAPGroupRecord *)(record->GetSiblingOrAncestorSibling());
					MSG_ViewIndex whereToInsert = MSG_VIEWINDEXNONE;
					if (!nextRecord)
					{
						whereToInsert = m_groupView.GetSize();
					}
					else
					{
						whereToInsert = FindGroupViewIndex(nextRecord);
					}

					int16 parentDepth = parentRecord->GetDepth();
					if (parentRecord == m_imapTree)
						parentDepth = -1;


					XP_Bool latestSubscribed = isSubscribed;

					// see if we had already updated the state
					// but it is uncommitted
					ChangeSubscribe key;
					key.groupname = ourFolderName;
					key.host = m_host;
					ChangeSubscribe* tmp = (ChangeSubscribe*)
						XP_Gethash(m_subscribeHash, &key, NULL);
					if (tmp) {
						// If it was in the ChangeSubscribe list,
						// then set the subscribe bit to what we had previously
						// set it to.
						latestSubscribed = tmp->subscribe;
					}

					// create the line
					msg_SubscribeLine *childLine = new msg_SubscribeLine(record,
						parentDepth+1, latestSubscribed, 0);

					StartingUpdate(MSG_NotifyInsertOrDelete, whereToInsert, 1);
					m_groupView.InsertAt(whereToInsert, childLine);
					EndingUpdate(MSG_NotifyInsertOrDelete, whereToInsert, 1);

				}
				else
				{
					// make sure that there's a twisty
					if (parentLineIndex != MSG_VIEWINDEXNONE)
					{
						StartingUpdate(MSG_NotifyChanged, parentLineIndex, 1);
						EndingUpdate(MSG_NotifyChanged, parentLineIndex, 1);
					}
				}
			}
			else
			{
				// don't need to do anything, right?  The parent isn't visible in
				// the pane, and it's not a top-level folder (parent is root),
				// so therefore the child can't be visible
			}
			return needChildrenListed;
		}
		else
			return FALSE;
	}
	else
	{
		if (lookedUpRecord->GetIsGroupFilledIn())
		{
			// the record already existed in the hash table.
			// we should update its state:
			lookedUpRecord->SetIsGroupFilledIn(FALSE);
			lookedUpRecord->SetHierarchySeparator(delimiter);
			lookedUpRecord->SetFlags(folder_flags);

			// update the line
			msg_SubscribeLine *thisLine = NULL;
			MSG_ViewIndex thisLineIndex = FindGroupViewIndex(lookedUpRecord);
			if (thisLineIndex != MSG_VIEWINDEXNONE)
				thisLine = m_groupView[thisLineIndex];

			if (thisLine)
			{
				if (!thisLine->GetSubscribed())
					thisLine->SetSubscribed(isSubscribed);

				if (thisLineIndex != MSG_VIEWINDEXNONE)
				{
					StartingUpdate(MSG_NotifyChanged, thisLineIndex, 1);
					EndingUpdate(MSG_NotifyChanged, thisLineIndex, 1);
				}
			}
		}

		FREEIF(ourFolderName);
		// We might need its children
		return FALSE;
	}
}


MsgERR
MSG_SubscribePane::FetchGroupList()
{
	if (!m_host) return eUNKNOWN;
	MSG_NewsHost *newsHost = m_host->GetNewsHost();
	if (newsHost)
	{
		StartingUpdate(MSG_NotifyAll, 0, 0);
		m_groupView.RemoveAll();
		EndingUpdate(MSG_NotifyAll, 0, 0);

		XP_ASSERT(newsHost);

		newsHost->Exhale();
		newsHost->EmptyInhale();

		newsHost->setLastUpdate(0);	// Causes us to reload everything.

		return CheckForNew(newsHost);
	}
	else if (m_host->IsIMAP())
	{
		// For IMAP, this means to clear our list and perform a LIST of
		// the top-level folders again.
		StartingUpdate(MSG_NotifyAll, 0, 0);
		// do we need to delete the storage?
		m_groupView.RemoveAll();
		EndingUpdate(MSG_NotifyAll, 0, 0);

		ClearAndFreeIMAPGroupList();
		ClearAndFreeIMAPSubscribedList();

		MSG_IMAPHost *imapHost = m_host->GetIMAPHost();
		if (!imapHost)
			return eUNKNOWN;
		char * url = CreateImapAllAndSubscribedMailboxDiscoveryUrl(imapHost->GetHostName());
        if (url)
        {
			MSG_UrlQueue::AddUrlToPane(url, NULL, this);
	        XP_FREE(url);
	    }

		return eSUCCESS;
	}
	else 
	{
		// not a news or IMAP host
		XP_ASSERT(0);
		return eUNKNOWN;
	}
}

XP_Bool
MSG_SubscribePane::AddGroupsAsNew()
{
	return (m_mode != MSG_SubscribeAll);
}



MsgERR
MSG_SubscribePane::DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
							 int32 numIndices)
{
	MsgERR status = 0;
	switch (command) {
	case MSG_ToggleSubscribed:
		status = ToggleSubscribed(indices, numIndices);
		break;
	case MSG_SetSubscribed:
		status = SetSubscribed(indices, numIndices, TRUE);
		break;
	case MSG_ClearSubscribed:
		status = SetSubscribed(indices, numIndices, FALSE);
		break;
	case MSG_FetchGroupList:
		status = FetchGroupList();
		break;
	case MSG_ExpandAll:
		status = ExpandAll(indices, numIndices);
		UpdateCounts();
		break;
	case MSG_CollapseAll:
		status = CollapseAll();
		UpdateCounts();
		break;
	case MSG_ClearNew:
		status = ClearNew();
		UpdateCounts();
		break;
	case MSG_CheckForNew:
		if (m_host && m_host->IsNews())
		{
			MSG_NewsHost *newsHost = m_host->GetNewsHost();
			status = newsHost ? CheckForNew(newsHost) : eUNKNOWN;
		}
		break;
	case MSG_UpdateMessageCount:
		UpdateCounts();
		break;
	default:
		status = MSG_LinedPane::DoCommand(command, indices, numIndices);
		break;
	}
	if (status == MK_OFFLINE)
	{
		FE_Alert(GetContext(), XP_GetString(XP_ALERT_OFFLINE_MODE_SELECTED));
		status = 0;
	}

	return status;
}


MsgERR
MSG_SubscribePane::GetCommandStatus(MSG_CommandType command,
									const MSG_ViewIndex* indices,
									int32 numindices,
									XP_Bool *selectable_pP,
									MSG_COMMAND_CHECK_STATE *selected_pP,
									const char **display_stringP,
									XP_Bool *plural_pP)
{
	// ###tw  This never returns a display_string, right now.  If someone
	// actually needs it, I'll be glad to add it; but I suspect all the FE's
	// are going to get these strings their own way, since these are all
	// dialog box buttons and not menu items.


	const char *display_string = 0;
	XP_Bool plural_p = FALSE;
	XP_Bool selectable_p = TRUE;
	XP_Bool selected_p = FALSE;
	XP_Bool selected_used_p = FALSE;
	int32 i;

	switch (command) {
	case MSG_ToggleSubscribed:
	case MSG_SetSubscribed:
	case MSG_ClearSubscribed:
		if (m_host && m_host->IsIMAP())
			selectable_p = TRUE;
		else
		{
			for (i=0 ; i<numindices ; i++)
			{
				if (!m_groupView[i]->CanExpand())
				{
					selectable_p = TRUE;
					break;
				}
			}
		}
		break;
	case MSG_FetchGroupList:
		selectable_p = (m_mode == MSG_SubscribeAll);
		break;
	case MSG_ExpandAll:
		if (m_host && m_host->IsIMAP())
		{
			selectable_p = FALSE;
			break;
		}
	case MSG_CollapseAll:
		if (m_host)
		{
			if (m_host->IsNews())
			{
				// only allow expand all for News
				for (i=0 ; i<numindices ; i++) {
					if (m_groupView[i]->CanExpand()) {
						selectable_p = TRUE;
						break;
					}
				}
			}
			else
				selectable_p = TRUE;
		}
		break;
	case MSG_ClearNew:
	case MSG_CheckForNew:
		selectable_p = (m_host && m_host->IsNews() && (m_mode == MSG_SubscribeNew));
		break;
	default:
		return MSG_Pane::GetCommandStatus(command, indices, numindices,
										  selectable_pP, selected_pP,
										  display_stringP, plural_pP);
	}

	if (selectable_pP) *selectable_pP = selectable_p;
	if (selected_pP) {
		if (selected_used_p) {
			if (selected_p) {
				*selected_pP = MSG_Checked;
			} else {
				*selected_pP = MSG_Unchecked;
			}
		} else {
			*selected_pP = MSG_NotUsed;
		}
	}
	if (display_stringP) *display_stringP = display_string;
	if (plural_pP) *plural_pP = plural_p;

	return 0;
}


int
MSG_SubscribePane::SetCallbacks(MSG_SubscribeCallbacks* callbacks,
								void* closure)
{
	m_callbacks = *callbacks;
	m_callbackclosure = closure;
	return SetMode(m_mode, TRUE);
}


int
MSG_SubscribePane::Cancel()
{
	if (m_master->GetUpgradingToIMAPSubscription())
	{
		m_master->SetUpgradingToIMAPSubscription(FALSE);
	}

	ChangeSubscribe* tmp = m_subscribeList;
	while (tmp) {
		ChangeSubscribe* next = tmp->next;
		delete [] tmp->groupname;
		tmp->groupname = NULL;
		tmp->next = NULL;
		delete tmp;
		tmp = next;
	}
	m_subscribeList = NULL;
	XP_Clrhash(m_subscribeHash);
	return 0;
}


MSG_Host*
MSG_SubscribePane::GetHost()
{
	return m_host;
}


int
MSG_SubscribePane::SetHost(MSG_Host* host)
{
	int status = 0;
	if (host != m_host) {
		if (m_host) {
			MSG_NewsHost *newsHost = m_host->GetNewsHost();
			if (newsHost)
				status = newsHost->Exhale();
			if (status < 0) return status;
		}
		m_host = host;
		if (m_host) {
			MSG_NewsHost *newsHost = m_host->GetNewsHost();
			if (newsHost)
				status = newsHost->Inhale();
			if (status < 0) return status;
		}
		if (m_host && m_host->IsNews())
		{
			return SetMode(m_mode, TRUE);
		}
		else
		{
			// IMAP only allows subscribe, not search or new
			return SetMode(MSG_SubscribeAll, TRUE);
		}
	}
	return 0;
}


MSG_SubscribeMode
MSG_SubscribePane::GetMode()
{
	return m_mode;
}


static int
CompareAddTimes(const void* e1, const void* e2)
{
	msg_GroupRecord* g1 = (*((msg_SubscribeLine**) e1))->GetGroup();
	msg_GroupRecord* g2 = (*((msg_SubscribeLine**) e2))->GetGroup();
	int delta = int(int32(g2->GetAddTime()) - int32(g1->GetAddTime()));
	if (delta) return delta;

	XP_ASSERT(e1 != e2);
	if (e1 == e2) return 0;

	// Oh, boy.  These two newsgroups seemed to have been added at the same
	// time.  We should display them in alphabetical order.  But we don't
	// want to just grab their fullnames, because that's pretty expensive and
	// qsort compare routines should be very fast.  So, we painfully compare
	// their names, step by step.
	// First, we find a common ancestor.

	int d1 = g1->GetDepth();
	int d2 = g2->GetDepth();

	msg_GroupRecord* origg1 = g1;

	while (d1 > d2) {
		g1 = g1->GetParent();
		d1--;
	}
	while (d2 > d1) {
		g2 = g2->GetParent();
		d2--;
	}
	if (g1 == g2) {
		// This can happen only if the initial depths weren't the same, and
		// one group is in fact an ancestor of the other.  In that case,
		// the ancestor comes first.
		if (g1 == origg1) return -1;
		else return 1;
	}
	while (g1->GetParent() != g2->GetParent()) {
		g1 = g1->GetParent();
		g2 = g2->GetParent();
	}
	return XP_STRCMP(g1->GetPartName(), g2->GetPartName());
}


int
MSG_SubscribePane::SetMode(MSG_SubscribeMode mode, XP_Bool force,
						   XP_Bool autofetch)
{
	time_t first;
	if (!force && mode == m_mode) return 0;
	InterruptContext(FALSE);
	StartingUpdate(MSG_NotifyAll, 0, 0);
	for (int32 i=0 ; i<m_groupView.GetSize() ; i++) {
		delete m_groupView[i];
		m_groupView[i] = NULL;
	}
	m_groupView.RemoveAll();

	MSG_NewsHost *newsHost = m_host ? m_host->GetNewsHost() : 0;
	int32 lasttime = newsHost ? newsHost->getLastUpdate() : 0;
	if (m_host) {
		m_mode = mode;
		switch(m_mode) {
		case MSG_SubscribeAll:
			if ((lasttime > 0) && newsHost) {
				GetKidsArray(newsHost->GetGroupTree(), &m_groupView, 0);
			}
			break;
		case MSG_SubscribeSearch:
			XP_ASSERT(newsHost);
			if (newsHost)
				FindAll(m_lastSearch);
			break;
		case MSG_SubscribeNew:
			XP_ASSERT(newsHost);
			if (newsHost)
			{
				first = newsHost->GetFirstNewDate();
				msg_GroupRecord* child;
				for (child = newsHost->GetGroupTree()->GetChildren();
					 child;
					 child = child->GetNextAlphabeticNoCategories()) {
					if (child->IsGroup() && child->GetAddTime() >= first) {
						msg_SubscribeLine* info =
							new msg_SubscribeLine(child, 0, WasSubscribed(child),
												  0);
						if (info) m_groupView.Add(info);
					}
				}
				m_groupView.QuickSort(CompareAddTimes);
			}
			break;
		default:
			XP_ASSERT(0);
			break;
		}
	}
	EndingUpdate(MSG_NotifyAll, 0, 0);
	UpdateCounts();
	if (autofetch && m_host && lasttime == 0) {
		if (m_mode == MSG_SubscribeAll) {
			if (m_callbacks.DoFetchGroups) {
				(*m_callbacks.DoFetchGroups)(this, m_callbackclosure);
			}
		} else {
			FE_Alert(GetContext(),
					 XP_GetString(MK_MSG_INCOMPLETE_NEWSGROUP_LIST));
		}
	}
	return 0;
}


MSG_ViewIndex MSG_SubscribePane::FindGroupViewIndex(msg_GroupRecord* group)
{
	if (!m_host) return MSG_VIEWINDEXNONE;
	XP_ASSERT(group);
	if (!group) return MSG_VIEWINDEXNONE;

	MSG_ViewIndex result;
	for (result = 0 ; result < m_groupView.GetSize() ; result++) {
		if (m_groupView[result]->GetGroup() == group) {
			return result;
		}
	}
	return MSG_VIEWINDEXNONE;
}


// only NNTP
MSG_ViewIndex
MSG_SubscribePane::FindGroupExpandIfNecessary(msg_GroupRecord* group)
{
	if (!m_host) return MSG_VIEWINDEXNONE;
	MSG_NewsHost *newsHost = m_host->GetNewsHost();
	XP_ASSERT(newsHost);
	if (!newsHost) return MSG_VIEWINDEXNONE;
	XP_ASSERT(group && group != newsHost->GetGroupTree());
	if (!group || group == newsHost->GetGroupTree()) return MSG_VIEWINDEXNONE;
	msg_GroupRecord* parent = group->GetParent();
	if (!group->IsGroup() && group->GetNumKids() == 1) {
		// This group has exactly one group inside of it, and that will be
		// represented by one line.  So we need to be searching for that line.
		// But if the search fails, then we need to expand the original parent.
		group = FindRealDescendent(group);
	}

	MSG_ViewIndex result;
	for (result = 0 ; result < m_groupView.GetSize() ; result++) {
		if (m_groupView[result]->GetGroup() == group) {
			return result;
		}
	}
	// Hmm.  The group must be missing because an ancestor is closed.
	// Find that ancestor and expand it.
	result = FindGroupExpandIfNecessary(parent);
	XP_ASSERT(result != MSG_VIEWINDEXNONE);
	if (result == MSG_VIEWINDEXNONE) return result;

	if (ExpansionDelta(result) > 0) {
		DoToggleExpansion(result, NULL);
	} else if (result < m_groupView.GetSize() - 1 &&
			   m_groupView[result+1]->GetGroup() == parent) {
		if (ExpansionDelta(result + 1) > 0) {
			DoToggleExpansion(result + 1, NULL);
		}
	}
	for (; result < m_groupView.GetSize() ; result++) {
		if (m_groupView[result]->GetGroup() == group) {
			return result;
		}
	}
	return MSG_VIEWINDEXNONE;
}


// only NNTP
MSG_ViewIndex
MSG_SubscribePane::FindFirst(const char* str)
{
	XP_ASSERT(str);
	if (!str || !*str) return MSG_VIEWINDEXNONE;
	XP_ASSERT(m_mode == MSG_SubscribeAll);
	if (m_mode != MSG_SubscribeAll) return MSG_VIEWINDEXNONE;
	if (!m_host) return MSG_VIEWINDEXNONE;
	MSG_NewsHost *newsHost = m_host->GetNewsHost();
	XP_ASSERT(newsHost);
	if (!newsHost || newsHost->getLastUpdate() == 0) return MSG_VIEWINDEXNONE;
	msg_GroupRecord* parent = newsHost->GetGroupTree();
	MSG_ViewIndex result = MSG_VIEWINDEXNONE;
	char* ptr = NULL;

	while (*str) {
		ptr = XP_STRCHR(str, '.');
		if (ptr) *ptr = '\0';
		msg_GroupRecord* child = NULL;
		/*if (!parent->IsCategoryContainer()) */ {
			for (child = parent->GetChildren();
				 child;
				 child = child->GetSibling()) {
				if (XP_STRCMP(child->GetPartName(), str) >= 0) break;
			}
		}
		if (!child) break;
		parent = child;
		if (XP_STRCMP(child->GetPartName(), str) != 0) {
			// This is not an exact match.  Ignore any other parts of the
			// input string.
			break;
		}
		if (ptr) {
			*ptr = '.';
			str = ptr + 1;
		} else {
			break;
		}
	}

	if (ptr) *ptr = '.';

	XP_ASSERT(parent);
	if (!parent || parent == newsHost->GetGroupTree()) return MSG_VIEWINDEXNONE;

	result = FindGroupExpandIfNecessary(parent);
	UpdateCounts();
	return result;
}


// only NNTP
int
MSG_SubscribePane::AddSearchResult(msg_GroupRecord* child)
{
	if (!child->IsGroup()) return 0;
	msg_SubscribeLine* line =
		new msg_SubscribeLine(child, 0, WasSubscribed(child), 0);
	if (!line) return MK_OUT_OF_MEMORY;
	int size = m_groupView.GetSize();
	StartingUpdate(MSG_NotifyInsertOrDelete, size, 1);
	m_groupView.InsertAt(size, line);
	EndingUpdate(MSG_NotifyInsertOrDelete, size, 1);
	return 0;
}


													

// only NNTP
int
MSG_SubscribePane::FindAll(const char* str)
{
	XP_ASSERT(m_mode == MSG_SubscribeSearch);
	if (m_mode != MSG_SubscribeSearch) return -1;
	if (!m_host) return 0;
	XP_ASSERT(m_host->IsNews());
	MSG_NewsHost *newsHost = m_host->GetNewsHost();
	if (!newsHost || newsHost->getLastUpdate() == 0) return 0;
	StartingUpdate(MSG_NotifyAll, 0, 0);
	m_groupView.RemoveAll();
	EndingUpdate(MSG_NotifyAll, 0, 0);

	if (str == NULL || *str == '\0') return 0;

	msg_GroupRecord* child;
	if (XP_STRCHR(str, '.') == NULL) {
		// There are no dots in the search string, so we can just search the
		// part names of each children.
		for (child = newsHost->GetGroupTree()->GetChildren();
			 child ;
			 child = child->GetNextAlphabeticNoCategories()) {
			if (XP_STRCASESTR(child->GetPartName(), str)) {
				msg_GroupRecord* done = child->GetSiblingOrAncestorSibling();
				for (;;) {
					AddSearchResult(child);
					msg_GroupRecord* next =
						child->GetNextAlphabeticNoCategories();
					if (next == done) break;
					child = next;
				}
			} else {
				const char* pretty = child->GetPrettyName();
				if (pretty && XP_STRCASESTR(pretty, str)) {
					AddSearchResult(child);
				}
			}
		}
	} else {
		// There are dots in the search string, so we have to painfully grab
		// the full name of every group to see if it matches.  This can
		// still be optimized, but it's harder, so I don't right now.  ###tw
		for (child = newsHost->GetGroupTree()->GetChildren();
			 child ;
			 child = child->GetNextAlphabeticNoCategories()) {
			if (child->IsGroup()) {
				char* fullname = child->GetFullName();
				if (!fullname) return MK_OUT_OF_MEMORY;
				if (XP_STRCASESTR(fullname, str)) {
					AddSearchResult(child);
				} else {
					const char* pretty = child->GetPrettyName();
					if (pretty && XP_STRCASESTR(pretty, str)) {
						AddSearchResult(child);
					}
				}
				delete [] fullname;
			}
		}
	}
	if (m_groupView.GetSize() == 0) {
		FE_Alert(GetContext(), XP_GetString(MK_MSG_SEARCH_FAILED));
	}
	return 0;
}


XP_Bool
MSG_SubscribePane::GetGroupNameLineByIndex(MSG_ViewIndex firstline,
										   int32 numlines,
										   MSG_GroupNameLine* data)
{
	if (!m_host || firstline == MSG_VIEWINDEXNONE ||
		firstline + numlines > m_groupView.GetSize() ||
		numlines < 0) return FALSE;
	int32 i;
	MSG_GroupNameLine* line;
	for (i=0, line=data ; i<numlines ; i++, line++) {
		msg_SubscribeLine* info = m_groupView[firstline + i];
		msg_GroupRecord *group = info->GetGroup();
		XP_ASSERT(group);
		if (!group) return FALSE;
		const char *pretty = group->GetPrettyName();
		if (m_host->IsNews())
		{
			char *tmp = group->GetFullName();
			if (!tmp) return FALSE;	// Out of memory.
			XP_STRNCPY_SAFE(line->name, tmp, sizeof(line->name));
			delete [] tmp;
		}
		else
		{
			// IMAP - don't display the whole path
			const char *partname = group->GetPartName();
			if (!partname) return FALSE;
			XP_STRNCPY_SAFE(line->name, partname, sizeof(line->name));
		}
		line->level = info->GetDepth();
		line->total = info->GetNumMessages();
		line->flags = 0;
		MSG_NewsHost *newsHost = m_host->GetNewsHost();
		if (newsHost && group->GetAddTime() > newsHost->GetFirstNewDate()) {
			line->flags |= MSG_GROUPNAME_FLAG_NEW_GROUP;
		}
		if (info->CanExpand()) {
			int length = XP_STRLEN(line->name);
			if (m_host->IsNews())
			{
				PR_snprintf(line->name + length, sizeof(line->name) - length - 1,
							".* (%ld groups)", info->GetNumSubGroups());
				// ###tw   I18N the above string.  (Of course, it wasn't i18n in
				// 3.0, either.)
			}
			else
			{
				int32 numSubGroups = info->GetNumSubGroups();
				char *subfolderString = numSubGroups == 1 ? 
					"subfolder" : "subfolders";
				PR_snprintf(line->name + length, sizeof(line->name) - length - 1,
							".* (%ld %s)", numSubGroups, subfolderString);
				// ###tw   I18N the above string.  (Of course, it wasn't i18n in
				// 3.0, either.)
			}
			line->flags |= MSG_GROUPNAME_FLAG_HASCHILDREN;
			if (firstline + i >= m_groupView.GetSize() - 1 ||
				m_groupView[firstline + i + 1]->GetDepth() <= line->level) {
				line->flags |= MSG_GROUPNAME_FLAG_ELIDED;
			}
		} else if (newsHost && pretty) {
			int length = XP_STRLEN(line->name);
			PR_snprintf(line->name + length, sizeof(line->name) - length - 1,
						" (%s)", pretty);
		}
		if (info->GetSubscribed()) {
			line->flags |= MSG_GROUPNAME_FLAG_SUBSCRIBED;
		}

		// set the folder type flags (IMAP public, IMAP personal, etc.)
		msg_IMAPGroupRecord *imapGroup = group->GetIMAPGroupRecord();
		if (imapGroup)
			line->flags |= imapGroup->GetFlags();
	}
	return TRUE;
}



XP_Bool
MSG_SubscribePane::WasSubscribed(msg_GroupRecord* group)
{
	char* groupname = group->GetFullName();
	if (!groupname || !m_host) return FALSE;

	// first look through the updated (uncommitted) subscribe list
	ChangeSubscribe key;
	key.groupname = groupname;
	key.host = m_host;
	ChangeSubscribe* tmp = (ChangeSubscribe*)
		XP_Gethash(m_subscribeHash, &key, NULL);
	if (tmp) {
		delete [] groupname;
		return tmp->subscribe;
	}

	// next, if it's IMAP, look through the original subscribed list
	// that we first got back from the server
	if (group->IsIMAPGroupRecord())
	{
		XP_Bool wasSubscribed = (m_imapSubscribedList->FindIndex(0, groupname) != -1);
		delete [] groupname;
		return wasSubscribed;
	}

	// otherwise, we must not be subscribed
	delete [] groupname;
	return FALSE;
}


void
MSG_SubscribePane::UpdateCounts()
{
	if (m_host && !NET_IsOffline())
	{
		MSG_NewsHost *newsHost = m_host->GetNewsHost();
		if (newsHost)
		{
			for (int i=0 ; i<m_groupView.GetSize() ; i++) {
				msg_SubscribeLine* info = m_groupView[i];
				if (info->GetNumMessages() < 0 && info->GetNumSubGroups() == 0) {
					// OK, found one, so kick off the URL.
					InterruptContext(FALSE);
					UpdateNewsCounts(newsHost);
					return;
				}
			}
		}
	}
}


void
MSG_SubscribePane::UpdateNewsCountsDone(int status)
{
	if (status >= 0) {
		UpdateCounts();
	}
}


int32
MSG_SubscribePane::GetNewsRCCount(MSG_NewsHost* host)
{
	XP_ASSERT(host == m_host);
	if (host != m_host) return 0;
	int num = m_groupView.GetSize();
	int32 result = 0;
	for (int i=0 ; i<num ; i++) {
		msg_SubscribeLine* info = m_groupView[i];
		if (info->GetNumMessages() < 0 && info->GetNumSubGroups() == 0) {
			result++;
		}
	}
	m_curUpdate = 0;
	return result;
}


char*
MSG_SubscribePane::GetNewsRCGroup(MSG_NewsHost* host)
{
	XP_ASSERT(host == m_host);
	if (host != m_host) return NULL;
	int num = m_groupView.GetSize();
	if (num <= 0) return NULL;
	if (m_curUpdate >= num) m_curUpdate = 0;
	int32 i = m_curUpdate;
	do {
		msg_SubscribeLine* info = m_groupView[i];
		if (info->GetNumMessages() < 0 && info->GetNumSubGroups() == 0) {
			info->SetNumMessages(0);	// so we don't keep returning this one.
			msg_GroupRecord* group = info->GetGroup();
			XP_ASSERT(group);
			if (!group) continue;
			char* tmp = group->GetFullName();
			char* result = NULL;
			if (tmp) {
				result = XP_STRDUP(tmp); // ### Ick.  malloc vs. new.
				delete [] tmp;
			}
			m_curUpdate = i;
			return result;
		}
		i++;
		if (i >= num) i = 0;
	} while (i != m_curUpdate);
	return NULL;
}



int 
MSG_SubscribePane::DisplaySubscribedGroup(MSG_NewsHost* host,
										  const char *groupname,
										  int32 /*oldest_message*/,
										  int32 /*youngest_message*/,
										  int32 total_messages,
										  XP_Bool /*nowvisiting*/)
{
	XP_ASSERT(host == m_host);
	if (host != m_host) return 0;

	XP_ASSERT(m_curUpdate >= 0 && m_curUpdate < m_groupView.GetSize());
	if (m_curUpdate < 0 || m_curUpdate >= m_groupView.GetSize()) return -1;

	msg_SubscribeLine* info = m_groupView[m_curUpdate];
	msg_GroupRecord* group = info->GetGroup();
	XP_ASSERT(group);
	if (!group) return -1;
	char* tmp = group->GetFullName();
	XP_Bool matches = (XP_STRCMP(groupname, tmp) == 0);
	delete [] tmp;
	if (matches) {
		StartingUpdate(MSG_NotifyChanged, m_curUpdate, 1);
		info->SetNumMessages(total_messages);
		EndingUpdate(MSG_NotifyChanged, m_curUpdate, 1);
	}
	return 0;
}


int MSG_SubscribePane::CommitSubscriptions()
{
	DisableUpdate();
	InterruptContext(FALSE);
	ChangeSubscribe* tmp = m_subscribeList;
	XP_Bool runningIMAP = FALSE;
	while (tmp) {
		XP_ASSERT(tmp->groupname);
		if (tmp->groupname)
		{
			if (tmp->host->IsNews())
			{
				MSG_NewsHost *newsHost = tmp->host->GetNewsHost();
				if (tmp->subscribe) {
					newsHost->AddGroup(tmp->groupname);
				} else {
					newsHost->RemoveGroup(tmp->groupname);
				}
				//delete [] tmp->groupname;
				//tmp->groupname = NULL;
			}
			else if (tmp->host->IsIMAP())
			{
				MSG_IMAPHost *imapHost = tmp->host->GetIMAPHost();
				// here's where we have to issue a subscribe or unsubscribe to the server

				if (imapHost)
				{
					char *imapSubscribeURL = 0;
					// whether or not we were originally subscribed to this folder
					XP_Bool originallySubscribed = (m_imapSubscribedList->FindIndex(0, tmp->groupname) != -1);

					if (tmp->subscribe && !originallySubscribed)
					{
						imapSubscribeURL = CreateIMAPSubscribeMailboxURL(imapHost->GetHostName(), tmp->groupname);
						if (imapHost->GetIsHostUsingSubscription() || m_master->GetUpgradingToIMAPSubscription())
							imapHost->SetHostNeedsFolderUpdate(TRUE);	// we only need to update the folder view if we're only
																		// showing subscribed folders
					}
					else if (!tmp->subscribe && originallySubscribed)
					{
						imapSubscribeURL = CreateIMAPUnsubscribeMailboxURL(imapHost->GetHostName(), tmp->groupname);
						if (imapHost->GetIsHostUsingSubscription() || m_master->GetUpgradingToIMAPSubscription())
							imapHost->SetHostNeedsFolderUpdate(TRUE);	// we only need to update the folder view if we're only
																		// showing subscribed folders
					}

					if (imapSubscribeURL)
					{
						runningIMAP = TRUE;
						MSG_UrlQueue::AddUrlToPane(imapSubscribeURL, NULL, this);
						XP_FREE(imapSubscribeURL);
					}
				}
			}
			else
			{
				// not News or IMAP - what is it?
				XP_ASSERT(0);
			}
		}
		ChangeSubscribe* next = tmp->next;
		//delete tmp;
		tmp = next;
	}

	if (m_master->GetUpgradingToIMAPSubscription())
	{
		m_master->SetIMAPSubscriptionUpgradedPrefs();
		m_master->SetUpgradingToIMAPSubscription(FALSE);
	}

	if (!runningIMAP)
	{
		// make sure the FE knows it's ok to shut down the pane
		FE_AllConnectionsComplete(GetContext());
	}
		
	return 0;
}

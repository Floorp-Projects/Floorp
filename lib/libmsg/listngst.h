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
#ifndef ListNGSt_H
#define ListNGSt_H

class MSG_Master;
class NewsGroupDB;
class MessageDBView;
class msg_NewsArtSet;

#include "msg.h"
#include "chngntfy.h"
#include "idarray.h"

  /* The below is all stuff that we remember for libnet about which
	 articles we've already seen in the current newsgroup. */
typedef struct MSG_NewsKnown {
	MSG_NewsHost* host;
	char* group_name;
	msg_NewsArtSet* set; /* Set of articles we've already gotten
								  from the newsserver (if it's marked
								  "read", then we've already gotten it).
								  If an article is marked "read", it
								  doesn't mean we're actually displaying
								  it; it may be an article that no longer
								  exists, or it may be one that we've
								  marked read and we're only viewing
								  unread messages. */

	int32 first_possible;	/* The oldest article in this group. */
	int32 last_possible;	/* The newest article in this group. */

	XP_Bool shouldGetOldest;
  } MSG_NewsKnown;


// This class should ultimately be part of a news group listing
// state machine - either by inheritance or delegation.
// Currently, a folder pane owns one and libnet news group listing
// related messages get passed to this object.
class ListNewsGroupState : public ChangeListener
{
public:
	ListNewsGroupState(const char *url, const char *groupName, MSG_Pane *pane);
	~ListNewsGroupState();
	int GetRangeOfArtsToDownload(MSG_NewsHost* host,
							 const char* group_name,
							 int32 first_possible,
							 int32 last_possible,
							 int32 maxextra,
							 int32* first,
							 int32* lastprotected);
	int AddToKnownArticles(MSG_NewsHost* host,
					   const char* group_name,
					   int32 first, int32 last);
	int InitXOVER(MSG_NewsHost* host,
			   const char *group_name,
			   uint32 first_msg, uint32 last_msg,
			   uint32 oldest_msg, uint32 youngest_msg);
	int ProcessXOVER(char *line);
	int ResetXOVER();
	int ProcessNonXOVER(char *line);
	int FinishXOVER (int status);

	MSG_Master		*GetMaster() {return m_master;}
	void			SetMaster(MSG_Master *master) {m_master = master;}
	void			SetView(MessageDBView *view);
	void			SetPane(MSG_Pane *pane) {m_pane = pane;}
	MSG_NewsHost*	GetHost() {return m_host;}
	const char *	GetGroupName() {return m_groupName;}
	const char *	GetURL() {return m_url;}
	virtual void	OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
	void			SetGetOldMessages(XP_Bool getOldMessages) {m_getOldMessages = getOldMessages;}
	XP_Bool			GetGetOldMessages() {return m_getOldMessages;}

protected:
	NewsGroupDB		*m_newsDB;
	MessageDBView	*m_msgDBView;		// open view on current download, if any
	MSG_Pane		*m_pane;
	XP_Bool			m_startedUpdate;
	XP_Bool			m_getOldMessages;
	XP_Bool			m_promptedAlready;
	XP_Bool			m_downloadAll;
	int32			m_maxArticles;
	char			*m_groupName;
	MSG_NewsHost	*m_host;
	char			*m_url;			// url we're retrieving
	MSG_Master		*m_master;

	MessageKey		m_lastProcessedNumber;
	MessageKey		m_firstMsgNumber;
	MessageKey		m_lastMsgNumber;

	struct MSG_NewsKnown	m_knownArts;
	msg_NewsArtSet	*m_set;
};

class ListNewsGroupArticleKeysState : public ChangeListener
{
public:
	ListNewsGroupArticleKeysState(MSG_NewsHost *host, const char *groupName, MSG_Pane *pane);
	~ListNewsGroupArticleKeysState();
	int AddArticleKey(int32 key);
	int FinishAddingArticleKeys();
protected:
	struct MSG_NewsKnown	m_idsOnServer;
	MSG_Pane				*m_pane;
	const char *			m_groupName;
	MSG_NewsHost			*m_host;
	NewsGroupDB				*m_newsDB;
	IDArray					m_idsInDB;
#ifdef DEBUG_bienvenu
	IDArray					m_idsDeleted;
#endif
	int32					m_dbIndex;
	MessageKey				m_highwater;
};


#endif

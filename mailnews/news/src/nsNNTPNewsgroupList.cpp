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

/*
 * formerly listngst.cpp
 * This class should ultimately be part of a news group listing
 * state machine - either by inheritance or delegation.
 * Currently, a folder pane owns one and libnet news group listing
 * related messages get passed to this object.
 */

#ifdef HAVE_PANES
class MSG_Master;
#endif
#ifdef HAVE_NEWSDB
class NewsGroupDB;
#endif
#ifdef HAVE_DBVIEW
class MessageDBView;
#endif
class msg_NewsArtSet;

#include "nsNNTPNewsgroupList.h"

#ifdef HAVE_NEWSDB
#include "newsdb.h"
#endif

#ifdef HAVE_DBVIEW
#include "msgdbvw.h"
#endif

#include "newsset.h"
#ifdef HAVE_PANES
#include "msgpane.h"
#endif


extern "C"
{
	extern int MK_OUT_OF_MEMORY;
	extern int MK_DISK_FULL;
	extern int MK_HDR_DOWNLOAD_COUNT;
	extern int MK_NO_NEW_DISC_MSGS;
}


// This class should ultimately be part of a news group listing
// state machine - either by inheritance or delegation.
// Currently, a folder pane owns one and libnet news group listing
// related messages get passed to this object.
class nsNNTPNewsgroupList : public nsIMsgNewsArticleList
#ifdef HAVE_CHANGELISTENER
    ,public ChangeListener
#endif
{
public:
  nsNNTPNewsgroupList();
  ~nsNNTPNewsgroupList();
    // nsIMsgNewsArticleList
	NS_IMETHOD GetRangeOfArtsToDownload(MSG_NewsHost* host,
							 const char* group_name,
							 int32 first_possible,
							 int32 last_possible,
							 int32 maxextra,
							 int32* first,
							 int32* lastprotected);
	NS_IMETHOD AddToKnownArticles(MSG_NewsHost* host,
                                  const char* group_name,
                                  int32 first, int32 last);

    // nsIMsgXOVERParser
    NS_IMETHOD InitXOVER(MSG_NewsHost* host,
                         const char *group_name,
                         uint32 first_msg, uint32 last_msg,
                         uint32 oldest_msg, uint32 youngest_msg);
  NS_IMETHOD ProcessXOVER(char *line);
  NS_IMETHOD ResetXOVER();
  NS_IMETHOD ProcessNonXOVER(char *line);
  NS_IMETHOD FinishXOVER(int status);
    
	MSG_Master		*GetMaster() {return m_master;}
	void			SetMaster(MSG_Master *master) {m_master = master;}
#ifdef HAVE_DBVIEW
	void			SetView(MessageDBView *view);
#endif
#ifdef HAVE_PANES
	void			SetPane(MSG_Pane *pane) {m_pane = pane;}
#endif
    PRBool          m_finishingXover;
	MSG_NewsHost*	GetHost() {return m_host;}
	const char *	GetGroupName() {return m_groupName;}
	const char *	GetURL() {return m_url;}
#ifdef HAVE_CHANGELISTENER
	virtual void	OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
#endif
	void			SetGetOldMessages(XP_Bool getOldMessages) {m_getOldMessages = getOldMessages;}
	XP_Bool			GetGetOldMessages() {return m_getOldMessages;}

protected:
#ifdef HAVE_NEWSDB
	NewsGroupDB		*m_newsDB;
#endif
#ifdef HAVE_DBVIEW
	MessageDBView	*m_msgDBView;		// open view on current download, if any
#endif
#ifdef HAVE_PANES
	MSG_Pane		*m_pane;
#endif
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
	int32			m_firstMsgToDownload;
	int32			m_lastMsgToDownload;

	struct MSG_NewsKnown	m_knownArts;
	msg_NewsArtSet	*m_set;
};



nsNNTPNewsgroupList::nsNNTPNewsgroupList()
{
      NS_INIT_REFCNT();
}

nsNNTPNewsgroupList::~nsNNTPNewsgroupList()
{
}

NS_IMPL_ADDREF(nsNNTPNewsgroupList);
NS_IMPL_RELEASE(nsNNTPNewsgroupList);
NS_IMPL_QUERYINTERFACE(nsNNTPNewsgroupList);

                  
nsNNTPNewsgroupList::InitnsNNTPNewsgroupList(const char *url, const char *groupName, MSG_Pane *pane);
{
#ifdef HAVE_NEWSDB
	m_newsDB = NULL;
#endif
#ifdef HAVE_DBVIEW
	m_msgDBView = NULL;
#endif
	m_groupName = XP_STRDUP(groupName);
	m_host = NULL;
	m_url = XP_STRDUP(url);
	m_lastProcessedNumber = 0;
	m_lastMsgNumber = 0;
	m_set = NULL;
#ifdef HAVE_PANES
	XP_ASSERT(pane);
	m_pane = pane;
	m_master = pane->GetMaster();
#endif
    m_finishingXover = PR_FALSE;

	m_startedUpdate = FALSE;
	XP_MEMSET(&m_knownArts, 0, sizeof(m_knownArts));
	m_knownArts.group_name = m_groupName;
	char* host_and_port = NET_ParseURL(url, GET_HOST_PART);
	m_host = m_master->FindHost(host_and_port,
								(url[0] == 's' || url[0] == 'S'),
								-1);
	FREEIF(host_and_port);
	m_knownArts.host = m_host;
	m_getOldMessages = FALSE;
	m_promptedAlready = FALSE;
	m_downloadAll = FALSE;
	m_maxArticles = 0;
	m_firstMsgToDownload = 0;
	m_lastMsgToDownload = 0;
}

nsNNTPNewsgroupList::~nsNNTPNewsgroupList()
{
	XP_FREE(m_url);
	XP_FREE(m_groupName);
    
#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
		m_msgDBView->Remove(this);
#endif

#ifdef HAVE_NEWSDB
	if (m_newsDB)
		m_newsDB->Close();
#endif
	delete m_knownArts.set;
}

void nsNNTPNewsgroupList::SetView(MessageDBView *view) 
{
#ifdef HAVE_DBVIEW
	m_msgDBView = view;
	if (view)
		view->Add(this);
#endif
}

#ifdef HAVE_CHANGELISTENER
void	nsNNTPNewsgroupList::OnAnnouncerGoingAway (ChangeAnnouncer *instigator)
{
#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
	{
		m_msgDBView->Remove(this);
		m_msgDBView->NotifyAnnouncerGoingAway(instigator);	// shout it to the world!
		m_msgDBView = NULL;
	}
#endif
}
#endif

int nsNNTPNewsgroupList::GetRangeOfArtsToDownload(MSG_NewsHost* host,
							 const char* group_name,
							 int32 first_possible,
							 int32 last_possible,
							 int32 maxextra,
							 int32* first,
							 int32* last)
{
	int status = 0;
	XP_Bool emptyGroup_p = FALSE;
	MsgERR		err;

	XP_ASSERT(first && last);
	if (!first || !last) return -1;

	*first = 0;
	*last = 0;

#ifdef HAVE_PANES
	if (m_pane != NULL && !m_finishingXover && !m_startedUpdate)
	{
		m_startedUpdate = TRUE;
		m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
	}
#endif

#ifdef HAVE_NEWSDB
	if (!m_newsDB)
	{
		if ((err = NewsGroupDB::Open(m_url, m_master, &m_newsDB)) != eSUCCESS)
			return ConvertMsgErrToMKErr(err);	
		else
		{
			m_set = m_newsDB->GetNewsArtSet();
			m_set->SetLastMember(last_possible);	// make sure highwater mark is valid.
			NewsFolderInfo *newsGroupInfo = m_newsDB->GetNewsFolderInfo();
			if (newsGroupInfo)
			{
				ENeoString knownArtsString;
				newsGroupInfo->GetKnownArtsSet(knownArtsString);
				if (last_possible < newsGroupInfo->GetHighWater())
					newsGroupInfo->SetHighWater(last_possible, TRUE);
				m_knownArts.set = msg_NewsArtSet::Create(knownArtsString);
			}
			else
			{
				m_knownArts.set = msg_NewsArtSet::Create();
				m_knownArts.set->AddRange(m_newsDB->GetLowWaterArticleNum(), m_newsDB->GetHighwaterArticleNum());
			}
#ifdef HAVE_PANES
			m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
			m_newsDB->ExpireUpTo(first_possible, m_pane->GetContext());
			m_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
#endif
			if (m_knownArts.set->IsMember(last_possible))	// will this be progress pane?
			{
				char *noNewMsgs = XP_GetString(MK_NO_NEW_DISC_MSGS);
#ifdef HAVE_PANES
				MWContext *context = m_pane->GetContext();
				MSG_Pane* parentpane = m_pane->GetParentPane();
				// send progress to parent pane, if any, because progress pane is going down.
				if (parentpane)
					context = parentpane->GetContext();
				FE_Progress (context, noNewMsgs);
#endif
			}
		}
	}
#endif
    
	if (maxextra <= 0 || last_possible < first_possible || last_possible < 1) 
	{
		emptyGroup_p = TRUE;
	}

	if (m_knownArts.host != host ||
	  m_knownArts.group_name == NULL ||
	  XP_STRCMP(m_knownArts.group_name, group_name) != 0 ||
	  !m_knownArts.set) 
	{
	/* We're displaying some other group.  Clear out that display, and set up
	   everything to return the proper first chunk. */
    		XP_ASSERT(FALSE);	// ### dmb todo - need nwo way of doing this
		if (emptyGroup_p)
		  return 0;
	}
	else
	{
	if (emptyGroup_p)
	  return 0;
	}

	m_knownArts.first_possible = first_possible;
	m_knownArts.last_possible = last_possible;

	/* Determine if we only want to get just new articles or more messages.
	If there are new articles at the end we haven't seen, we always want to get those first.  
	Otherwise, we get the newest articles we haven't gotten, if we're getting more. 
	My thought for now is that opening a newsgroup should only try to get new articles.
	Selecting "More Messages" will first try to get unseen messages, then old messages. */

	if (m_getOldMessages || !m_knownArts.set->IsMember(last_possible)) 
	{
#ifdef HAVE_PANES
		XP_Bool notifyMaxExceededOn = (m_pane && !m_finishingXover && m_pane->GetPrefs() && m_pane->GetPrefs()->GetNewsNotifyOn());
#else
        XP_Bool notifyMaxExceededOn = FALSE;
#endif
		// if the preference to notify when downloading more than x headers is not on,
		// and we're downloading new headers, set maxextra to a very large number.
		if (!m_getOldMessages && !notifyMaxExceededOn)
			maxextra = 0x7FFFFFFFL;

		status = m_knownArts.set->LastMissingRange(first_possible, last_possible,
								  first, last);
		if (status < 0) 
			return status;
		if (*first > 0 && *last - *first >= maxextra) 
		{
			if (!m_getOldMessages && !m_promptedAlready && notifyMaxExceededOn)
			{
#ifdef HAVE_PANES
				MSG_FolderInfoNews *newsFolder = m_pane->GetMaster()->FindNewsFolder(m_host, m_groupName, FALSE);
				XP_Bool result = FE_NewsDownloadPrompt(m_pane->GetContext(),
													*last - *first + 1,
													&m_downloadAll, newsFolder);
#endif
				if (result)
				{
					m_maxArticles = 0;

					PREF_GetIntPref("news.max_articles", &m_maxArticles);
					NET_SetNumberOfNewsArticlesInListing(m_maxArticles);
					maxextra = m_maxArticles;
					if (!m_downloadAll)
					{
						XP_Bool markOldRead = FALSE;
						PREF_GetBoolPref("news.mark_old_read", &markOldRead);
						if (markOldRead && m_set)
							m_set->AddRange(*first, *last - maxextra); 
						*first = *last - maxextra + 1;
					}
				}
				else
					*first = *last = 0;
				m_promptedAlready = TRUE;
			}
			else if (m_promptedAlready && !m_downloadAll)
				*first = *last - m_maxArticles + 1;
			else if (!m_downloadAll)
				*first = *last - maxextra + 1;
		}
	}
#ifdef DEBUG_bienvenu
	XP_Trace("GetRangeOfArtsToDownload(first possible = %ld, last possible = %ld, first = %ld, last = %ld maxextra = %ld\n",
			first_possible, last_possible, *first, *last, maxextra);
#endif
	m_firstMsgToDownload = *first;
	m_lastMsgToDownload = *last;
	return 0;
}

int nsNNTPNewsgroupList::AddToKnownArticles(MSG_NewsHost* host,
					   const char* group_name,
					   int32 first, int32 last)
{
	int		status;
	if (m_knownArts.host != host ||
	  m_knownArts.group_name == NULL ||
	  XP_STRCMP(m_knownArts.group_name, group_name) != 0 ||
	  !m_knownArts.set) 
	{
		m_knownArts.host = host;
		FREEIF(m_knownArts.group_name);
		m_knownArts.group_name = XP_STRDUP(group_name);
		delete m_knownArts.set;
		m_knownArts.set = msg_NewsArtSet::Create();

		if (!m_knownArts.group_name || !m_knownArts.set) {
		  return MK_OUT_OF_MEMORY;
		}

	}

	status = m_knownArts.set->AddRange(first, last);
#ifdef HAVE_NEWSDB
	if (m_newsDB)
	{
		NeoDBStack dbStack(m_newsDB->GetDB());

		NewsFolderInfo *newsGroupInfo = m_newsDB->GetNewsFolderInfo();
		if (newsGroupInfo)
		{
			char *output = m_knownArts.set->Output();
			if (output)
				newsGroupInfo->SetKnownArtsSet(output);
			delete[] output;
		}
	}
#endif
	return status;
}




nsIM

nsresult
nsNNTPNewsgroupList::InitXOVER(MSG_NewsHost* /*host*/,
			const char * /*group_name*/,
			uint32 first_msg, uint32 last_msg,
			uint32 /*oldest_msg*/, uint32 /*youngest_msg*/)
{
    
	int		status = 0;

	// Tell the FE to show the GetNewMessages progress dialog
#ifdef HAVE_PANES
	FE_PaneChanged (m_pane, FALSE, MSG_PanePastPasswordCheck, 0);
#endif
	/* Consistency checks, not that I know what to do if it fails (it will
	 probably handle it OK...) */
	XP_ASSERT(first_msg <= last_msg);

	/* If any XOVER lines from the last time failed to come in, mark those
	   messages as read. */
	if (m_lastProcessedNumber < m_lastMsgNumber) 
	{
		m_set->AddRange(m_lastProcessedNumber + 1, m_lastMsgNumber);
	}
	m_firstMsgNumber = first_msg;
	m_lastMsgNumber = last_msg;
	m_lastProcessedNumber = first_msg > 1 ? first_msg - 1 : 1;

	return status;
}

#define NEWS_ART_DISPLAY_FREQ		10

nsresult
nsNNTPNewsgroupList::ProcessXOVER(char *line)
{
	int status = 0;
	char *next;
	uint32 message_number;
	//  int32 lines;
	XP_Bool read_p = FALSE;

	XP_ASSERT (line);
	if (!line)
		return -1;
#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
	{
		status = ConvertMsgErrToMKErr( m_msgDBView->AddHdrFromServerLine(line, &message_number));
		if (status < 0)
		{
#ifdef HAVE_PANES
			if (status == MK_DISK_FULL || status == MK_OUT_OF_MEMORY)
				FE_Alert(m_pane->GetContext(), XP_GetString(status));
#endif
			return status;
		}
	}
	else 
#endif
#ifdef HAVE_NEWSDB
    if (m_newsDB != NULL)
	{
		status = ConvertMsgErrToMKErr(m_newsDB->AddHdrFromXOver(line, &message_number));
	}
	else
		return -1;
#endif

	next = line;


	XP_ASSERT(message_number > m_lastProcessedNumber ||
			message_number == 1);
	if (m_set && message_number > m_lastProcessedNumber + 1)
	{
		/* There are some articles that XOVER skipped; they must no longer
		   exist.  Mark them as read in the newsrc, so we don't include them
		   next time in our estimated number of unread messages. */
		if (m_set->AddRange(m_lastProcessedNumber + 1, message_number - 1)) 
		{
		  /* This isn't really an important enough change to warrant causing
			 the newsrc file to be saved; we haven't gathered any information
			 that won't also be gathered for free next time.
		   */
		}
	}

	m_lastProcessedNumber = message_number;
	if (m_knownArts.set) 
	{
		status = m_knownArts.set->Add(message_number);
		if (status < 0) return status;
	}

	if (message_number > m_lastMsgNumber)
	m_lastMsgNumber = message_number;
	else if (message_number < m_firstMsgNumber)
	m_firstMsgNumber = message_number;

	if (m_set) {
		read_p = m_set->IsMember(message_number);
	}

	/* Update the thermometer with a percentage of articles retrieved.
	*/
	if (m_lastMsgNumber > m_firstMsgNumber)
	{
		int32	totToDownload = m_lastMsgToDownload - m_firstMsgToDownload;
		int32	lastIndex = m_lastProcessedNumber - m_firstMsgNumber + 1;
		int32	numDownloaded = lastIndex;
		int32	totIndex = m_lastMsgNumber - m_firstMsgNumber + 1;
		int32 	percent = (totIndex) ? (int32)(100.0 * (double)numDownloaded / (double)totToDownload) : 0;

#ifdef HAVE_PANES
		FE_SetProgressBarPercent (m_pane->GetContext(), percent);
#endif
		
		/* only update every 10 articles for speed */
		if ( (totIndex <= NEWS_ART_DISPLAY_FREQ) || ((lastIndex % NEWS_ART_DISPLAY_FREQ) == 0) || (lastIndex == totIndex))
		{
			char *statusTemplate = XP_GetString (MK_HDR_DOWNLOAD_COUNT);
			char *statusString = PR_smprintf (statusTemplate, numDownloaded, totToDownload);
#ifdef HAVE_PANES
			FE_Progress (m_pane->GetContext(), statusString);
#endif
			XP_FREE(statusString);
		}
	}
	return status;
}

nsresult
nsNNTPNewsgroupList::XOVERReset()
{
	m_lastMsgNumber = m_firstMsgNumber;
	m_lastProcessedNumber = m_lastMsgNumber;
	return 0;
}

/* When we don't have XOVER, but use HEAD, this is called instead.
   It reads lines until it has a whole header block, then parses the
   headers; then takes selected headers and creates an XOVER line
   from them.  This is more for simplicity and code sharing than
   anything else; it means we end up parsing some things twice.
   But if we don't have XOVER, things are going to be so horribly
   slow anyway that this just doesn't matter.
 */

nsresult
nsNNTPNewsgroupList::ProcessNonXOVER (char * /*line*/)
{
	// ### dmb write me
  int status = 0;
  return status;
}


nsresult
nsNNTPNewsgroupList::FinishXOVER (int status)
{
	struct MSG_NewsKnown* k;

	/* If any XOVER lines from the last time failed to come in, mark those
	 messages as read. */

	if (status >= 0 && m_lastProcessedNumber < m_lastMsgNumber) 
	{
		m_set->AddRange(m_lastProcessedNumber + 1, m_lastMsgNumber);
	}

#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
	{
		// we should think about not doing this if status != MK_INTERRUPTED, but we'd need to still clean up the view.
		m_msgDBView->AddNewMessages();
		m_msgDBView->Remove(this);
		m_msgDBView = NULL;
	}
#endif
#ifdef HAVE_NEWSDB
	if (m_newsDB)
	{
		m_newsDB->Close();
		m_newsDB = NULL;
	}
#endif

	k = &m_knownArts;

	if (k->set) 
	{
		int32 n = k->set->FirstNonMember();
		if (n < k->first_possible || n > k->last_possible) 
		{
		  /* We know we've gotten all there is to know.  Take advantage of that to
			 update our counts... */
			// ### dmb
		}
	}

    if (m_finishingXover)
	{
		// turn on m_finishingXover - this is a horrible hack to avoid recursive 
		// calls which happen when the fe selects a message as a result of getting EndingUpdate,
		// which interrupts this url right before it was going to finish and causes FinishXOver
		// to get called again.
        m_finishingXover = TRUE;
		// if we haven't started an update, start one so the fe
		// will know to update the size of the view.
		if (!m_startedUpdate)
		{
#ifdef HAVE_PANES
			m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
#endif
			m_startedUpdate = TRUE;
		}
#ifdef HAVE_PANES
		m_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
#endif
		m_startedUpdate = FALSE;

		if (m_lastMsgNumber > 0)
		{
#ifdef HAVE_PANES
			MWContext *context = m_pane->GetContext();
			MSG_Pane* parentpane = m_pane->GetParentPane();
			// send progress to parent pane, if any, because progress pane is going down.
			if (parentpane)
				context = parentpane->GetContext();

			char *statusTemplate = XP_GetString (MK_HDR_DOWNLOAD_COUNT);
			char *statusString = PR_smprintf (statusTemplate,  m_lastProcessedNumber - m_firstMsgNumber + 1, m_lastMsgNumber - m_firstMsgNumber + 1);

			if (statusString)
			{
				FE_Progress (context, statusString);
				XP_FREE(statusString);
			}
#endif
		}
		MSG_FolderInfoNews *newsFolder = NULL;
#ifdef HAVE_PANES
		newsFolder = (m_pane) ? savePane->GetMaster()->FindNewsFolder(m_host, m_groupName, FALSE) : 0;
		FE_PaneChanged(m_pane, FALSE, MSG_PaneNotifyFolderLoaded, (uint32)newsFolder);
#endif
	}
	return 0;
	// nsNNTPNewsgroupList object gets deleted by the master when a new one is created.
}

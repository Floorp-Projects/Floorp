/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"    // precompiled header...
#include "MailNewsTypes.h"

//#define HAVE_NEWSDB

#ifdef HAVE_PANES
class MSG_Master;
#endif
#ifdef HAVE_NEWSDB
class nsNewsDatabase;
#endif
#ifdef HAVE_DBVIEW
class MessageDBView;
#endif

// we have a HAVE_XPGETSTRING in this file which needs fixed once we have XP_GetString


#include "nsNNTPNewsgroupList.h"
#include "nsINNTPArticleList.h"
#include "nsNNTPArticleSet.h"

#include "msgCore.h"

#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#ifdef HAVE_NEWSDB
#include "nsNewsDatabase.h"
#include "nsDBFolderInfo.h"
#endif

#ifdef HAVE_DBVIEW
#include "msgdbvw.h"
#endif


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

extern PRInt32 net_NewsChunkSize;

// This class should ultimately be part of a news group listing
// state machine - either by inheritance or delegation.
// Currently, a folder pane owns one and libnet news group listing
// related messages get passed to this object.
class nsNNTPNewsgroupList : public nsINNTPNewsgroupList
#ifdef HAVE_CHANGELISTENER
/* ,public ChangeListener */
#endif
{
public:
  nsNNTPNewsgroupList(nsINNTPHost *, nsINNTPNewsgroup*);
  nsNNTPNewsgroupList();
  virtual  ~nsNNTPNewsgroupList();
  NS_DECL_ISUPPORTS

    
  NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 first_possible,
                                      PRInt32 last_possible,
                                      PRInt32 maxextra,
                                      PRInt32* first,
                                      PRInt32* lastprotected,
                                      PRInt32 *status);
  NS_IMETHOD AddToKnownArticles(PRInt32 first, PRInt32 last);

  // XOVER parser to populate this class
  NS_IMETHOD InitXOVER(PRInt32 first_msg, PRInt32 last_msg);
  NS_IMETHOD ProcessXOVER(const char *line, PRUint32 * status);
  NS_IMETHOD ResetXOVER();
  NS_IMETHOD ProcessNonXOVER(const char *line);
  NS_IMETHOD FinishXOVER(int status, int *newstatus);
  NS_IMETHOD ClearXOVERState();

    
private:
  NS_METHOD Init(nsINNTPHost *, nsINNTPNewsgroup*) { return NS_OK;}
  NS_METHOD InitNewsgroupList(const char *url, const char *groupName);

  NS_METHOD CleanUp();
    
#ifdef HAVE_MASTER
  MSG_Master		*GetMaster() {return m_master;}
  void			SetMaster(MSG_Master *master) {m_master = master;}
#endif
  
#ifdef HAVE_DBVIEW
  void			SetView(MessageDBView *view);
#endif
#ifdef HAVE_PANES
  void			SetPane(MSG_Pane *pane) {m_pane = pane;}
#endif
  PRBool          m_finishingXover;
  nsINNTPHost*	GetHost() {return m_host;}
  const char *	GetGroupName() {return m_groupName;}
  const char *	GetURL() {return m_url;}
  
#ifdef HAVE_CHANGELISTENER
  virtual void	OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
#endif
  void			SetGetOldMessages(PRBool getOldMessages) {m_getOldMessages = getOldMessages;}
  PRBool			GetGetOldMessages() {return m_getOldMessages;}
  
protected:
#ifdef HAVE_NEWSDB
  nsNewsDatabase	*m_newsDB;
#endif
#ifdef HAVE_DBVIEW
  MessageDBView	*m_msgDBView;		// open view on current download, if any
#endif
#ifdef HAVE_PANES
  MSG_Pane		*m_pane;
#endif
  PRBool			m_startedUpdate;
  PRBool			m_getOldMessages;
  PRBool			m_promptedAlready;
  PRBool			m_downloadAll;
  PRInt32			m_maxArticles;
  char			*m_groupName;
  nsINNTPHost	*m_host;
  char			*m_url;			// url we're retrieving
#ifdef HAVE_MASTER
  MSG_Master		*m_master;
#endif
  
  nsMsgKey		m_lastProcessedNumber;
  nsMsgKey		m_firstMsgNumber;
  nsMsgKey		m_lastMsgNumber;
  PRInt32			m_firstMsgToDownload;
  PRInt32			m_lastMsgToDownload;
  
  struct MSG_NewsKnown	m_knownArts;
  nsNNTPArticleSet	*m_set;

};



nsNNTPNewsgroupList::nsNNTPNewsgroupList(nsINNTPHost* host,
                                         nsINNTPNewsgroup *newsgroup)
{
    NS_INIT_REFCNT();
    Init(host, newsgroup);
}


nsNNTPNewsgroupList::~nsNNTPNewsgroupList()
{
}

NS_IMPL_ISUPPORTS(nsNNTPNewsgroupList, nsINNTPNewsgroupList::GetIID());

nsresult
nsNNTPNewsgroupList::InitNewsgroupList(const char *url, const char *groupName)
#ifdef HAVE_PANES
, MSG_Pane *pane);
#endif
{
#ifdef HAVE_NEWSDB
	m_newsDB = NULL;
#endif
#ifdef HAVE_DBVIEW
	m_msgDBView = NULL;
#endif
	m_groupName = PL_strdup(groupName);
	m_host = NULL;
	m_url = PL_strdup(url);
	m_lastProcessedNumber = 0;
	m_lastMsgNumber = 0;
	m_set = NULL;
#ifdef HAVE_PANES
	PR_ASSERT(pane);
	m_pane = pane;
	m_master = pane->GetMaster();
#endif
    m_finishingXover = PR_FALSE;

	m_startedUpdate = PR_FALSE;
	memset(&m_knownArts, 0, sizeof(m_knownArts));
	m_knownArts.group_name = m_groupName;
#ifdef HAVE_URLPARSER
	char* host_and_port = NET_ParseURL(url, GET_HOST_PART);
#ifdef HAVE_MASTER
	m_host = m_master->FindHost(host_and_port,
								(url[0] == 's' || url[0] == 'S'),
								-1);
#endif
    PR_FREEIF(host_and_port);
#endif
	m_knownArts.host = m_host;
	m_getOldMessages = PR_FALSE;
	m_promptedAlready = PR_FALSE;
	m_downloadAll = PR_FALSE;
	m_maxArticles = 0;
	m_firstMsgToDownload = 0;
	m_lastMsgToDownload = 0;

    return NS_MSG_SUCCESS;
}

nsresult
nsNNTPNewsgroupList::CleanUp() {
	PR_Free(m_url);
	PR_Free(m_groupName);
    
#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
		m_msgDBView->Remove(this);
#endif

#ifdef HAVE_NEWSDB
	if (m_newsDB)
		m_newsDB->Close();
#endif
	delete m_knownArts.set;
    
    return NS_OK;
}

#ifdef HAVE_DBVIEW
void nsNNTPNewsgroupList::SetView(MessageDBView *view) 
{
	m_msgDBView = view;
	if (view)
		view->Add(this);
}
#endif

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

nsresult
nsNNTPNewsgroupList::GetRangeOfArtsToDownload(
                                              /*nsINNTPHost* host,
                                                const char* group_name,*/
                                              PRInt32 first_possible,
                                              PRInt32 last_possible,
                                              PRInt32 maxextra,
                                              PRInt32* first,
                                              PRInt32* last,
                                              PRInt32 *status)
{
	PRBool emptyGroup_p = PR_FALSE;

	PR_ASSERT(first && last);
	if (!first || !last) return NS_MSG_FAILURE;

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
		nsresult err;
		if ((err = nsNewsDatabase::Open(m_url, NULL/* m_master */, &m_newsDB)) != NS_OK)
            {
                return err;
            }
		else
		{
			m_set = m_newsDB->GetNewsArtSet();
			m_set->SetLastMember(last_possible);	// make sure highwater mark is valid.
			nsDBFolderInfo *newsGroupInfo = m_newsDB->GetDBFolderInfo();
			if (newsGroupInfo)
			{
				nsString knownArtsString;
				newsGroupInfo->GetKnownArtsSet(knownArtsString);
				if (last_possible < newsGroupInfo->GetHighWater())
					newsGroupInfo->SetHighWater(last_possible, TRUE);
				m_knownArts.set = nsNNTPArticleSet::Create(knownArtsString);
			}
			else
			{
				m_knownArts.set = nsNNTPArticleSet::Create();
				m_knownArts.set->AddRange(m_newsDB->GetLowWaterArticleNum(), m_newsDB->GetHighwaterArticleNum());
			}
#ifdef HAVE_PANES
			m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
			m_newsDB->ExpireUpTo(first_possible, m_pane->GetContext());
			m_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
#endif
			if (m_knownArts.set->IsMember(last_possible))	// will this be progress pane?
			{
#ifdef HAVE_PANES
				char *noNewMsgs = XP_GetString(MK_NO_NEW_DISC_MSGS);
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

    // this is just a temporary hack. these used to be parameters
    // to this function, but then we were mutually dependant between this
    // class and nsNNTPHost
    nsINNTPHost *host=m_knownArts.host;
    const char* group_name = m_knownArts.group_name;
    if (m_knownArts.host != host ||
	  m_knownArts.group_name == NULL ||
	  PL_strcmp(m_knownArts.group_name, group_name) != 0 ||
	  !m_knownArts.set) 
	{
	/* We're displaying some other group.  Clear out that display, and set up
	   everything to return the proper first chunk. */
    		PR_ASSERT(PR_FALSE);	// ### dmb todo - need nwo way of doing this
            if (emptyGroup_p) {
                if (status) *status=0;
                return NS_OK;
            }
	}
	else
	{
        if (emptyGroup_p) {
            if (status) *status=0;
            return NS_OK;
        }
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
		PRBool notifyMaxExceededOn = (m_pane && !m_finishingXover && m_pane->GetPrefs() && m_pane->GetPrefs()->GetNewsNotifyOn());
#else
        PRBool notifyMaxExceededOn = PR_FALSE;
#endif
		// if the preference to notify when downloading more than x headers is not on,
		// and we're downloading new headers, set maxextra to a very large number.
		if (!m_getOldMessages && !notifyMaxExceededOn)
			maxextra = 0x7FFFFFFFL;
        int result =
            m_knownArts.set->LastMissingRange(first_possible, last_possible,
                                              first, last);
		if (result < 0) {
            if (status) *status=result;
			return NS_ERROR_NOT_INITIALIZED;
        }
		if (*first > 0 && *last - *first >= maxextra) 
		{
			if (!m_getOldMessages && !m_promptedAlready && notifyMaxExceededOn)
			{
#ifdef HAVE_PANES
				nsINNTPNewsgroup *newsFolder = m_pane->GetMaster()->FindNewsFolder(m_host, m_groupName, PR_FALSE);
				PRBool result = FE_NewsDownloadPrompt(m_pane->GetContext(),
													*last - *first + 1,
													&m_downloadAll, newsFolder);
#else
                PRBool result = PR_FALSE;
#endif
				if (result)
				{
					m_maxArticles = 0;

#ifdef HAVE_PREFS
					PREF_GetIntPref("news.max_articles", &m_maxArticles);
#endif
                    net_NewsChunkSize = m_maxArticles;
					maxextra = m_maxArticles;
					if (!m_downloadAll)
					{
						PRBool markOldRead = PR_FALSE;
#ifdef HAVE_PREFS
						PREF_GetBoolPref("news.mark_old_read", &markOldRead);
#endif
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
	PR_LogPrint("GetRangeOfArtsToDownload(first possible = %ld, last possible = %ld, first = %ld, last = %ld maxextra = %ld\n",
			first_possible, last_possible, *first, *last, maxextra);
#endif
	m_firstMsgToDownload = *first;
	m_lastMsgToDownload = *last;
    if (status) *status=0;
	return NS_OK;
}

nsresult
nsNNTPNewsgroupList::AddToKnownArticles(PRInt32 first, PRInt32 last)
{
	int		status;
    // another temporary hack
    nsINNTPHost *host = m_knownArts.host;
    const char* group_name = m_knownArts.group_name;
    
	if (m_knownArts.host != host ||
	  m_knownArts.group_name == NULL ||
	  PL_strcmp(m_knownArts.group_name, group_name) != 0 ||
	  !m_knownArts.set) 
	{
		m_knownArts.host = host;
		PR_FREEIF(m_knownArts.group_name);
		m_knownArts.group_name = PL_strdup(group_name);
		delete m_knownArts.set;
		m_knownArts.set = nsNNTPArticleSet::Create();

		if (!m_knownArts.group_name || !m_knownArts.set) {
		  return MK_OUT_OF_MEMORY;
		}

	}

	status = m_knownArts.set->AddRange(first, last);
#ifdef HAVE_NEWSDB
	if (m_newsDB)
	{

		nsDBFolderInfo *newsGroupInfo = m_newsDB->GetDBFolderInfo();
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




nsresult
nsNNTPNewsgroupList::InitXOVER(PRInt32 first_msg, PRInt32 last_msg)
{
    
	int		status = 0;

	// Tell the FE to show the GetNewMessages progress dialog
#ifdef HAVE_PANES
	FE_PaneChanged (m_pane, PR_FALSE, MSG_PanePastPasswordCheck, 0);
#endif
	/* Consistency checks, not that I know what to do if it fails (it will
	 probably handle it OK...) */
	PR_ASSERT(first_msg <= last_msg);

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
nsNNTPNewsgroupList::ProcessXOVER(const char *line, PRUint32 *status)
{
	const char *next;
	PRUint32 message_number=0;
	//  PRInt32 lines;
	PRBool read_p = PR_FALSE;

	PR_ASSERT (line);
	if (!line)
      return NS_MSG_FAILURE;
    
#ifdef HAVE_DBVIEW
	if (m_msgDBView != NULL)
	{
        int result=
            ConvertMsgErrToMKErr( m_msgDBView->AddHdrFromServerLine(line, &message_number));
		if (result < 0)
		{
#ifdef HAVE_PANES
			if (result == MK_DISK_FULL || result == MK_OUT_OF_MEMORY)
				FE_Alert(m_pane->GetContext(), XP_GetString(result));
#endif
            if (status) *status=result;
            return NS_ERROR_NOT_INITIALIZED;
		}
	}
	else 
#endif
#ifdef HAVE_NEWSDB
    if (m_newsDB != NULL)
	{
		int result = ConvertMsgErrToMKErr(m_newsDB->AddHdrFromXOver(line, &message_number));
        if (status) *status=result;
	}
	else
		return NS_ERROR_NOT_INITIALIZED;
#endif

	next = line;


	PR_ASSERT(message_number > m_lastProcessedNumber ||
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
		int result = m_knownArts.set->Add(message_number);
		if (result < 0) {
            if (status) *status = result;
            return NS_ERROR_NOT_INITIALIZED;
        }
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
		PRInt32	totToDownload = m_lastMsgToDownload - m_firstMsgToDownload;
		PRInt32	lastIndex = m_lastProcessedNumber - m_firstMsgNumber + 1;
		PRInt32	numDownloaded = lastIndex;
		PRInt32	totIndex = m_lastMsgNumber - m_firstMsgNumber + 1;

#ifdef HAVE_PANES
		PRInt32 	percent = (totIndex) ? (PRInt32)(100.0 * (double)numDownloaded / (double)totToDownload) : 0;
		FE_SetProgressBarPercent (m_pane->GetContext(), percent);
#endif
		
		/* only update every 10 articles for speed */
		if ( (totIndex <= NEWS_ART_DISPLAY_FREQ) || ((lastIndex % NEWS_ART_DISPLAY_FREQ) == 0) || (lastIndex == totIndex))
		{
#ifdef HAVE_XPGETSTRING
			char *statusTemplate = XP_GetString (MK_HDR_DOWNLOAD_COUNT);
#else
			char *statusTemplate = "XP_GetString not implemented in nsNNTPNewsgroupList.";
#endif
			char *statusString = PR_smprintf (statusTemplate, numDownloaded, totToDownload);
#ifdef HAVE_PANES
			FE_Progress (m_pane->GetContext(), statusString);
#endif
			PR_Free(statusString);
		}
	}
    
	return NS_OK;
}

nsresult
nsNNTPNewsgroupList::ResetXOVER()
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
nsNNTPNewsgroupList::ProcessNonXOVER (const char * /*line*/)
{
	// ### dmb write me
    return NS_OK;
}


nsresult
nsNNTPNewsgroupList::FinishXOVER (int status, int *newstatus)
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
		PRInt32 n = k->set->FirstNonMember();
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
		m_startedUpdate = PR_FALSE;

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
				PR_Free(statusString);
			}
#endif
		}
#ifdef HAVE_PANES
		nsINNTPNewsgroup *newsFolder =
            (m_pane) ?
            savePane->GetMaster()->FindNewsFolder(m_host, m_groupName, PR_FALSE) :
            0;
		FE_PaneChanged(m_pane, PR_FALSE, MSG_PaneNotifyFolderLoaded, (PRUint32)newsFolder);
#endif
	}
    if (newstatus) *newstatus=0;
    return NS_OK;
	// nsNNTPNewsgroupList object gets deleted by the master when a new one is created.
}

// this used to be in the master:
// void MSG_Master::ClearListNewsGroupState(MSG_NewsHost* host,
//                                          const char *newsGroupName)
//   {
//     MSG_FolderInfoNews *newsFolder = FindNewsFolder(host, newsGroupName);
//     ListNewsGroupState *state = (newsFolder) ? newsFolder->GetListNewsGroupState
//     if (state != NULL)
//     {
//         delete state;
//         newsFolder->SetListNewsGroupState(NULL);
//     }
// }

nsresult
nsNNTPNewsgroupList::ClearXOVERState()
{
    return NS_OK;
}


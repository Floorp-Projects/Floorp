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
#include "nsCOMPtr.h"
#include "nsIDBFolderInfo.h"
#include "nsINewsDatabase.h"
#include "nsIMsgStatusFeedback.h"

#ifdef HAVE_PANES
class MSG_Master;
#endif

// we have a HAVE_XPGETSTRING in this file which needs fixed once we have XP_GetString

#include "nsNNTPNewsgroupList.h"

#include "nsINNTPArticleList.h"
#include "nsMsgKeySet.h"

#include "nsINNTPNewsgroup.h"
#include "nsNNTPNewsgroup.h"

#include "nsINNTPHost.h"
#include "nsNNTPHost.h"

#include "msgCore.h"

#include "plstr.h"
#include "prmem.h"
#include "prprf.h"

#include "nsCRT.h"
#include "xp_mcom.h"

#include "nsMsgDatabase.h"

#include "nsIDBFolderInfo.h"

#ifdef HAVE_PANES
#include "msgpane.h"
#endif

#include "nsNewsUtils.h"

#include "nsMsgDBCID.h"

#include "nsIPref.h"

#define PREF_NEWS_MAX_ARTICLES "news.max_articles"
#define PREF_NEWS_MARK_OLD_READ "news.mark_old_read"

static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID);

extern PRInt32 net_NewsChunkSize;

nsNNTPNewsgroupList::nsNNTPNewsgroupList()
{
    NS_INIT_REFCNT();
}


nsNNTPNewsgroupList::~nsNNTPNewsgroupList()
{
}

NS_IMPL_ISUPPORTS(nsNNTPNewsgroupList, nsINNTPNewsgroupList::GetIID());


nsresult
nsNNTPNewsgroupList::Initialize(nsINNTPHost *host, nsINntpUrl *runningURL, nsINNTPNewsgroup *newsgroup, const char *username, const char *hostname, const char *groupname)
{
	m_newsDB = nsnull;
	m_groupName = PL_strdup(groupname);
        
        if (username) {
		m_uri = PR_smprintf("%s/%s@%s/%s",kNewsRootURI,username,hostname,groupname);
	}
        else {
		m_uri = PR_smprintf("%s/%s/%s",kNewsRootURI,hostname,groupname);
	}
	m_lastProcessedNumber = 0;
	m_lastMsgNumber = 0;
	m_set = nsnull;
#ifdef HAVE_PANES
	PR_ASSERT(pane);
	m_pane = pane;
	m_master = pane->GetMaster();
#endif
    m_finishingXover = PR_FALSE;

	m_startedUpdate = PR_FALSE;
	memset(&m_knownArts, 0, sizeof(m_knownArts));
	m_knownArts.group_name = m_groupName;
	m_host = host;
	m_newsgroup = newsgroup;
	m_knownArts.host = m_host;
	m_knownArts.set = nsMsgKeySet::Create();
	m_getOldMessages = PR_FALSE;
	m_promptedAlready = PR_FALSE;
	m_downloadAll = PR_FALSE;
	m_maxArticles = 0;
	m_firstMsgToDownload = 0;
	m_lastMsgToDownload = 0;
	m_runningURL=runningURL;
    return NS_OK;
}

nsresult
nsNNTPNewsgroupList::CleanUp() {
	PR_Free(m_uri);
	PR_Free(m_groupName);
    
	if (m_newsDB) {
		m_newsDB->Commit(nsMsgDBCommitType::kSessionCommit);
		m_newsDB->Close(PR_TRUE);
	}

	if (m_knownArts.set) {
		delete m_knownArts.set;
		m_knownArts.set = nsnull;
	}
    
    return NS_OK;
}

#ifdef HAVE_CHANGELISTENER
void	nsNNTPNewsgroupList::OnAnnouncerGoingAway (ChangeAnnouncer *instigator)
{
}
#endif

nsresult 
nsNNTPNewsgroupList::GetDatabase(const char *uri, nsIMsgDatabase **db)
{
    if (*db == nsnull) {
        nsFileSpec path;
		nsresult rv = nsNewsURI2Path(kNewsRootURI, uri, path);
		if (NS_FAILED(rv)) return rv;

        nsresult newsDBOpen = NS_OK;
        nsCOMPtr <nsIMsgDatabase> newsDBFactory;

        rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, nsIMsgDatabase::GetIID(), getter_AddRefs(newsDBFactory));
        if (NS_SUCCEEDED(rv) && newsDBFactory) {
				nsCOMPtr <nsIFileSpec> dbFileSpec;
				NS_NewFileSpecWithSpec(path, getter_AddRefs(dbFileSpec));
                newsDBOpen = newsDBFactory->Open(dbFileSpec, PR_TRUE, PR_FALSE, (nsIMsgDatabase **) db);
#ifdef DEBUG_NEWS
                if (NS_SUCCEEDED(newsDBOpen)) {
                    printf ("newsDBFactory->Open() succeeded\n");
                }
                else {
                    printf ("newsDBFactory->Open() failed\n");
                }
#endif /* DEBUG_NEWS */
                return rv;
        }
#ifdef DEBUG_NEWS
        else {
            printf("nsComponentManager::CreateInstance(kCNewsDB,...) failed\n");
        }
#endif
    }
    
    return NS_OK;
}

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
		m_startedUpdate = PR_TRUE;
		m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
	}
#endif

	if (!m_newsDB)
	{
		nsresult err;
		if ((err = GetDatabase(GetURI(), &m_newsDB)) != NS_OK) {
            return err;
        }
		else {
			nsresult rv = NS_OK;

            nsCOMPtr<nsINewsDatabase> db(do_QueryInterface(m_newsDB, &rv));
            if (NS_FAILED(rv)) return rv;
            
	    rv = db->GetUnreadSet(&m_set);
            if (NS_FAILED(rv) || !m_set) {
                return rv;
            }
            
			m_set->SetLastMember(last_possible);	// make sure highwater mark is valid.
			nsCOMPtr <nsIDBFolderInfo> newsGroupInfo;
			rv = m_newsDB->GetDBFolderInfo(getter_AddRefs(newsGroupInfo));
			if (NS_SUCCEEDED(rv) && newsGroupInfo)
			{
				nsAutoString knownArtsString;
                nsMsgKey mark;
				newsGroupInfo->GetKnownArtsSet(&knownArtsString);
                
                rv = newsGroupInfo->GetHighWater(&mark);
                if (NS_FAILED(rv)) {
                    return rv;
                }
				if (last_possible < ((PRInt32)mark))
					newsGroupInfo->SetHighWater(last_possible, PR_TRUE);
				if (m_knownArts.set) {
					delete m_knownArts.set;
				}
				m_knownArts.set = nsMsgKeySet::Create(nsCAutoString(knownArtsString));
			}
			else
			{	
				if (m_knownArts.set) {
					delete m_knownArts.set;
				}
				m_knownArts.set = nsMsgKeySet::Create();
                nsMsgKey low, high;
                rv = m_newsDB->GetLowWaterArticleNum(&low);
                if (NS_FAILED(rv)) return rv;
                rv = m_newsDB->GetHighWaterArticleNum(&high);
                if (NS_FAILED(rv)) return rv;
                
				m_knownArts.set->AddRange(low,high);
            }
#ifdef HAVE_PANES
			m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
			m_newsDB->ExpireUpTo(first_possible, m_pane->GetContext());
			m_pane->EndingUpdate(MSG_NotifyNone, 0, 0);
#endif /* HAVE_PANES */
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
#else
				SetProgressStatus("There are no new messages on the server.");
#endif /* HAVE_PANES */
			}
		}
	}
    
	if (maxextra <= 0 || last_possible < first_possible || last_possible < 1) 
	{
		emptyGroup_p = PR_TRUE;
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
        nsresult rv = NS_OK;
        NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
        if (NS_FAILED(rv) || (!prefs)) {
            return rv;
        }    
#ifdef HAVE_PANES
		PRBool notifyMaxExceededOn = (m_pane && !m_finishingXover && m_pane->GetPrefs() && m_pane->GetPrefs()->GetNewsNotifyOn());
#else
        PRBool notifyMaxExceededOn = PR_TRUE;   // check prefs GetPrefs()->GetNewsNotifyOn
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
                PRBool download = PR_TRUE;  // yes, I'd like some news messages
#ifdef HAVE_PANES
				nsINNTPNewsgroup *newsFolder = m_pane->GetMaster()->FindNewsFolder(m_host, m_groupName, PR_FALSE);
				download = FE_NewsDownloadPrompt(m_pane->GetContext(),
													*last - *first + 1,
													&m_downloadAll, newsFolder);
#endif
				if (download)
				{
					m_maxArticles = 0;

                    rv = prefs->GetIntPref(PREF_NEWS_MAX_ARTICLES, &m_maxArticles);
                    if (NS_FAILED(rv)) {
#ifdef DEBUG_NEWS
                        printf("get pref of PREF_NEWS_MAX_ARTICLES failed\n");
#endif
                        m_maxArticles = 0;
                    }
                    
                    net_NewsChunkSize = m_maxArticles;
					maxextra = m_maxArticles;
					if (!m_downloadAll)
					{
						PRBool markOldRead = PR_FALSE;

						rv = prefs->GetBoolPref(PREF_NEWS_MARK_OLD_READ, &markOldRead);
                        if (NS_FAILED(rv)) {
#ifdef DEBUG_NEWS
                            printf("get pref of PREF_NEWS_MARK_OLD_READ failed\n");
#endif                           
                        }

						if (markOldRead && m_set)
							m_set->AddRange(*first, *last - maxextra); 
						*first = *last - maxextra + 1;
					}
				}
				else
					*first = *last = 0;
				m_promptedAlready = PR_TRUE;
			}
			else if (m_promptedAlready && !m_downloadAll)
				*first = *last - m_maxArticles + 1;
			else if (!m_downloadAll)
				*first = *last - maxextra + 1;
		}
	}
#ifdef DEBUG_NEWS
	printf("GetRangeOfArtsToDownload(first possible = %d, last possible = %d, first = %d, last = %d maxextra = %d\n",first_possible, last_possible, *first, *last, maxextra);
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
		if (m_knownArts.set) {
			delete m_knownArts.set;
		}
		m_knownArts.set = nsMsgKeySet::Create();

		if (!m_knownArts.group_name || !m_knownArts.set) {
		  return NS_ERROR_OUT_OF_MEMORY;
		}

	}

	status = m_knownArts.set->AddRange(first, last);

	if (m_newsDB) {
		nsresult rv = NS_OK;
		nsCOMPtr <nsIDBFolderInfo> newsGroupInfo;
		rv = m_newsDB->GetDBFolderInfo(getter_AddRefs(newsGroupInfo));
		if (NS_SUCCEEDED(rv) && newsGroupInfo) {
			char *output = m_knownArts.set->Output();
			if (output) {
				nsString str(output);
				newsGroupInfo->SetKnownArtsSet(&str);
			}
			delete [] output;
			output = nsnull;
		}
	}

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

/* Given a string and a length, removes any "Re:" strings from the front.
   It also deals with that "Re[2]:" thing that some mailers do.

   Returns PR_TRUE if it made a change, PR_FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
PRBool 
nsNNTPNewsgroupList::msg_StripRE(const char **stringP, PRUint32 *lengthP)
{
  const char *s, *s_end;
  const char *last;
  PRUint32 L;
  PRBool result = PR_FALSE;

  if (!stringP) return PR_FALSE;
  s = *stringP;
  L = lengthP ? *lengthP : PL_strlen(s);
  
  s_end = s + L;
  last = s;

 AGAIN:

  while (s < s_end && XP_IS_SPACE(*s))
	s++;

  if (s < (s_end-2) &&
	  (s[0] == 'r' || s[0] == 'R') &&
	  (s[1] == 'e' || s[1] == 'E'))
	{
	  if (s[2] == ':')
		{
		  s = s+3;			/* Skip over "Re:" */
		  result = PR_TRUE;	/* Yes, we stripped it. */
		  goto AGAIN;		/* Skip whitespace and try again. */
		}
	  else if (s[2] == '[' || s[2] == '(')
		{
		  const char *s2 = s+3;		/* Skip over "Re[" */

		  /* Skip forward over digits after the "[". */
		  while (s2 < (s_end-2) && XP_IS_DIGIT(*s2))
			s2++;

		  /* Now ensure that the following thing is "]:"
			 Only if it is do we alter `s'.
		   */
		  if ((s2[0] == ']' || s2[0] == ')') && s2[1] == ':')
			{
			  s = s2+2;			/* Skip over "]:" */
			  result = PR_TRUE;	/* Yes, we stripped it. */
			  goto AGAIN;		/* Skip whitespace and try again. */
			}
		}
	}

  /* Decrease length by difference between current ptr and original ptr.
	 Then store the current ptr back into the caller. */
  if (lengthP) *lengthP -= (s - (*stringP));
  *stringP = s;

  return result;
}


nsresult
nsNNTPNewsgroupList::ParseLine(char *line, PRUint32 * message_number) 
{
	nsresult rv = NS_OK;
	nsCOMPtr <nsIMsgDBHdr> newMsgHdr;
    
	if (!line || !message_number) {
		return NS_ERROR_NULL_POINTER;
	}

	char *next = line;

#define GET_TOKEN()								\
  line = next;									\
  next = (line ? PL_strchr (line, '\t') : 0);	\
  if (next) *next++ = 0

	GET_TOKEN ();
#ifdef DEBUG_NEWS											/* message number */
	printf("message number = %ld\n", atol(line));
#endif
	*message_number = atol(line);
 
	if (atol(line) == 0)					/* bogus xover data */
	return NS_ERROR_UNEXPECTED;

	m_newsDB->CreateNewHdr(*message_number, getter_AddRefs(newMsgHdr));
  	if (NS_FAILED(rv)) return rv;           

	GET_TOKEN (); /* subject */
	if (line)

	{
		const char *subject = line;  /* #### const evilness */
		PRUint32 subjectLen = nsCRT::strlen(line);

		/* strip "Re: " */
		if (msg_StripRE(&subject, &subjectLen))
		{
			PRUint32 flags;
			// todo:
			// use OrFlags()?
			// I don't think I need to get flags, since
			// this is a new header.
			rv = newMsgHdr->GetFlags(&flags);
    			if (NS_FAILED(rv)) return rv;
			rv = newMsgHdr->SetFlags(flags | MSG_FLAG_HAS_RE);
    			if (NS_FAILED(rv)) return rv;
		}

#ifdef DEBUG_NEWS
		printf("subject = %s\n",subject);
#endif
		rv = newMsgHdr->SetSubject(subject);
    		if (NS_FAILED(rv)) return rv;
		
	}

  GET_TOKEN ();											/* author */
  if (line) {
#ifdef DEBUG_spitzer
	printf("author = %s\n", line);
#endif
	rv = newMsgHdr->SetAuthor(line);
    	if (NS_FAILED(rv)) return rv;
  }

  GET_TOKEN ();	
  if (line) {
	PRTime date;
	PRStatus status = PR_ParseTimeString (line, PR_FALSE, &date);
	if (PR_SUCCESS == status) {

#ifdef DEBUG_NEWS
		printf("date = %s\n", line);
#endif
		rv = newMsgHdr->SetDate(date);					/* date */
		if (NS_FAILED(rv)) return rv;
	}
  }

  GET_TOKEN ();											/* message id */
  if (line) {
#ifdef DEBUG_NEWS
	printf("message id = %s\n", line);
#endif
	char *strippedId = line;

	if (strippedId[0] == '<')
		strippedId++;

	char * lastChar = strippedId + PL_strlen(strippedId) -1;

	if (*lastChar == '>')
		*lastChar = '\0';

	rv = newMsgHdr->SetMessageId(strippedId);
  	if (NS_FAILED(rv)) return rv;           
  }

  GET_TOKEN ();											/* references */
  if (line) {
#ifdef DEBUG_NEWS
	printf("references = %s\n",line);
#endif
	rv = newMsgHdr->SetReferences(line);
  	if (NS_FAILED(rv)) return rv;           
  }

  GET_TOKEN ();											/* bytes */
  if (line) {
	PRUint32 msgSize = 0;
	msgSize = (line) ? atol (line) : 0;

#ifdef DEBUG_NEWS
	printf("bytes = %d\n", msgSize);
#endif
	rv = newMsgHdr->SetMessageSize(msgSize);
  	if (NS_FAILED(rv)) return rv;           
  }

  GET_TOKEN ();											/* lines */
  if (line) {
	PRUint32 numLines = 0;
	numLines = line ? atol (line) : 0;
#ifdef DEBUG_NEWS	
	printf("lines = %d\n", numLines);
#endif
	rv = newMsgHdr->SetLineCount(numLines);
  	if (NS_FAILED(rv)) return rv;           
  }

  GET_TOKEN ();											/* xref */
  
  rv = m_newsDB->AddNewHdrToDB(newMsgHdr, PR_TRUE);
  if (NS_FAILED(rv)) return rv;           

  return NS_OK;
}

nsresult
nsNNTPNewsgroupList::ProcessXOVERLINE(const char *line, PRUint32 *status)
{
	PRUint32 message_number=0;
	//  PRInt32 lines;
	PRBool read_p = PR_FALSE;
	nsresult rv = NS_OK;

	PR_ASSERT (line);
	if (!line)
        return NS_ERROR_NULL_POINTER;

	if (m_newsDB)
	{
		char *xoverline = PL_strdup(line);
		if (!xoverline) return NS_ERROR_OUT_OF_MEMORY;
		rv = ParseLine(xoverline, &message_number);
		PL_strfree(xoverline);
		xoverline = nsnull;
		if (NS_FAILED(rv)) return rv;
	}
	else
		return NS_ERROR_NOT_INITIALIZED;

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

		PRInt32 	percent = (totIndex) ? (PRInt32)(100.0 * (double)numDownloaded / (double)totToDownload) : 0;
#ifdef HAVE_PANES
		FE_SetProgressBarPercent (m_pane->GetContext(), percent);
#else	
		SetProgressBarPercent(percent);
#endif
		
		/* only update every  NEWS_ART_DISPLAY_FREQ articles for speed */
		if ( (totIndex <= NEWS_ART_DISPLAY_FREQ) || ((lastIndex % NEWS_ART_DISPLAY_FREQ) == 0) || (lastIndex == totIndex))
		{
#ifdef HAVE_XPGETSTRING
			char *statusTemplate = XP_GetString (MK_HDR_DOWNLOAD_COUNT);
			char *statusString = PR_smprintf (statusTemplate, numDownloaded, totToDownload);
#else
			char *statusString = PR_smprintf ("Received %d of %d headers", numDownloaded, totToDownload);
#endif
#ifdef HAVE_PANES
			FE_Progress (m_pane->GetContext(), statusString);
#else
			SetProgressStatus(statusString);
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
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsNNTPNewsgroupList::FinishXOVERLINE(int status, int *newstatus)
{
	struct MSG_NewsKnown* k;

	/* If any XOVER lines from the last time failed to come in, mark those
	 messages as read. */

	if (status >= 0 && m_lastProcessedNumber < m_lastMsgNumber) 
	{
		m_set->AddRange(m_lastProcessedNumber + 1, m_lastMsgNumber);
	}

	if (m_newsDB)
	{
#ifdef DEBUG_NEWS
        printf("committing summary file changes\n");
#endif
		m_newsDB->Commit(nsMsgDBCommitType::kSessionCommit);
		m_newsDB->Close(PR_TRUE);
		m_newsDB = nsnull;
	}


	k = &m_knownArts;

	if (!k) {
		return NS_ERROR_NULL_POINTER;
	}


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
        m_finishingXover = PR_TRUE;
		// if we haven't started an update, start one so the fe
		// will know to update the size of the view.
		if (!m_startedUpdate)
		{
#ifdef HAVE_PANES
			m_pane->StartingUpdate(MSG_NotifyNone, 0, 0);
#endif
			m_startedUpdate = PR_TRUE;
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
#else
			char *statusString = PR_smprintf ("Downloading articles %d-%d",  m_lastProcessedNumber - m_firstMsgNumber + 1, m_lastMsgNumber - m_firstMsgNumber + 1);
#endif
			if (statusString)
			{
#ifdef HAVE_PANES
				FE_Progress (context, statusString);
#else
				SetProgressStatus(statusString);
#endif
				PR_Free(statusString);
			}
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

nsresult
nsNNTPNewsgroupList::GetGroupName(char **_retval)
{
	if (_retval) {
		*_retval = m_groupName;
		return NS_OK;
	}
	else {
		return NS_ERROR_NULL_POINTER;
	}
}

void
nsNNTPNewsgroupList::SetProgressBarPercent(int percent)
{
#ifdef DEBUG_NEWS
        printf("nsNNTPNewsgroupList::SetProgressBarPercent(%d)\n",percent);
#endif
        if (!m_runningURL) return;

        nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
        if (mailnewsUrl) {
                nsCOMPtr <nsIMsgStatusFeedback> feedback;
                mailnewsUrl->GetStatusFeedback(getter_AddRefs(feedback));

                if (feedback) {
                        feedback->ShowProgress(percent);
                }
        }
} 

void
nsNNTPNewsgroupList::SetProgressStatus(char *message)
{
#ifdef DEBUG_NEWS
        printf("nsNNTPNewsgroupList::SetProgressStatus(%s)\n",message);
#endif
        PRUnichar *progressMsg = nsnull;
        // PRUnichar *progressMsg = NNTPGetStringByID(aMsgId);

        if (!m_runningURL) return;

        nsCOMPtr <nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
        if (mailnewsUrl) {
                nsCOMPtr <nsIMsgStatusFeedback> feedback;
                mailnewsUrl->GetStatusFeedback(getter_AddRefs(feedback));

                char *printfString = PR_smprintf("%s", message);
                if (printfString) {
                        nsString formattedString(printfString);
                        progressMsg = nsCRT::strdup(formattedString.GetUnicode());
                }
                if (feedback) {
                        feedback->ShowStatusString(progressMsg);
                }
        }
        PR_FREEIF(progressMsg);
}     

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

 /* This is a stub event sink for a NNTP Newsgroup introduced by mscott to test
   the NNTP protocol */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include "nsCRT.h"
#include "prmem.h"
#include <stdio.h>

#include "nsISupports.h" /* interface nsISupports */

#include "nsINNTPNewsgroup.h"
#include "nsINNTPNewsgroupList.h"
#include "nsNNTPArticleSet.h"
#include "nsNNTPNewsgroupList.h"

#include "MailNewsTypes.h"

class nsNNTPNewsgroupListStub : public nsINNTPNewsgroupList {

 public: 
	nsNNTPNewsgroupListStub(nsINNTPHost * host, nsINNTPNewsgroup * newsgroup);
    virtual  ~nsNNTPNewsgroupListStub();

	NS_DECL_ISUPPORTS

	NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 first_message, PRInt32 last_message, PRInt32 maxextra, PRInt32 *real_first_message, 
	  PRInt32 *real_last_message, PRInt32 *_retval);

	NS_IMETHOD AddToKnownArticles(PRInt32 first_message, PRInt32 last_message);

	NS_IMETHOD InitXOVER(PRInt32 first_message, PRInt32 last_message);

	NS_IMETHOD ProcessXOVER(const char *line, PRUint32 *status);

	NS_IMETHOD ProcessNonXOVER(const char *line);
	NS_IMETHOD ResetXOVER();

	NS_IMETHOD FinishXOVER(PRInt32 status, PRInt32 *newstatus);
	NS_IMETHOD ClearXOVERState();
 protected:
	 NS_METHOD Init(nsINNTPHost *, nsINNTPNewsgroup*);

	 PRBool          m_finishingXover;
	 nsINNTPHost*	GetHost() {return m_host;}
	 const char *	GetGroupName() {return m_groupName;}
  
	 PRBool			m_startedUpdate;
	 PRBool			m_getOldMessages;
	 PRBool			m_promptedAlready;
	 PRBool			m_downloadAll;
	 PRInt32			m_maxArticles;
	 char			*m_groupName;
	 nsINNTPHost	*m_host;
	 nsINNTPNewsgroup * m_group;
  
	nsMsgKey		m_lastProcessedNumber;
	nsMsgKey		m_firstMsgNumber;
	nsMsgKey		m_lastMsgNumber;
	PRInt32			m_firstMsgToDownload;
	PRInt32			m_lastMsgToDownload;

	struct MSG_NewsKnown	m_knownArts;
	nsNNTPArticleSet	*m_set;
};


NS_IMPL_ISUPPORTS(nsNNTPNewsgroupListStub, nsINNTPNewsgroupList::GetIID());

nsNNTPNewsgroupListStub::nsNNTPNewsgroupListStub(nsINNTPHost* host,
                                         nsINNTPNewsgroup *newsgroup)
{
    NS_INIT_REFCNT();
    Init(host, newsgroup);
}


nsNNTPNewsgroupListStub::~nsNNTPNewsgroupListStub()
{
}

nsresult nsNNTPNewsgroupListStub::Init(nsINNTPHost * host, nsINNTPNewsgroup * newsgroup)
{
	m_group = newsgroup;
	NS_IF_ADDREF(newsgroup);

	if (m_group)
		m_group->GetName(&m_groupName);

	m_host = host;
	NS_IF_ADDREF(host);

	m_lastProcessedNumber = 0;
	m_lastMsgNumber = 0;
	m_set = nsNNTPArticleSet::Create(host);
    m_finishingXover = PR_FALSE;
	m_startedUpdate = PR_FALSE;
	m_knownArts.host = nsnull;
	m_knownArts.set = m_set;
	m_knownArts.first_possible = 0;
	m_knownArts.last_possible = 0;
	m_knownArts.shouldGetOldest = PR_FALSE; 
	m_knownArts.group_name = m_groupName;

	m_knownArts.host = m_host;
	NS_IF_ADDREF(m_host);

	m_getOldMessages = PR_FALSE;
	m_promptedAlready = PR_FALSE;
	m_downloadAll = PR_FALSE;
	m_maxArticles = 0;
	m_firstMsgToDownload = 0;
	m_lastMsgToDownload = 0;

    return NS_OK;
}

nsresult nsNNTPNewsgroupListStub::GetRangeOfArtsToDownload(
                                              PRInt32 first_possible,
                                              PRInt32 last_possible,
                                              PRInt32 maxextra,
                                              PRInt32* first,
                                              PRInt32* last,
                                              PRInt32 *status)
{
	PRBool emptyGroup_p = PR_FALSE;

	if (!first || !last) return NS_ERROR_FAILURE;

	*first = 0;
	*last = 0;

	m_set->SetLastMember(last_possible);	// make sure highwater mark is valid.

	m_knownArts.first_possible = first_possible;
	m_knownArts.last_possible = last_possible;

	/* Determine if we only want to get just new articles or more messages.
	If there are new articles at the end we haven't seen, we always want to get those first.  
	Otherwise, we get the newest articles we haven't gotten, if we're getting more. 
	My thought for now is that opening a newsgroup should only try to get new articles.
	Selecting "More Messages" will first try to get unseen messages, then old messages. */

	if (m_getOldMessages || !m_knownArts.set->IsMember(last_possible)) 
	{

	}
	m_firstMsgToDownload = *first;
	m_lastMsgToDownload = *last;
    if (status) *status=0;
	return NS_OK;
}

nsresult nsNNTPNewsgroupListStub::AddToKnownArticles(PRInt32 first, PRInt32 last)
{
	printf ("Adding articles %d to %d to known article set. \n", first, last);
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
		  return NS_ERROR_FAILURE;
		}

	}

	status = m_knownArts.set->AddRange(first, last);
	return status;
}

nsresult nsNNTPNewsgroupListStub::InitXOVER(PRInt32 first_msg, PRInt32 last_msg)
{
    
	int		status = 0;

	/* Consistency checks, not that I know what to do if it fails (it will
	 probably handle it OK...) */

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

nsresult nsNNTPNewsgroupListStub::ProcessXOVER(const char *line, PRUint32 *status)
{
	const char *next;
	PRUint32 message_number=0;
	//  PRInt32 lines;
	PRBool read_p = PR_FALSE;

	if (!line)
      return NS_ERROR_FAILURE;
    
	next = line;

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
	
	return NS_OK;
}

nsresult
nsNNTPNewsgroupListStub::ResetXOVER()
{
	printf("Resetting XOVER for newsgroup list. \n");
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

nsresult nsNNTPNewsgroupListStub::ProcessNonXOVER (const char * /*line*/)
{
	// ### dmb write me
    return NS_OK;
}


nsresult nsNNTPNewsgroupListStub::FinishXOVER (int status, int *newstatus)
{
	struct MSG_NewsKnown* k;

	/* If any XOVER lines from the last time failed to come in, mark those
	 messages as read. */

	if (status >= 0 && m_lastProcessedNumber < m_lastMsgNumber) 
	{
		m_set->AddRange(m_lastProcessedNumber + 1, m_lastMsgNumber);
	}

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
        m_finishingXover = PR_TRUE;
		// if we haven't started an update, start one so the fe
		// will know to update the size of the view.
		if (!m_startedUpdate)
		{
			m_startedUpdate = PR_TRUE;
		}
		m_startedUpdate = PR_FALSE;

		if (m_lastMsgNumber > 0)
		{
		}

	}
    if (newstatus) *newstatus=0;
    return NS_OK;
	// nsNNTPNewsgroupList object gets deleted by the master when a new one is created.
}


nsresult nsNNTPNewsgroupListStub::ClearXOVERState()
{
    return NS_OK;
}


extern "C" nsresult NS_NewNewsgroupList(nsINNTPNewsgroupList **aInstancePtrResult,
                    nsINNTPHost *newsHost,
                    nsINNTPNewsgroup *newsgroup)
{
	nsNNTPNewsgroupListStub * stub = nsnull;
	stub = new nsNNTPNewsgroupListStub(newsHost, newsgroup);
	nsresult rv = stub->QueryInterface(nsINNTPNewsgroupList::GetIID(), (void **) aInstancePtrResult);
	return rv;
}

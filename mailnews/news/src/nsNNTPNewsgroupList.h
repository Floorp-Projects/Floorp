/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
 * formerly listngst.h
 * This class should ultimately be part of a news group listing
 * state machine - either by inheritance or delegation.
 * Currently, a folder pane owns one and libnet news group listing
 * related messages get passed to this object.
 */
#ifndef nsNNTPNewsgroupListState_h___
#define nsNNTPNewsgroupListState_h___

#include "nsINNTPHost.h"
#include "nsINNTPNewsgroupList.h"
#include "nsINNTPNewsgroup.h"
#include "nsIMsgDatabase.h"
#include "nsMsgKeySet.h"
#include "nsINntpUrl.h"


/* The below is all stuff that we remember for netlib about which
   articles we've already seen in the current newsgroup. */

typedef struct MSG_NewsKnown {
	nsINNTPHost* host;
	char* group_name;
	nsMsgKeySet* set; /* Set of articles we've already gotten
								  from the newsserver (if it's marked
								  "read", then we've already gotten it).
								  If an article is marked "read", it
								  doesn't mean we're actually displaying
								  it; it may be an article that no longer
								  exists, or it may be one that we've
								  marked read and we're only viewing
								  unread messages. */

	PRInt32 first_possible;	/* The oldest article in this group. */
	PRInt32 last_possible;	/* The newest article in this group. */

	PRBool shouldGetOldest;
} MSG_NewsKnown;

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
  nsNNTPNewsgroupList();
  virtual  ~nsNNTPNewsgroupList();
  NS_DECL_ISUPPORTS
  NS_DECL_NSINNTPNEWSGROUPLIST
private:
  NS_METHOD CleanUp();
     
  PRBool          m_finishingXover;
  nsINNTPHost*	GetHost() {return m_host;}
  const char *	GetURI() {return m_uri;}
  
#ifdef HAVE_CHANGELISTENER
  virtual void	OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
#endif
  nsresult			ParseLine(char *line, PRUint32 *message_number);
  PRBool			msg_StripRE(const char **stringP, PRUint32 *lengthP);
  nsresult			GetDatabase(const char *uri, nsIMsgDatabase **db);
  void				SetProgressBarPercent(int percent);
  void				SetProgressStatus(char *message);

protected:
  nsIMsgDatabase	*m_newsDB;
  PRBool			m_startedUpdate;
  PRBool			m_getOldMessages;
  PRBool			m_promptedAlready;
  PRBool			m_downloadAll;
  PRInt32			m_maxArticles;
  char			*m_groupName;
  nsINNTPHost	*m_host;
  nsINNTPNewsgroup *m_newsgroup;
  char 			*m_username;			
  char 			*m_hostname;
  char			*m_uri;
  
  nsMsgKey		m_lastProcessedNumber;
  nsMsgKey		m_firstMsgNumber;
  nsMsgKey		m_lastMsgNumber;
  PRInt32			m_firstMsgToDownload;
  PRInt32			m_lastMsgToDownload;
  
  struct MSG_NewsKnown	m_knownArts;
  nsMsgKeySet		*m_set;
  nsINntpUrl		*m_runningURL;
};
    
#endif /* nsNNTPNewsgroupListState_h___ */








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

    
  NS_IMETHOD GetRangeOfArtsToDownload(PRInt32 first_possible,
                                      PRInt32 last_possible,
                                      PRInt32 maxextra,
                                      PRInt32* first,
                                      PRInt32* lastprotected,
                                      PRInt32 *status);
  NS_IMETHOD AddToKnownArticles(PRInt32 first, PRInt32 last);

  // XOVER parser to populate this class
  NS_IMETHOD InitXOVER(PRInt32 first_msg, PRInt32 last_msg);
  NS_IMETHOD ProcessXOVERLINE(const char *line, PRUint32 * status);
  NS_IMETHOD ResetXOVER();
  NS_IMETHOD ProcessNonXOVER(const char *line);
  NS_IMETHOD FinishXOVERLINE(int status, int *newstatus);
  NS_IMETHOD ClearXOVERState();
  NS_IMETHOD GetGroupName(char **_retval);
    
  NS_IMETHOD Initialize(nsINNTPHost *host, nsINntpUrl *runningURL, nsINNTPNewsgroup *newsgroup, const char *username, const char *hostname, const char *groupname);

private:
  NS_METHOD CleanUp();
    
#ifdef HAVE_MASTER
  MSG_Master		*GetMaster() {return m_master;}
  void			SetMaster(MSG_Master *master) {m_master = master;}
#endif
  
#ifdef HAVE_PANES
  void			SetPane(MSG_Pane *pane) {m_pane = pane;}
#endif
  PRBool          m_finishingXover;
  nsINNTPHost*	GetHost() {return m_host;}
  const char *	GetURI() {return m_uri;}
  
#ifdef HAVE_CHANGELISTENER
  virtual void	OnAnnouncerGoingAway (ChangeAnnouncer *instigator);
#endif
  void				SetGetOldMessages(PRBool getOldMessages) {m_getOldMessages = getOldMessages;}
  PRBool			GetGetOldMessages() {return m_getOldMessages;}
  nsresult			ParseLine(char *line, PRUint32 *message_number);
  PRBool			msg_StripRE(const char **stringP, PRUint32 *lengthP);
  nsresult			GetDatabase(const char *uri, nsIMsgDatabase **db);
  void				SetProgressBarPercent(int percent);
  void				SetProgressStatus(char *message);

protected:
  nsIMsgDatabase	*m_newsDB;
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
  nsINNTPNewsgroup *m_newsgroup;
  char			*m_uri;			
#ifdef HAVE_MASTER
  MSG_Master		*m_master;
#endif
  
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

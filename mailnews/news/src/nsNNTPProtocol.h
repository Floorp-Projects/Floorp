/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIStreamListener.h"
#include "rosetta.h"
#include HG40855

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state

#define NNTP_PAUSE_FOR_READ			0x00000001  /* should we pause for the next read */
#define NNTP_PROXY_AUTH_REQUIRED    0x00000002  /* is auth required */
#define NNTP_SENT_PROXY_AUTH        0x00000004  /* have we sent a proxy auth? */
#define NNTP_NEWSRC_PERFORMED       0x00000008  /* have we done a newsrc update? */
#define NNTP_READER_PERFORMED       0x00000010  /* have we sent any cmds to the server yet? */
#define NNTP_USE_FANCY_NEWSGROUP    0x00000020	/* use LIST XACTIVE or LIST */
#define NNTP_DESTROY_PROGRESS_GRAPH 0x00000040  /* do we need to destroy graph progress */  

class nsNNTPProtocol : public nsIStreamListener
{
public:
	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////

	// mscott; I don't think we need to worry about this yet so I'll leave it stubbed out for now
	NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_OK;} ;
	
	// Whenever data arrives from the connection, core netlib notifices the protocol by calling
	// OnDataAvailable. We then read and process the incoming data from the input stream. 
	NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);

	NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);

	// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
	NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

	// Ideally, a protocol should only have to support the stream listener methods covered above. 
	// However, we don't have this nsIStreamListenerLite interface defined yet. Until then, we are using
	// nsIStreamListener so we need to add stubs for the heavy weight stuff we don't want to use.

	NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) { return NS_OK;}
	NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg) { return NS_OK;}

	////////////////////////////////////////////////////////////////////////////////////////
	// End of nsIStreamListenerSupport
	////////////////////////////////////////////////////////////////////////////////////////

	nsString m_ProxyServer;		/* proxy server hostname */

	// Flag manipulators
	PRBool TestFlag  (PRUInt32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUInt32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUInt32 flag) { m_flags &= ~flag; }

private:
	// News Event Sinks
    nsIMsgXOVERParser *		m_xoverParser;
    nsIMsgNewsArticleList *	m_articleList;

	nsIMsgNewsHost			* m_newsHost;
	nsIMsgNewsgroup			* m_newsgroup;
	nsIMsgOfflineNewsState  * m_offlineNewsState;

	
	PRUInt32 m_flags; // used to store flag information

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	StatesEnum  m_nextState;
    StatesEnum  m_nextStateAfterResponse;
	PRInt       m_typeWanted;     /* Article, List, or Group */
    PRInt       m_reponseCode;    /* code returned from NNTP server */
	PRInt		m_previousResponseCode; 
    char       *m_responseText;   /* text returned from NNTP server */

#ifdef XP_WIN
	PRBool		calling_netlib_all_the_time;
#endif

    char   *	m_dataBuf;
    PRUInt32     m_dataBufSize;

	/* for group command */
    char    * m_path; /* message id */

    char    * m_groupName;
    PRInt32   m_firstArticle;
    PRInt32   m_lastArticle;
    PRInt32   m_firstPossibleArticle;
    PRInt32   m_lastPossibleArticle;

	PRInt32	  m_numArticlesLoaded;	/* How many articles we got XOVER lines for. */
	PRInt32	  m_numArticlesWanted; /* How many articles we wanted to get XOVER lines for. */

	// Cancelation specific state. In particular, the headers that should be 
	// used for the cancelation message. 
	// mscott: we can probably replace this stuff with nsString
    nsString  m_cancelFromHdr;
    nsString  m_cancelNewsgroups;
    nsString  m_cancelDistribution;
    nsString  m_cancelID;
    nsString  m_cancelMessageFile;
    PRInt	  m_cancelStatus;

	// variables for ReadNewsRC
	PRInt32   m_newsRCListIndex;
	PRInt32   m_newsRCListCount;

	// Per news article state information. (article number, author, subject, id, etc
    PRInt32   m_articleNumber;   /* current article number */

	PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */
	
	PRInt	  ProcessNewsState(nsIInputStream * inputStream, PRUint32 length)
	PRInt	  CloseConnection(); // releases and closes down this protocol instance...

	////////////////////////////////////////////////////////////////////////////////////////
	// Protocol Methods --> This protocol is state driven so each protocol method is designed 
	//						to re-act to the current "state". I've attempted to group them 
	//						together based on functionality. 
	////////////////////////////////////////////////////////////////////////////////////////

	// gets the response code from the nntp server and the response line. Returns the TCP return code 
	// from the read.
	PRInt NewsResponse(); 

	// Interpret the server response after the connect. 
	// Returns negative if the server responds unexpectedly
	PRInt LoginResponse(); 
	PRInt SendModeReader();
	PRInt SendModeReaderResponse();

	PRInt SendListExtensions();
	PRInt SendListExtensionsResponse();
	
	PRInt SendListSearches();
	PRInt SendListSearchesResponse();

	PRInt SendListSearchHeaders();
	PRInt SendListSearchHeadersResponse();

	PRInt GetProperties();
	PRInt GetPropertiesResponse();

	PRInt SendListSubscriptions();
	PRInt SendListSubscriptionsResponse();

	// Figure out what the first command is and send it. 
	// Returns the status from the NETWrite.
	PRInt SendFirstNNTPCommand();

	// Interprets the server response from the first command sent.
	// returns negative if the server responds unexpectedly.
	PRInt SendFirstNNTPCommandResponse();

	PRInt SendGroupForArticle();
	PRInt SendGroupForArticleResponse();

	PRInt SendArticleNumber();
	PRInt BeginArticle();
	PRInt ReadArticle();

	PRInt BeginAuthorization();
	PRInt AuthorizationResponse();

	PRInt NewsPasswordResponse();

	PRInt BeginReadNewsList();
	PRInt ReadNewsList();

	// Newsgroup specific protocol handlers
	PRInt DisplayNewsgroups();
	PRInt BeginNewsgroups();
	PRInt ProcessNewsgroups();

	PRInt ReadNewsgroup();
	PRInt ReadNewsgroupResponse();

	PRInt ReadNewsgroupBody();

	// Protocol handlers used for posting data 
	PRInt PostData();
	PRInt PostDataResponse();

	PRInt CheckForArticle();

	// NewsRC specific
	PRInt DisplayNewsRC();
	PRInt DisplayNewsRCResponse();

	// start off the xover command 
	PRInt BeginReadXover();

	// process the xover list as it comes from the server and load it into the sort list.  
	PRInt ReadXover();
	// See if the xover response is going to return us data. If the proper code isn't returned then 
	// assume xover isn't supported and use normal read_group.
	PRInt ReadXoverResponse();

	PRInt XoverSend();
	PRInt ProcessXover();

	PRInt FigureNextChunk();

	// Canceling
	PRInt StartCancel();
	PRInt Cancel();

	// XPAT 
	PRInt XPATSend();
	PRInt XPATResponse();
	PRInt ListPrettyNames();
	PRInt ListPrettyNamesResponse();

	PRInt ListXActive();
	PRInt ListXActiveResponse();

	PRInt ListGroup();
	PRInt ListGroupResponse();

	// Searching Protocol....
	PRInt Search();
	PRInt SearchResponse();
	PRInt SearchResults();

	////////////////////////////////////////////////////////////////////////////////////////
	// End of Protocol Methods
	////////////////////////////////////////////////////////////////////////////////////////
};


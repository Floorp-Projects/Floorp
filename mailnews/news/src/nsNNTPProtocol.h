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

#ifndef nsNNTPProtocol_h___
#define nsNNTPProtocol_h___

#include "nsIStreamListener.h"
#include "nsITransport.h"
#include "rosetta.h"
#include HG40855

#include "nsIOutputStream.h"
#include "nsINntpUrl.h"

#include "nsIWebShell.h"  // mscott - this dependency should only be temporary!

#include "nsINNTPNewsgroupList.h"
#include "nsINNTPArticleList.h"
#include "nsINNTPHost.h"
#include "nsINNTPNewsgroup.h"
#include "nsIMsgOfflineNewsState.h"

// this is only needed as long as our libmime hack is in place
#include "prio.h"

#ifdef XP_UNIX
#define ARTICLE_PATH "/usr/tmp/tempArticle.eml"
#define ARTICLE_PATH_URL ARTICLE_PATH
#endif

#ifdef XP_PC
#define ARTICLE_PATH  "c:\\temp\\tempArticle.eml"
#define ARTICLE_PATH_URL "C|/temp/tempArticle.eml"
#endif

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state)

#define NNTP_PAUSE_FOR_READ			0x00000001  /* should we pause for the next read */
#define NNTP_PROXY_AUTH_REQUIRED    0x00000002  /* is auth required */
#define NNTP_SENT_PROXY_AUTH        0x00000004  /* have we sent a proxy auth? */
#define NNTP_NEWSRC_PERFORMED       0x00000008  /* have we done a newsrc update? */
#define NNTP_READER_PERFORMED       0x00000010  /* have we sent any cmds to the server yet? */
#define NNTP_USE_FANCY_NEWSGROUP    0x00000020	/* use LIST XACTIVE or LIST */
#define NNTP_DESTROY_PROGRESS_GRAPH 0x00000040  /* do we need to destroy graph progress */  
#define NNTP_SOME_PROTOCOL_SUCCEEDED 0x0000080  /* some protocol has suceeded so don't kill the connection */
#define NNTP_NO_XOVER_SUPPORT       0x00000100  /* xover command is not supported here */

/* states of the machine
 */
typedef enum _StatesEnum {
NNTP_RESPONSE,
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE,
NNTP_CONNECTIONS_ARE_AVAILABLE,
#endif
NNTP_CONNECT,
NNTP_CONNECT_WAIT,
HG07711
NNTP_LOGIN_RESPONSE,
NNTP_SEND_MODE_READER,
NNTP_SEND_MODE_READER_RESPONSE,
SEND_LIST_EXTENSIONS,
SEND_LIST_EXTENSIONS_RESPONSE,
SEND_LIST_SEARCHES,
SEND_LIST_SEARCHES_RESPONSE,
NNTP_LIST_SEARCH_HEADERS,
NNTP_LIST_SEARCH_HEADERS_RESPONSE,
NNTP_GET_PROPERTIES,
NNTP_GET_PROPERTIES_RESPONSE,
SEND_LIST_SUBSCRIPTIONS,
SEND_LIST_SUBSCRIPTIONS_RESPONSE,
SEND_FIRST_NNTP_COMMAND,
SEND_FIRST_NNTP_COMMAND_RESPONSE,
SETUP_NEWS_STREAM,
NNTP_BEGIN_AUTHORIZE,
NNTP_AUTHORIZE_RESPONSE,
NNTP_PASSWORD_RESPONSE,
NNTP_READ_LIST_BEGIN,
NNTP_READ_LIST,
DISPLAY_NEWSGROUPS,
NNTP_NEWGROUPS_BEGIN,
NNTP_NEWGROUPS,
NNTP_BEGIN_ARTICLE,
NNTP_READ_ARTICLE,
NNTP_XOVER_BEGIN,
NNTP_FIGURE_NEXT_CHUNK,
NNTP_XOVER_SEND,
NNTP_XOVER_RESPONSE,
NNTP_XOVER,
NEWS_PROCESS_XOVER,
NNTP_READ_GROUP,
NNTP_READ_GROUP_RESPONSE,
NNTP_READ_GROUP_BODY,
NNTP_SEND_GROUP_FOR_ARTICLE,
NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE,
NNTP_PROFILE_ADD,
NNTP_PROFILE_ADD_RESPONSE,
NNTP_PROFILE_DELETE,
NNTP_PROFILE_DELETE_RESPONSE,
NNTP_SEND_ARTICLE_NUMBER,
NEWS_PROCESS_BODIES,
NNTP_PRINT_ARTICLE_HEADERS,
NNTP_SEND_POST_DATA,
NNTP_SEND_POST_DATA_RESPONSE,
NNTP_CHECK_FOR_MESSAGE,
NEWS_NEWS_RC_POST,
NEWS_DISPLAY_NEWS_RC,
NEWS_DISPLAY_NEWS_RC_RESPONSE,
NEWS_START_CANCEL,
NEWS_DO_CANCEL,
NNTP_XPAT_SEND,
NNTP_XPAT_RESPONSE,
NNTP_SEARCH,
NNTP_SEARCH_RESPONSE,
NNTP_SEARCH_RESULTS,
NNTP_LIST_PRETTY_NAMES,
NNTP_LIST_PRETTY_NAMES_RESPONSE,
NNTP_LIST_XACTIVE,
NNTP_LIST_XACTIVE_RESPONSE,
NNTP_LIST_GROUP,
NNTP_LIST_GROUP_RESPONSE,
NEWS_DONE,
NEWS_ERROR,
NNTP_ERROR,
NEWS_FREE
} StatesEnum;

class nsNNTPProtocol : public nsIStreamListener
{
public:
	// Creating a protocol instance requires the URL which needs to be run AND it requires
	// a transport layer. 
	nsNNTPProtocol(nsIURL * aURL, nsITransport * transportLayer);
	
	virtual ~nsNNTPProtocol();

	// aConsumer is typically a display stream you may want the results to be displayed into...

	PRInt32 LoadURL(nsIURL * aURL, nsISupports * aConsumer /* consumer of the url */ = nsnull);
	PRBool  IsRunningUrl() { return m_urlInProgress;} // returns true if we are currently running a url and false otherwise...

	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////

	// mscott; I don't think we need to worry about this yet so I'll leave it stubbed out for now
	NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_OK;} ;
	
	// Whenever data arrives from the connection, core netlib notifies the protocol by calling
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

	char * m_ProxyServer;		/* proxy server hostname */

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

private:
	// the following flag is used to determine when a url is currently being run. It is cleared on calls
	// to ::StopBinding and it is set whenever we call Load on a url
	PRBool	m_urlInProgress;	

	// part of temporary libmime converstion trick......these should go away once MIME uses a new stream
	// converter interface...
	PRFileDesc* m_tempArticleFile;

	// News Event Sinks
    nsINNTPNewsgroupList	* m_newsgroupList;
    nsINNTPArticleList		* m_articleList;

	nsINNTPHost				* m_newsHost;
	nsINNTPNewsgroup		* m_newsgroup;
	nsIMsgOfflineNewsState  * m_offlineNewsState;

	// Ouput stream for writing commands to the socket
	nsITransport			* m_transport; 
	nsIOutputStream			* m_outputStream;   // this will be obtained from the transport interface
	nsIStreamListener	    * m_outputConsumer; // this will be obtained from the transport interface
	nsIWebShell				* m_displayConsumer; // if we are displaying an article this is the rfc-822 display sink...

	// the nsINntpURL that is currently running
	nsINntpUrl				* m_runningURL;
	
	PRUint32 m_flags; // used to store flag information

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	StatesEnum  m_nextState;
    StatesEnum  m_nextStateAfterResponse;
	PRInt32     m_typeWanted;     /* Article, List, or Group */
    PRInt32     m_responseCode;    /* code returned from NNTP server */
	PRInt32 	m_previousResponseCode; 
    char       *m_responseText;   /* text returned from NNTP server */
	char	   *m_hostName;

    char		*m_dataBuf;
    PRUint32	 m_dataBufSize;

	/* for group command */
    char     *m_path;			  /* message id */
    char     *m_currentGroup;     /* current group */

    PRInt32   m_firstArticle;
    PRInt32   m_lastArticle;
    PRInt32   m_firstPossibleArticle;
    PRInt32   m_lastPossibleArticle;

	PRInt32	  m_numArticlesLoaded;	/* How many articles we got XOVER lines for. */
	PRInt32	  m_numArticlesWanted; /* How many articles we wanted to get XOVER lines for. */

	// Cancelation specific state. In particular, the headers that should be 
	// used for the cancelation message. 
	// mscott: we can probably replace this stuff with nsString
    char	 *m_cancelFromHdr;
    char     *m_cancelNewsgroups;
    char     *m_cancelDistribution;
    char     *m_cancelID;
    char     *m_cancelMessageFile;
    PRInt32	  m_cancelStatus;

	// variables for ReadNewsRC
	PRInt32   m_newsRCListIndex;
	PRInt32   m_newsRCListCount;

	// Per news article state information. (article number, author, subject, id, etc
	char	 *m_messageID;
    PRInt32   m_articleNumber;   /* current article number */
	char	 *m_commandSpecificData;
	char	 *m_searchData;

	PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */
	
	PRInt32	  ProcessNewsState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length);
	PRInt32	  CloseConnection(); // releases and closes down this protocol instance...

	// initialization function given a new url and transport layer
	void Initialize(nsIURL * aURL, nsITransport * transportLayer);

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line);

	// SendData not only writes the NULL terminated data in dataBuffer to our output stream
	// but it also informs the consumer that the data has been written to the stream.
	PRInt32 SendData(const char * dataBuffer);

	////////////////////////////////////////////////////////////////////////////////////////
	// Protocol Methods --> This protocol is state driven so each protocol method is 
	//						designed to re-act to the current "state". I've attempted to 
	//						group them together based on functionality. 
	////////////////////////////////////////////////////////////////////////////////////////

	// gets the response code from the nntp server and the response line. Returns the TCP return code 
	// from the read.
	PRInt32 NewsResponse(nsIInputStream * inputStream, PRUint32 length); 

	// Interpret the server response after the connect. 
	// Returns negative if the server responds unexpectedly
	PRInt32 LoginResponse(); 
	PRInt32 SendModeReader();
	PRInt32 SendModeReaderResponse();

	PRInt32 SendListExtensions();
	PRInt32 SendListExtensionsResponse(nsIInputStream * inputStream, PRUint32 length);
	
	PRInt32 SendListSearches();
	PRInt32 SendListSearchesResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 SendListSearchHeaders();
	PRInt32 SendListSearchHeadersResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 GetProperties();
	PRInt32 GetPropertiesResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 SendListSubscriptions();
	PRInt32 SendListSubscriptionsResponse(nsIInputStream * inputStream, PRUint32 length);

	// Figure out what the first command is and send it. 
	// Returns the status from the NETWrite.
	PRInt32 SendFirstNNTPCommand(nsIURL * url);

	// Interprets the server response from the first command sent.
	// returns negative if the server responds unexpectedly.
	PRInt32 SendFirstNNTPCommandResponse();

	PRInt32 SetupForTransfer();

	PRInt32 SendGroupForArticle();
	PRInt32 SendGroupForArticleResponse();

	PRInt32 SendArticleNumber();
	PRInt32 BeginArticle();
	PRInt32 ReadArticle(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 BeginAuthorization();
	PRInt32 AuthorizationResponse();

	PRInt32 PasswordResponse();

	PRInt32 BeginReadNewsList();
	PRInt32 ReadNewsList(nsIInputStream * inputStream, PRUint32 length);

	// Newsgroup specific protocol handlers
	PRInt32 DisplayNewsgroups();
	PRInt32 BeginNewsgroups();
	PRInt32 ProcessNewsgroups(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 ReadNewsgroup();
	PRInt32 ReadNewsgroupResponse();

	PRInt32 ReadNewsgroupBody(nsIInputStream * inputStream, PRUint32 length);

	// Protocol handlers used for posting data 
	PRInt32 PostData();
	PRInt32 PostDataResponse();

	PRInt32 CheckForArticle();

	// NewsRC specific
	PRInt32 DisplayNewsRC();
	PRInt32 DisplayNewsRCResponse();

	// start off the xover command 
	PRInt32 BeginReadXover();

	// process the xover list as it comes from the server and load it into the sort list.  
	PRInt32 ReadXover(nsIInputStream * inputStream, PRUint32 length);
	// See if the xover response is going to return us data. If the proper code isn't returned then 
	// assume xover isn't supported and use normal read_group.
	PRInt32 ReadXoverResponse();

	PRInt32 XoverSend();
	PRInt32 ProcessXover();

	PRInt32 FigureNextChunk();

	// Canceling
	PRInt32 StartCancel();
	PRInt32 Cancel();

	// XPAT 
	PRInt32 XPATSend();
	PRInt32 XPATResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 ListPrettyNames();
	PRInt32 ListPrettyNamesResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 ListXActive();
	PRInt32 ListXActiveResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 ListGroup();
	PRInt32 ListGroupResponse(nsIInputStream * inputStream, PRUint32 length);

	// Searching Protocol....
	PRInt32 Search();
	PRInt32 SearchResponse();
	PRInt32 SearchResults(nsIInputStream *inputStream, PRUint32 length);

	////////////////////////////////////////////////////////////////////////////////////////
	// End of Protocol Methods
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ParseURL(nsIURL * aURL, char ** aHostAndPort, PRBool * bValP, char ** aGroup, char ** aMessageID, char ** aCommandSpecificData);
};

NS_BEGIN_EXTERN_C

nsresult NS_MailNewsLoadUrl(const nsString& urlString, nsISupports * aConsumer);
nsresult NS_NewNntpUrl(nsINntpUrl ** aResult, const nsString urlSpec);

NS_END_EXTERN_C

#endif  // nsNNTPProtocol_h___

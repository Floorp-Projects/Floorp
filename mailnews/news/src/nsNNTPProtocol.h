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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsNNTPProtocol_h___
#define nsNNTPProtocol_h___

#include "nsMsgProtocol.h"

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsINntpUrl.h"
#include "nsINntpIncomingServer.h"
#include "nsINNTPProtocol.h"

#include "nsINNTPNewsgroupList.h"
#include "nsINNTPArticleList.h"
#include "nsIMsgNewsFolder.h"
#include "nsIMsgWindow.h"

#include "nsMsgLineBuffer.h"
#include "nsSpecialSystemDirectory.h"
#include "nsXPIDLString.h"
#include "nsIStringBundle.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsICacheListener.h"

// this is only needed as long as our libmime hack is in place
#include "prio.h"

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
NEWS_POST_DONE,
NEWS_ERROR,
NNTP_ERROR,
NEWS_FREE,
NEWS_FINISHED
} StatesEnum;

class nsICacheEntryDescriptor;

class nsNNTPProtocol : public nsINNTPProtocol, public nsITimerCallback, public nsICacheListener, public nsMsgProtocol
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSINNTPPROTOCOL
  NS_DECL_NSICACHELISTENER

  // nsITimerCallback interfaces
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  
  // Creating a protocol instance requires the URL 
  // need to call Initialize after we do a new of nsNNTPProtocol
  nsNNTPProtocol(nsIURI * aURL, nsIMsgWindow *aMsgWindow);	
  virtual ~nsNNTPProtocol();
  
  // stop binding is a "notification" informing us that the stream associated with aURL is going away. 
  NS_IMETHOD OnStopRequest(nsIRequest *request, nsISupports * aCtxt, nsresult aStatus);
  
  char * m_ProxyServer;		/* proxy server hostname */
  
  NS_IMETHOD Cancel(nsresult status);  // handle stop button
  NS_IMETHOD GetContentType(char * *aContentType);
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt);
  nsresult LoadUrl(nsIURI * aURL, nsISupports * aConsumer);

private:
	// over-rides from nsMsgProtocol
	virtual nsresult ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									      PRUint32 sourceOffset, PRUint32 length);
	virtual nsresult CloseSocket();

	// we have our own implementation of SendData which writes to the nntp log
	// and then calls the base class to transmit the data
	PRInt32 SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging = PR_FALSE);

  nsresult CleanupAfterRunningUrl();

  void ParseHeaderForCancel(char *buf);

  static PRBool CheckIfAuthor(nsISupports *aElement, void *data);

  nsCOMPtr<nsIOutputStream> m_tempArticleStream; 
   
  nsCOMPtr <nsINNTPNewsgroupList> m_newsgroupList;
  nsCOMPtr <nsINNTPArticleList> m_articleList;

  nsCOMPtr <nsIMsgNewsFolder> m_newsFolder;
  nsCOMPtr <nsIMsgWindow> m_msgWindow;

	nsCOMPtr<nsIInputStream> mDisplayInputStream;
	nsCOMPtr<nsIOutputStream> mDisplayOutputStream;
	nsMsgLineStreamBuffer   * m_lineStreamBuffer; // used to efficiently extract lines from the incoming data stream
	// the nsINntpURL that is currently running
	nsCOMPtr<nsINntpUrl> m_runningURL;
  PRBool      m_connectionBusy;
  PRBool      m_fromCache;  // is this connection from the cache?
  PRTime      m_lastActiveTimeStamp;
	nsNewsAction m_newsAction;

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	StatesEnum  m_nextState;
  StatesEnum  m_nextStateAfterResponse;
	PRInt32     m_typeWanted;     /* Article, List, or Group */
  PRInt32     m_responseCode;    /* code returned from NNTP server */
	PRInt32 	m_previousResponseCode; 
  char       *m_responseText;   /* text returned from NNTP server */
    
  char		*m_dataBuf;
  PRUint32	 m_dataBufSize;

	/* for group command */
  char     *m_path;			  /* message id */
  nsCString m_currentGroup;     /* current group */

  PRInt32   m_firstArticle;
  PRInt32   m_lastArticle;
  PRInt32   m_firstPossibleArticle;
  PRInt32   m_lastPossibleArticle;

	PRInt32	  m_numArticlesLoaded;	/* How many articles we got XOVER lines for. */
	PRInt32	  m_numArticlesWanted; /* How many articles we wanted to get XOVER lines for. */
    PRInt32   m_maxArticles;        /* max articles to get during an XOVER */

	// Cancelation specific state. In particular, the headers that should be 
	// used for the cancelation message. 
	// mscott: we can probably replace this stuff with nsString
  char	 *m_cancelFromHdr;
  char     *m_cancelNewsgroups;
  char     *m_cancelDistribution;
  char     *m_cancelID;
  PRInt32	  m_cancelStatus;

	// variables for ReadNewsRC
	PRInt32   m_newsRCListIndex;
	PRInt32   m_newsRCListCount;

	// variable for ReadNewsList
	PRInt32   m_readNewsListCount;

	// Per news article state information. (article number, author, subject, id, etc
	char	 *m_messageID;
    PRInt32   m_articleNumber;   /* current article number */
	char	 *m_commandSpecificData;
	char	 *m_searchData;

	PRInt32   m_originalContentLength; /* the content length at the time of calling graph progress */

	
	nsCOMPtr<nsIStringBundle> m_stringBundle;

  nsCOMPtr<nsINntpIncomingServer> m_nntpServer;

	nsresult GetNewsStringByName(const char *aName, PRUnichar **aString);
	nsresult GetNewsStringByID(PRInt32 stringID, PRUnichar **aString);

  PRInt32 PostMessageInFile(nsIFileSpec * filePath);

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line);

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
	PRInt32 SendFirstNNTPCommand(nsIURI * url);

	// Interprets the server response from the first command sent.
	// returns negative if the server responds unexpectedly.
	PRInt32 SendFirstNNTPCommandResponse();

	PRInt32 SetupForTransfer();

	PRInt32 SendGroupForArticle();
	PRInt32 SendGroupForArticleResponse();

	PRInt32 SendArticleNumber();
	PRInt32 BeginArticle();
	PRInt32 ReadArticle(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 DisplayArticle(nsIInputStream * inputStream, PRUint32 length);

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
	PRInt32 DoCancel();

	// XPAT 
	PRInt32 XPATSend();
	PRInt32 XPATResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 ListPrettyNames();
	PRInt32 ListPrettyNamesResponse(nsIInputStream * inputStream, PRUint32 length);

	PRInt32 ListXActive();
	PRInt32 ListXActiveResponse(nsIInputStream * inputStream, PRUint32 length);

    // for "?list-ids"
	PRInt32 SendListGroup();
	PRInt32 SendListGroupResponse(nsIInputStream * inputStream, PRUint32 length);

	// Searching Protocol....
	PRInt32 Search();
	PRInt32 SearchResponse();
	PRInt32 SearchResults(nsIInputStream *inputStream, PRUint32 length);

	////////////////////////////////////////////////////////////////////////////////////////
	// End of Protocol Methods
	////////////////////////////////////////////////////////////////////////////////////////

	nsresult ParseURL(nsIURI * aURL, char ** aGroup, char ** aMessageID, char ** aCommandSpecificData);

	void SetProgressBarPercent(PRUint32 aProgress, PRUint32 aProgressMax);
	nsresult SetProgressStatus(const PRUnichar *aMessage);
  nsresult SetCheckingForNewNewsStatus(PRInt32 current, PRInt32 total);
  nsresult MarkCurrentMsgRead(); // marks the message corresponding to the currently running url read.
	nsresult InitializeNewsFolderFromUri(const char *uri);
	void TimerCallback();
	nsCOMPtr <nsIInputStream> mInputStream;
  nsCOMPtr <nsITimer> mUpdateTimer; 
	nsresult AlertError(PRInt32 errorCode, const char *text);
	PRInt32 mBytesReceived;
  PRInt32 mBytesReceivedSinceLastStatusUpdate;
  PRTime m_startTime;
  PRInt32 mNumGroupsListed;
  nsMsgKey m_key;

  nsresult SetCurrentGroup(); /* sets m_currentGroup.  should be called after doing a successful GROUP command */
  nsresult CleanupNewsgroupList(); /* cleans up m_newsgroupList, and set it to null */

    // cache related helper methods
  nsresult OpenCacheEntry(); // makes a request to the cache service for a cache entry for a url
  PRBool ReadFromLocalCache(); // attempts to read the url out of our local (offline) cache....
  nsresult ReadFromNewsConnection(); // creates a new news connection to read the url 
  nsresult ReadFromMemCache(nsICacheEntryDescriptor *entry); // attempts to read the url out of our memory cache
  nsresult SetupPartExtractorListener(nsIStreamListener * aConsumer);
};


#endif  // nsNNTPProtocol_h___

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
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   David Bienvenu <bienvenu@netscape.com>
 *   Jeff Tsai <jefft@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 */

#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */
#include "msgCore.h"    // precompiled header...
#include "MailNewsTypes.h"
#include "nntpCore.h"

#include "nsIMsgHdr.h"
#include "nsNNTPProtocol.h"
#include "nsINNTPArticleList.h"
#include "nsIOutputStream.h"
#include "nsFileStream.h"
#include "nsIMemory.h"
#include "nsIPipe.h"
#include "nsCOMPtr.h"

#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"

#include "nsINntpUrl.h"

#include "nsCRT.h"

#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "nsEscape.h"

#include "prprf.h"
#include "merrors.h"

/* include event sink interfaces for news */

#include "nsIMsgHeaderParser.h" 
#include "nsIMsgSearchSession.h"
#include "nsIMsgSearchAdapter.h"
#include "nsIMsgStatusFeedback.h"

#include "nsMsgKeySet.h"

#include "nsNewsUtils.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsINetSupportDialogService.h"
#include "nsIMsgAccountManager.h"

#include "nsIPrompt.h"
#include "nsIMsgStatusFeedback.h" 

#include "nsIMsgFolder.h"
#include "nsIMsgNewsFolder.h"

#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFCID.h"

#include "nsIPref.h"

#include "nsIMsgWindow.h"
#include "nntpCore.h"

#undef GetPort  // XXX Windows!
#undef SetPort  // XXX Windows!

#define PREF_NEWS_CANCEL_CONFIRM	"news.cancel.confirm"
#define PREF_NEWS_CANCEL_ALERT_ON_SUCCESS "news.cancel.alert_on_success"
#define DEFAULT_NEWS_CHUNK_SIZE -1
#define READ_NEWS_LIST_COUNT_MAX 20 /* number of groups to process at a time when reading the list from the server */
#define READ_NEWS_LIST_TIMEOUT 50	/* uSec to wait until doing more */

#define NEWS_MSGS_URL       "chrome://messenger/locale/news.properties"
// ***jt -- the following were pirated from xpcom/io/nsByteBufferInputStream
// which is not currently in the build system
class nsDummyBufferStream : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_IMETHOD Close(void) {
        NS_NOTREACHED("nsDummyBufferStream::Close");
        return NS_ERROR_FAILURE;
    }

    // nsIInputStream methods:
    NS_IMETHOD Available(PRUint32 *aLength) { 
        *aLength = mLength;
        return NS_OK;
    }
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
        PRUint32 amt = PR_MIN(aCount, mLength);
        if (amt > 0) {
            nsCRT::memcpy(aBuf, mBuffer, amt);
            mBuffer += amt;
            mLength -= amt;
        }
        *aReadCount = amt;
        return NS_OK;
    } 

    // nsDummyBufferStream methods:
    nsDummyBufferStream(const char* buffer, PRUint32 length)
        : mBuffer(buffer), mLength(length) {NS_INIT_REFCNT();}
    virtual ~nsDummyBufferStream() {}

protected:
    const char* mBuffer;
    PRUint32    mLength;
};

// todo:  get rid of this
extern "C"
{
char * NET_SACopy (char **destination, const char *source);
char * NET_SACat (char **destination, const char *source);

}

static NS_DEFINE_CID(kCHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kNNTPArticleListCID, NS_NNTPARTICLELIST_CID);
static NS_DEFINE_CID(kNNTPHostCID, NS_NNTPHOST_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kCMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID,NS_PREF_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

typedef struct _cancelInfoEntry {
    char *from;
    char *old_from;
} cancelInfoEntry;

// quiet compiler warnings by defining these function prototypes
char *MSG_UnEscapeSearchUrl (const char *commandSpecificData);

/* Logging stuff */

PRLogModuleInfo* NNTP = NULL;
#define out     PR_LOG_ALWAYS

#define NNTP_LOG_READ(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Receiving: %s", buf)) ;

#define NNTP_LOG_WRITE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("Sending: %s", buf)) ;

#define NNTP_LOG_NOTE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("%s",buf)) ;

char *stateLabels[] = {
"NNTP_RESPONSE",
#ifdef BLOCK_UNTIL_AVAILABLE_CONNECTION
"NNTP_BLOCK_UNTIL_CONNECTIONS_ARE_AVAILABLE",
"NNTP_CONNECTIONS_ARE_AVAILABLE",
#endif
"NNTP_CONNECT",
"NNTP_CONNECT_WAIT",
"NNTP_LOGIN_RESPONSE",
"NNTP_SEND_MODE_READER",
"NNTP_SEND_MODE_READER_RESPONSE",
"SEND_LIST_EXTENSIONS",
"SEND_LIST_EXTENSIONS_RESPONSE",
"SEND_LIST_SEARCHES",
"SEND_LIST_SEARCHES_RESPONSE",
"NNTP_LIST_SEARCH_HEADERS",
"NNTP_LIST_SEARCH_HEADERS_RESPONSE",
"NNTP_GET_PROPERTIES",
"NNTP_GET_PROPERTIES_RESPONSE",
"SEND_LIST_SUBSCRIPTIONS",
"SEND_LIST_SUBSCRIPTIONS_RESPONSE",
"SEND_FIRST_NNTP_COMMAND",
"SEND_FIRST_NNTP_COMMAND_RESPONSE",
"SETUP_NEWS_STREAM",
"NNTP_BEGIN_AUTHORIZE",
"NNTP_AUTHORIZE_RESPONSE",
"NNTP_PASSWORD_RESPONSE",
"NNTP_READ_LIST_BEGIN",
"NNTP_READ_LIST",
"DISPLAY_NEWSGROUPS",
"NNTP_NEWGROUPS_BEGIN",
"NNTP_NEWGROUPS",
"NNTP_BEGIN_ARTICLE",
"NNTP_READ_ARTICLE",
"NNTP_XOVER_BEGIN",
"NNTP_FIGURE_NEXT_CHUNK",
"NNTP_XOVER_SEND",
"NNTP_XOVER_RESPONSE",
"NNTP_XOVER",
"NEWS_PROCESS_XOVER",
"NNTP_READ_GROUP",
"NNTP_READ_GROUP_RESPONSE",
"NNTP_READ_GROUP_BODY",
"NNTP_SEND_GROUP_FOR_ARTICLE",
"NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE",
"NNTP_PROFILE_ADD",
"NNTP_PROFILE_ADD_RESPONSE",
"NNTP_PROFILE_DELETE",
"NNTP_PROFILE_DELETE_RESPONSE",
"NNTP_SEND_ARTICLE_NUMBER",
"NEWS_PROCESS_BODIES",
"NNTP_PRINT_ARTICLE_HEADERS",
"NNTP_SEND_POST_DATA",
"NNTP_SEND_POST_DATA_RESPONSE",
"NNTP_CHECK_FOR_MESSAGE",
"NEWS_NEWS_RC_POST",
"NEWS_DISPLAY_NEWS_RC",
"NEWS_DISPLAY_NEWS_RC_RESPONSE",
"NEWS_START_CANCEL",
"NEWS_DO_CANCEL",
"NNTP_XPAT_SEND",
"NNTP_XPAT_RESPONSE",
"NNTP_SEARCH",
"NNTP_SEARCH_RESPONSE",
"NNTP_SEARCH_RESULTS",
"NNTP_LIST_PRETTY_NAMES",
"NNTP_LIST_PRETTY_NAMES_RESPONSE",
"NNTP_LIST_XACTIVE_RESPONSE",
"NNTP_LIST_XACTIVE",
"NNTP_LIST_GROUP",
"NNTP_LIST_GROUP_RESPONSE",
"NEWS_DONE",
"NEWS_POST_DONE",
"NEWS_ERROR",
"NNTP_ERROR",
"NEWS_FREE",
"NEWS_FINISHED"
};


/* end logging */

/* Forward declarations */

#define LIST_WANTED     0
#define ARTICLE_WANTED  1
#define CANCEL_WANTED   2
#define GROUP_WANTED    3
#define NEWS_POST       4
#define READ_NEWS_RC    5
#define NEW_GROUPS      6
#define SEARCH_WANTED   7
#define PRETTY_NAMES_WANTED 8
#define PROFILE_WANTED	9
#define IDS_WANTED		10

/* the output_buffer_size must be larger than the largest possible line
 * 2000 seems good for news
 *
 * jwz: I increased this to 4k since it must be big enough to hold the
 * entire button-bar HTML, and with the new "mailto" format, that can
 * contain arbitrarily long header fields like "references".
 *
 * fortezza: proxy auth is huge, buffer increased to 8k (sigh).
 */
#define OUTPUT_BUFFER_SIZE (4096*2)

/* the amount of time to subtract from the machine time
 * for the newgroup command sent to the nntp server
 */
#define NEWGROUPS_TIME_OFFSET 60L*60L*12L   /* 12 hours */

/* Allow the user to open at most this many connections to one news host*/
#define kMaxConnectionsPerHost 3

/* Keep this many connections cached. The cache is global, not per host */
#define kMaxCachedConnections 2

/* globals */
/* mscott: I wonder if we still need these? I'd like to abstract them out into a NNTP protocol manager
   (the object that is going to manage the NNTP connections. it would keep track of the connection list.)
*/
/* PRIVATE XP_List * nntp_connection_list=0; */
PRInt32 net_NewsChunkSize=DEFAULT_NEWS_CHUNK_SIZE; 
/* PRIVATE PRInt32 net_news_timeout = 170; */
/* seconds that an idle NNTP conn can live */

#if 0
static char * last_password = 0;
static char * last_password_hostname = 0;
static char * last_username=0;
static char * last_username_hostname=0;
#endif /* 0 */
/* end of globals I'd like to move somewhere else */


////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////
#ifdef XP_WIN
char *XP_AppCodeName = "Mozilla";
#else
const char *XP_AppCodeName = "Mozilla";
#endif
#define NET_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))

char * NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    PR_Free(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
char * NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

char *MSG_UnEscapeSearchUrl (const char *commandSpecificData)
{
	char *result = (char*) PR_Malloc (PL_strlen(commandSpecificData) + 1);
	if (result)
	{
		char *resultPtr = result;
		while (1)
		{
			char ch = *commandSpecificData++;
			if (!ch)
				break;
			if (ch == '\\')
			{
				char scratchBuf[3];
				scratchBuf[0] = (char) *commandSpecificData++;
				scratchBuf[1] = (char) *commandSpecificData++;
				scratchBuf[2] = '\0';
				int accum = 0;
				PR_sscanf(scratchBuf, "%X", &accum);
				*resultPtr++ = (char) accum;
			}
			else
				*resultPtr++ = ch;
		}
		*resultPtr = '\0';
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////////////////////
// END OF TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(nsDummyBufferStream)
NS_IMPL_RELEASE(nsDummyBufferStream)
NS_IMETHODIMP 
nsDummyBufferStream::QueryInterface(REFNSIID aIID, void** result)
{
    if (!result) return NS_ERROR_NULL_POINTER;
    *result = nsnull;
    if (aIID.Equals(NS_GET_IID(nsIInputStream)))
        *result = NS_STATIC_CAST(nsIInputStream*, this);
    if (*result)
    {
        AddRef();
        return NS_OK;
    }
    return NS_ERROR_NO_INTERFACE;
}

NS_IMPL_ADDREF_INHERITED(nsNNTPProtocol, nsMsgProtocol)
NS_IMPL_RELEASE_INHERITED(nsNNTPProtocol, nsMsgProtocol)

NS_INTERFACE_MAP_BEGIN(nsNNTPProtocol)
    NS_INTERFACE_MAP_ENTRY(nsINNTPProtocol)
	NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsMsgProtocol)

nsNNTPProtocol::nsNNTPProtocol(nsIURI * aURL, nsIMsgWindow *aMsgWindow)
    : nsMsgProtocol(aURL)
{
	if (!NNTP)
		NNTP = PR_NewLogModule("NNTP");

    m_ProxyServer = nsnull;
    m_lineStreamBuffer = nsnull;
    m_responseText = nsnull;
    m_dataBuf = nsnull;
    m_path = nsnull;
    
	m_cancelFromHdr = nsnull;
	m_cancelNewsgroups = nsnull;
	m_cancelDistribution = nsnull;
	m_cancelID = nsnull;

	m_messageID = nsnull;

    m_commandSpecificData = nsnull;
    m_searchData = nsnull;
	mBytesReceived = 0;

    if (aMsgWindow) {
        m_msgWindow = aMsgWindow;
    }

	m_runningURL = null_nsCOMPtr();
  m_connectionBusy = PR_FALSE;
  m_fromCache = PR_FALSE;
  LL_I2L(m_lastActiveTimeStamp, 0);
}

nsNNTPProtocol::~nsNNTPProtocol()
{
    if (m_nntpServer) {
        m_nntpServer->WriteNewsrcFile();
        m_nntpServer->RemoveConnection(this);
    }
    if (m_lineStreamBuffer) {
        delete m_lineStreamBuffer;
    }
    if (mUpdateTimer) {
      mUpdateTimer->Cancel();
	  mUpdateTimer = nsnull;
	}
}

NS_IMETHODIMP nsNNTPProtocol::Initialize(nsIURI * aURL, nsIMsgWindow *aMsgWindow)
{
    nsresult rv = NS_OK;
    net_NewsChunkSize = DEFAULT_NEWS_CHUNK_SIZE;
    PRBool isSecure = PR_FALSE;

    if (aMsgWindow) {
        m_msgWindow = aMsgWindow;
    }
    nsMsgProtocol::InitFromURI(aURL);

	rv = m_url->GetHost(getter_Copies(m_hostName));
	if (NS_FAILED(rv)) return rv;
	rv = m_url->GetPreHost(getter_Copies(m_userName));
	if (NS_FAILED(rv)) return rv;

    // retrieve the AccountManager
    NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, NS_MSGACCOUNTMANAGER_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    // find the server
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = accountManager->FindServer(m_userName,
                                    m_hostName,
                                    "nntp",
                                    getter_AddRefs(server));
	if (NS_FAILED(rv)) return rv;
	if (!server) return NS_ERROR_FAILURE;
    
	m_nntpServer = do_QueryInterface(server, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!m_nntpServer) return NS_ERROR_FAILURE;

	PRInt32 max_articles;
	rv = m_nntpServer->GetMaxArticles(&max_articles);
	if (NS_SUCCEEDED(rv)) {
		net_NewsChunkSize = max_articles;
	}

    rv = server->GetIsSecure(&isSecure);
    if (NS_FAILED(rv)) return rv;

    PRInt32 port = 0;
    rv = m_url->GetPort(&port);
    if (NS_FAILED(rv) || (port<=0)) {
		rv = server->GetPort(&port);
        if (NS_FAILED(rv)) return rv;

		if (port<=0) {
			if (isSecure) {
            	port = SECURE_NEWS_PORT;
        	}
        	else {
            	port = NEWS_PORT;
        	}
		}

        rv = m_url->SetPort(port);
        if (NS_FAILED(rv)) return rv;
    }

	NS_PRECONDITION(m_url, "invalid URL passed into NNTP Protocol");

	// Right now, we haven't written an nsNNTPURL yet. When we do, we'll pull the event sink
	// data out of it and set our event sink member variables from it. For now, just set them
	// to NULL. 
	
	// query the URL for a nsINNTPUrl
   
	m_runningURL = do_QueryInterface(m_url);
  m_connectionBusy = PR_TRUE;
	if (NS_SUCCEEDED(rv) && m_runningURL)
	{
  	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
    if (mailnewsUrl)
      mailnewsUrl->SetMsgWindow(aMsgWindow);
		// okay, now fill in our event sinks...Note that each getter ref counts before
		// it returns the interface to us...we'll release when we are done
		m_runningURL->GetNewsgroupList(getter_AddRefs(m_newsgroupList));
		m_runningURL->GetNntpArticleList(getter_AddRefs(m_articleList));
		m_runningURL->GetNntpHost(getter_AddRefs(m_newsHost));
		m_runningURL->GetNewsgroup(getter_AddRefs(m_newsgroup));
		m_runningURL->GetOfflineNewsState(getter_AddRefs(m_offlineNewsState));
	}
  else {
    return rv;
  }
	
  if (!m_socketIsOpen)
  {
  // call base class to set up the transport
    if (isSecure) {
	    rv = OpenNetworkSocket(m_url, "ssl-forcehandshake");
    }
    else {
	    rv = OpenNetworkSocket(m_url, nsnull);
    }
  }
	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_dataBufSize = OUTPUT_BUFFER_SIZE;

	m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, CRLF, PR_TRUE /* create new lines */);

	m_nextState = SEND_FIRST_NNTP_COMMAND;
  m_nextStateAfterResponse = NNTP_CONNECT;
	m_typeWanted = 0;
	m_responseCode = 0;
	m_previousResponseCode = 0;
	m_responseText = nsnull;

	m_currentGroup = "";
	m_path = nsnull;

	m_firstArticle = 0;
	m_lastArticle = 0;
	m_firstPossibleArticle = 0;
	m_lastPossibleArticle = 0;
	m_numArticlesLoaded = 0;
	m_numArticlesWanted = 0;

	m_newsRCListIndex = 0;
	m_newsRCListCount = 0;
	
	PR_FREEIF(m_messageID);
	m_messageID = nsnull;


	m_articleNumber = 0;
	m_originalContentLength = 0;
	m_cancelID = nsnull;
	m_cancelFromHdr = nsnull;
	m_cancelNewsgroups = nsnull;
	m_cancelDistribution = nsnull;
	return NS_OK;
}

nsresult
nsNNTPProtocol::InitializeNewsFolderFromUri(const char *uri)
{
        nsresult rv;

        NS_ENSURE_ARG_POINTER(uri);

		PR_LOG(NNTP,PR_LOG_ALWAYS,("InitializeNewsFolderFromUri(%s)",uri));

        NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
        if (NS_FAILED(rv)) return(rv);

        nsCOMPtr<nsIRDFResource> resource;
        rv = rdf->GetResource(uri, getter_AddRefs(resource));
        if (NS_FAILED(rv)) return(rv);

        m_newsFolder = do_QueryInterface(resource, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "uri is not for a news folder!"); 
        if (NS_FAILED(rv)) return rv;

        if (!m_newsFolder) return NS_ERROR_FAILURE;

        return NS_OK;
}

/* void IsBusy (out boolean aIsConnectionBusy); */
NS_IMETHODIMP nsNNTPProtocol::IsBusy(PRBool *aIsConnectionBusy)
{
  NS_ENSURE_ARG_POINTER(aIsConnectionBusy);
  *aIsConnectionBusy = m_connectionBusy;
  return NS_OK;
}

NS_IMETHODIMP nsNNTPProtocol::GetIsCachedConnection(PRBool *aIsCachedConnection)
{
  NS_ENSURE_ARG_POINTER(aIsCachedConnection);
  *aIsCachedConnection = m_fromCache;
  return NS_OK;
}

NS_IMETHODIMP nsNNTPProtocol::SetIsCachedConnection(PRBool aIsCachedConnection)
{
  m_fromCache = aIsCachedConnection;
  return NS_OK;
}

/* void GetLastActiveTimeStamp (out PRTime aTimeStamp); */
NS_IMETHODIMP nsNNTPProtocol::GetLastActiveTimeStamp(PRTime *aTimeStamp)
{
  NS_ENSURE_ARG_POINTER(aTimeStamp);
  *aTimeStamp = m_lastActiveTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP nsNNTPProtocol::LoadNewsUrl(nsIURI * aURL, nsISupports * aConsumer)
{
  // clear the previous channel listener and use the new one....
  // don't reuse an existing channel listener
  m_channelListener = nsnull;
  m_channelListener = do_QueryInterface(aConsumer);
  return LoadUrl(aURL, aConsumer);
}


nsresult nsNNTPProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer)
{
  NS_ENSURE_ARG_POINTER(aURL);

  PRBool bVal = PR_FALSE;
  nsXPIDLCString group;
  char *commandSpecificData = nsnull;
  PRBool cancel = PR_FALSE;
  m_ContentType = "";
  nsCOMPtr <nsINNTPNewsgroupPost> message;
  nsresult rv = NS_OK;

  // if this connection comes from the cache, we need to initialize the
  // load group here, by generating the start request notification.
  if (m_fromCache)
  {
	  if (m_channelListener)
		  rv = m_channelListener->OnStartRequest(this, m_channelContext);
  }

  m_articleNumber = -1;
  rv = aURL->GetHost(getter_Copies(m_hostName));
  if (NS_FAILED(rv)) return rv;
  rv = aURL->GetPreHost(getter_Copies(m_userName));
  if (NS_FAILED(rv)) return rv;

  m_runningURL = do_QueryInterface(aURL, &rv);
  if (NS_FAILED(rv)) return rv;
  m_connectionBusy = PR_TRUE;
  m_runningURL->GetNewsAction(&m_newsAction);

  // okay, now fill in our event sinks...Note that each getter ref counts before
  // it returns the interface to us...we'll release when we are done
  m_runningURL->GetNewsgroupList(getter_AddRefs(m_newsgroupList));
  m_runningURL->GetNntpArticleList(getter_AddRefs(m_articleList));
  m_runningURL->GetNntpHost(getter_AddRefs(m_newsHost));
  m_runningURL->GetNewsgroup(getter_AddRefs(m_newsgroup));
  m_runningURL->GetOfflineNewsState(getter_AddRefs(m_offlineNewsState));


  if (NS_FAILED(rv)) goto FAIL;

  PR_FREEIF(m_messageID);
  m_messageID = nsnull;
  rv = ParseURL(aURL,&bVal, getter_Copies(group), &m_messageID, &commandSpecificData);
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to parse news url");
  //if (NS_FAILED(rv)) return rv;

  // if we don't have a news host already, go get one...
  if (!m_newsHost)
  {
      m_newsHost = do_CreateInstance(kNNTPHostCID, &rv);
      if (NS_FAILED(rv) || (!m_newsHost)) goto FAIL;

      // m_newsHost holds m_runningURL (make this a weak reference)
      // m_runningURL holds m_newsHost
      // need to make sure there is no cycle. 

      // retrieve the AccountManager
      NS_WITH_SERVICE(nsIMsgAccountManager, accountManager, NS_MSGACCOUNTMANAGER_PROGID, &rv);
      if (NS_FAILED(rv)) goto FAIL;

      // find the incoming server
      nsCOMPtr<nsIMsgIncomingServer> server;
      rv = accountManager->FindServer(m_userName,
                                    m_hostName,
                                    "nntp",
                                    getter_AddRefs(server));
      if (NS_FAILED(rv) || !server) goto FAIL;

	  m_nntpServer = do_QueryInterface(server, &rv);
	  if (NS_FAILED(rv) || !m_nntpServer) goto FAIL;

      m_newsHost->Initialize(m_runningURL, m_userName, m_hostName);

	  // save it on our url for future use....
	  m_runningURL->SetNntpHost(m_newsHost);

	  if (NS_FAILED(rv)) goto FAIL;
  }

  if (NS_FAILED(rv)) 
	goto FAIL;

  if (m_messageID && commandSpecificData && !PL_strcmp (commandSpecificData, "?cancel"))
	cancel = PR_TRUE;

  NET_SACopy(&m_path, m_messageID);

  /* make sure the user has a news host configured */
#if UNREADY_CODE
  if (cancel /* && !ce->URL_s->internal_url */)
  {
	  /* Don't allow manual "news:ID?cancel" URLs, only internal ones. */
	  *status = -1;
	  goto FAIL;
  }
  else
#endif
  /* We are posting a user-written message
     if and only if this message has a message to post
     Cancel messages are created later with a ?cancel URL
  */
  rv = m_runningURL->GetMessageToPost(getter_AddRefs(message));
  if (NS_SUCCEEDED(rv) && message)
	{
#ifdef UNREADY_CODE
	  /* news://HOST done with a POST instead of a GET;
		 this means a new message is being posted.
		 Don't allow this unless it's an internal URL.
	   */
	  if (!ce->URL_s->internal_url)
		{
		  *status = -1;
		  goto FAIL;
		}
#endif
      /*	  NS_ASSERTION (!group && !message_id && !commandSpecificData, "something not null"); */
	  m_typeWanted = NEWS_POST;
	  NET_SACopy(&m_path, "");
	}
  else 
	if (m_messageID)
	{
	  /* news:MESSAGE_ID
		 news:/GROUP/MESSAGE_ID (useless)
		 news://HOST/MESSAGE_ID
		 news://HOST/GROUP/MESSAGE_ID (useless)
	   */
	  if (cancel)
		m_typeWanted = CANCEL_WANTED;
	  else
		m_typeWanted = ARTICLE_WANTED;
	}
  else if (commandSpecificData)
	{
	  if (PL_strstr (commandSpecificData, "?newgroups"))
	    /* news://HOST/?newsgroups
	        news:/GROUP/?newsgroups (useless)
	        news:?newsgroups (default host)
	     */
	    m_typeWanted = NEW_GROUPS;
    else
	  {
		  if (PL_strstr(commandSpecificData, "?list-pretty"))
		  {
			  m_typeWanted = PRETTY_NAMES_WANTED;
			  m_commandSpecificData = nsCRT::strdup(commandSpecificData);
		  }
		  else if (PL_strstr(commandSpecificData, "?profile"))
		  {
			  m_typeWanted = PROFILE_WANTED;
			  m_commandSpecificData = nsCRT::strdup(commandSpecificData);
		  }
		  else if (PL_strstr(commandSpecificData, "?list-ids"))
		  {
			  m_typeWanted= IDS_WANTED;
			  m_commandSpecificData = nsCRT::strdup(commandSpecificData);
		  }
		  else
		  {
			  m_typeWanted = SEARCH_WANTED;
			  m_commandSpecificData = nsCRT::strdup(commandSpecificData);
			  m_searchData = m_commandSpecificData;
        nsUnescape(m_searchData);
      }
	  }
	}
  else if (group)
	{
	  /* news:GROUP
		 news:/GROUP
		 news://HOST/GROUP
	   */
	  m_currentGroup.Assign(group);

	  if (PL_strchr ((const char *)m_currentGroup, '*')) {
		m_typeWanted = LIST_WANTED;
	  }
	  else {
		nsXPIDLCString newsgroupURI;
		rv = aURL->GetSpec(getter_Copies(newsgroupURI));
		if (NS_FAILED(rv)) return(rv);
    
		PRBool containsGroup = PR_TRUE;
		NS_ASSERTION(m_nntpServer,"no nntp server");
		if (m_nntpServer) {
			rv = m_nntpServer->ContainsNewsgroup((const char *)m_currentGroup,&containsGroup);
			if (NS_FAILED(rv)) return rv;

			if (!containsGroup) {
				rv = m_nntpServer->SubscribeToNewsgroup((const char *)m_currentGroup);
				if (NS_FAILED(rv)) return rv;
			}
		}
				
		rv = InitializeNewsFolderFromUri((const char *)newsgroupURI);
		if (NS_FAILED(rv)) return rv;

		m_typeWanted = GROUP_WANTED;
	  }
	}
  else
	{
	  /* news:
	     news://HOST
	   */
	  m_typeWanted = READ_NEWS_RC;
	}

#ifdef UNREADY_CODE
  /* let's look for the user name and password in the url */
  {
		char * unamePwd = NET_ParseURL(url, GET_USERNAME_PART | GET_PASSWORD_PART);
		char *userName,*colon,*password=NULL;

		/* get the username & password out of the combo string */
		if( (colon = PL_strchr(unamePwd, ':')) != NULL ) 
		{
			*colon = '\0';
			userName = nsCRT::strdup(unamePwd);
			password = nsCRT::strdup(colon+1);
			*colon = ':';
			PR_FREEIF(unamePwd);
		}
		else 
		{
			userName = unamePwd;
		}
		if (userName)
		{
		    char *mungedUsername = XP_STRDUP(userName);
            cd->newsgroup->SetUsername(mungedUsername);
			PR_FREEIF(mungedUsername);
			PR_FREEIF(userName);
		}
		if (password)
		{
		    char *mungedPassword = XP_STRDUP(password);
            cd->newsgroup->SetPassword(mungedPassword);

			PR_FREEIF(mungedPassword);
			PR_FREEIF(password);
		}

    }
#endif
  /* At this point, we're all done parsing the URL, and know exactly
	 what we want to do with it.
   */
#ifdef UNREADY_CODE
#ifndef NO_ARTICLE_CACHEING
  /* Turn off caching on all news entities, except articles. */
  /* It's very important that this be turned off for CANCEL_WANTED;
	 for the others, I don't know what cacheing would cause, but
	 it could only do harm, not good. */
  if(m_typeWanted != ARTICLE_WANTED)
#endif
  	ce->format_out = CLEAR_CACHE_BIT (ce->format_out);
#endif

#ifdef UNREADY_CODE
  if (m_typeWanted == ARTICLE_WANTED)
  {
		PRUint32 number = 0;
        nsresult rv;
        PRBool articleIsOffline;
        rv = m_newsgroup->IsOfflineArticle(number,&articleIsOffline);

		if (NET_IsOffline() || (NS_SUCCEEDED(rv) && articleIsOffline))
		{
			ce->local_file = PR_TRUE;
			cd->articleIsOffline = articleIsOffline;
			ce->socket = NULL;

			if (!articleIsOffline)
				ce->format_out = CLEAR_CACHE_BIT(ce->format_out);
			NET_SetCallNetlibAllTheTime(ce->window_id,"mknews");
            
            rv = nsComponentManager::CreateInstance(kMsgOfflineNewsStateCID,
                                            nsnull,
                                            NS_GET_IID(nsIMsgOfflineNewsState),
                                            getter_AddRefs(cd->offline_state));

            if (NS_FAILED(rv) || ! cd->offline_state) {
                goto FAIL;
            }

            rv = cd->offline_state->Initialize(cd->newsgroup, number);
            if (NS_FAILED(rv)) goto FAIL;
		}
  }
  if (cd->offline_state)
	  goto FAIL;	/* we don't need to do any of this connection stuff */
#endif


  m_nextState = SEND_FIRST_NNTP_COMMAND;

 FAIL:
  PR_FREEIF (commandSpecificData);

  if (NS_FAILED(rv))
  {
	  AlertError(rv, nsnull);
      return rv;
  }
  else 
  {
	  if (!m_socketIsOpen)
	  {
		  m_nextStateAfterResponse = m_nextState;
		  m_nextState = NNTP_RESPONSE; 
	  }

	  rv = nsMsgProtocol::LoadUrl(aURL, aConsumer);
  }

  return rv;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsNNTPProtocol::OnStopRequest(nsIChannel * aChannel, nsISupports * aContext, nsresult aStatus, const PRUnichar* aMsg)
{
		nsMsgProtocol::OnStopRequest(aChannel, aContext, aStatus, aMsg);

    // nsMsgProtocol::OnStopRequest() has called m_channelListener. There is
    // no need to be called again in CloseSocket(). Let's clear it here.
    if (m_channelListener)
        m_channelListener = null_nsCOMPtr();

	// okay, we've been told that the send is done and the connection is going away. So 
	// we need to release all of our state

#if 0
    // cancel any outstanding udpate timer
    if (mUpdateTimer) {
      mUpdateTimer->Cancel();
	  mUpdateTimer = nsnull;
	}
#endif

	return CloseSocket();
}

NS_IMETHODIMP nsNNTPProtocol::Cancel(nsresult status)  // handle stop button
{
	m_nextState = NNTP_ERROR;
	return nsMsgProtocol::Cancel(NS_BINDING_ABORTED);
}


/* The main news load init routine, and URL parser.
   The syntax of news URLs is:

	 To list all hosts:
	   news:

	 To list all groups on a host, or to post a new message:
	   news://HOST

	 To list some articles in a group:
	   news:GROUP
	   news:/GROUP
	   news://HOST/GROUP

	 To list a particular range of articles in a group:
	   news:GROUP/N1-N2
	   news:/GROUP/N1-N2
	   news://HOST/GROUP/N1-N2

	 To retrieve an article:
	   news:MESSAGE-ID                (message-id must contain "@")

    To cancel an article:
	   news:MESSAGE-ID?cancel

	 (Note that message IDs may contain / before the @:)

	   news:SOME/ID@HOST?headers=all
	   news:/SOME/ID@HOST?headers=all
	   news:/SOME?ID@HOST?headers=all
        are the same as
	   news://HOST/SOME/ID@HOST?headers=all
	   news://HOST//SOME/ID@HOST?headers=all
	   news://HOST//SOME?ID@HOST?headers=all
        bug: if the ID is <//xxx@host> we will parse this correctly:
          news://non-default-host///xxx@host
        but will mis-parse it if it's on the default host:
          news://xxx@host
        whoever had the idea to leave the <> off the IDs should be flogged.
		So, we'll make sure we quote / in message IDs as %2F.

 */
nsresult nsNNTPProtocol::ParseURL(nsIURI * aURL, PRBool * bValP, char ** aGroup, char ** aMessageID,
								  char ** aCommandSpecificData)
{
	PRInt32 status = 0;
	nsXPIDLCString fullPath;
	char *group = 0;
    char *message_id = 0;
    char *command_specific_data = 0;
	char * s = 0;

	// get the file path part and store it as the group...
	aURL->GetPath(getter_Copies(fullPath));
	if ((const char *)fullPath && ((*(const char *)fullPath) == '/'))
		group = PL_strdup((const char *)fullPath+1); 
	else
		group = PL_strdup((const char *)fullPath);

	// more to do here, but for now, this works.
	// only escape if we are doing a search
	if (m_newsAction == nsINntpUrl::ActionSearch) { 
		nsUnescape (group);
	}

    /* "group" now holds the part after the host name:
	 "message@id?search" or "/group/xxx?search" or "/message?id@xx?search"

	 If there is an @, this is a message ID; else it is a group.
	 Either way, there may be search data at the end.
    */

	if (PL_strchr (group, '@') || PL_strstr(group,"%40")) {
      message_id = nsUnescape(group);
	  group = 0;
	}
    else if (!*group)
	{
   	  nsCRT::free (group);
	  group = 0;
	}

    /* At this point, the search data is attached to `message_id' (if there
	 is one) or `group' (if there is one) or `host_and_port' (otherwise.)
	 Seperate the search data from what it is clinging to, being careful
	 to interpret the "?" only if it comes after the "@" in an ID, since
	 the syntax of message IDs is tricky.  (There may be a ? in the
	 random-characters part of the ID (before @), but not in the host part
	 (after @).)
   */
   if (message_id || group)
   {
	  char *start;
	  if (message_id) {
		  start = PL_strchr(message_id,'@');
	  }
	  else {
		  start = group; /*  ? group : hostAndPort; */ // mscott -- fix me...necko sloppiness on my part
	  }

	  /* Take off the "?" or "#" search data */
	  for (s = start; *s; s++)
		if (*s == '?' || *s == '#')
		  break;
	  
	  if (*s)
	  {
		  command_specific_data = nsCRT::strdup (s);
		  *s = 0;
		  if (!command_specific_data)
			{
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
			}
		}

	  /* Discard any now-empty strings. */
	  if (message_id && !*message_id)
		{
		  PR_Free (message_id);
		  message_id = 0;
		}
	  else if (group && !*group)
		{
		  PR_Free (group);
		  group = 0;
		}
	}

  FAIL:
  NS_ASSERTION (!message_id || message_id != group, "something not null");
  if (status >= 0)
  {  
      if (aGroup) 
		  *aGroup = group;
      else 
		  nsCRT::free(group);
      
      if (aMessageID) *aMessageID = message_id;
      else PR_FREEIF(message_id);
      
	  if (aCommandSpecificData) 
		  *aCommandSpecificData = command_specific_data;
      else PR_FREEIF(command_specific_data);
  }
  else
  {
	  PR_FREEIF (group);
	  PR_FREEIF (message_id);
	  PR_FREEIF (command_specific_data);
  }

  // mscott - this function might need to be re-written to use nsresults
  // so we don't lose the nature of the error in this return code I'm adding.
  if (status < 0)
	  return NS_ERROR_FAILURE;
  else
	  return NS_OK;
}
/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

PRInt32 nsNNTPProtocol::SendData(nsIURI * aURL, const char * dataBuffer)
{
	NNTP_LOG_WRITE(dataBuffer);
#ifdef DEBUG_sspitzer
	printf("SEND: %s\n",dataBuffer);
#endif
	return nsMsgProtocol::SendData(aURL, dataBuffer); // base class actually transmits the data
}

/* gets the response code from the nntp server and the
 * response line
 *
 * returns the TCP return code from the read
 */
PRInt32 nsNNTPProtocol::NewsResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char * line = nsnull;
	PRUint32 status = 0;
	nsresult rv;

	NS_PRECONDITION(nsnull != inputStream, "invalid input stream");
	
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

    if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	if(!line)
		return status;

    ClearFlag(NNTP_PAUSE_FOR_READ);  /* don't pause if we got a line */

    /* almost correct */
    if(status > 1)
	{
#ifdef UNREADY_CODE
        ce->bytes_received += status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->content_length);
#else
		mBytesReceived += status;
#endif
	}

    NET_SACopy(&m_responseText, line+4);

	m_previousResponseCode = m_responseCode;

    PR_sscanf(line, "%d", &m_responseCode);
#ifdef DEBUG_sspitzer
	printf("RECV: %s\n",line);
#endif
	
	if (m_responseCode == MK_NNTP_RESPONSE_AUTHINFO_DENIED) {
		/* login failed */
		AlertError(MK_NNTP_AUTH_FAILED, m_responseText);

		/* forget the password & username, since login failed */
		if (m_newsFolder) {
			rv = m_newsFolder->ForgetGroupUsername();
			rv = m_newsFolder->ForgetGroupPassword();	
		}
    }

	/* authentication required can come at any time
	 */
	if (MK_NNTP_RESPONSE_AUTHINFO_REQUIRE == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_REQUIRE == m_responseCode) 
	{
		m_nextState = NNTP_BEGIN_AUTHORIZE;
	}
	else if (MK_NNTP_RESPONSE_PERMISSION_DENIED == m_responseCode)
	{
		PR_FREEIF(line);
		return (0);
	}
	else {
    	m_nextState = m_nextStateAfterResponse;
    }

	PR_FREEIF(line);
    return(0);  /* everything ok */
}

/* interpret the server response after the connect
 *
 * returns negative if the server responds unexpectedly
 */
 
PRInt32 nsNNTPProtocol::LoginResponse()
{
	PRBool postingAllowed = m_responseCode == MK_NNTP_RESPONSE_POSTING_ALLOWED;

    if(MK_NNTP_RESPONSE_TYPE(m_responseCode)!=MK_NNTP_RESPONSE_TYPE_OK)
	{
		AlertError(MK_NNTP_ERROR_MESSAGE, m_responseText);

    	m_nextState = NNTP_ERROR;
#ifdef UNREADY_CODE
        cd->control_con->prev_cache = PR_FALSE; /* to keep if from reconnecting */
#endif
        return MK_BAD_NNTP_CONNECTION;
	}

	//  ###mscott: I'm commenting this out right now since phil has a comment saying it is dead code...
	//	cd->control_con->posting_allowed = postingAllowed; /* ###phil dead code */
    
	m_newsHost->SetPostingAllowed(postingAllowed);
    m_nextState = NNTP_SEND_MODE_READER;
    return(0);  /* good */
}

PRInt32 nsNNTPProtocol::SendModeReader()
{  
	nsresult status = NS_OK;
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, NNTP_CMD_MODE_READER); 
    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_SEND_MODE_READER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ); 
    return(status);
}

PRInt32 nsNNTPProtocol::SendModeReaderResponse()
{
	SetFlag(NNTP_READER_PERFORMED);
	/* ignore the response code and continue
	 */
    PRBool pushAuth;
    nsresult rv = m_newsHost->GetPushAuth(&pushAuth);
    if (NS_SUCCEEDED(rv) && pushAuth)
		/* if the news host is set up to require volunteered (pushed) authentication,
		 * do that before we do anything else
		 */
		m_nextState = NNTP_BEGIN_AUTHORIZE;
	else
		m_nextState = SEND_LIST_EXTENSIONS;

	return(0);
}

PRInt32 nsNNTPProtocol::SendListExtensions()
{
	PRInt32 status = 0;
	nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
	if (url)
		status = SendData(url, NNTP_CMD_LIST_EXTENSIONS);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = SEND_LIST_EXTENSIONS_RESPONSE;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return status;
}

PRInt32 nsNNTPProtocol::SendListExtensionsResponse(nsIInputStream * inputStream, PRUint32 length)
{
	PRUint32 status = 0; 

	if (MK_NNTP_RESPONSE_TYPE(m_responseCode) == MK_NNTP_RESPONSE_TYPE_OK)
	{
		char *line = nsnull;

		PRBool pauseForMoreData = PR_FALSE;
		line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

		if(pauseForMoreData)
		{
			SetFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		}
		if (!line)
			return status;  /* no line yet */

		if ('.' != line[0])
            m_newsHost->AddExtension(line);
		else
		{
			/* tell libmsg that it's ok to ask this news host for extensions */		
			m_newsHost->SetSupportsExtensions(PR_TRUE);
			/* all extensions received */
			m_nextState = SEND_LIST_SEARCHES;
			ClearFlag(NNTP_PAUSE_FOR_READ);
		}
	}
	else
	{
		/* LIST EXTENSIONS not recognized 
		 * tell libmsg not to ask for any more extensions and move on to
		 * the real NNTP command we were trying to do. */
		 
		 m_newsHost->SetSupportsExtensions(PR_FALSE);
		 m_nextState = SEND_FIRST_NNTP_COMMAND;
	}

	return status;
}

PRInt32 nsNNTPProtocol::SendListSearches()
{  
    nsresult rv;
    PRBool searchable=PR_FALSE;
	PRInt32 status = 0;
    
    rv = m_newsHost->QueryExtension("SEARCH",&searchable);
    if (NS_SUCCEEDED(rv) && searchable)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, NNTP_CMD_LIST_SEARCHES);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = SEND_LIST_SEARCHES_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since SEARCH isn't supported, move on to GET */
		m_nextState = NNTP_GET_PROPERTIES;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return status;
}

PRInt32 nsNNTPProtocol::SendListSearchesResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line = nsnull;
	PRUint32 status = 0;

	NS_PRECONDITION(inputStream, "invalid input stream");
	
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if (!line)
		return status;  /* no line yet */

	if ('.' != line[0])
	{
		m_newsHost->AddSearchableGroup(line);
	}
	else
	{
		/* all searchable groups received */
		/* LIST SRCHFIELDS is legal if the server supports the SEARCH extension, which */
		/* we already know it does */
		m_nextState = NNTP_LIST_SEARCH_HEADERS;
		ClearFlag(NNTP_PAUSE_FOR_READ); 
	}

	PR_FREEIF(line);
	return status;
}

PRInt32 nsNNTPProtocol::SendListSearchHeaders()
{
	PRInt32 status = 0;
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, NNTP_CMD_LIST_SEARCH_FIELDS);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_SEARCH_HEADERS_RESPONSE;
	SetFlag(NNTP_PAUSE_FOR_READ);

	return status;
}

PRInt32 nsNNTPProtocol::SendListSearchHeadersResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line = nsnull;
	PRUint32 status = 0; 

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if (!line)
		return status;  /* no line yet */

	if ('.' != line[0])
        m_newsHost->AddSearchableHeader(line);
	else
	{
		m_nextState = NNTP_GET_PROPERTIES;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	PR_FREEIF(line);
	return status;
}

PRInt32 nsNNTPProtocol::GetProperties()
{
    nsresult rv;
    PRBool setget=PR_FALSE;
	PRInt32 status = 0; 
    
    rv = m_newsHost->QueryExtension("SETGET",&setget);
    if (NS_SUCCEEDED(rv) && setget)
	{
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, NNTP_CMD_GET_PROPERTIES);
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_GET_PROPERTIES_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since GET isn't supported, move on LIST SUBSCRIPTIONS */
		m_nextState = SEND_LIST_SUBSCRIPTIONS;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	return status;
}

PRInt32 nsNNTPProtocol::GetPropertiesResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line = NULL;
	PRUint32 status = 0;

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if (!line)
		return status;  /* no line yet */

	if ('.' != line[0])
	{
		char *propertyName = nsCRT::strdup(line);
		if (propertyName)
		{
			char *space = PL_strchr(propertyName, ' ');
			if (space)
			{
				char *propertyValue = space + 1;
				*space = '\0';
                m_newsHost->AddPropertyForGet(propertyName, propertyValue);
			}
			PR_Free(propertyName);
		}
	}
	else
	{
		/* all GET properties received, move on to LIST SUBSCRIPTIONS */
		m_nextState = SEND_LIST_SUBSCRIPTIONS;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	PR_FREEIF(line);
	return status;
}

PRInt32 nsNNTPProtocol::SendListSubscriptions()
{
   PRInt32 status = 0; 
#if 0
    nsresult rv;
    PRBool searchable=PR_FALSE;
    rv = m_newsHost->QueryExtension("LISTSUBSCR",&listsubscr);
    if (NS_SUCCEEDED(rv) && listsubscr)
#else
	if (0)
#endif
	{
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, NNTP_CMD_LIST_SUBSCRIPTIONS);
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = SEND_LIST_SUBSCRIPTIONS_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		/* since LIST SUBSCRIPTIONS isn't supported, move on to real work */
		m_nextState = SEND_FIRST_NNTP_COMMAND;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	return status;
}

PRInt32 nsNNTPProtocol::SendListSubscriptionsResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line = NULL;
	PRUint32 status = 0;

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if (!line)
		return status;  /* no line yet */

	if ('.' != line[0])
	{
#if 0
		char *url = PR_smprintf ("%s//%s/%s", NEWS_SCHEME, m_hostName, line);
		if (url)
			MSG_AddSubscribedNewsgroup (cd->pane, url);
#endif
	}
	else
	{
		/* all default subscriptions received */
		m_nextState = SEND_FIRST_NNTP_COMMAND;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}

	PR_FREEIF(line);
	return status;
}

/* figure out what the first command is and send it
 *
 * returns the status from the NETWrite */

PRInt32 nsNNTPProtocol::SendFirstNNTPCommand(nsIURI * url)
{
	char *command=0;
	PRInt32 status = 0;

	if (m_typeWanted == ARTICLE_WANTED)
	  {
		nsresult rv1,rv2;
       
		nsMsgKey key = nsMsgKey_None;
		rv1 = m_runningURL->GetMessageKey(&key);

        nsXPIDLCString newsgroupName;
        rv2 = m_runningURL->GetNewsgroupName(getter_Copies(newsgroupName));
		
		if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2) && ((const char *)newsgroupName) && (key != nsMsgKey_None))
		  {
			m_articleNumber = key;
     
			if (((const char *)m_currentGroup) && !PL_strcmp ((const char *)m_currentGroup, (const char *)newsgroupName))
			  m_nextState = NNTP_SEND_ARTICLE_NUMBER;
			else
			  m_nextState = NNTP_SEND_GROUP_FOR_ARTICLE;

			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		  }
	  }

	if(m_typeWanted == NEWS_POST)
      {  /* posting to the news group */
        NET_SACopy(&command, "POST");
      }
    else if(m_typeWanted == READ_NEWS_RC)
      {
		/* extract post method from the url when we have it... */
#ifdef HAVE_NEWS_URL
		if(ce->URL_s->method == URL_POST_METHOD ||
								PL_strchr(ce->URL_s->address, '?'))
        	m_nextState = NEWS_NEWS_RC_POST;
		else
#endif
        	m_nextState = NEWS_DISPLAY_NEWS_RC;
		return(0);
      } 
	else if(m_typeWanted == NEW_GROUPS)
	{
        PRUint32 last_update;
        nsresult rv;
		
		if (!m_newsHost) {
			NNTP_LOG_NOTE("m_newsHost is null, panic!");
			return -1;
		}
        rv = m_newsHost->GetLastUpdatedTime(&last_update);
		char small_buf[64];
        PRExplodedTime  expandedTime;

		if(!last_update)
		{
			AlertError(MK_NNTP_NEWSGROUP_SCAN_ERROR, nsnull);
			m_nextState = NEWS_ERROR;
			return(MK_INTERRUPTED);
		}
	
		/* subtract some hours just to be sure */
		last_update -= NEWGROUPS_TIME_OFFSET;
	
        {
           PRInt64  secToUSec, timeInSec, timeInUSec;
           LL_I2L(timeInSec, last_update);
           LL_I2L(secToUSec, PR_USEC_PER_SEC);
           LL_MUL(timeInUSec, timeInSec, secToUSec);
           PR_ExplodeTime(timeInUSec, PR_LocalTimeParameters, &expandedTime);
        }
		PR_FormatTimeUSEnglish(small_buf, sizeof(small_buf), 
                               "NEWGROUPS %y%m%d %H%M%S", &expandedTime);
		
        NET_SACopy(&command, small_buf);

	}
    else if(m_typeWanted == LIST_WANTED)
    {
	
		ClearFlag(NNTP_USE_FANCY_NEWSGROUP);
        PRUint32 last_update;
      	
		if (!m_newsHost) {
			NNTP_LOG_NOTE("m_newsHost is null, panic!");
			return -1;
		}
		nsresult rv = m_newsHost->GetLastUpdatedTime(&last_update);
        
        if (NS_SUCCEEDED(rv) && last_update!=0)
		{
			m_nextState = DISPLAY_NEWSGROUPS;
        	return(0);
	    }
		else
		{
#ifdef UNREADY_CODE
#ifdef BUG_21013
			if(!FE_Confirm(ce->window_id, XP_GetString(XP_CONFIRM_SAVE_NEWSGROUPS)))
	  		  {
				m_nextState = NEWS_ERROR;
				return(MK_INTERRUPTED);
	  		  }
#endif /* BUG_21013 */
#endif
			PRBool xactive=PR_FALSE;
			rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
			if (NS_SUCCEEDED(rv) && xactive)
			{
				NET_SACopy(&command, "LIST XACTIVE");
				SetFlag(NNTP_USE_FANCY_NEWSGROUP);
			}
			else
			{
				NET_SACopy(&command, "LIST");
			}
		}
	}
	else if(m_typeWanted == GROUP_WANTED) 
    {
        /* Don't use MKParse because the news: access URL doesn't follow traditional
         * rules. For instance, if the article reference contains a '#',
         * the rest of it is lost.
         */
        char * slash;
        char * group_name;
        nsresult rv=NS_ERROR_NULL_POINTER;

        NET_SACopy(&command, "GROUP ");
        if (!m_newsgroup) {
			NNTP_LOG_NOTE("m_newsHost is null, panic!");
			return -1;
		}
            
		rv = m_newsgroup->GetName(&group_name);
        slash = PL_strchr(group_name, '/');
		
        m_firstArticle = 0;
        m_lastArticle = 0;
        if (slash)
		{
            *slash = '\0';
            (void) PR_sscanf(slash+1, "%d-%d", &m_firstArticle, &m_lastArticle);
		}

        m_currentGroup = group_name;
        NET_SACat(&command, (const char *)m_currentGroup);
		PR_FREEIF(group_name);
      }
	else if (m_typeWanted == SEARCH_WANTED)
	{
		nsresult rv;
		PRBool searchable=PR_FALSE;
		if (!m_newsHost) {
			NNTP_LOG_NOTE("m_newsHost is null, panic!");
			return -1;
		}
		rv = m_newsHost->QueryExtension("SEARCH", &searchable);
		if (NS_SUCCEEDED(rv) && searchable)
		{
			/* use the SEARCH extension */
			char *slash = PL_strchr (m_commandSpecificData, '/');
			if (slash)
			{
				char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
				if (allocatedCommand)
				{
					NET_SACopy (&command, allocatedCommand);
					PR_Free(allocatedCommand);
				}
			}
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_SEARCH_RESPONSE;
		}
		else
		{
            nsXPIDLCString group_name;
            
			/* for XPAT, we have to GROUP into the group before searching */
			NET_SACopy(&command, "GROUP ");
			if (!m_newsgroup) {
				NNTP_LOG_NOTE("m_newsHost is null, panic!");
				return -1;
			}
            rv = m_newsgroup->GetName(getter_Copies(group_name));
            NET_SACat (&command, group_name);
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_XPAT_SEND;
		}
	}
	else if (m_typeWanted == PRETTY_NAMES_WANTED)
	{
		nsresult rv;
		PRBool listpretty=PR_FALSE;
		rv = m_newsHost->QueryExtension("LISTPRETTY",&listpretty);
		if (NS_SUCCEEDED(rv) && listpretty)
		{
			m_nextState = NNTP_LIST_PRETTY_NAMES;
			return 0;
		}
		else
		{
			NS_ASSERTION(PR_FALSE, "unexpected");
			m_nextState = NNTP_ERROR;
		}
	}
	else if (m_typeWanted == PROFILE_WANTED)
	{
		char *slash = PL_strchr (m_commandSpecificData, '/');
		if (slash)
		{
			char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
			if (allocatedCommand)
			{
				NET_SACopy(&command, allocatedCommand);
				PR_Free(allocatedCommand);
			}
		}
		m_nextState = NNTP_RESPONSE;
#ifdef UNREADY_CODE
		if (PL_strstr(ce->URL_s->address, "PROFILE NEW"))
			m_nextStateAfterResponse = NNTP_PROFILE_ADD_RESPONSE;
		else
#endif
			m_nextStateAfterResponse = NNTP_PROFILE_DELETE_RESPONSE;
	}
	else if (m_typeWanted == IDS_WANTED)
	{
		m_nextState = NNTP_LIST_GROUP;
		return 0;
	}
    else  /* article or cancel */
	{
		if (m_typeWanted == CANCEL_WANTED)
			NET_SACopy(&command, "HEAD ");
		else
			NET_SACopy(&command, "ARTICLE ");
		if (*m_path != '<')
			NET_SACat(&command,"<");
		NET_SACat(&command, m_path);
		if (PL_strchr(command+8, '>')==0) 
			NET_SACat(&command,">");
	}

    NET_SACat(&command, CRLF);
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, command);
    PR_Free(command);

	m_nextState = NNTP_RESPONSE;
	if (m_typeWanted != SEARCH_WANTED && m_typeWanted != PROFILE_WANTED)
		m_nextStateAfterResponse = SEND_FIRST_NNTP_COMMAND_RESPONSE;
	SetFlag(NNTP_PAUSE_FOR_READ);
    return(status);

} /* sent first command */


/* interprets the server response from the first command sent
 *
 * returns negative if the server responds unexpectedly 
 */

PRInt32 nsNNTPProtocol::SendFirstNNTPCommandResponse()
{
	PRInt32 status = 0;
	PRInt32 major_opcode = MK_NNTP_RESPONSE_TYPE(m_responseCode);

	if((major_opcode == MK_NNTP_RESPONSE_TYPE_CONT &&
        m_typeWanted == NEWS_POST)
	 	|| (major_opcode == MK_NNTP_RESPONSE_TYPE_OK &&
            m_typeWanted != NEWS_POST) )
      {

        m_nextState = SETUP_NEWS_STREAM;
		SetFlag(NNTP_SOME_PROTOCOL_SUCCEEDED);
        return(0);  /* good */
      }
    else
      {
		if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP &&
            m_typeWanted == GROUP_WANTED) {
			NNTP_LOG_NOTE("group not found!");
#ifdef UNREADY_CODE
			MSG_GroupNotFound(cd->pane, cd->host, cd->control_con->current_group, PR_TRUE /* opening group */);
#else
            m_newsHost->GroupNotFound((const char *)m_currentGroup,
                                          PR_TRUE /* opening */);
#endif
        }

        /* if the server returned a 400 error then it is an expected
         * error.  the NEWS_ERROR state will not sever the connection
         */
        if(major_opcode == MK_NNTP_RESPONSE_TYPE_CANNOT)
          m_nextState = NEWS_ERROR;
        else
          m_nextState = NNTP_ERROR;

        nsresult rv = NS_OK;
        nsXPIDLCString group_name;

        if (m_newsgroup) {
            rv = m_newsgroup->GetName(getter_Copies(group_name));
        }

        if (NS_SUCCEEDED(rv) && group_name) {
			//  the right thing todo is:  
            //  get the right docshell, gotten from the nsIMsgWindow
            //  build up a data url
            //  call (docshell)->LoadURL() with that data url
#ifdef NOT_WORKING_YET
        	char outputBuffer[OUTPUT_BUFFER_SIZE];
            PRUint32 count = 0;
            
            nsXPIDLString newsErrorStr;
			GetNewsStringByName("htmlNewsError", getter_Copies(newsErrorStr));
			nsCAutoString cString(newsErrorStr);
            PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE, (const char *) cString, m_responseText);
			mDisplayOutputStream->Write(outputBuffer, PL_strlen(outputBuffer), &count);
            
			GetNewsStringByName("articleExpired", getter_Copies(newsErrorStr));
			nsCAutoString cString2(newsErrorStr);
            PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE, (const char *) cString2);
			mDisplayOutputStream->Write(outputBuffer, PL_strlen(outputBuffer), &count);
            
			nsMsgKey key = nsMsgKey_None;
			rv = m_runningURL->GetMessageKey(&key);
            NS_ASSERTION(m_messageID && (key != nsMsgKey_None), "unexpected");
			if (m_messageID && (key != nsMsgKey_None)) {
				PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE,"<P>&lt;%.512s&gt; (%lu)", m_messageID, key);
				mDisplayOutputStream->Write(outputBuffer, PL_strlen(outputBuffer), &count);
			}

			GetNewsStringByName("removeExpiredArtLinkText", getter_Copies(newsErrorStr));
			nsCAutoString cString3(newsErrorStr);
            if (m_userName) {
                PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE,"<P> <A HREF=\"%s/%s@%s/%s?list-ids\">%s</A> </P>\n", kNewsRootURI, (const char *)m_userName, (const char *)m_hostName, 
					(const char *) group_name, (const char *) cString3);
            }
            else {
                PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE,"<P> <A HREF=\"%s/%s/%s?list-ids\">%s</A> </P>\n", kNewsRootURI, (const char *)m_hostName, 
					(const char *) group_name, (const char *) cString3);
            }
			mDisplayOutputStream->Write(outputBuffer, PL_strlen(outputBuffer), &count);
#endif /* NOT_WORKING_YET */
        }
		return MK_NNTP_SERVER_ERROR;
      }

	/* start the graph progress indicator
     */
#ifdef UNREADY_CODE
    FE_GraphProgressInit(ce->window_id, ce->URL_s, ce->URL_s->content_length);
#else
	NNTP_LOG_NOTE("start the graph progress indicator");
#endif
	SetFlag(NNTP_DESTROY_PROGRESS_GRAPH);
#ifdef UNREADY_CODE
    m_originalContentLength = ce->URL_s->content_length;
#endif
	return(status);
}

PRInt32 nsNNTPProtocol::SendGroupForArticle()
{
  nsresult rv;
  PRInt32 status = 0; 

  nsXPIDLCString groupname;
  rv = m_newsgroup->GetName(getter_Copies(groupname));
  NS_ASSERTION(NS_SUCCEEDED(rv) && (const char *)groupname && nsCRT::strlen((const char *)groupname), "no group name");

  char outputBuffer[OUTPUT_BUFFER_SIZE];
  
  PR_snprintf(outputBuffer, 
			OUTPUT_BUFFER_SIZE, 
			"GROUP %.512s" CRLF, 
			(const char *)groupname);

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
  if (mailnewsurl)
	status = SendData(mailnewsurl, outputBuffer);

  m_nextState = NNTP_RESPONSE;
  m_nextStateAfterResponse = NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE;
  SetFlag(NNTP_PAUSE_FOR_READ);

  return(status);
}

PRInt32 nsNNTPProtocol::SendGroupForArticleResponse()
{
  nsresult rv;
  /* ignore the response code and continue
   */
  m_nextState = NNTP_SEND_ARTICLE_NUMBER;

  nsXPIDLCString groupname;
  rv = m_newsgroup->GetName(getter_Copies(groupname));
  NS_ASSERTION(NS_SUCCEEDED(rv) && (const char *)groupname && nsCRT::strlen((const char *)groupname), "no group name");
  m_currentGroup = (const char *)groupname;


  return(0);
}


PRInt32 nsNNTPProtocol::SendArticleNumber()
{
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0; 
	PR_snprintf(outputBuffer, OUTPUT_BUFFER_SIZE, "ARTICLE %lu" CRLF, m_articleNumber);

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = SEND_FIRST_NNTP_COMMAND_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

    return(status);
}

PRInt32 nsNNTPProtocol::BeginArticle()
{
  if (m_typeWanted != ARTICLE_WANTED &&
	  m_typeWanted != CANCEL_WANTED)
	return 0;

  /*  Set up the HTML stream
   */ 
#ifdef UNREADY_CODE
  ce->URL_s->content_type = nsCRT::strdup (MESSAGE_RFC822);
#endif

#ifdef NO_ARTICLE_CACHEING
  ce->format_out = CLEAR_CACHE_BIT (ce->format_out);
#endif

  if (m_typeWanted == CANCEL_WANTED)
  {
#ifdef UNREADY_CODE
	  NS_ASSERTION(ce->format_out == FO_PRESENT, "format_out != FO_PRESENT");
	  ce->format_out = FO_PRESENT;
#endif
  }

  /* Only put stuff in the fe_data if this URL is going to get
	 passed to MIME_MessageConverter(), since that's the only
	 thing that knows what to do with this structure. */
#ifdef UNREADY_CODE
  if (CLEAR_CACHE_BIT(ce->format_out) == FO_PRESENT)
	{
	  status = net_InitializeNewsFeData (ce);
	  if (status < 0)
		{
		  /* #### what error message? */
		  return status;
		}
	}

  cd->stream = NET_StreamBuilder(ce->format_out, ce->URL_s, ce->window_id);
  NS_ASSERTION (cd->stream, "no stream");
  if (!cd->stream) return -1;
#endif

  // if we have a channel listener,
  // create a pipe to pump the message into...the output will go to whoever
  // is consuming the message display
  if (m_channelListener) {
      nsresult rv;
      rv = NS_NewPipe(getter_AddRefs(mDisplayInputStream), getter_AddRefs(mDisplayOutputStream));
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create pipe");
      // TODO: return on failure?
  }

  if (m_newsAction == nsINntpUrl::ActionSaveMessageToDisk)
  {
      // get the file involved from the url
      nsCOMPtr<nsIFileSpec> msgSpec;
      nsCOMPtr<nsIMsgMessageUrl> msgurl = do_QueryInterface(m_runningURL);
      msgurl->GetMessageFile(getter_AddRefs(msgSpec));

      nsFileSpec fileSpec;
      if (msgSpec)
      {
          msgSpec->GetFileSpec(&fileSpec);

          fileSpec.Delete(PR_FALSE);
          nsCOMPtr <nsISupports> supports;
          NS_NewIOFileStream(getter_AddRefs(supports), fileSpec,
                         PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, 00700);
          m_tempArticleStream = do_QueryInterface(supports);
          PRBool needDummyHeaders = PR_FALSE;
          msgurl->GetAddDummyEnvelope(&needDummyHeaders);
          if (needDummyHeaders)
          {
              nsCString result;
              char *ct;
              PRUint32 writeCount;
              time_t now = time ((time_t*) 0);
              ct = ctime(&now);
              ct[24] = 0;
              result = "From - ";
              result += ct;
              result += MSG_LINEBREAK;

              m_tempArticleStream->Write(result.GetBuffer(), result.Length(),
                                         &writeCount);
              result = "X-Mozilla-Status: 0001";
              result += MSG_LINEBREAK;
              m_tempArticleStream->Write(result.GetBuffer(), result.Length(),
                                         &writeCount);
              result =  "X-Mozilla-Status2: 00000000";
              result += MSG_LINEBREAK;
              m_tempArticleStream->Write(result.GetBuffer(), result.Length(),
                                         &writeCount);
          }
      }
  }
  m_nextState = NNTP_READ_ARTICLE;

  return 0;
}

PRInt32 nsNNTPProtocol::DisplayArticle(nsIInputStream * inputStream, PRUint32 length)
{
	char *line = nsnull;
	PRUint32 status = 0;
	
	PRBool pauseForMoreData = PR_FALSE;
	if (m_channelListener)
	{

		line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
 		if(pauseForMoreData)
		{
			PRUint32 inlength = 0;
			mDisplayInputStream->Available(&inlength);
			if (inlength > 0) // broadcast our batched up ODA changes
				m_channelListener->OnDataAvailable(this, m_channelContext, mDisplayInputStream, 0, inlength);
			SetFlag(NNTP_PAUSE_FOR_READ);
			PR_FREEIF(line);
			return status;
		}

		if (line[0] == '.' && line[1] == 0)
		{
			m_nextState = NEWS_DONE;
			nsCOMPtr<nsIMsgDBHdr> msgHdr;
			nsresult rv = NS_OK;

			rv = m_runningURL->GetMessageHeader(getter_AddRefs(msgHdr));

			if (NS_SUCCEEDED(rv) && msgHdr) {
				msgHdr->MarkRead(PR_TRUE);
            }

			ClearFlag(NNTP_PAUSE_FOR_READ);

			PRUint32 inlength = 0;
			mDisplayInputStream->Available(&inlength);
			if (inlength > 0) // broadcast our batched up ODA changes
				m_channelListener->OnDataAvailable(this, m_channelContext, mDisplayInputStream, 0, inlength);
			PR_FREEIF(line);
			return status;
		}
		else // we aren't finished with the message yet
		{
			PRUint32 count = 0;
			mDisplayOutputStream->Write(line, PL_strlen(line), &count);
			mDisplayOutputStream->Write(MSG_LINEBREAK, PL_strlen(MSG_LINEBREAK), &count);
		}

		PR_FREEIF(line);
	}
 
	return 0;	
}


PRInt32 nsNNTPProtocol::ReadArticle(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRUint32 status = 0;
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	
	PRBool pauseForMoreData = PR_FALSE;

	// if we have a channel listener, spool directly to it....
	// otherwise we must be doing something like save to disk or cancel
	// in which case we are doing the work.
	if (m_channelListener) {
		return DisplayArticle(inputStream, length);
        }

	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if(status > 1)
	{		
#ifdef UNREADY_CODE
		ce->bytes_received += status;
		FE_GraphProgress(ce->window_id, ce->URL_s,
						 ce->bytes_received, status,
						 ce->URL_s->content_length);
#else
		mBytesReceived += status;
#endif
	}

	if(!line)
	  return(status);  /* no line yet or error */
	
    nsCOMPtr<nsISupports> ctxt = do_QueryInterface(m_runningURL);

	if (m_typeWanted == CANCEL_WANTED && m_responseCode != MK_NNTP_RESPONSE_ARTICLE_HEAD)
	{
		/* HEAD command failed. */
		PR_FREEIF(line);
		return MK_NNTP_CANCEL_ERROR;
	}

	if (line[0] == '.' && line[1] == 0)
	{
		if (m_typeWanted == CANCEL_WANTED)
			m_nextState = NEWS_START_CANCEL;
		else
			m_nextState = NEWS_DONE;

		// and close the article file if it was open....
		if (m_tempArticleStream)
			m_tempArticleStream->Close();

		// now mark the message as read
#ifdef DEBUG_sspitzer
		printf("should we be marking later, after the message has finished loading?\n");
#endif
		nsCOMPtr<nsIMsgDBHdr> msgHdr;
		nsresult rv = NS_OK;

		rv = m_runningURL->GetMessageHeader(getter_AddRefs(msgHdr));

		if (NS_SUCCEEDED(rv)) {
			msgHdr->MarkRead(PR_TRUE);
		}

		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		if (line[0] == '.')
			PL_strcpy (outputBuffer, line + 1);
		else
			PL_strcpy (outputBuffer, line);

		/* When we're sending this line to a converter (ie,
		   it's a message/rfc822) use the local line termination
		   convention, not CRLF.  This makes text articles get
		   saved with the local line terminators.  Since SMTP
		   and NNTP mandate the use of CRLF, it is expected that
		   the local system will convert that to the local line
		   terminator as it is read.
		 */
        // ** jt - in the case of save message to the stationary file if the
        // message is to be uploaded to the imap server we need to end the
        // line with canonical line endings, i.e., CRLF
        nsCOMPtr<nsIMsgMessageUrl> msgurl = do_QueryInterface(m_runningURL);
        PRBool canonicalLineEnding = PR_FALSE;
        if (msgurl)
            msgurl->GetCanonicalLineEnding(&canonicalLineEnding);
        if (canonicalLineEnding)
            PL_strcat(outputBuffer, CRLF);
        else
            PL_strcat (outputBuffer, MSG_LINEBREAK);
		/* Don't send content-type to mime parser if we're doing a cancel
		  because it confuses mime parser into not parsing.
		  */
		if (m_typeWanted != CANCEL_WANTED || nsCRT::strncmp(outputBuffer, "Content-Type:", 13))
		{
            // if we are attempting to cancel, we want to snarf the headers and save the aside, which is what
            // ParseHeaderForCancel() does.
            if (m_typeWanted == CANCEL_WANTED) {
                ParseHeaderForCancel(outputBuffer);
            }

			if (m_tempArticleStream)
			{
				PRUint32 count = 0;
				m_tempArticleStream->Write(outputBuffer, PL_strlen(outputBuffer), &count);
			}
		}
	}

	PR_FREEIF(line);

	return 0;
}

void nsNNTPProtocol::ParseHeaderForCancel(char *buf)
{
    nsCString header(buf);
    PRInt32 colon = header.FindChar(':');
    if (!colon)
		return;

    nsCString value("");
    header.Right(value, header.Length() - colon -1);
    value.StripWhitespace();
    
    switch (header.First()) {
    case 'F': case 'f':
        if (header.Find("From") == 0) {
            if (m_cancelFromHdr) PR_FREEIF(m_cancelFromHdr);
			m_cancelFromHdr = PL_strdup(value.GetBuffer());
        }
        break;
    case 'M': case 'm':
        if (header.Find("Message-ID") == 0) {
            if (m_cancelID) PR_FREEIF(m_cancelID);
			m_cancelID = PL_strdup(value.GetBuffer());
        }
        break;
    case 'N': case 'n':
        if (header.Find("Newsgroups") == 0) {
            if (m_cancelNewsgroups) PR_FREEIF(m_cancelNewsgroups);
			m_cancelNewsgroups = PL_strdup(value.GetBuffer());
        }
        break;
     case 'D': case 'd':
        if (header.Find("Distributions") == 0) {
            if (m_cancelDistribution) PR_FREEIF(m_cancelDistribution);
			m_cancelDistribution = PL_strdup(value.GetBuffer());
        }       
        break;
    }

  return;
}

nsresult
nsNNTPProtocol::SetNewsFolder()
{
	nsresult rv;

	// xxx todo:  I need to fix this so this is passed in when I create the nsNNTPProtocol

    if (!m_newsFolder) {
        nsCAutoString folderURI("news://");

		if ((const char *)m_userName) {
			folderURI += (const char *)m_userName;
			folderURI += "@";
		}
        folderURI += (const char *)m_hostName;

        nsXPIDLCString newsgroupName;
        rv = m_runningURL->GetNewsgroupName(getter_Copies(newsgroupName));
		if (NS_FAILED(rv)) return rv;

        if ((const char *)newsgroupName) {
        	folderURI += "/";
            folderURI += (const char *)newsgroupName;
		}

        rv = InitializeNewsFolderFromUri((const char *)folderURI);
		if (NS_FAILED(rv)) return rv;
    }
	return NS_OK;
}

PRInt32 nsNNTPProtocol::BeginAuthorization()
{
	char * command = 0;
    nsXPIDLCString username;
    nsresult rv = NS_OK;
	PRInt32 status = 0;
	nsXPIDLCString cachedUsername;

	SetNewsFolder();

    if (m_newsFolder) {
	    rv = m_newsFolder->GetGroupUsername(getter_Copies(cachedUsername));
    }

    if (NS_FAILED(rv) || !cachedUsername) {
		rv = NS_OK;
		NNTP_LOG_NOTE("ask for the news username");

        nsXPIDLString usernamePromptText;
        GetNewsStringByName("enterUsername", getter_Copies(usernamePromptText));
        if (m_newsFolder) {
            if (!m_msgWindow) {
		        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		        if (mailnewsurl) {
                    rv = mailnewsurl->GetMsgWindow(getter_AddRefs(m_msgWindow));
                }
			}

			rv = m_newsFolder->GetGroupUsernameWithUI(usernamePromptText, nsnull, m_msgWindow, getter_Copies(username));
        }
        else {
#ifdef DEBUG_sspitzer
            printf("we don't know the folder\n");
            printf("this can happen if someone gives us just an article url\n");
#endif
            return(MK_NNTP_AUTH_FAILED);
        }

        if (NS_FAILED(rv)) {
			AlertError(MK_NNTP_AUTH_FAILED, "Aborted by user");
            return(MK_NNTP_AUTH_FAILED);
        } 
	} // !username

	if (NS_FAILED(rv) || (!username && !cachedUsername)) {
		  return(MK_NNTP_AUTH_FAILED);
	}

	NET_SACopy(&command, "AUTHINFO user ");
	if (cachedUsername) {
		PR_LOG(NNTP,PR_LOG_ALWAYS,("use %s as the username",(const char *)cachedUsername));
		NET_SACat(&command, (const char *)cachedUsername);
	}
	else {
		PR_LOG(NNTP,PR_LOG_ALWAYS,("use %s as the username",(const char *)username));
		NET_SACat(&command, (const char *)username);
	}
	NET_SACat(&command, CRLF);

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, command);

	PR_Free(command);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_AUTHORIZE_RESPONSE;

	SetFlag(NNTP_PAUSE_FOR_READ);

	return status;
}

PRInt32 nsNNTPProtocol::AuthorizationResponse()
{
	nsresult rv = NS_OK;
	PRInt32 status = 0;

	
    if (MK_NNTP_RESPONSE_AUTHINFO_OK == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK == m_responseCode) 
	  {
		/* successful login */
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_newsHost->GetPushAuth(&pushAuth);
        
        if (!TestFlag(NNTP_READER_PERFORMED))
			m_nextState = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
        else if (NS_SUCCEEDED(rv) && pushAuth)
			m_nextState = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			m_nextState = SEND_FIRST_NNTP_COMMAND;

		return(0); 
	  }
	else if (MK_NNTP_RESPONSE_AUTHINFO_CONT == m_responseCode)
	  {
		/* password required
		 */	
		char * command = 0;
        nsXPIDLCString password;
	    nsXPIDLCString cachedPassword;

		SetNewsFolder();
        if (m_newsFolder) {
            rv = m_newsFolder->GetGroupPassword(getter_Copies(cachedPassword));
        }
		if (NS_FAILED(rv) || !cachedPassword) {
			rv = NS_OK;
			NNTP_LOG_NOTE("ask for the news password");

            nsXPIDLString passwordPromptText;
            GetNewsStringByName("enterPassword", getter_Copies(passwordPromptText));
            nsXPIDLString passwordPromptTitleText;
            GetNewsStringByName("enterPasswordTitle", getter_Copies(passwordPromptTitleText));

            if (m_newsFolder) {
                if (!m_msgWindow) {
                    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
                    if (mailnewsurl) {
                        rv = mailnewsurl->GetMsgWindow(getter_AddRefs(m_msgWindow));
                    }
                }

				rv = m_newsFolder->GetGroupPasswordWithUI(passwordPromptText, passwordPromptTitleText, m_msgWindow, getter_Copies(password));
            }
            else {
                NNTP_LOG_NOTE("we don't know the folder");
                NNTP_LOG_NOTE("this can happen if someone gives us just an article url");
                return(MK_NNTP_AUTH_FAILED);
            }
            
            if (NS_FAILED(rv)) {
				AlertError(MK_NNTP_AUTH_FAILED,"Aborted by user");
                return(MK_NNTP_AUTH_FAILED);
            }
		}
		  
		if(NS_FAILED(rv) || (!password && !cachedPassword)) {
		  return(MK_NNTP_AUTH_FAILED);
		}

		NET_SACopy(&command, "AUTHINFO pass ");
		if (cachedPassword) {
			PR_LOG(NNTP,PR_LOG_ALWAYS,("use %s as the password",(const char *)cachedPassword));			NET_SACat(&command, (const char *)cachedPassword);
		}
		else {
			PR_LOG(NNTP,PR_LOG_ALWAYS,("use %s as the password",(const char *)password)); 
			NET_SACat(&command, (const char *)password);
		}
		NET_SACat(&command, CRLF);
	
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, command);

		PR_FREEIF(command);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_PASSWORD_RESPONSE;
		SetFlag(NNTP_PAUSE_FOR_READ);

		return status;
	  }
	else
	{
        /* login failed */
		AlertError(MK_NNTP_AUTH_FAILED, m_responseText);

		if (m_newsFolder) {
			rv = m_newsFolder->ForgetGroupUsername();
			rv = m_newsFolder->ForgetGroupPassword();	
		}

        return(MK_NNTP_AUTH_FAILED);
	  }
		
	NS_ASSERTION(0,"should never get here");
	return(-1);

}

PRInt32 nsNNTPProtocol::PasswordResponse()
{
	nsresult rv = NS_OK;

    if (MK_NNTP_RESPONSE_AUTHINFO_OK == m_responseCode ||
        MK_NNTP_RESPONSE_AUTHINFO_SIMPLE_OK == m_responseCode) 
	  {
        /* successful login */
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_newsHost->GetPushAuth(&pushAuth);
        
		if (!TestFlag(NNTP_READER_PERFORMED))
			m_nextState = NNTP_SEND_MODE_READER;
		/* If we're here because the host needs pushed authentication, then we 
		 * should jump back to SEND_LIST_EXTENSIONS
		 */
        else if (NS_SUCCEEDED(rv) && pushAuth)
			m_nextState = SEND_LIST_EXTENSIONS;
		else
			/* Normal authentication */
			m_nextState = SEND_FIRST_NNTP_COMMAND;

		// if we are posting, m_newsgroup will be null
		if (!m_newsgroupList && m_newsgroup) {
    		nsXPIDLCString groupName;
	    	rv = m_newsgroup->GetName(getter_Copies(groupName));
			if (NS_SUCCEEDED(rv)) {
				rv = m_newsHost->GetNewsgroupList(groupName, getter_AddRefs(m_newsgroupList));
			}
		}
		if (m_newsgroupList) {
        	rv = m_newsgroupList->ResetXOVER();
		}
        return(0);
	  }
	else
	{
        /* login failed */
		AlertError(MK_NNTP_AUTH_FAILED, m_responseText);

		if (m_newsFolder) {
			rv = m_newsFolder->ForgetGroupUsername();
			rv = m_newsFolder->ForgetGroupPassword();	
		}

        return(MK_NNTP_AUTH_FAILED);
	  }
		
	NS_ASSERTION(0,"should never get here");
	return(-1);
}

PRInt32 nsNNTPProtocol::DisplayNewsgroups()
{
	m_nextState = NEWS_DONE;
    ClearFlag(NNTP_PAUSE_FOR_READ);

	PR_LOG(NNTP,PR_LOG_ALWAYS,("about to display newsgroups. path: %s",m_path));

#if 0
	/* #### Now ignoring "news:alt.fan.*"
	   Need to open the root tree of the default news host and keep
	   opening one child at each level until we've exhausted the
	   wildcard...
	 */
	if(rv < 0)
       return(rv);  
	else
#endif
       return(MK_DATA_LOADED);  /* all finished */
}

PRInt32 nsNNTPProtocol::BeginNewsgroups()
{
	PRInt32 status = 0; 
	m_nextState = NNTP_NEWGROUPS;
#ifdef UNREADY_CODE
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));

	ce->bytes_received = 0;
#else
	mBytesReceived = 0;
#endif
	return(status);
}

PRInt32 nsNNTPProtocol::ProcessNewsgroups(nsIInputStream * inputStream, PRUint32 length)
{
	char *line, *s, *s1=NULL, *s2=NULL, *flag=NULL;
	PRInt32 oldest, youngest;
	PRUint32 status = 0;
    nsresult rv = NS_OK;
    
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

    if(!line)
        return(status);  /* no line yet */

    /* End of list? 
	 */
    if (line[0]=='.' && line[1]=='\0')
	{
		ClearFlag(NNTP_PAUSE_FOR_READ);
		PRBool xactive=PR_FALSE;
		rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
		if (NS_SUCCEEDED(rv) && xactive)
		{
          char *groupName;
          rv = m_newsHost->GetFirstGroupNeedingExtraInfo(&groupName);
		  if (NS_SUCCEEDED(rv) && m_newsgroup)
		  {
                rv = m_newsHost->FindGroup(groupName, getter_AddRefs(m_newsgroup));
                NS_ASSERTION(NS_SUCCEEDED(rv), "FindGroup failed");
				m_nextState = NNTP_LIST_XACTIVE;
				PR_LOG(NNTP,PR_LOG_ALWAYS,("listing xactive for %s", groupName));
				PR_FREEIF(line);
				return 0;
		  }
		}
		m_nextState = NEWS_DONE;

#ifdef UNREADY_CODE
		if(ce->bytes_received == 0)
		{
			/* #### no new groups */
		}
#endif

		PR_FREEIF(line);
		if(status > 0)
        	return MK_DATA_LOADED;
		else
        	return status;
    }
    else if (line [0] == '.' && line [1] == '.')
      /* The NNTP server quotes all lines beginning with "." by doubling it. */
      line++;

    /* almost correct
     */
    if(status > 1)
    {
#ifdef UNREADY_CODE
        ce->bytes_received += status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->content_length);
#else
		mBytesReceived += status;
#endif
    }

    /* format is "rec.arts.movies.past-films 7302 7119 y"
	 */
	s = PL_strchr (line, ' ');
	if (s)
	{
		*s = 0;
		s1 = s+1;
		s = PL_strchr (s1, ' ');
		if (s)
		{
			*s = 0;
			s2 = s+1;
			s = PL_strchr (s2, ' ');
			if (s)
			{
			  *s = 0;
			  flag = s+1;
			}
		 }
	}
	youngest = s2 ? atol(s1) : 0;
	oldest   = s1 ? atol(s2) : 0;

#ifdef UNREADY_CODE
	ce->bytes_received++;  /* small numbers of groups never seem to trigger this */
#else
	mBytesReceived += status;
#endif
	m_newsHost->AddNewNewsgroup(line, oldest, youngest, flag, PR_FALSE);

    PRBool xactive=PR_FALSE;
    rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
    if (NS_SUCCEEDED(rv) && xactive)
	{
      m_newsHost->SetGroupNeedsExtraInfo(line, PR_TRUE);
	}

	PR_FREEIF(line);
    return(status);
}

/* Ahhh, this like print's out the headers and stuff
 *
 * always returns 0
 */
	 
PRInt32 nsNNTPProtocol::BeginReadNewsList()
{
	m_readNewsListCount = 0;
    m_nextState = NNTP_READ_LIST;
	PRInt32 status = 0;
#ifdef UNREADY_CODE
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));
#endif
	 
    return(status);
}

/* display a list of all or part of the newsgroups list
 * from the news server
 */

PRInt32 nsNNTPProtocol::ReadNewsList(nsIInputStream * inputStream, PRUint32 length)
{
	nsresult rv;
    char * line;
    char * description;
    int i=0;
	PRUint32 status = 1;
	
	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

    if(!line)
        return(status);  /* no line yet */

            /* End of list? */
    if (line[0]=='.' && line[1]=='\0')
    {
		PRBool listpnames=PR_FALSE;
		rv = m_newsHost->QueryExtension("LISTPNAMES",&listpnames);
		if (NS_SUCCEEDED(rv) && listpnames)
			m_nextState = NNTP_LIST_PRETTY_NAMES;
		else
			m_nextState = DISPLAY_NEWSGROUPS;
        ClearFlag(NNTP_PAUSE_FOR_READ);
		PR_FREEIF(line);
        return 0;  
    }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(status > 1)
    {
#ifdef UNREADY_CODE
    	ce->bytes_received += status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->content_length);
#else
		mBytesReceived += status;

		if (m_msgWindow) {
        	nsCOMPtr <nsIMsgStatusFeedback> msgStatusFeedback;

        	rv = m_msgWindow->GetStatusFeedback(getter_AddRefs(msgStatusFeedback));
        	if (NS_FAILED(rv)) return rv;
// XXXXX
                nsXPIDLString statusString;
		
                nsCOMPtr<nsIStringBundleService> bundleService = 
                do_GetService(kStringBundleServiceCID, &rv);
                NS_ENSURE_SUCCESS(rv, rv);

               	nsCOMPtr<nsIStringBundle> bundle;
                rv = bundleService->CreateBundle(NEWS_MSGS_URL, nsnull, 
                                            getter_AddRefs(bundle));
                NS_ENSURE_SUCCESS(rv, rv);

                nsAutoString bytesStr; bytesStr.AppendInt(mBytesReceived);
		
                const PRUnichar *formatStrings[] = { bytesStr.GetUnicode() };
		const PRUnichar *propertyTag = NS_LITERAL_STRING("bytesReceived");
                rv = bundle->FormatStringFromName(propertyTag,
                                                  formatStrings, 1,
                                                  getter_Copies(statusString));

        	rv = msgStatusFeedback->ShowStatusString(statusString);
        	if (NS_FAILED(rv)) return rv;
		}
#endif
    }
    
	 /* find whitespace seperator if it exits */
    for(i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
        ;  /* null body */

    if(line[i] == '\0')
        description = &line[i];
    else
        description = &line[i+1];

    line[i] = 0; /* terminate group name */

	/* store all the group names */
#if 0
    m_newsHost->AddNewNewsgroup(line, 0, 0, "", PR_FALSE);
#else
	NS_ASSERTION(m_nntpServer, "no nntp incoming server");
	if (m_nntpServer) {
		m_readNewsListCount++;
		rv = m_nntpServer->AddNewsgroupToSubscribeDS(line);
	}
	else {
		rv = NS_ERROR_FAILURE;
	}
#endif

	if (m_readNewsListCount == READ_NEWS_LIST_COUNT_MAX) {
		m_readNewsListCount = 0;
 	    if (mUpdateTimer) {
			mUpdateTimer->Cancel();
			mUpdateTimer = nsnull;
	    }
        mUpdateTimer = do_CreateInstance("component://netscape/timer", &rv);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create timer");
		if (NS_FAILED(rv)) return -1;

		mInputStream = inputStream;

		const PRUint32 kUpdateTimerDelay = READ_NEWS_LIST_TIMEOUT;
		rv = mUpdateTimer->Init(NS_STATIC_CAST(nsITimerCallback*,this), kUpdateTimerDelay);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to init timer");
		if (NS_FAILED(rv)) return -1;

		m_nextState = NEWS_FINISHED;
    }

	PR_FREEIF(line);
	if (NS_FAILED(rv)) return -1;
    return(status);
}

void
nsNNTPProtocol::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  mUpdateTimer = nsnull;    // release my hold  
  TimerCallback();
}

void nsNNTPProtocol::TimerCallback()
{
	m_nextState = NNTP_READ_LIST;
	ProcessProtocolState(nsnull, mInputStream, 0,0); 
#if 0
	mInputStream = null;
#endif
	return;
}

/* start the xover command
 */

PRInt32 nsNNTPProtocol::BeginReadXover()
{
    PRInt32 count;     /* Response fields */
    nsresult rv = NS_OK;
    
	/* Make sure we never close and automatically reopen the connection at this
	   point; we'll confuse libmsg too much... */

	SetFlag(NNTP_SOME_PROTOCOL_SUCCEEDED); 

	/* We have just issued a GROUP command and read the response.
	   Now parse that response to help decide which articles to request
	   xover data for.
	 */
    PR_sscanf(m_responseText,
		   "%d %d %d", 
		   &count, 
		   &m_firstPossibleArticle, 
		   &m_lastPossibleArticle);

	/* We now know there is a summary line there; make sure it has the
	   right numbers in it. */
    rv = m_newsHost->DisplaySubscribedGroup(m_newsgroup,
                                            m_firstPossibleArticle,
                                            m_lastPossibleArticle,
                                            count, PR_TRUE);

    if (NS_FAILED(rv)) return -1;

	m_numArticlesLoaded = 0;
	m_numArticlesWanted = net_NewsChunkSize > 0 ? net_NewsChunkSize : 1L << 30;

	m_nextState = NNTP_FIGURE_NEXT_CHUNK;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return 0;
}

PRInt32 nsNNTPProtocol::FigureNextChunk()
{
    nsresult rv = NS_OK;
	PRInt32 status = 0;

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (m_firstArticle > 0) 
	{
      nsXPIDLCString groupName;
      
	  rv = m_newsgroup->GetName(getter_Copies(groupName));

      /* XXX - parse state stored in MSG_Pane cd->pane */
      if (NS_SUCCEEDED(rv))
          rv = m_newsHost->GetNewsgroupList(groupName, getter_AddRefs(m_newsgroupList));

      PR_LOG(NNTP,PR_LOG_ALWAYS,("add to known articles:  %d - %d", m_firstArticle, m_lastArticle));

      if (NS_SUCCEEDED(rv) && m_newsgroupList) {
          rv = m_newsgroupList->AddToKnownArticles(m_firstArticle,
                                                 m_lastArticle);
      }

	  if (NS_FAILED(rv))
      {
		return status;
	  }
	}
										 

	if (m_numArticlesLoaded >= m_numArticlesWanted) 
	{
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}

	if (!m_newsgroupList) {
    	nsXPIDLCString groupName;
	    rv = m_newsgroup->GetName(getter_Copies(groupName));
		if (NS_SUCCEEDED(rv))
			rv = m_newsHost->GetNewsgroupList(groupName, getter_AddRefs(m_newsgroupList));
	}
        
    if (NS_SUCCEEDED(rv) && m_newsgroupList) {
        rv = m_newsgroupList->GetRangeOfArtsToDownload(m_msgWindow,
					      m_firstPossibleArticle,
                                              m_lastPossibleArticle,
                                              m_numArticlesWanted -
                                              m_numArticlesLoaded,
                                              &(m_firstArticle),
                                              &(m_lastArticle),
                                              &status);
	
	}

	if (NS_FAILED(rv))
	  return status;

	if (m_firstArticle <= 0 || m_firstArticle > m_lastArticle) 
	{
	  /* Nothing more to get. */
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}

	PR_LOG(NNTP,PR_LOG_ALWAYS,("Chunk will be (%d-%d)", m_firstArticle, m_lastArticle));
    
	m_articleNumber = m_firstArticle;

    /* was MSG_InitXOVER() */
    if (m_newsgroupList) {
        rv = m_newsgroupList->InitXOVER(m_firstArticle, m_lastArticle);
	}

    /* convert nsresult->status */
    status = !NS_SUCCEEDED(rv);

	if (status < 0) 
	  return status;

	ClearFlag(NNTP_PAUSE_FOR_READ);
	if (TestFlag(NNTP_NO_XOVER_SUPPORT)) 
		m_nextState = NNTP_READ_GROUP;
	else 
		m_nextState = NNTP_XOVER_SEND;

	return 0;
}

PRInt32 nsNNTPProtocol::XoverSend()
{
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0;

    PR_snprintf(outputBuffer, 
				OUTPUT_BUFFER_SIZE,
				"XOVER %d-%d" CRLF, 
				m_firstArticle, 
				m_lastArticle);

	PR_LOG(NNTP,PR_LOG_ALWAYS,("XOVER %d-%d", m_firstArticle, m_lastArticle));
	NNTP_LOG_WRITE(outputBuffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_XOVER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);
#ifdef UNREADY_CODE
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_LISTARTICLES));
#endif

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer); 
	return status;
}

/* see if the xover response is going to return us data
 * if the proper code isn't returned then assume xover
 * isn't supported and use
 * normal read_group
 */

PRInt32 nsNNTPProtocol::ReadXoverResponse()
{
#ifdef TEST_NO_XOVER_SUPPORT
	m_responseCode = MK_NNTP_RESPONSE_CHECK_ERROR; /* pretend XOVER generated an error */
#endif

    if(m_responseCode != MK_NNTP_RESPONSE_XOVER_OK)
    {
        /* If we didn't get back "224 data follows" from the XOVER request,
		   then that must mean that this server doesn't support XOVER.  Or
		   maybe the server's XOVER support is busted or something.  So,
		   in that case, fall back to the very slow HEAD method.

		   But, while debugging here at HQ, getting into this state means
		   something went very wrong, since our servers do XOVER.  Thus
		   the assert.
         */
		/*NS_ASSERTION (0,"something went very wrong");*/
		m_nextState = NNTP_READ_GROUP;
		SetFlag(NNTP_NO_XOVER_SUPPORT);
    }
    else
    {
        m_nextState = NNTP_XOVER;
    }

    return(0);  /* continue */
}

/* process the xover list as it comes from the server
 * and load it into the sort list.  
 */

PRInt32 nsNNTPProtocol::ReadXover(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
    nsresult rv;
	PRUint32 status = 1;

    PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

    if(!line)
		return(status);  /* no line yet or TCP error */

    if(line[0] == '.' && line[1] == '\0')
    {
		m_nextState = NNTP_FIGURE_NEXT_CHUNK;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		PR_FREEIF(line);
		return(0);
    }
	else if (line [0] == '.' && line [1] == '.')
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;

    /* almost correct
     */
    if(status > 1)
    {
#ifdef UNREADY_CODE
        ce->bytes_received += status;
        FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status,
						 ce->URL_s->content_length);
#else
		mBytesReceived += status;
#endif
	}

    rv = m_newsgroupList->ProcessXOVERLINE(line, &status);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to process the XOVERLINE");

    m_numArticlesLoaded++;
    PR_FREEIF(line);
    return status; /* keep going */
}


/* Finished processing all the XOVER data.
 */

PRInt32 nsNNTPProtocol::ProcessXover()
{
    nsresult rv;
	PRInt32 status = 0;

    /* xover_parse_state stored in MSG_Pane cd->pane */
	if (!m_newsgroupList) {
		return NS_ERROR_NULL_POINTER;
	}

    rv = m_newsgroupList->FinishXOVERLINE(0,&status);

	if (NS_SUCCEEDED(rv) && status < 0) return status;

	m_nextState = NEWS_DONE;

    return(MK_DATA_LOADED);
}

PRInt32 nsNNTPProtocol::ReadNewsgroup()
{
	if(m_articleNumber > m_lastArticle)
    {  /* end of groups */

		m_nextState = NNTP_FIGURE_NEXT_CHUNK;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return(0);
    }
    else
    {
		char outputBuffer[OUTPUT_BUFFER_SIZE];
        PR_snprintf(outputBuffer, 
				   OUTPUT_BUFFER_SIZE,  
				   "HEAD %ld" CRLF, 
				   m_articleNumber++);
        m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_READ_GROUP_RESPONSE;

        SetFlag(NNTP_PAUSE_FOR_READ);
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			return SendData(mailnewsurl, outputBuffer);
		else
			return 0;
    }
}

/* See if the "HEAD" command was successful
 */

PRInt32 nsNNTPProtocol::ReadNewsgroupResponse()
{
  nsresult rv;
  
  if (m_responseCode == MK_NNTP_RESPONSE_ARTICLE_HEAD)
  {     /* Head follows - parse it:*/
	  m_nextState = NNTP_READ_GROUP_BODY;

	  if(m_messageID)
		*m_messageID = '\0';

	  /* Give the message number to the header parser. */
      rv = m_newsgroupList->ProcessNonXOVER(m_responseText);
      /* convert nsresult->status */
      return !NS_SUCCEEDED(rv);
  }
  else
  {
	  NNTP_LOG_NOTE(("Bad group header found!"));
	  m_nextState = NNTP_READ_GROUP;
	  return(0);
  }
}

/* read the body of the "HEAD" command
 */
PRInt32 nsNNTPProtocol::ReadNewsgroupBody(nsIInputStream * inputStream, PRUint32 length)
{
  char *line;
  nsresult rv;
  PRUint32 status = 1;

  PRBool pauseForMoreData = PR_FALSE;
  line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

  /* if TCP error of if there is not a full line yet return
   */
  if(!line)
	return status;

  PR_LOG(NNTP,PR_LOG_ALWAYS,("read_group_body: got line: %s|",line));

  /* End of body? */
  if (line[0]=='.' && line[1]=='\0')
  {
	  m_nextState = NNTP_READ_GROUP;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
  }
  else if (line [0] == '.' && line [1] == '.')
	/* The NNTP server quotes all lines beginning with "." by doubling it. */
	line++;

  rv = m_newsgroupList->ProcessNonXOVER(line);
  /* convert nsresult->status */
  PR_FREEIF(line);
  return !NS_SUCCEEDED(rv);
}


nsresult nsNNTPProtocol::GetNewsStringByID(PRInt32 stringID, PRUnichar **aString)
{
	nsresult res;
	nsAutoString	resultString; resultString.AssignWithConversion("???");
	if (!m_stringBundle)
	{
		char*       propertyURL = NEWS_MSGS_URL;

		NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			nsILocale   *locale = nsnull;

			res = sBundleService->CreateBundle(propertyURL, locale, getter_AddRefs(m_stringBundle));
		}
	}
	if (m_stringBundle) {
		PRUnichar *ptrv = nsnull;
		res = m_stringBundle->GetStringFromID(stringID, &ptrv);

		if (NS_FAILED(res)) 
		{
			resultString.AssignWithConversion("[StringID");
			resultString.AppendInt(stringID, 10);
			resultString.AppendWithConversion("?]");
			*aString = resultString.ToNewUnicode();
		}
		else
		{
			*aString = ptrv;
		}
	}
	else
	{
		res = NS_OK;
		*aString = resultString.ToNewUnicode();
	}
	return res;
}

nsresult nsNNTPProtocol::GetNewsStringByName(const char *aName, PRUnichar **aString)
{
	nsresult res;
	nsAutoString	resultString; resultString.AssignWithConversion("???");
	if (!m_stringBundle)
	{
		char*       propertyURL = NEWS_MSGS_URL;

		NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			nsILocale   *locale = nsnull;

			res = sBundleService->CreateBundle(propertyURL, locale, getter_AddRefs(m_stringBundle));
		}
	}
	if (m_stringBundle)
	{
		nsAutoString unicodeName; unicodeName.AssignWithConversion(aName);

		PRUnichar *ptrv = nsnull;
		res = m_stringBundle->GetStringFromName(unicodeName.GetUnicode(), &ptrv);

		if (NS_FAILED(res)) 
		{
			resultString.AssignWithConversion("[StringName");
			resultString.AppendWithConversion(aName);
			resultString.AppendWithConversion("?]");
			*aString = resultString.ToNewUnicode();
		}
		else
		{
			*aString = ptrv;
		}
	}
	else
	{
		res = NS_OK;
		*aString = resultString.ToNewUnicode();
	}
	return res;
}

// sspitzer:  PostMessageInFile is derived from nsSmtpProtocol::SendMessageInFile()
PRInt32 nsNNTPProtocol::PostMessageInFile(nsIFileSpec *aPostMessageFile)
{
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningURL);
    if (url && aPostMessageFile)
        nsMsgProtocol::PostMessage(url, aPostMessageFile);

    SetFlag(NNTP_PAUSE_FOR_READ);
    
	// for now, we are always done at this point..we aren't making multiple
    // calls to post data...

	// always issue a '.' and CRLF when we are done...
    PL_strcpy(m_dataBuf, CRLF "." CRLF);
	if (url)
		SendData(url, m_dataBuf);
#ifdef UNREADY_CODE
    NET_Progress(CE_WINDOW_ID,
                 XP_GetString(XP_MESSAGE_SENT_WAITING_MAIL_REPLY));
#endif /* UNREADY_CODE */
    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;
    return(0);
}
    
PRInt32 nsNNTPProtocol::PostData()
{
    /* returns 0 on done and negative on error
     * positive if it needs to continue.
     */
	NNTP_LOG_NOTE("nsNNTPProtocol::PostData()");
    nsresult rv = NS_OK;
    
    nsCOMPtr <nsINNTPNewsgroupPost> message;
    rv = m_runningURL->GetMessageToPost(getter_AddRefs(message));
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIFileSpec> filePath;
        rv = message->GetPostMessageFile(getter_AddRefs(filePath));
        if (NS_SUCCEEDED(rv)) {
            PostMessageInFile(filePath);
        }
     }

#ifdef UNREADY_CODE
    status = NET_WritePostData(ce->window_id, ce->URL_s,
                                  ce->socket,
								  &cd->write_post_data_data,
								  PR_TRUE);

    SetFlag(NNTP_PAUSE_FOR_READ);

    if(status == 0)
    {
        /* normal done
         */
        PL_strcpy(cd->output_buffer, CRLF "." CRLF);
        NNTP_LOG_WRITE(cd->output_buffer);
        status = (int) NET_BlockingWrite(ce->socket,
                                            cd->output_buffer,
                                            PL_strlen(cd->output_buffer));
		NNTP_LOG_WRITE(cd->output_buffer);

		NET_Progress(ce->window_id,
					XP_GetString(XP_MESSAGE_SENT_WAITING_NEWS_REPLY));

        NET_ClearConnectSelect(ce->window_id, ce->socket);
#ifdef XP_WIN
		if(cd->calling_netlib_all_the_time)
		{
			cd->calling_netlib_all_the_time = PR_FALSE;
#if 0
          /* this should be handled by NET_ClearCallNetlibAllTheTime */
			net_call_all_the_time_count--;
			if(net_call_all_the_time_count == 0)
#endif /* 0 */
				NET_ClearCallNetlibAllTheTime(ce->window_id,"mknews");
		}
#endif /* XP_WIN */
        NET_SetReadSelect(ce->window_id, ce->socket);
		ce->con_sock = 0;

        m_nextState = NNTP_RESPONSE;
        m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;
        return(0);
      }

    return(status);
#endif /* UNREADY_CODE */
    return 0;
}


/* interpret the response code from the server
 * after the post is done
 */   
PRInt32 nsNNTPProtocol::PostDataResponse()
{
	if (m_responseCode != MK_NNTP_RESPONSE_POST_OK) 
	{
	  AlertError(MK_NNTP_ERROR_MESSAGE,m_responseText);
#ifdef UNREADY_CODE
	  ce->URL_s->error_msg =
		NET_ExplainErrorDetails(MK_NNTP_ERROR_MESSAGE, 
								m_responseText ? m_responseText : "");
	  if (m_responseCode == MK_NNTP_RESPONSE_POST_FAILED 
		  && MSG_GetPaneType(cd->pane) == MSG_COMPOSITIONPANE
		  && MSG_IsDuplicatePost(cd->pane) &&
		  MSG_GetCompositionMessageID(cd->pane)) {
		/* The news server won't let us post.  We suspect that we're submitting
		   a duplicate post, and that's why it's failing.  So, let's go see
		   if there really is a message out there with the same message-id.
		   If so, we'll just silently pretend everything went well. */
		PR_snprintf(cd->output_buffer, OUTPUT_BUFFER_SIZE, "STAT %s" CRLF,
					MSG_GetCompositionMessageID(cd->pane));
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_CHECK_FOR_MESSAGE;
		NNTP_LOG_WRITE(cd->output_buffer);
		return (int) NET_BlockingWrite(ce->socket, cd->output_buffer,
									   PL_strlen(cd->output_buffer));
	  }

	  MSG_ClearCompositionMessageID(cd->pane); /* So that if the user tries
													  to just post again, we
													  won't immediately decide
													  that this was a duplicate
													  message and ignore the
													  error. */
#endif
	  m_nextState = NEWS_ERROR;
	  return(MK_NNTP_ERROR_MESSAGE);
	}
    m_nextState = NEWS_POST_DONE;
	ClearFlag(NNTP_PAUSE_FOR_READ);
    return(MK_DATA_LOADED);
}

PRInt32 nsNNTPProtocol::CheckForArticle()
{
  m_nextState = NEWS_ERROR;
  if (m_responseCode >= 220 && m_responseCode <= 223) {
	/* Yes, this article is already there, we're all done. */
	return MK_DATA_LOADED;
  } 
  else 
  {
	/* The article isn't there, so the failure we had earlier wasn't due to
	   a duplicate message-id.  Return the error from that previous
	   posting attempt (which is already in ce->URL_s->error_msg). */
#ifdef UNREADY_CODE
	MSG_ClearCompositionMessageID(cd->pane);
#endif
	return MK_NNTP_ERROR_MESSAGE;
  }
}

#define NEWS_GROUP_DISPLAY_FREQ		20

PRInt32 nsNNTPProtocol::DisplayNewsRC()
{
    nsresult rv;
	PRInt32 status = 0; 

	if(!TestFlag(NNTP_NEWSRC_PERFORMED))
	{
		SetFlag(NNTP_NEWSRC_PERFORMED);
        rv = m_nntpServer->GetNumGroupsNeedingCounts(&m_newsRCListCount);
		if (NS_FAILED(rv)) return -1;
	}
	
	nsCOMPtr <nsISupports> currChild;
	rv = m_nntpServer->GetFirstGroupNeedingCounts(getter_AddRefs(currChild));
	if (NS_FAILED(rv)) return -1;

	nsCOMPtr<nsIFolder> currFolder;
    currFolder = do_QueryInterface(currChild, &rv);
	if (NS_FAILED(rv)) return -1;
	if (!currFolder) return -1;

    m_newsFolder = do_QueryInterface(currFolder, &rv);
	if (NS_FAILED(rv)) return -1;
	if (!m_newsFolder) return -1;

	nsXPIDLString name;
	rv = currFolder->GetName(getter_Copies(name));
	if (NS_FAILED(rv)) return -1;
	if (!name) return -1;

	// do I need asciiName?
	nsCAutoString asciiName; asciiName.AssignWithConversion(name);
	m_currentGroup = (const char *)asciiName;

	if(NS_SUCCEEDED(rv) && ((const char *)m_currentGroup))
    {
		/* send group command to server
		 */
		char outputBuffer[OUTPUT_BUFFER_SIZE];

		PR_snprintf(outputBuffer, OUTPUT_BUFFER_SIZE, "GROUP %.512s" CRLF, (const char *)m_currentGroup);
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, outputBuffer);
		SetProgressBarPercent(m_newsRCListCount, m_newsRCListIndex);
		
		/* only update every 20 groups for speed */
		if ((m_newsRCListCount >= NEWS_GROUP_DISPLAY_FREQ) && ((m_newsRCListIndex % NEWS_GROUP_DISPLAY_FREQ) == 0 ||
									(m_newsRCListIndex == m_newsRCListCount)))
		{
			char thisGroup[20];
			char totalGroups[20];
			char *statusText;
			
			PR_snprintf (thisGroup, sizeof(thisGroup), "%ld", (long) m_newsRCListIndex);
			PR_snprintf (totalGroups, sizeof(totalGroups), "%ld", (long) m_newsRCListCount);

			statusText = PR_smprintf ("%s / %s", thisGroup, totalGroups);
			if (statusText)
			{
				SetProgressStatus(statusText);
				PR_Free(statusText);
			}
		}
		
		m_newsRCListIndex++;

		SetFlag(NNTP_PAUSE_FOR_READ);
		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NEWS_DISPLAY_NEWS_RC_RESPONSE;
    }
	else
	{
		if (m_newsRCListCount)
		{
			SetProgressBarPercent(0, -1);
			m_newsRCListCount = 0;
		}
		else if (m_responseCode == MK_NNTP_RESPONSE_LIST_OK)  
		{
			/*
			 * 5-9-96 jefft 
			 * If for some reason the news server returns an empty 
			 * newsgroups list with a nntp response code MK_NNTP_RESPONSE_LIST_OK -- list of
			 * newsgroups follows. We set status to MK_EMPTY_NEWS_LIST
			 * to end the infinite dialog loop.
			 */
			status = MK_EMPTY_NEWS_LIST;
		}
		m_nextState = NEWS_DONE;
	
		if(status > -1)
		  return MK_DATA_LOADED; 
		else
		  return(status);
	}

	return(status); /* keep going */

}

/* Parses output of GROUP command */
PRInt32 nsNNTPProtocol::DisplayNewsRCResponse()
{
	nsresult rv = NS_OK;
	PRInt32 status = 0;
    if(m_responseCode == MK_NNTP_RESPONSE_GROUP_SELECTED)
    {
		char *num_arts = 0, *low = 0, *high = 0, *group = 0;
		PRInt32 first_art, last_art;

		/* line looks like:
		 *     211 91 3693 3789 comp.infosystems
		 */

		num_arts = m_responseText;
		low = PL_strchr(num_arts, ' ');

		if(low)
		{
			first_art = atol(low);
			*low++ = '\0';
			high= PL_strchr(low, ' ');
		}
		if(high)
		{
			*high++ = '\0';
			group = PL_strchr(high, ' ');
		}
		if(group)
		{
			*group++ = '\0';
			/* the group name may be contaminated by "group selected" at
			   the end.  This will be space separated from the group name.
			   If a space is found in the group name terminate at that
			   point. */
			strtok(group, " ");
			last_art = atol(high);
		}

        rv = m_nntpServer->DisplaySubscribedGroup(m_newsFolder,
                                              low ? atol(low) : 0,
                                              high ? atol(high) : 0,
                                              atol(num_arts));
		NS_ASSERTION(NS_SUCCEEDED(rv),"DisplaySubscribedGroup() failed");
		if (NS_FAILED(rv)) status = -1;
		if (status < 0)
		  return status;
	  }
	  else if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP)
	  {
          m_newsHost->GroupNotFound((const char *)m_currentGroup, PR_FALSE);
	  }
	  /* it turns out subscribe ui depends on getting this displaysubscribedgroup call,
	     even if there was an error.
	  */
	  if(m_responseCode != MK_NNTP_RESPONSE_GROUP_SELECTED)
	  {
		/* only on news server error or when zero articles
		 */
		NS_ASSERTION(PR_FALSE,"check this code");
        rv = m_nntpServer->DisplaySubscribedGroup(m_newsFolder, 0, 0, 0);
		NS_ASSERTION(NS_SUCCEEDED(rv),"DisplaySubscribedGroup() failed");
	  }

	m_nextState = NEWS_DISPLAY_NEWS_RC;
		
	return 0;
}

PRInt32 nsNNTPProtocol::StartCancel()
{
  PRInt32 status = 0;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
  if (mailnewsurl)
	status = SendData(mailnewsurl, NNTP_CMD_POST);

  m_nextState = NNTP_RESPONSE;
  m_nextStateAfterResponse = NEWS_DO_CANCEL;
  SetFlag(NNTP_PAUSE_FOR_READ);
  return (status);
}

PRBool nsNNTPProtocol::CheckIfAuthor(nsISupports *aElement, void *data)
{
    nsresult rv;

    cancelInfoEntry *cancelInfo = (cancelInfoEntry*) data;

    if (cancelInfo->from) {
		NNTP_LOG_NOTE("already found a match, no need to go any further");
        // keep going
        return PR_TRUE;
    }
    
    nsCOMPtr<nsIMsgIdentity> identity = do_QueryInterface(aElement, &rv);
    if (NS_FAILED(rv)) {
        // keep going
        return PR_TRUE;
    }
    
    if (identity) {
        identity->GetEmail(&cancelInfo->from);
		PR_LOG(NNTP,PR_LOG_ALWAYS,("from = %s", cancelInfo->from));
    }
    
    nsCOMPtr<nsIMsgHeaderParser> parser;                  
    rv = nsComponentManager::CreateInstance(kCHeaderParserCID,
                                            nsnull,
                                            NS_GET_IID(nsIMsgHeaderParser),
                                            getter_AddRefs(parser));

    if (NS_FAILED(rv)) {
        PR_FREEIF(cancelInfo->from);
        cancelInfo->from = nsnull;

        // keep going
        return PR_TRUE;
    }

	NNTP_LOG_NOTE("got a header parser...");
    
    char *us = nsnull;
    char *them = nsnull;
    nsresult rv1 = parser->ExtractHeaderAddressMailboxes(nsnull, cancelInfo->from, &us);
    nsresult rv2 = parser->ExtractHeaderAddressMailboxes(nsnull, cancelInfo->old_from, &them);
    
	PR_LOG(NNTP,PR_LOG_ALWAYS,("us = %s, them = %s", us, them));

    if ((NS_FAILED(rv1) || NS_FAILED(rv2) || PL_strcasecmp(us, them))) {
		NNTP_LOG_NOTE("no match.  don't set cancel email");
        PR_FREEIF(cancelInfo->from);
        cancelInfo->from = nsnull;

        PR_FREEIF(us);
        PR_FREEIF(them);

        // keep going
        return PR_TRUE;
    }
    else {
		NNTP_LOG_NOTE("got a match");
        // we have a match, stop.
        return PR_FALSE;
    }          
}

PRInt32 nsNNTPProtocol::DoCancel()
{
    int status = 0;
    nsresult rv = NS_OK;
    char *id = nsnull;
    char *subject = nsnull;
    char *newsgroups = nsnull;
    char *distribution = nsnull;
    char *other_random_headers = nsnull;
    char *body = nsnull;
    cancelInfoEntry cancelInfo;
	PRBool requireConfirmationForCancel = PR_TRUE;
	PRBool showAlertAfterCancel = PR_TRUE;

	int L;
#ifdef USE_LIBMSG
	MSG_CompositionFields *fields = NULL;
#endif 

	SetNewsFolder();

  /* #### Should we do a more real check than this?  If the POST command
	 didn't respond with "MK_NNTP_RESPONSE_POST_SEND_NOW Ok", then it's not ready for us to throw a
	 message at it...   But the normal posting code doesn't do this check.
	 Why?
   */
  NS_ASSERTION (m_responseCode == MK_NNTP_RESPONSE_POST_SEND_NOW, "code != POST_SEND_NOW");

  // These shouldn't be set yet, since the headers haven't been "flushed"
  // "Distribution: " doesn't appear to be required, so
  // don't assert on m_cancelDistribution
  NS_ASSERTION (m_cancelID &&
			 m_cancelFromHdr &&
			 m_cancelNewsgroups, "null ptr");

  newsgroups = m_cancelNewsgroups;
  distribution = m_cancelDistribution;
  id = m_cancelID;
  cancelInfo.old_from = m_cancelFromHdr;
  cancelInfo.from = nsnull;

  nsCOMPtr <nsIPref> prefs = do_GetService(kPrefServiceCID, &rv);
  if (NS_FAILED(rv) || !prefs) return -1;  /* unable to get the pref service */
  
  nsCOMPtr<nsIPrompt> dialog;
  if (m_runningURL)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(m_runningURL));    
    rv = GetPromptDialogFromUrl(msgUrl, getter_AddRefs(dialog));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ASSERTION (id && newsgroups, "null ptr");
  if (!id || !newsgroups) return -1; /* "unknown error"... */

  m_cancelNewsgroups = nsnull;
  m_cancelDistribution = nsnull;
  m_cancelFromHdr = nsnull;
  m_cancelID = nsnull;
  
  L = PL_strlen (id);
  
  subject = (char *) PR_Malloc (L + 20);
  other_random_headers = (char *) PR_Malloc (L + 20);
  body = (char *) PR_Malloc (PL_strlen (XP_AppCodeName) + 100);
  
  nsXPIDLString alertText;
  nsXPIDLString confirmText;
  
  PRInt32 confirmCancelResult = 0;

  /* Make sure that this loser isn't cancelling someone else's posting.
	 Yes, there are occasionally good reasons to do so.  Those people
	 capable of making that decision (news admins) have other tools with
	 which to cancel postings (like telnet.)
     
	 Don't do this if server tells us it will validate user. DMB 3/19/97
   */
  PRBool cancelchk=PR_FALSE;
  rv = m_newsHost->QueryExtension("CANCELCHK",&cancelchk);
  if (NS_SUCCEEDED(rv) && !cancelchk) {
	  NNTP_LOG_NOTE("CANCELCHK not supported");
      
      // get the current identity from the news session....
      NS_WITH_SERVICE(nsIMsgAccountManager,accountManager,kCMsgAccountManagerCID,&rv);
      if (NS_SUCCEEDED(rv) && accountManager) {
          nsCOMPtr<nsISupportsArray> identities;
          rv = accountManager->GetAllIdentities(getter_AddRefs(identities));
          if (NS_FAILED(rv)) return -1;

          // CheckIfAuthor will set cancelInfo.from if a match is found
          identities->EnumerateForwards(CheckIfAuthor, (void *)&cancelInfo);
      }
  
      if (!cancelInfo.from) {
          GetNewsStringByName("cancelDisallowed", getter_Copies(alertText));
          rv = dialog->Alert(nsnull, alertText);
		  // XXX:  todo, check rv?
          
          status = MK_NNTP_CANCEL_DISALLOWED;
          m_nextState = NEWS_ERROR; /* even though it worked */
          ClearFlag(NNTP_PAUSE_FOR_READ);
          goto FAIL;
      }
      else {
		  PR_LOG(NNTP,PR_LOG_ALWAYS,("CANCELCHK not supported, so post the cancel message as %s",cancelInfo.from));
      }
  }
  else {
	  NNTP_LOG_NOTE("CANCELCHK supported, don't do the us vs. them test");
  }
  
  // QA needs to be able to disable this confirm dialog, for the automated tests.  see bug #31057
  rv = prefs->GetBoolPref(PREF_NEWS_CANCEL_CONFIRM, &requireConfirmationForCancel);
  if (NS_FAILED(rv) || requireConfirmationForCancel) 
  {
       /* Last chance to cancel the cancel.*/
	      GetNewsStringByName("cancelConfirm", getter_Copies(confirmText));
	      rv = dialog->Confirm(nsnull, confirmText, &confirmCancelResult);
	      // XXX:  todo, check rv?
  } 
  else {
	confirmCancelResult = 1;
  }

  if (confirmCancelResult != 1) {
      // they cancelled the cancel
      status = MK_NNTP_NOT_CANCELLED;
      goto FAIL;
  }  
  
  if (!subject || !other_random_headers || !body) {
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
  }
  
  PL_strcpy (subject, "cancel ");
  PL_strcat (subject, id);

  PL_strcpy (other_random_headers, "Control: cancel ");
  PL_strcat (other_random_headers, id);
  PL_strcat (other_random_headers, CRLF);
  if (distribution) {
	  PL_strcat (other_random_headers, "Distribution: ");
	  PL_strcat (other_random_headers, distribution);
	  PL_strcat (other_random_headers, CRLF);
  }

  PL_strcpy (body, "This message was cancelled from within ");
  PL_strcat (body, XP_AppCodeName);
  PL_strcat (body, "." CRLF);
  
#ifdef USE_LIBMSG
  fields = MSG_CreateCompositionFields(cancelInfo.from, 0, 0, 0, 0, 0, newsgroups,
									   0, 0, subject, id, other_random_headers,
									   0, 0, news_url);
#endif 
  
  m_cancelStatus = 0;

  {
    /* NET_BlockingWrite() should go away soon? I think. */
    /* The following are what we really need to cancel a posted message */
    char *data;
    data = PR_smprintf("From: %s" CRLF
                       "Newsgroups: %s" CRLF
                       "Subject: %s" CRLF
                       "References: %s" CRLF
                       "%s" CRLF /* other_random_headers */
                       "%s"     /* body */
                       CRLF "." CRLF CRLF, /* trailing SMTP "." */
                       cancelInfo.from, newsgroups, subject, id,
                       other_random_headers, body);
    
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, data);
    PR_Free (data);
    if (status < 0) {
		nsCAutoString errorText;
		errorText.AppendInt(status);
		AlertError(MK_TCP_WRITE_ERROR,(const char *)errorText);
		goto FAIL;
	}

    SetFlag(NNTP_PAUSE_FOR_READ);
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_SEND_POST_DATA_RESPONSE;

    // QA needs to be able to turn this alert off, for the automate tests.  see bug #31057
	rv = prefs->GetBoolPref(PREF_NEWS_CANCEL_ALERT_ON_SUCCESS, &showAlertAfterCancel);
	if (NS_FAILED(rv) || showAlertAfterCancel) {
		GetNewsStringByName("messageCancelled", getter_Copies(alertText));
		rv = dialog->Alert(nsnull, alertText);
		// XXX:  todo, check rv?
	}

	
    if (!m_runningURL) return -1;

    // delete the message from the db here.
    nsMsgKey key = nsMsgKey_None;
    rv = m_runningURL->GetMessageKey(&key);

    NS_ASSERTION(NS_SUCCEEDED(rv) && m_newsFolder && (key != nsMsgKey_None), "need more to remove this message from the db");
    if ((key != nsMsgKey_None) && (m_newsFolder)) {
		rv = m_newsFolder->RemoveMessage(key);
    }
  }
   
FAIL:
  NS_ASSERTION(m_newsFolder,"no news folder");
  if (m_newsFolder) {
  	rv = m_newsFolder->CancelComplete();
  }
  PR_FREEIF (id);
  PR_FREEIF (cancelInfo.old_from);
  PR_FREEIF (cancelInfo.from);
  PR_FREEIF (subject);
  PR_FREEIF (newsgroups);
  PR_FREEIF (distribution);
  PR_FREEIF (other_random_headers);
  PR_FREEIF (body);

#ifdef USE_LIBMSG
  if (fields)
	  MSG_DestroyCompositionFields(fields);
#endif 
  
  return status;
}
  
PRInt32 nsNNTPProtocol::XPATSend()
{
	int status = 0;
	char *thisTerm = NULL;
    
	if (m_searchData &&
		(thisTerm = PL_strchr(m_searchData, '/')) != NULL) {
		/* extract the XPAT encoding for one query term */
        /* char *next_search = NULL; */
		char *command = NULL;
		char *unescapedCommand = NULL;
		char *endOfTerm = NULL;
		NET_SACopy (&command, ++thisTerm);
		endOfTerm = PL_strchr(command, '/');
		if (endOfTerm)
			*endOfTerm = '\0';
		NET_SACat(&command, CRLF);
	
		unescapedCommand = MSG_UnEscapeSearchUrl(command);

		/* send one term off to the server */
		NNTP_LOG_WRITE(command);
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, unescapedCommand);

		m_nextState = NNTP_RESPONSE;
		m_nextStateAfterResponse = NNTP_XPAT_RESPONSE;
	    SetFlag(NNTP_PAUSE_FOR_READ);

		PR_Free(command);
		PR_Free(unescapedCommand);
	}
	else
	{
		m_nextState = NEWS_DONE;
		status = MK_DATA_LOADED;
	}
	return status;
}

PRInt32 nsNNTPProtocol::XPATResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRUint32 status = 1; 

	if (m_responseCode != MK_NNTP_RESPONSE_XPAT_OK)
	{
		AlertError(MK_NNTP_ERROR_MESSAGE,m_responseText);
    	m_nextState = NNTP_ERROR;
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_NNTP_SERVER_ERROR;
	}

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	if (line)
	{
		if (line[0] != '.')
		{
			long articleNumber;
			PR_sscanf(line, "%ld", &articleNumber);
	    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	    if (mailnewsurl)
      {
        nsCOMPtr <nsIMsgSearchSession> searchSession;
        nsCOMPtr <nsIMsgSearchAdapter> searchAdapter;
        mailnewsurl->GetSearchSession(getter_AddRefs(searchSession));
        if (searchSession)
        {
          searchSession->GetRunningAdapter(getter_AddRefs(searchAdapter));
          if (searchAdapter)
            searchAdapter->AddHit((PRUint32) articleNumber);
        }
      }
		}
		else
		{
			/* set up the next term for next time around */
			char *nextTerm = PL_strchr(m_searchData, '/');

			if (nextTerm)
				m_searchData = ++nextTerm;
			else
				m_searchData = nsnull;

			m_nextState = NNTP_XPAT_SEND;
			ClearFlag(NNTP_PAUSE_FOR_READ);
			PR_FREEIF(line);
			return 0;
		}
	}
	PR_FREEIF(line);
	return 0;
}

PRInt32 nsNNTPProtocol::ListPrettyNames()
{

    nsXPIDLCString group_name;
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0; 

    nsresult rv = m_newsgroup->GetName(getter_Copies(group_name));
	PR_snprintf(outputBuffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST PRETTYNAMES %.512s" CRLF,
            NS_SUCCEEDED(rv) ? (const char *) group_name : "");
    
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer);
	NNTP_LOG_NOTE(outputBuffer);
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_PRETTY_NAMES_RESPONSE;

	return status;
}

PRInt32 nsNNTPProtocol::ListPrettyNamesResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	char *prettyName;
	PRUint32 status = 0;

	if (m_responseCode != MK_NNTP_RESPONSE_LIST_OK)
	{
		m_nextState = DISPLAY_NEWSGROUPS;
/*		m_nextState = NEWS_DONE; */
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	if (line)
	{
		if (line[0] != '.')
		{
			int i;
			/* find whitespace seperator if it exits */
			for (i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
				;  /* null body */

			if(line[i] == '\0')
				prettyName = &line[i];
			else
				prettyName = &line[i+1];

			line[i] = 0; /* terminate group name */
			if (i > 0)
              m_newsHost->SetPrettyName(line,prettyName);

			PR_LOG(NNTP,PR_LOG_ALWAYS,("adding pretty name %s", prettyName));
		}
		else
		{
			m_nextState = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list */
/*			m_nextState = NEWS_DONE;	 */ /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ);
			PR_FREEIF(line);
			return 0;
		}
	}
	PR_FREEIF(line);
	return 0;
}

PRInt32 nsNNTPProtocol::ListXActive()
{ 
	nsXPIDLCString group_name;
    	nsresult rv;
	rv = m_newsgroup->GetName(getter_Copies(group_name));
	// XXX: check rv?
	PRInt32 status = 0;
	char outputBuffer[OUTPUT_BUFFER_SIZE];

	PR_snprintf(outputBuffer, 
			OUTPUT_BUFFER_SIZE, 
			"LIST XACTIVE %.512s" CRLF,
            (const char *) group_name);

	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer);

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_XACTIVE_RESPONSE;

	return status;
}

PRInt32 nsNNTPProtocol::ListXActiveResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRUint32 status = 0;

	NS_ASSERTION(m_responseCode == MK_NNTP_RESPONSE_LIST_OK, "code != LIST_OK");
	if (m_responseCode != MK_NNTP_RESPONSE_LIST_OK)
	{
		m_nextState = DISPLAY_NEWSGROUPS;
/*		m_nextState = NEWS_DONE; */
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_DATA_LOADED;
	}

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	 /* almost correct */
	if(status > 1)
	{
#ifdef UNREADY_CODE
		ce->bytes_received += status;
		FE_GraphProgress(ce->window_id, ce->URL_s, ce->bytes_received, status, ce->URL_s->content_length);
#else
		mBytesReceived += status;
#endif
	}

	if (line)
	{
		if (line[0] != '.')
		{
			char *s = line;
			/* format is "rec.arts.movies.past-films 7302 7119 csp"
			 */
			while (*s && !NET_IS_SPACE(*s))
				s++;
			if (s)
			{
				char flags[32];	/* ought to be big enough */
				*s = 0;
				PR_sscanf(s + 1,
					   "%d %d %31s", 
					   &m_firstPossibleArticle, 
					   &m_lastPossibleArticle,
					   flags);

                m_newsHost->AddNewNewsgroup(line,
                                          m_firstPossibleArticle,
                                          m_lastPossibleArticle, flags, PR_TRUE);
				/* we're either going to list prettynames first, or list
                   all prettynames every time, so we won't care so much
                   if it gets interrupted. */
				PR_LOG(NNTP,PR_LOG_ALWAYS,("got xactive for %s of %s", line, flags));
				/*	This isn't required, because the extra info is
                    initialized to false for new groups. And it's
                    an expensive call.
				*/
				/* MSG_SetGroupNeedsExtraInfo(cd->host, line, PR_FALSE); */
			}
		}
		else
		{
          nsresult rv;
          PRBool xactive=PR_FALSE;
          rv = m_newsHost->QueryExtension("XACTIVE",&xactive);
          if (m_typeWanted == NEW_GROUPS &&
              NS_SUCCEEDED(rv) && xactive)
			{
                nsCOMPtr <nsINNTPNewsgroup> old_newsgroup;
                old_newsgroup = m_newsgroup;
                char *groupName;
                
                m_newsHost->GetFirstGroupNeedingExtraInfo(&groupName);
                m_newsHost->FindGroup(groupName, getter_AddRefs(m_newsgroup));
                // see if we got a different group
                if (old_newsgroup && m_newsgroup &&
                    (old_newsgroup != m_newsgroup))
                /* make sure we're not stuck on the same group */
                {
					PR_LOG(NNTP,PR_LOG_ALWAYS,("listing xactive for %s", groupName));
					m_nextState = NNTP_LIST_XACTIVE;
			        ClearFlag(NNTP_PAUSE_FOR_READ); 
					PR_FREEIF(line);
					return 0;
				}
				else
                {
                    m_newsgroup = null_nsCOMPtr();
				}
			}
            PRBool listpname;
            rv = m_newsHost->QueryExtension("LISTPNAME",&listpname);
            if (NS_SUCCEEDED(rv) && listpname)
				m_nextState = NNTP_LIST_PRETTY_NAMES;
			else
				m_nextState = DISPLAY_NEWSGROUPS;	/* this assumes we were doing a list - who knows? */
/*			m_nextState = NEWS_DONE;	 */ /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ);
			PR_FREEIF(line);
			return 0;
		}
	}
	PR_FREEIF(line);
	return 0;
}

PRInt32 nsNNTPProtocol::ListGroup()
{
    nsresult rv;
    nsXPIDLCString group_name;
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0; 
    rv = m_newsgroup->GetName(getter_Copies(group_name));
    
	PR_snprintf(outputBuffer, 
			OUTPUT_BUFFER_SIZE, 
			"listgroup %.512s" CRLF,
                (const char *) group_name);
    rv = nsComponentManager::CreateInstance(kNNTPArticleListCID,
                                            nsnull,
                                            NS_GET_IID(nsINNTPArticleList),
                                            getter_AddRefs(m_articleList));
    if (NS_FAILED(rv)) return -1;  // ???

    m_articleList->Initialize(m_newsHost, m_newsgroup);
	
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer); 

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_GROUP_RESPONSE;

	return status;
}

PRInt32 nsNNTPProtocol::ListGroupResponse(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRUint32 status = 0;

	NS_ASSERTION(m_responseCode == MK_NNTP_RESPONSE_GROUP_SELECTED, "code != GROUP_SELECTED");
	if (m_responseCode != MK_NNTP_RESPONSE_GROUP_SELECTED)
	{
		m_nextState = NEWS_DONE; 
		ClearFlag(NNTP_PAUSE_FOR_READ);
		return MK_DATA_LOADED;
	}

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	NNTP_LOG_READ(line);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}

	if (line)
	{
		if (line[0] != '.')
		{
			nsMsgKey found_id = nsMsgKey_None;
            nsresult rv;
			PR_sscanf(line, "%ld", &found_id);
            
            rv = m_articleList->AddArticleKey(found_id);
		}
		else
		{
			m_nextState = NEWS_DONE;	 /* ### dmb - don't really know */
			ClearFlag(NNTP_PAUSE_FOR_READ); 
			PR_FREEIF(line);
			return 0;
		}
	}
	PR_FREEIF(line);
	return 0;
}


PRInt32 nsNNTPProtocol::Search()
{
	NS_ASSERTION(0,"Search not implemented");
	return 0;
}

PRInt32 nsNNTPProtocol::SearchResponse()
{
    if (MK_NNTP_RESPONSE_TYPE(m_responseCode) == MK_NNTP_RESPONSE_TYPE_OK)
		m_nextState = NNTP_SEARCH_RESULTS;
	else
		m_nextState = NEWS_DONE;
	ClearFlag(NNTP_PAUSE_FOR_READ);
	return 0;
}

PRInt32 nsNNTPProtocol::SearchResults(nsIInputStream *inputStream, PRUint32 length)
{
	char *line = NULL;
	PRUint32 status = 1;

	PRBool pauseForMoreData = PR_FALSE;
	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);

	if(pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		return 0;
	}
	if (!line)
		return status;  /* no line yet */

	if ('.' != line[0])
	{
#ifdef UNREADY_CODE
		MSG_AddNewsSearchHit (ce->window_id, line);
#endif
	}
	else
	{
		/* all overview lines received */
		m_nextState = NEWS_DONE;
		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	PR_FREEIF(line);
	return status;
}

/* Sets state for the transfer. This used to be known as net_setup_news_stream */
PRInt32 nsNNTPProtocol::SetupForTransfer()
{ 
    if (m_typeWanted == NEWS_POST)
	{
        m_nextState = NNTP_SEND_POST_DATA;
#ifdef UNREADY_CODE
		NET_Progress(ce->window_id, XP_GetString(MK_MSG_DELIV_NEWS));
#endif
	}
    else if(m_typeWanted == LIST_WANTED)
	{
		if (TestFlag(NNTP_USE_FANCY_NEWSGROUP))
			m_nextState = NNTP_LIST_XACTIVE_RESPONSE;
		else
			m_nextState = NNTP_READ_LIST_BEGIN;
	}
    else if(m_typeWanted == GROUP_WANTED)
		m_nextState = NNTP_XOVER_BEGIN;
	else if(m_typeWanted == NEW_GROUPS)
		m_nextState = NNTP_NEWGROUPS_BEGIN;
    else if(m_typeWanted == ARTICLE_WANTED ||
			m_typeWanted== CANCEL_WANTED)
	    m_nextState = NNTP_BEGIN_ARTICLE;
	else if (m_typeWanted== SEARCH_WANTED)
		m_nextState = NNTP_XPAT_SEND;
	else if (m_typeWanted == PRETTY_NAMES_WANTED)
		m_nextState = NNTP_LIST_PRETTY_NAMES;
#ifdef UNREADY_CODE
	else if (m_typeWanted == PROFILE_WANTED)
	{
		if (PL_strstr(ce->URL_s->address, "PROFILE NEW"))
			m_nextState = NNTP_PROFILE_ADD;
		else
			m_nextState = NNTP_PROFILE_DELETE;
	}
#endif
	else
	  {
		NS_ASSERTION(0, "unexpected");
		return -1;
	  }

   return(0); /* good */
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following method is used for processing the news state machine. 
// It returns a negative number (mscott: we'll change this to be an enumerated type which we'll coordinate
// with the netlib folks?) when we are done processing.
//////////////////////////////////////////////////////////////////////////////////////////////////////////
nsresult nsNNTPProtocol::ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									      PRUint32 sourceOffset, PRUint32 length)
{
	PRInt32 status = 0; 
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (!mailnewsurl)
    return NS_OK; // probably no data available - it's OK.

#ifdef UNREADY_CODE
    if (m_offlineNewsState != NULL)
	{
		return NET_ProcessOfflineNews(ce, cd);
	}
#endif
    
	ClearFlag(NNTP_PAUSE_FOR_READ); 

    while(!TestFlag(NNTP_PAUSE_FOR_READ))
	{
		PR_LOG(NNTP,PR_LOG_ALWAYS,("Next state: %s",stateLabels[m_nextState]));
		// examine our current state and call an appropriate handler for that state.....
        switch(m_nextState)
        {
            case NNTP_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = NewsResponse(inputStream, length);
                break;

			// mscott: I've removed the states involving connections on the assumption
			// that core netlib will now be managing that information.

            case NNTP_LOGIN_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = LoginResponse();
                break;

			case NNTP_SEND_MODE_READER:
                status = SendModeReader(); 
                break;

			case NNTP_SEND_MODE_READER_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendModeReaderResponse(); 
                break;

			case SEND_LIST_EXTENSIONS:
				status = SendListExtensions(); 
				break;
			case SEND_LIST_EXTENSIONS_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendListExtensionsResponse(inputStream, length);
				break;
			case SEND_LIST_SEARCHES:
				status = SendListSearches(); 
				break;
			case SEND_LIST_SEARCHES_RESPONSE:
				if (inputStream == nsnull) 
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendListSearchesResponse(inputStream, length); 
				break;
			case NNTP_LIST_SEARCH_HEADERS:
				status = SendListSearchHeaders();
				break;
			case NNTP_LIST_SEARCH_HEADERS_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendListSearchHeadersResponse(inputStream, length); 
				break;
			case NNTP_GET_PROPERTIES:
				status = GetProperties();
				break;
			case NNTP_GET_PROPERTIES_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = GetPropertiesResponse(inputStream, length);
				break;				
			case SEND_LIST_SUBSCRIPTIONS:
				status = SendListSubscriptions();
				break;
			case SEND_LIST_SUBSCRIPTIONS_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendListSubscriptionsResponse(inputStream, length);
				break;

            case SEND_FIRST_NNTP_COMMAND:
                status = SendFirstNNTPCommand(url);
                break;
            case SEND_FIRST_NNTP_COMMAND_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendFirstNNTPCommandResponse();
                break;

            case NNTP_SEND_GROUP_FOR_ARTICLE:
                status = SendGroupForArticle();
                break;
            case NNTP_SEND_GROUP_FOR_ARTICLE_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendGroupForArticleResponse();
                break;
            case NNTP_SEND_ARTICLE_NUMBER:
                status = SendArticleNumber();
                break;

            case SETUP_NEWS_STREAM:
                status = SetupForTransfer();
                break;

			case NNTP_BEGIN_AUTHORIZE:
				status = BeginAuthorization(); 
                break;

			case NNTP_AUTHORIZE_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = AuthorizationResponse(); 
                break;

			case NNTP_PASSWORD_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = PasswordResponse();
                break;
    
			// read list
            case NNTP_READ_LIST_BEGIN:
                status = BeginReadNewsList(); 
                break;
            case NNTP_READ_LIST:
                status = ReadNewsList(inputStream, length);
                break;

			// news group
			case DISPLAY_NEWSGROUPS:
				status = DisplayNewsgroups(); 
                break;
			case NNTP_NEWGROUPS_BEGIN:
				status = BeginNewsgroups();
                break;
   			case NNTP_NEWGROUPS:
				status = ProcessNewsgroups(inputStream, length);
                break;
    
			// article specific
            case NNTP_BEGIN_ARTICLE:
				status = BeginArticle(); 
                break;

            case NNTP_READ_ARTICLE:
				status = ReadArticle(inputStream, length);
				break;
			
            case NNTP_XOVER_BEGIN:
#ifdef UNREADY_CODE
			    NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_READ_NEWSGROUPINFO));
#endif
			    status = BeginReadXover();
                break;

			case NNTP_FIGURE_NEXT_CHUNK:
				status = FigureNextChunk(); 
				break;

			case NNTP_XOVER_SEND:
				status = XoverSend();
				break;
    
            case NNTP_XOVER:
                status = ReadXover(inputStream, length);
                break;

            case NNTP_XOVER_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = ReadXoverResponse();
                break;

            case NEWS_PROCESS_XOVER:
		    case NEWS_PROCESS_BODIES:
#ifdef UNREADY_CODE
                NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_SORT_ARTICLES));
#endif
				status = ProcessXover();
                break;

            case NNTP_READ_GROUP:
                status = ReadNewsgroup();
                break;
    
            case NNTP_READ_GROUP_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = ReadNewsgroupResponse();
                break;

            case NNTP_READ_GROUP_BODY:
                status = ReadNewsgroupResponse();
                break;

	        case NNTP_SEND_POST_DATA:
	            status = PostData();
	            break;
	        case NNTP_SEND_POST_DATA_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = PostDataResponse();
	            break;

			case NNTP_CHECK_FOR_MESSAGE:
				status = CheckForArticle();
				break;

			case NEWS_NEWS_RC_POST:
#ifdef UNREADY_CODE
		        status = net_NewsRCProcessPost(ce);
#endif
		        break;

            case NEWS_DISPLAY_NEWS_RC:
		        status = DisplayNewsRC(); 
				break;
            case NEWS_DISPLAY_NEWS_RC_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = DisplayNewsRCResponse();
		        break;

			// cancel
            case NEWS_START_CANCEL:
		        status = StartCancel();
		        break;

            case NEWS_DO_CANCEL:
		        status = DoCancel();
		        break;

			// XPAT
			case NNTP_XPAT_SEND:
				status = XPATSend();
				break;
			case NNTP_XPAT_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = XPATResponse(inputStream, length);
				break;

			// search
			case NNTP_SEARCH:
				status = Search();
				break;
			case NNTP_SEARCH_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SearchResponse();
				break;
			case NNTP_SEARCH_RESULTS:
				status = SearchResults(inputStream, length);
				break;

			
			case NNTP_LIST_PRETTY_NAMES:
				status = ListPrettyNames();
				break;
			case NNTP_LIST_PRETTY_NAMES_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = ListPrettyNamesResponse(inputStream, length);
				break;
			case NNTP_LIST_XACTIVE:
				status = ListXActive();
				break;
			case NNTP_LIST_XACTIVE_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = ListXActiveResponse(inputStream, length);
				break;
			case NNTP_LIST_GROUP:
				status = ListGroup();
				break;
			case NNTP_LIST_GROUP_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = ListGroupResponse(inputStream, length);
				break;
	        case NEWS_DONE:
				m_nextState = NEWS_FREE;

#if 0   // mscott 01/04/99. This should be temporary until I figure out what to do with this code.....
			  if (cd->stream)
				COMPLETE_STREAM;

	            cd->next_state = NEWS_FREE;
                /* set the connection unbusy
     	         */
    		    cd->control_con->busy = PR_FALSE;
                NET_TotalNumberOfOpenConnections--;

				NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				NET_RefreshCacheFileExpiration(ce->URL_s);
#endif
	            break;

			case NEWS_POST_DONE:
				NNTP_LOG_NOTE("NEWS_POST_DONE");
				mailnewsurl->SetUrlState(PR_FALSE, NS_OK);
				m_nextState = NEWS_FREE;
				break;
	        case NEWS_ERROR:
				NNTP_LOG_NOTE("NEWS_ERROR");
				mailnewsurl->SetUrlState(PR_FALSE, NS_ERROR_FAILURE);
				m_nextState = NEWS_FREE;
#if 0   // mscott 01/04/99. This should be temporary until I figure out what to do with this code.....
	            if(cd->stream)
		             ABORT_STREAM(status);

    	        /* set the connection unbusy
     	         */
    		    cd->control_con->busy = PR_FALSE;
                NET_TotalNumberOfOpenConnections--;

				if(cd->control_con->csock != NULL)
				  {
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
				  }
#endif
	            break;

	        case NNTP_ERROR:
#if 0   // mscott 01/04/99. This should be temporary until I figure out what to do with this code.....
	            if(cd->stream)
				  {
		            ABORT_STREAM(status);
					cd->stream=0;
				  }
    
				if(cd->control_con && cd->control_con->csock != NULL)
				  {
					NNTP_LOG_NOTE(("Clearing read and connect select on socket %d",
															cd->control_con->csock));
					NET_ClearConnectSelect(ce->window_id, cd->control_con->csock);
					NET_ClearReadSelect(ce->window_id, cd->control_con->csock);
#ifdef XP_WIN
					if(cd->calling_netlib_all_the_time)
					{
						cd->calling_netlib_all_the_time = PR_FALSE;
						NET_ClearCallNetlibAllTheTime(ce->window_id,"mknews");
					}
#endif /* XP_WIN */

#if defined(XP_WIN) || (defined(XP_UNIX)&&defined(UNIX_ASYNC_DNS))
                    NET_ClearDNSSelect(ce->window_id, cd->control_con->csock);
#endif /* XP_WIN || XP_UNIX */
				    net_nntp_close (cd->control_con, status);  /* close the
																  socket */ 
					NET_TotalNumberOfOpenConnections--;
					ce->socket = NULL;
				  }
#endif // mscott's temporary #if 0...
                /* check if this connection came from the cache or if it was
                 * a new connection.  If it was not new lets start it over
				 * again.  But only if we didn't have any successful protocol
				 * dialog at all.
                 */

				// mscott: I've removed the code that used to be here because it involved connection
				// management which should now be handled by the netlib module.
				m_nextState = NEWS_FREE;
				break;
    
            case NEWS_FREE:
				// mscott: we really haven't worked out how we are going to 
				// keep the news connection open. We probably want a news connection
				// cache so we aren't creating new connections to process each request...
				// but until that time, we always want to properly shutdown the connection


        m_connectionBusy = PR_FALSE;
        mailnewsurl->SetUrlState(PR_FALSE, NS_OK);
        m_lastActiveTimeStamp = PR_Now(); // remmeber when we last used this connection.
        return CleanupAfterRunningUrl();
			case NEWS_FINISHED:
				return NS_OK;
				break;
            default:
                /* big error */
                return NS_ERROR_FAILURE;
          
		} // end switch

        if(status < 0 && m_nextState != NEWS_ERROR &&
           m_nextState != NNTP_ERROR && m_nextState != NEWS_FREE)
        {
			m_nextState = NNTP_ERROR;
            ClearFlag(NNTP_PAUSE_FOR_READ);
        }
      
	} /* end big while */

	return NS_OK; /* keep going */
}

NS_IMETHODIMP nsNNTPProtocol::CloseConnection()
{
	SendData(nsnull, NNTP_CMD_QUIT); // this will cause OnStopRequest get called, which will call CloseSocket()
  return NS_OK;
}

nsresult nsNNTPProtocol::CleanupAfterRunningUrl()
{
  /* do we need to know if we're parsing xover to call finish xover?  */
  /* yes, I think we do! Why did I think we should??? */
  /* If we've gotten to NEWS_FREE and there is still XOVER
     data, there was an error or we were interrupted or
     something.  So, tell libmsg there was an abnormal
     exit so that it can free its data. */
  
  nsresult rv = NS_OK;

	if (m_channelListener)
		rv = m_channelListener->OnStopRequest(this, m_channelContext, NS_OK, nsnull);

	if (m_loadGroup)
		m_loadGroup->RemoveChannel(NS_STATIC_CAST(nsIChannel *, this), nsnull, NS_OK, nsnull);

	if (m_newsgroupList)
	{
		int status;
       /* XXX - how/when to Release() this? */
        rv = m_newsgroupList->FinishXOVERLINE(0,&status);
		NS_ASSERTION(NS_SUCCEEDED(rv), "FinishXOVERLINE failed");
	}
	else
	{
      /* XXX - state is stored in the newshost - should
         we be releasing it here? */
      /* NS_RELEASE(m_newsgroup->GetNewsgroupList()); */
	}

    PR_FREEIF(m_path);
    PR_FREEIF(m_responseText);
    PR_FREEIF(m_dataBuf);

    PR_FREEIF(m_cancelNewsgroups);
    m_cancelNewsgroups = nsnull;
    PR_FREEIF(m_cancelDistribution);
    m_cancelDistribution = nsnull;
    PR_FREEIF(m_cancelFromHdr);
    m_cancelFromHdr = nsnull;
    PR_FREEIF(m_cancelID);  
    m_cancelID = nsnull;
    
	m_runningURL = null_nsCOMPtr();
  m_connectionBusy = PR_FALSE;
  return NS_OK;
}

nsresult nsNNTPProtocol::CloseSocket()
{
  CleanupAfterRunningUrl(); // is this needed?
	return nsMsgProtocol::CloseSocket();
}

void nsNNTPProtocol::SetProgressBarPercent(PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (mProgressEventSink)
    mProgressEventSink->OnProgress(this, m_channelContext, aProgress, aProgressMax);                                       
}

void
nsNNTPProtocol::SetProgressStatus(char *message)
{
  nsresult rv;
  PR_LOG(NNTP,PR_LOG_ALWAYS,("nsNNTPProtocol::SetProgressStatus(%s)",message));
  if (mProgressEventSink) 
  {
    nsAutoString formattedString; 
    formattedString.AssignWithConversion(message);
    rv = mProgressEventSink->OnStatus(this, m_channelContext, NS_OK, formattedString.GetUnicode());      // XXX i18n message
    NS_ASSERTION(NS_SUCCEEDED(rv), "dropping error result");
  }
}

NS_IMETHODIMP nsNNTPProtocol::GetContentType(char * *aContentType)
{	
	if (!aContentType) return NS_ERROR_NULL_POINTER;

  // if we've been set with a content type, then return it....
  // this happens when we go through libmime now as it sets our new content type
  if (!m_ContentType.IsEmpty())
  {
    *aContentType = m_ContentType.ToNewCString();
    return NS_OK;
  }

  // otherwise do what we did before...

	if ((const char *)m_currentGroup && nsCRT::strlen((const char *)m_currentGroup)) {
		// if it is an article url, it has a @ or %40 in it.
    if (PL_strchr((const char *)m_currentGroup,'@') || PL_strstr((const char *)m_currentGroup,"%40") 
        || m_typeWanted == ARTICLE_WANTED) {
			*aContentType = nsCRT::strdup("message/rfc822");
		}
		else {
			*aContentType = nsCRT::strdup("x-application-newsgroup");
		}
	}
	else {
		*aContentType = nsCRT::strdup("message/rfc822");
	}
	if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
	return NS_OK;
}

nsresult
nsNNTPProtocol::AlertError(PRInt32 errorCode, const char *text)
{
	nsresult rv = NS_OK;

  // get the prompt from the running url....
  if (m_runningURL)
  {
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(m_runningURL));
    nsCOMPtr<nsIPrompt> dialog;
    rv = GetPromptDialogFromUrl(msgUrl, getter_AddRefs(dialog));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString newsString;
	  rv = GetNewsStringByID(errorCode, getter_Copies(newsString));
	  if (NS_FAILED(rv)) return rv;

	  nsAutoString alertText;
	  alertText.AssignWithConversion("NEWS ERROR:  ");
	  alertText.Append((const PRUnichar *)newsString);

	  if (text)
		  alertText.AppendWithConversion(text);

	  rv = dialog->Alert(nsnull, alertText.GetUnicode());
  }
  
  return rv;
}

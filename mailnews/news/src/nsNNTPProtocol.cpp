/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Alec Flett <alecf@netscape.com>
 *   David Bienvenu <bienvenu@netscape.com>
 *   Jeff Tsai <jefft@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Håkan Waara <hwaara@chello.se>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build (sorry this breaks the PCH) */
#endif

#include "msgCore.h"    // precompiled header...
#include "MailNewsTypes.h"
#include "nntpCore.h"
#include "nsNetUtil.h"

#include "nsIMsgHdr.h"
#include "nsNNTPProtocol.h"
#include "nsINNTPArticleList.h"
#include "nsIOutputStream.h"
#include "nsFileStream.h"
#include "nsIMemory.h"
#include "nsIPipe.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"

#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"

#include "nsINntpUrl.h"

#include "nsCRT.h"

#include "prtime.h"
#include "prlog.h"
#include "prerror.h"
#include "nsEscape.h"
#include "xp_core.h"

#include "prprf.h"

/* include event sink interfaces for news */

#include "nsIMsgHeaderParser.h" 
#include "nsIMsgSearchSession.h"
#include "nsIMsgSearchAdapter.h"
#include "nsIMsgStatusFeedback.h"

#include "nsMsgKeySet.h"

#include "nsNewsUtils.h"
#include "nsMsgUtils.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgAccountManager.h"

#include "nsIPrompt.h"
#include "nsIMsgStatusFeedback.h" 

#include "nsIFolder.h"
#include "nsIMsgFolder.h"
#include "nsIMsgNewsFolder.h"
#include "nsIDocShell.h"

// for the memory cache...
#include "nsICacheEntryDescriptor.h"
#include "nsICacheSession.h"
#include "nsIStreamListener.h"
#include "nsNetCID.h"

#include "nsIPref.h"

#include "nsIMsgWindow.h"
#include "nsIWindowWatcher.h"

#include "nsINntpService.h"
#include "nntpCore.h"
#include "nsIStreamConverterService.h"

#undef GetPort  // XXX Windows!
#undef SetPort  // XXX Windows!

#define PREF_NEWS_CANCEL_CONFIRM	"news.cancel.confirm"
#define PREF_NEWS_CANCEL_ALERT_ON_SUCCESS "news.cancel.alert_on_success"
#define READ_NEWS_LIST_COUNT_MAX 500 /* number of groups to process at a time when reading the list from the server */
#define READ_NEWS_LIST_TIMEOUT 50	/* uSec to wait until doing more */
#define RATE_STR_BUF_LEN 32
#define UPDATE_THRESHHOLD 25600 /* only update every 25 KB */

// NNTP extensions are supported yet
// until the extension code is ported, 
// we'll skip right to the first nntp command 
// after doing "mode reader"
// and "pushed" authentication (if necessary),
//#define HAVE_NNTP_EXTENSIONS

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

static NS_DEFINE_CID(kIStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kNNTPArticleListCID, NS_NNTPARTICLELIST_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kPrefServiceCID,NS_PREF_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);

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
PR_LOG(NNTP, out, ("(%p) Receiving: %s", this, buf)) ;

#define NNTP_LOG_WRITE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("(%p) Sending: %s", this, buf)) ;

#define NNTP_LOG_NOTE(buf) \
if (NNTP==NULL) \
    NNTP = PR_NewLogModule("NNTP"); \
PR_LOG(NNTP, out, ("(%p) %s",this, buf)) ;

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

////////////////////////////////////////////////////////////////////////////////////////////
// TEMPORARY HARD CODED FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////
#ifdef XP_WIN
static char *XP_AppCodeName = "Mozilla";
#else
static const char *XP_AppCodeName = "Mozilla";
#endif
#define NET_IS_SPACE(x) ((x)==' ' || (x)=='\t')

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
  NS_INTERFACE_MAP_ENTRY(nsICacheListener)
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
    m_key = nsMsgKey_None;

    m_commandSpecificData = nsnull;
    m_searchData = nsnull;

	mBytesReceived = 0;
    mBytesReceivedSinceLastStatusUpdate = 0;
    m_startTime = PR_Now();

    if (aMsgWindow) {
        m_msgWindow = aMsgWindow;
    }

	m_runningURL = nsnull;
  SetIsBusy(PR_FALSE);
  m_fromCache = PR_FALSE;
    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) creating",this));
    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) initializing, so unset m_currentGroup",this));
	m_currentGroup = "";
  LL_I2L(m_lastActiveTimeStamp, 0);
}

nsNNTPProtocol::~nsNNTPProtocol()
{
    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) destroying",this));
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
    PRBool isSecure = PR_FALSE;

    if (aMsgWindow) {
        m_msgWindow = aMsgWindow;
    }
    nsMsgProtocol::InitFromURI(aURL);

    nsXPIDLCString userName;
    rv = m_url->GetPreHost(getter_Copies(userName));
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString hostName;
    rv = m_url->GetHost(getter_Copies(hostName));
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr <nsIMsgAccountManager> accountManager = do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    // find the server
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = accountManager->FindServer((const char *)userName, (const char *)hostName, "nntp",
                                    getter_AddRefs(server));
    NS_ENSURE_SUCCESS(rv, NS_MSG_INVALID_OR_MISSING_SERVER);
    if (!server) return NS_MSG_INVALID_OR_MISSING_SERVER;
    
    m_nntpServer = do_QueryInterface(server, &rv);
    NS_ENSURE_SUCCESS(rv, NS_MSG_INVALID_OR_MISSING_SERVER);
    if (!m_nntpServer) return NS_MSG_INVALID_OR_MISSING_SERVER;

	rv = m_nntpServer->GetMaxArticles(&m_maxArticles);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = server->GetIsSecure(&isSecure);
    NS_ENSURE_SUCCESS(rv,rv);

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

	NS_PRECONDITION(m_url , "invalid URL passed into NNTP Protocol");

	m_runningURL = do_QueryInterface(m_url);
  SetIsBusy(PR_TRUE);
  PRBool msgIsInLocalCache = PR_FALSE;
	if (NS_SUCCEEDED(rv) && m_runningURL)
	{
  	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
    if (mailnewsUrl)
    {
      mailnewsUrl->SetMsgWindow(aMsgWindow);
      mailnewsUrl->GetMsgIsInLocalCache(&msgIsInLocalCache);
    }
    if (msgIsInLocalCache)
      return NS_OK; // probably don't need to do anything else - definitely don't want
                    // to open the socket.
	}
  else {
    return rv;
  }
	
  if (!m_socketIsOpen)
  {

    nsCOMPtr<nsIInterfaceRequestor> ir;
    if (aMsgWindow) {
      nsCOMPtr<nsIDocShell> docShell;
      aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
      ir = do_QueryInterface(docShell);
    }

    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) opening connection to %s on port %d",this, hostName.get(), port));
    // call base class to set up the transport

    PRInt32 port = 0;
    nsXPIDLCString hostName;
    m_url->GetPort(&port);
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_nntpServer);
    if (server)
      server->GetRealHostName(getter_Copies(hostName));

    if (isSecure)
      rv = OpenNetworkSocketWithInfo(hostName.get(), port, "ssl-forcehandshake", ir);
    else
      rv = OpenNetworkSocketWithInfo(hostName.get(), port, nsnull, ir);

	NS_ENSURE_SUCCESS(rv,rv);
	m_nextState = NNTP_LOGIN_RESPONSE;
  }
  else {
    m_nextState = SEND_FIRST_NNTP_COMMAND;
  }
	m_dataBuf = (char *) PR_Malloc(sizeof(char) * OUTPUT_BUFFER_SIZE);
	m_dataBufSize = OUTPUT_BUFFER_SIZE;

  if (!m_lineStreamBuffer)
	  m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE /* create new lines */);

	m_typeWanted = 0;
	m_responseCode = 0;
	m_previousResponseCode = 0;
	m_responseText = nsnull;

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

    m_key = nsMsgKey_None;

	m_articleNumber = 0;
	m_originalContentLength = 0;
	m_cancelID = nsnull;
	m_cancelFromHdr = nsnull;
	m_cancelNewsgroups = nsnull;
	m_cancelDistribution = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsNNTPProtocol::GetIsBusy(PRBool *aIsBusy)
{
  NS_ENSURE_ARG_POINTER(aIsBusy);
  *aIsBusy = m_connectionBusy;
  return NS_OK;
}

NS_IMETHODIMP nsNNTPProtocol::SetIsBusy(PRBool aIsBusy)
{
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) setting busy to %d",this, aIsBusy));
  m_connectionBusy = aIsBusy;
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
  nsCOMPtr<nsINntpUrl> newsUrl (do_QueryInterface(aURL));
  newsUrl->GetNewsAction(&m_newsAction);

  SetupPartExtractorListener(m_channelListener);
  return LoadUrl(aURL, aConsumer);
}


// WARNING: the cache stream listener is intended to be accessed from the UI thread!
// it will NOT create another proxy for the stream listener that gets passed in...
class nsNntpCacheStreamListener : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsNntpCacheStreamListener ();
  virtual ~nsNntpCacheStreamListener();

  nsresult Init(nsIStreamListener * aStreamListener, nsIChannel* channel, nsIMsgMailNewsUrl *aRunningUrl);
protected:
    nsCOMPtr<nsIChannel> mChannelToUse;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIMsgMailNewsUrl> mRunningUrl;
};

NS_IMPL_ADDREF(nsNntpCacheStreamListener);
NS_IMPL_RELEASE(nsNntpCacheStreamListener);

NS_INTERFACE_MAP_BEGIN(nsNntpCacheStreamListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END

nsNntpCacheStreamListener::nsNntpCacheStreamListener()
{
  NS_INIT_ISUPPORTS();
}

nsNntpCacheStreamListener::~nsNntpCacheStreamListener()
{}

nsresult nsNntpCacheStreamListener::Init(nsIStreamListener * aStreamListener, nsIChannel* channel,
                                         nsIMsgMailNewsUrl *aRunningUrl)
{
  NS_ENSURE_ARG(aStreamListener);
  NS_ENSURE_ARG(channel);
  
  mChannelToUse = channel;

  mListener = aStreamListener;
  mRunningUrl = aRunningUrl;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpCacheStreamListener::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  nsCOMPtr <nsILoadGroup> loadGroup;
  nsCOMPtr <nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);

  mChannelToUse->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
    loadGroup->AddRequest(ourRequest, nsnull /* context isupports */);
  return mListener->OnStartRequest(ourRequest, aCtxt);
}

NS_IMETHODIMP
nsNntpCacheStreamListener::OnStopRequest(nsIRequest *request, nsISupports * aCtxt, nsresult aStatus)
{
  nsCOMPtr <nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);
  nsresult rv = mListener->OnStopRequest(ourRequest, aCtxt, aStatus);
  nsCOMPtr <nsILoadGroup> loadGroup;
  mChannelToUse->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
      loadGroup->RemoveRequest(ourRequest, nsnull, aStatus);

  // clear out mem cache entry so we're not holding onto it.
  if (mRunningUrl)
    mRunningUrl->SetMemCacheEntry(nsnull);

  mListener = nsnull;
  nsCOMPtr <nsINNTPProtocol> nntpProtocol = do_QueryInterface(mChannelToUse);
  if (nntpProtocol) {
    rv = nntpProtocol->SetIsBusy(PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  mChannelToUse = nsnull;
  return rv;
}

NS_IMETHODIMP
nsNntpCacheStreamListener::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt, nsIInputStream * aInStream, PRUint32 aSourceOffset, PRUint32 aCount)
{
    nsCOMPtr <nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);
    return mListener->OnDataAvailable(ourRequest, aCtxt, aInStream, aSourceOffset, aCount);
}

nsresult nsNNTPProtocol::SetupPartExtractorListener(nsIStreamListener * aConsumer)
{
  if (m_newsAction == nsINntpUrl::ActionFetchPart)
  {
    nsCOMPtr<nsIStreamConverterService> converter = do_GetService(kIStreamConverterServiceCID);
    if (converter && aConsumer)
    {
      nsCOMPtr<nsIStreamListener> newConsumer;
      nsIChannel * channel;
      QueryInterface(NS_GET_IID(nsIChannel), (void **) &channel);
      converter->AsyncConvertData(NS_LITERAL_STRING("message/rfc822").get(), NS_LITERAL_STRING("*/*").get(),
           aConsumer, channel, getter_AddRefs(newConsumer));
      if (newConsumer)
        m_channelListener = newConsumer;
      NS_IF_RELEASE(channel);
    }
  }

  return NS_OK;
}

nsresult nsNNTPProtocol::ReadFromMemCache(nsICacheEntryDescriptor *entry)
{
  NS_ENSURE_ARG(entry);

  nsCOMPtr<nsITransport> cacheTransport;
  nsresult rv = entry->GetTransport(getter_AddRefs(cacheTransport));
     
  if (NS_SUCCEEDED(rv))
  {
    nsXPIDLCString group;
    nsXPIDLCString commandSpecificData;
    // do this to get m_key set, so that marking the message read will work.
    rv = ParseURL(m_url, getter_Copies(group), &m_messageID, getter_Copies(commandSpecificData));

    nsNntpCacheStreamListener * cacheListener = new nsNntpCacheStreamListener();
    NS_ADDREF(cacheListener);

    SetLoadGroup(m_loadGroup);
    m_typeWanted = ARTICLE_WANTED;

    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
    cacheListener->Init(m_channelListener, NS_STATIC_CAST(nsIChannel *, this), mailnewsUrl);
    
    nsCOMPtr<nsIRequest> request;
    m_ContentType = ""; // reset the content type for the upcoming read....
    rv = cacheTransport->AsyncRead(cacheListener, m_channelContext, 0, PRUint32(-1), 0, getter_AddRefs(request));
    NS_RELEASE(cacheListener);
 
    MarkCurrentMsgRead();
    if (NS_SUCCEEDED(rv)) // ONLY if we succeeded in actually starting the read should we return
    {
      // we're not calling nsMsgProtocol::AsyncRead(), which calls nsNNTPProtocol::LoadUrl, so we need to take care of some stuff it does.
      m_channelListener = nsnull;
      return rv;
    }
  }

  return rv;
}

nsresult nsNNTPProtocol::ReadFromNewsConnection()
{
  return nsMsgProtocol::AsyncOpen(m_channelListener, m_channelContext);
}

// for messages stored in our offline cache, we have special code to handle that...
// If it's in the local cache, we return true and we can abort the download because
// this method does the rest of the work.
PRBool nsNNTPProtocol::ReadFromLocalCache()
{
  PRBool msgIsInLocalCache = PR_FALSE;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL);
  mailnewsUrl->GetMsgIsInLocalCache(&msgIsInLocalCache);

  if (msgIsInLocalCache)
  {
    nsXPIDLCString group;
    nsXPIDLCString commandSpecificData;
    rv = ParseURL(m_url, getter_Copies(group), &m_messageID, getter_Copies(commandSpecificData));
    nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(m_newsFolder);
    if (folder && NS_SUCCEEDED(rv))
    {
    // we want to create a file channel and read the msg from there.
      nsCOMPtr<nsITransport> fileChannel;
      PRUint32 offset=0, size=0;
      rv = folder->GetOfflineFileTransport(m_key, &offset, &size, getter_AddRefs(fileChannel));

      // get the file channel from the folder, somehow (through the message or
      // folder sink?) We also need to set the transfer offset to the message offset
      if (fileChannel && NS_SUCCEEDED(rv))
      {
        // dougt - This may break the ablity to "cancel" a read from offline mail reading.
        // fileChannel->SetLoadGroup(m_loadGroup);

        m_typeWanted = ARTICLE_WANTED;
        nsNntpCacheStreamListener * cacheListener = new nsNntpCacheStreamListener();
        NS_ADDREF(cacheListener);
        cacheListener->Init(m_channelListener, NS_STATIC_CAST(nsIChannel *, this), mailnewsUrl);
        nsCOMPtr<nsIRequest> request;
        rv = fileChannel->AsyncRead(cacheListener, m_channelContext, offset, size, 0, getter_AddRefs(request));
        NS_RELEASE(cacheListener);
        MarkCurrentMsgRead();

        if (NS_SUCCEEDED(rv)) // ONLY if we succeeded in actually starting the read should we return
        {
          m_ContentType = "";
          m_channelListener = nsnull;
          return PR_TRUE;
        }
      }
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsNNTPProtocol::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry, nsCacheAccessMode access, nsresult status)
{
  nsresult rv = NS_OK;

  if (NS_SUCCEEDED(status)) 
  {
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL, &rv);
    mailnewsUrl->SetMemCacheEntry(entry);

    // if we have write access then insert a "stream T" into the flow so data 
    // gets written to both 
    if (access & nsICache::ACCESS_WRITE && !(access & nsICache::ACCESS_READ))
    {
      entry->MarkValid();
      // use a stream listener Tee to force data into the cache and to our current channel listener...
      nsCOMPtr<nsIStreamListener> newListener;
      nsCOMPtr<nsIStreamListenerTee> tee = do_CreateInstance(kStreamListenerTeeCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsITransport> transport;
      rv = entry->GetTransport(getter_AddRefs(transport));
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIOutputStream> out;
      rv = transport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(out));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = tee->Init(m_channelListener, out);
      m_channelListener = do_QueryInterface(tee);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else
    {
      rv = ReadFromMemCache(entry);
      if (access & nsICache::ACCESS_WRITE)
        entry->MarkValid();
      if (NS_SUCCEEDED(rv)) return NS_OK; // kick out if reading from the cache succeeded...
    }
  } // if we got a valid entry back from the cache...

  // if reading from the cache failed or if we are writing into the cache, default to ReadFromImapConnection.
  return ReadFromNewsConnection();
}

nsresult nsNNTPProtocol::OpenCacheEntry()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL, &rv);
  // get the cache session from our nntp service...
  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICacheSession> cacheSession;
  rv = nntpService->GetCacheSession(getter_AddRefs(cacheSession));
  NS_ENSURE_SUCCESS(rv, rv);

  // Open a cache entry with key = url
  nsXPIDLCString urlSpec;
  mailnewsUrl->GetSpec(getter_Copies(urlSpec));
  // for now, truncate of the query part so we don't duplicate urls in the cache...
  char * anchor = PL_strrchr(urlSpec, '?');
  if (anchor)
    *anchor = '\0';
  return cacheSession->AsyncOpenCacheEntry(urlSpec, nsICache::ACCESS_READ_WRITE, this);
}

NS_IMETHODIMP nsNNTPProtocol::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  nsresult rv;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningURL, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  PRInt32 port;
  rv = mailnewsUrl->GetPort(&port);
  if (NS_FAILED(rv))
      return rv;
 
  rv = NS_CheckPortSafety(port, "news");
  if (NS_FAILED(rv))
      return rv;

  m_channelContext = ctxt;
  m_channelListener = listener;
  m_runningURL->GetNewsAction(&m_newsAction);
  // first, check if this is a message load that should come from either
  // the memory cache or the local msg cache.
  if (mailnewsUrl && (m_newsAction == nsINntpUrl::ActionFetchArticle || m_newsAction == nsINntpUrl::ActionFetchPart))
  {
    SetupPartExtractorListener(m_channelListener);
    if (ReadFromLocalCache())
     return NS_OK;

    rv = OpenCacheEntry();
    if (NS_SUCCEEDED(rv)) return NS_OK; // if this didn't return an error then jump out now...

  }
  nsCOMPtr<nsIRequest> parentRequest;
  return nsMsgProtocol::AsyncOpen(listener, ctxt);
}

nsresult nsNNTPProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer)
{
  NS_ENSURE_ARG_POINTER(aURL);

  nsXPIDLCString group;
  nsXPIDLCString commandSpecificData;
  PRBool cancel = PR_FALSE;
  m_ContentType = "";
  nsresult rv = NS_OK;

  m_runningURL = do_QueryInterface(aURL, &rv);
  if (NS_FAILED(rv)) return rv;
  m_runningURL->GetNewsAction(&m_newsAction);

  SetIsBusy(PR_TRUE);

  PR_FREEIF(m_messageID);
  m_messageID = nsnull;

  rv = ParseURL(aURL, getter_Copies(group), &m_messageID, getter_Copies(commandSpecificData));
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to parse news url");
  //if (NS_FAILED(rv)) return rv;

  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) m_messageID = %s",this, m_messageID?m_messageID:"(null)"));
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) group = %s",this,group.get()));
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) commandSpecificData = %s",this,commandSpecificData.get()?commandSpecificData.get():"(null)"));
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) m_key = %d",this,m_key));
 
  // for now, only support "news://host/message-id?cancel", and not "news://host/group#key?cancel"
  if (m_messageID && !PL_strcmp(commandSpecificData.get(), "?cancel")) {
     // XXX todo, don't allow manual cancel urls.  only allow internal ones
     cancel = PR_TRUE;
  }

  NS_MsgSACopy(&m_path, m_messageID);

  /* We are posting a user-written message
     if and only if this message has a message to post
     Cancel messages are created later with a ?cancel URL
  */
  nsCOMPtr <nsINNTPNewsgroupPost> message;
  rv = m_runningURL->GetMessageToPost(getter_AddRefs(message));
  if (NS_SUCCEEDED(rv) && message)
	{
	  m_typeWanted = NEWS_POST;
	  NS_MsgSACopy(&m_path, "");
	}
  else 
	if (m_messageID || (m_key != nsMsgKey_None))
	{
	  /* 
    news_message://HOST/GROUP#key
    news://HOST/MESSAGE_ID

    not sure about these:

    news:MESSAGE_ID
    news:/GROUP/MESSAGE_ID (useless)
    news://HOST/GROUP/MESSAGE_ID (useless)
    */
	  if (cancel)
		m_typeWanted = CANCEL_WANTED;
	  else
		m_typeWanted = ARTICLE_WANTED;
	}
  else if (!commandSpecificData.IsEmpty())
	{
	  if (PL_strstr (commandSpecificData.get(), "?newgroups"))
	    /* news://HOST/?newsgroups
	        news:/GROUP/?newsgroups (useless)
	        news:?newsgroups (default host)
	     */
	    m_typeWanted = NEW_GROUPS;
    else
	  {
		  if (PL_strstr(commandSpecificData.get(), "?list-pretty"))
		  {
			  m_typeWanted = PRETTY_NAMES_WANTED;
			  m_commandSpecificData = ToNewCString(commandSpecificData);
		  }
		  else if (PL_strstr(commandSpecificData.get(), "?profile"))
		  {
			  m_typeWanted = PROFILE_WANTED;
			  m_commandSpecificData = ToNewCString(commandSpecificData);
		  }
		  else if (PL_strstr(commandSpecificData.get(), "?list-ids"))
		  {
			  m_typeWanted = IDS_WANTED;
			  m_commandSpecificData = ToNewCString(commandSpecificData);

              rv = m_nntpServer->FindGroup(group.get(), getter_AddRefs(m_newsFolder));
              if (!m_newsFolder) goto FAIL;
		  }
		  else
		  {
              m_typeWanted = SEARCH_WANTED;
              m_commandSpecificData = ToNewCString(commandSpecificData);
              m_searchData = m_commandSpecificData;
              nsUnescape(m_searchData);

              rv = m_nntpServer->FindGroup(group.get(), getter_AddRefs(m_newsFolder));
              if (!m_newsFolder) goto FAIL;
          }
	  }
	}
  else if (!group.IsEmpty())
  {
    /* news:GROUP
       news:/GROUP
       news://HOST/GROUP
     */
    if (PL_strchr(group.get(),'*'))
      m_typeWanted = LIST_WANTED;
    else 
    {
      if (m_nntpServer)
      {
        PRBool containsGroup = PR_TRUE;
        rv = m_nntpServer->ContainsNewsgroup(group.get(),&containsGroup);
        if (NS_FAILED(rv)) goto FAIL;

        if (!containsGroup) 
        {
          // We have the name of a newsgroup which we're not subscribed to,
          // the next step is to ask the user whether we should subscribe to it.

          nsCOMPtr<nsIPrompt> dialog;

          if (m_msgWindow)
              m_msgWindow->GetPromptDialog(getter_AddRefs(dialog));

          if (!dialog)
          {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
            wwatch->GetNewPrompter(nsnull, getter_AddRefs(dialog));
          }

          nsXPIDLString statusString, confirmText;
          nsCOMPtr<nsIStringBundle> bundle;
          nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);

          bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
          const PRUnichar *formatStrings[1] = { NS_ConvertUTF8toUCS2(group).get() };

          rv = bundle->FormatStringFromName(NS_LITERAL_STRING("autoSubscribeText").get(),
                                            formatStrings, 1,
                                            getter_Copies(confirmText));

          PRBool confirmResult = PR_FALSE;
          rv = dialog->Confirm(nsnull, confirmText, &confirmResult);
          NS_ENSURE_SUCCESS(rv, rv);

          if (confirmResult)
          {
            rv = m_nntpServer->SubscribeToNewsgroup(group.get());
            containsGroup = PR_TRUE;
          }
          else
            return rv;
        }

        // If we have a group (since before, or just subscribed), set the m_newsFolder.
        if (containsGroup)
        {
          rv = m_nntpServer->FindGroup(group.get(), getter_AddRefs(m_newsFolder));
          if (!m_newsFolder) goto FAIL;
        }
      }
      m_typeWanted = GROUP_WANTED;
    }
  }
  else
    // news: or news://HOST
    m_typeWanted = READ_NEWS_RC;

  // if this connection comes from the cache, we need to initialize the
  // load group here, by generating the start request notification. nsMsgProtocol::OnStartRequest
  // ignores the first parameter (which is supposed to be the channel) so we'll pass in null.
  if (m_fromCache)
    nsMsgProtocol::OnStartRequest(nsnull, aURL);

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


  FAIL:
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
NS_IMETHODIMP nsNNTPProtocol::OnStopRequest(nsIRequest *request, nsISupports * aContext, nsresult aStatus)
{
    // If failed, remove incomplete cache entry (and it'll be reloaded next time).
    if (NS_FAILED(aStatus) && m_runningURL)
    {
#ifdef DEBUG_CAVIN
        printf("*** Status failed in nsNNTPProtocol::OnStopRequest(), so clean up cache entry for the running url.");
#endif
        nsCOMPtr <nsICacheEntryDescriptor> memCacheEntry;
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
        if (mailnewsurl)
          mailnewsurl->GetMemCacheEntry(getter_AddRefs(memCacheEntry));
        if (memCacheEntry)
          memCacheEntry->Doom();
    }

    nsMsgProtocol::OnStopRequest(request, aContext, aStatus);

    // nsMsgProtocol::OnStopRequest() has called m_channelListener. There is
    // no need to be called again in CloseSocket(). Let's clear it here.
    if (m_channelListener) {
        m_channelListener = nsnull;
    }

	// okay, we've been told that the send is done and the connection is going away. So 
	// we need to release all of our state
	return CloseSocket();
}

NS_IMETHODIMP nsNNTPProtocol::Cancel(nsresult status)  // handle stop button
{
	m_nextState = NNTP_ERROR;
	return nsMsgProtocol::Cancel(NS_BINDING_ABORTED);
}

/* 
   FIX THIS COMMENT, this is how 4.x worked.  6.x is different.

   The main news load init routine, and URL parser.
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
nsresult 
nsNNTPProtocol::ParseURL(nsIURI * aURL, char ** aGroup, char ** aMessageID,
								  char ** aCommandSpecificData)
{
    NS_ENSURE_ARG_POINTER(aURL);
    NS_ENSURE_ARG_POINTER(aGroup);
    NS_ENSURE_ARG_POINTER(aMessageID);
    NS_ENSURE_ARG_POINTER(aCommandSpecificData);

	PRInt32 status = 0;
	char *group = 0;
    char *message_id = 0;
    char *command_specific_data = 0;
	char *s = 0;

    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) ParseURL",this));

    nsresult rv;
    nsCOMPtr <nsIMsgFolder> folder;
    nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(m_runningURL, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsXPIDLCString spec;
    rv = msgUrl->GetOriginalSpec(getter_Copies(spec));
    NS_ENSURE_SUCCESS(rv,rv);

    // if the original spec is non empty, use it to determine m_newsFolder and m_key
    if (spec.get() && spec.get()[0]) {
        PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) original message spec = %s",this,spec.get()));

        rv = nntpService->DecomposeNewsURI(spec.get(), getter_AddRefs(folder), &m_key);
        NS_ENSURE_SUCCESS(rv,rv);

        // since we are reading a message in this folder, we can set m_newsFolder
        m_newsFolder = do_QueryInterface(folder, &rv);
        NS_ENSURE_SUCCESS(rv,rv);

        // if we are cancelling, we aren't done.  we still need to parse out the messageID from the url
        // later, we'll use m_newsFolder and m_key to delete the message from the DB, if the cancel is successful.
        if (m_newsAction != nsINntpUrl::ActionCancelArticle) {
            return NS_OK;
        }
    }
    else {
        // clear this, we'll set it later.
        m_newsFolder = nsnull;
        m_currentGroup = "";
    }

	// get the file path part and store it as the group...
	nsXPIDLCString fullPath;
	rv = aURL->GetPath(getter_Copies(fullPath));
    NS_ENSURE_SUCCESS(rv,rv);

    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) fullPath = %s",this, (const char *)fullPath));

	if (fullPath.get() && fullPath.get()[0] == '/')
		group = PL_strdup((const char *)fullPath+1); 
	else
		group = PL_strdup((const char *)fullPath);

	// more to do here, but for now, this works.
	// only escape if we are doing a search
	if (m_newsAction == nsINntpUrl::ActionSearch) { 
		nsUnescape(group);
	}
	else if (PL_strchr(group, '@') || PL_strstr(group,"%40")) {
      message_id = nsUnescape(group);
	  group = 0;
	}
    else if (!*group) {
   	  nsCRT::free(group);
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
   if (message_id || group) {
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
	  
	  if (*s) {
		  command_specific_data = nsCRT::strdup (s);
		  *s = 0;
		  if (!command_specific_data) {
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
          }
      }

	  /* Discard any now-empty strings. */
	  if (message_id && !*message_id) {
		  PR_Free (message_id);
		  message_id = 0;
      }
	  else if (group && !*group) {
		  PR_Free(group);
		  group = 0;
      }
	}

  FAIL:
  NS_ASSERTION (!message_id || message_id != group, "something not null");
  if (status >= 0) {  
      *aGroup = group;
      *aMessageID = message_id;
      *aCommandSpecificData = command_specific_data;
  }
  else {
	  PR_FREEIF(group);
	  PR_FREEIF(message_id);
	  PR_FREEIF(command_specific_data);
  }

  // if we are cancelling, we've got our message id, our m_key and our m_newsFolder.
  // bail out now to prevent messing those up.
  if (m_newsAction == nsINntpUrl::ActionCancelArticle) {
    if (status < 0)
      return NS_ERROR_FAILURE;
    else
      return NS_OK;
  }

  nsXPIDLCString serverURI;

  if (*aMessageID) {
    // if this is a message id, use the pre path (the server) for the folder uri.
    rv = aURL->GetPrePath(getter_Copies(serverURI));
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else if (*aGroup) {
    if (PL_strchr(*aGroup,'*')) {
      rv = aURL->GetPrePath(getter_Copies(serverURI));
      NS_ENSURE_SUCCESS(rv,rv);
    }
    else {
      // let if fall through.  we'll set m_newsFolder later.
    }
  }

  if (serverURI.get() && serverURI.get()[0]) {
    // if we get here, we, we are either doing:
    // news://host/message-id or news://host/*
    // (but not news://host/message-id?cancel)
    // for authentication, we se set m_newsFolder to be the server's folder.
    // while we are here, we set m_nntpServer.
    rv = nntpService->DecomposeNewsURI(serverURI.get(), getter_AddRefs(folder), &m_key);
    NS_ENSURE_SUCCESS(rv,rv);
    
    // since we are reading a message in this folder, we can set m_newsFolder
    m_newsFolder = do_QueryInterface(folder, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    
    rv = m_newsFolder->GetNntpServer(getter_AddRefs(m_nntpServer));
    NS_ENSURE_SUCCESS(rv,rv);

    m_currentGroup = "";
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

PRInt32 nsNNTPProtocol::SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging)
{
    if (!aSuppressLogging) {
        NNTP_LOG_WRITE(dataBuffer);
    }
    else {
        PR_LOG(NNTP, out, ("(%p) Logging suppressed for this command (it probably contained authentication information)", this));
    }

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
        mBytesReceivedSinceLastStatusUpdate += status;
#endif
	}

    NS_MsgSACopy(&m_responseText, line+4);

	m_previousResponseCode = m_responseCode;

    PR_sscanf(line, "%d", &m_responseCode);
	
	if (m_responseCode == MK_NNTP_RESPONSE_AUTHINFO_DENIED) {
		/* login failed */
		AlertError(MK_NNTP_AUTH_FAILED, m_responseText);

        NS_ASSERTION(m_newsFolder, "no newsFolder");
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

	m_nntpServer->SetPostingAllowed(postingAllowed);
    m_nextState = NNTP_SEND_MODE_READER;
    return(0);  /* good */
}

PRInt32 nsNNTPProtocol::SendModeReader()
{  
	nsresult rv = NS_OK;
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL,&rv);
    NS_ENSURE_SUCCESS(rv,rv);

	rv = SendData(mailnewsurl, NNTP_CMD_MODE_READER); 
    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_SEND_MODE_READER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ); 

    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}

PRInt32 nsNNTPProtocol::SendModeReaderResponse()
{
  SetFlag(NNTP_READER_PERFORMED);
  
  /* ignore the response code and continue
	 */
  PRBool pushAuth = PR_FALSE;
  nsresult rv = NS_OK;

  NS_ASSERTION(m_nntpServer, "no server, see bug #107797");
  if (m_nntpServer) {
    rv = m_nntpServer->GetPushAuth(&pushAuth);
  }
  if (NS_SUCCEEDED(rv) && pushAuth) {
		/* if the news host is set up to require volunteered (pushed) authentication,
    * do that before we do anything else
    */
    m_nextState = NNTP_BEGIN_AUTHORIZE;
  }
  else {
#ifdef HAVE_NNTP_EXTENSIONS
    m_nextState = SEND_LIST_EXTENSIONS;
#else
		m_nextState = SEND_FIRST_NNTP_COMMAND;
#endif  /* HAVE_NNTP_EXTENSIONS */
  }

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

        if ('.' != line[0]) {
            m_nntpServer->AddExtension(line);
        }
		else
		{
			/* tell libmsg that it's ok to ask this news host for extensions */		
			m_nntpServer->SetSupportsExtensions(PR_TRUE);
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
		 
		 m_nntpServer->SetSupportsExtensions(PR_FALSE);
		 m_nextState = SEND_FIRST_NNTP_COMMAND;
	}

	return status;
}

PRInt32 nsNNTPProtocol::SendListSearches()
{  
    nsresult rv;
    PRBool searchable=PR_FALSE;
	PRInt32 status = 0;
    
    rv = m_nntpServer->QueryExtension("SEARCH",&searchable);
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
		m_nntpServer->AddSearchableGroup(line);
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
        m_nntpServer->AddSearchableHeader(line);
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
    
    rv = m_nntpServer->QueryExtension("SETGET",&setget);
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
                m_nntpServer->AddPropertyForGet(propertyName, propertyValue);
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
    rv = m_nntpServer->QueryExtension("LISTSUBSCR",&listsubscr);
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
        NS_ASSERTION(0,"fix me");
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

	if (m_typeWanted == ARTICLE_WANTED) {
		if (m_key != nsMsgKey_None) {
		    nsresult rv;
            nsXPIDLCString newsgroupName;
            if (m_newsFolder) {
                rv = m_newsFolder->GetAsciiName(getter_Copies(newsgroupName));
                NS_ENSURE_SUCCESS(rv,rv);
            }

		    PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) current group = %s, desired group = %s", this, m_currentGroup.get(), newsgroupName.get()));
            // if the current group is the desired group, we can just issue the ARTICLE command
            // if not, we have to do a GROUP first
			if (!PL_strcmp(m_currentGroup.get(), newsgroupName.get()))
			  m_nextState = NNTP_SEND_ARTICLE_NUMBER;
			else
			  m_nextState = NNTP_SEND_GROUP_FOR_ARTICLE;

			ClearFlag(NNTP_PAUSE_FOR_READ);
			return 0;
		  }
	  }

	if(m_typeWanted == NEWS_POST)
      {  /* posting to the news group */
        NS_MsgSACopy(&command, "POST");
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
		
		if (!m_nntpServer) {
			NNTP_LOG_NOTE("m_nntpServer is null, panic!");
			return -1;
		}
        rv = m_nntpServer->GetLastUpdatedTime(&last_update);
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
		
        NS_MsgSACopy(&command, small_buf);

	}
    else if(m_typeWanted == LIST_WANTED)
    {
	    nsresult rv;

		ClearFlag(NNTP_USE_FANCY_NEWSGROUP);
        PRUint32 last_update;
      	
        NS_ASSERTION(m_nntpServer, "no m_nntpServer");
		if (!m_nntpServer) {
          NNTP_LOG_NOTE("m_nntpServer is null, panic!");
          return -1;
		}

		rv = m_nntpServer->GetLastUpdatedTime(&last_update); 
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
			rv = m_nntpServer->QueryExtension("XACTIVE",&xactive);
			if (NS_SUCCEEDED(rv) && xactive)
			{
				NS_MsgSACopy(&command, "LIST XACTIVE");
				SetFlag(NNTP_USE_FANCY_NEWSGROUP);
			}
			else
			{
				NS_MsgSACopy(&command, "LIST");
			}
		}
	}
	else if(m_typeWanted == GROUP_WANTED) 
    {
        nsresult rv = NS_ERROR_NULL_POINTER;
        
        NS_ASSERTION(m_newsFolder, "m_newsFolder is null, panic!");
        if (!m_newsFolder) return -1;

        nsXPIDLCString group_name;
		rv = m_newsFolder->GetAsciiName(getter_Copies(group_name));
        NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get newsgroup name");
        if (NS_FAILED(rv)) return -1;
		
        m_firstArticle = 0;
        m_lastArticle = 0;
        
        NS_MsgSACopy(&command, "GROUP ");
        NS_MsgSACat(&command, (const char *)group_name);
      }
	else if (m_typeWanted == SEARCH_WANTED)
	{
		nsresult rv;
		PRBool searchable=PR_FALSE;
		if (!m_nntpServer) {
			NNTP_LOG_NOTE("m_nntpServer is null, panic!");
			return -1;
		}
		rv = m_nntpServer->QueryExtension("SEARCH", &searchable);
		if (NS_SUCCEEDED(rv) && searchable)
		{
			/* use the SEARCH extension */
			char *slash = PL_strchr (m_commandSpecificData, '/');
			if (slash)
			{
				char *allocatedCommand = MSG_UnEscapeSearchUrl (slash + 1);
				if (allocatedCommand)
				{
					NS_MsgSACopy (&command, allocatedCommand);
					PR_Free(allocatedCommand);
				}
			}
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_SEARCH_RESPONSE;
		}
		else
		{
            PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) doing GROUP for XPAT", this));
            nsXPIDLCString group_name;
            
			/* for XPAT, we have to GROUP into the group before searching */
			if (!m_newsFolder) {
				NNTP_LOG_NOTE("m_newsFolder is null, panic!");
				return -1;
			}
            rv = m_newsFolder->GetAsciiName(getter_Copies(group_name));
            if (NS_FAILED(rv)) return -1;

			NS_MsgSACopy(&command, "GROUP ");
            NS_MsgSACat (&command, group_name);

            // force a GROUP next time
            m_currentGroup = "";
			m_nextState = NNTP_RESPONSE;
			m_nextStateAfterResponse = NNTP_XPAT_SEND;
		}
	}
	else if (m_typeWanted == PRETTY_NAMES_WANTED)
	{
		nsresult rv;
		PRBool listpretty=PR_FALSE;
		rv = m_nntpServer->QueryExtension("LISTPRETTY",&listpretty);
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
				NS_MsgSACopy(&command, allocatedCommand);
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
        NS_ASSERTION(m_path, "no m_path, see bugs #57659 and #72317");
        if (!m_path) return -1;

		if (m_typeWanted == CANCEL_WANTED) {
			NS_MsgSACopy(&command, "HEAD ");
        }
		else {
            NS_ASSERTION(m_typeWanted == ARTICLE_WANTED, "not cancel, and not article");
			NS_MsgSACopy(&command, "ARTICLE ");
        }

		if (*m_path != '<')
			NS_MsgSACat(&command,"<");

		NS_MsgSACat(&command, m_path);

		if (PL_strchr(command+8, '>')==0) 
			NS_MsgSACat(&command,">");
	}

    NS_MsgSACat(&command, CRLF);
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
        nsresult rv = NS_OK;

        nsXPIDLCString group_name;
        NS_ASSERTION(m_newsFolder, "no newsFolder");
        if (m_newsFolder) {
            rv = m_newsFolder->GetAsciiName(getter_Copies(group_name));
        }

		if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP &&
            m_typeWanted == GROUP_WANTED) {
            PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) group (%s) not found, so unset m_currentGroup",this,(const char *)group_name));
            m_currentGroup = "";

            m_nntpServer->GroupNotFound(m_msgWindow, group_name.get(), PR_TRUE /* opening */);
        }

        /* if the server returned a 400 error then it is an expected
         * error.  the NEWS_ERROR state will not sever the connection
         */
        if(major_opcode == MK_NNTP_RESPONSE_TYPE_CANNOT)
          m_nextState = NEWS_ERROR;
        else
          m_nextState = NNTP_ERROR;
        // if we have no channel listener, then we're likely downloading
        // the message for offline use (or at least not displaying it)
        PRBool savingArticleOffline = (m_channelListener == nsnull);

        nsCOMPtr <nsICacheEntryDescriptor> memCacheEntry;
        if (m_runningURL)
        {
          nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
          if (mailnewsurl)
            mailnewsurl->GetMemCacheEntry(getter_AddRefs(memCacheEntry));
          // invalidate mem cache entry.
          if (memCacheEntry)
            memCacheEntry->Doom();
        }

        if (NS_SUCCEEDED(rv) && group_name && !savingArticleOffline) {
            MarkCurrentMsgRead();
            nsXPIDLString titleStr;
			rv = GetNewsStringByName("htmlNewsErrorTitle", getter_Copies(titleStr));
            NS_ENSURE_SUCCESS(rv,rv);

            nsXPIDLString newsErrorStr;
			rv = GetNewsStringByName("htmlNewsError", getter_Copies(newsErrorStr));
            NS_ENSURE_SUCCESS(rv,rv);
            nsAutoString errorHtml;
            errorHtml.Append(newsErrorStr);

            errorHtml.Append(NS_LITERAL_STRING("<b>").get());
            errorHtml.AppendWithConversion(m_responseText);
            errorHtml.Append(NS_LITERAL_STRING("</b><p>").get());

			rv = GetNewsStringByName("articleExpired", getter_Copies(newsErrorStr));
            NS_ENSURE_SUCCESS(rv,rv);
            errorHtml.Append(newsErrorStr);
            
            char outputBuffer[OUTPUT_BUFFER_SIZE];

			if ((m_key != nsMsgKey_None) && m_newsFolder) {
                nsXPIDLCString messageID;
                rv = m_newsFolder->GetMessageIdForKey(m_key, getter_Copies(messageID));
                if (NS_SUCCEEDED(rv)) {
                    PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE,"<P>&lt;%.512s&gt; (%lu)", (const char *)messageID, m_key);
                    errorHtml.AppendWithConversion(outputBuffer);
                }
			}

            if (m_newsFolder) {
                nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(m_newsFolder, &rv);
                if (NS_SUCCEEDED(rv) && folder) {
                    nsXPIDLCString folderURI;
                    rv = folder->GetURI(getter_Copies(folderURI));
                    if (NS_SUCCEEDED(rv)) {
                        PR_snprintf(outputBuffer,OUTPUT_BUFFER_SIZE,"<P> <A HREF=\"%s?list-ids\">", (const char *)folderURI);
                    }
                }
            }

            errorHtml.AppendWithConversion(outputBuffer);

			rv = GetNewsStringByName("removeExpiredArtLinkText", getter_Copies(newsErrorStr));
            NS_ENSURE_SUCCESS(rv,rv);
            errorHtml.Append(newsErrorStr);
            errorHtml.Append(NS_LITERAL_STRING("</A> </P>").get());

            if (!m_msgWindow) {
                nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
                if (mailnewsurl) {
                    rv = mailnewsurl->GetMsgWindow(getter_AddRefs(m_msgWindow));
                    NS_ENSURE_SUCCESS(rv,rv);
                }
            }
            if (!m_msgWindow) return NS_ERROR_FAILURE;

            // note, this will cause us to close the connection.
            // this will call nsDocShell::LoadURI(), which will
            // call nsDocShell::Stop(STOP_NETWORK), which will eventually
            // call nsNNTPProtocol::Cancel(), which will close the socket.
            // we need to fix this, since the connection is still valid.
            rv = m_msgWindow->DisplayHTMLInMessagePane((const PRUnichar *)titleStr, errorHtml.get());
            NS_ENSURE_SUCCESS(rv,rv);
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
  rv = m_newsFolder->GetAsciiName(getter_Copies(groupname));
  NS_ASSERTION(NS_SUCCEEDED(rv) && groupname.get() && groupname.get()[0], "no group name");

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

nsresult 
nsNNTPProtocol::SetCurrentGroup()
{
  nsresult rv;
  nsXPIDLCString groupname;
  NS_ASSERTION(m_newsFolder, "no news folder");
  if (!m_newsFolder) {
    m_currentGroup = "";
    return NS_ERROR_UNEXPECTED;
  }

  rv = m_newsFolder->GetAsciiName(getter_Copies(groupname));
  NS_ASSERTION(NS_SUCCEEDED(rv) && groupname.get()[0], "no group name");
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) SetCurrentGroup to %s",this,(const char *)groupname));
  m_currentGroup = (const char *)groupname;
  return NS_OK;
}

PRInt32 nsNNTPProtocol::SendGroupForArticleResponse()
{
  /* ignore the response code and continue
   */
  m_nextState = NNTP_SEND_ARTICLE_NUMBER;

  SetCurrentGroup();

  return(0);
}


PRInt32 nsNNTPProtocol::SendArticleNumber()
{
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0; 
	PR_snprintf(outputBuffer, OUTPUT_BUFFER_SIZE, "ARTICLE %lu" CRLF, m_key);

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
          nsresult rv;
          m_tempArticleStream = do_QueryInterface(supports, &rv);
          NS_ASSERTION(NS_SUCCEEDED(rv) && m_tempArticleStream,"failed to get article stream");
          if (NS_FAILED(rv) || !m_tempArticleStream) return -1;

          PRBool needDummyHeaders = PR_FALSE;
          msgurl->GetAddDummyEnvelope(&needDummyHeaders);
          if (needDummyHeaders)
          {
              nsCAutoString result;
              char *ct;
              PRUint32 writeCount;
              time_t now = time ((time_t*) 0);
              ct = ctime(&now);
              ct[24] = 0;
              result = "From - ";
              result += ct;
              result += MSG_LINEBREAK;

              m_tempArticleStream->Write(result.get(), result.Length(),
                                         &writeCount);
              result = "X-Mozilla-Status: 0001";
              result += MSG_LINEBREAK;
              m_tempArticleStream->Write(result.get(), result.Length(),
                                         &writeCount);
              result =  "X-Mozilla-Status2: 00000000";
              result += MSG_LINEBREAK;
              m_tempArticleStream->Write(result.get(), result.Length(),
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

    if (m_newsFolder)
      m_newsFolder->NotifyDownloadedLine(line, m_key);

		if (line[0] == '.' && line[1] == 0)
		{
			m_nextState = NEWS_DONE;
      MarkCurrentMsgRead();

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

            // skip over the quoted '.'
            if (line[0] == '.') {
			    mDisplayOutputStream->Write(line+1, PL_strlen(line)-1, &count);
            }
            else {
			    mDisplayOutputStream->Write(line, PL_strlen(line), &count);
            }
			mDisplayOutputStream->Write(MSG_LINEBREAK, PL_strlen(MSG_LINEBREAK), &count);
		}

		PR_FREEIF(line);
	}
 
	return 0;	
}

nsresult nsNNTPProtocol::MarkCurrentMsgRead()
{
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  nsresult rv = NS_OK;
  
  // if this is a message id url, (news://host/message-id) then don't go try to mark it read
  if (m_runningURL && !m_messageID && (m_key != nsMsgKey_None)) {
    rv = m_runningURL->GetMessageHeader(getter_AddRefs(msgHdr));
    
    if (NS_SUCCEEDED(rv) && msgHdr)
    {
      PRBool isRead;
      msgHdr->GetIsRead(&isRead);
      if (!isRead)
        msgHdr->MarkRead(PR_TRUE);
    }
  }
  return rv;
}

PRInt32 nsNNTPProtocol::ReadArticle(nsIInputStream * inputStream, PRUint32 length)
{
	char *line;
	PRUint32 status = 0;
	char *outputBuffer;
	
	PRBool pauseForMoreData = PR_FALSE;

	// if we have a channel listener, spool directly to it....
	// otherwise we must be doing something like save to disk or cancel
	// in which case we are doing the work.
	if (m_channelListener) {
		return DisplayArticle(inputStream, length);
        }

	line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
  if (m_newsFolder && line)
    m_newsFolder->NotifyDownloadedLine(line, m_key);

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
        mBytesReceivedSinceLastStatusUpdate += status;
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

		ClearFlag(NNTP_PAUSE_FOR_READ);
	}
	else
	{
		if (line[0] == '.')
			outputBuffer = line + 1;
		else
			outputBuffer = line;

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

				/* When we're sending this line to a converter (ie,
				   it's a message/rfc822) use the local line termination
				   convention, not CRLF.  This makes text articles get
				   saved with the local line terminators.  Since SMTP
				   and NNTP mandate the use of CRLF, it is expected that
				   the local system will convert that to the local line
				   terminator as it is read.
				 */
				// ** jto - in the case of save message to the stationary file if the
				// message is to be uploaded to the imap server we need to end the
				// line with canonical line endings, i.e., CRLF
				nsCOMPtr<nsIMsgMessageUrl> msgurl = do_QueryInterface(m_runningURL);
				PRBool canonicalLineEnding = PR_FALSE;
				if (msgurl)
					msgurl->GetCanonicalLineEnding(&canonicalLineEnding);

				if (canonicalLineEnding)
					m_tempArticleStream->Write(CRLF, PL_strlen(CRLF), &count);
				else
					m_tempArticleStream->Write(MSG_LINEBREAK, PL_strlen(MSG_LINEBREAK), &count);
			}
		}
	}

	PR_FREEIF(line);

	return 0;
}

void nsNNTPProtocol::ParseHeaderForCancel(char *buf)
{
    nsCAutoString header(buf);
    PRInt32 colon = header.FindChar(':');
    if (!colon)
		return;

    nsCAutoString value;
    header.Right(value, header.Length() - colon -1);
    value.StripWhitespace();
    
    switch (header.First()) {
    case 'F': case 'f':
        if (header.Find("From",PR_TRUE) == 0) {
            PR_FREEIF(m_cancelFromHdr);
			m_cancelFromHdr = ToNewCString(value);
        }
        break;
    case 'M': case 'm':
        if (header.Find("Message-ID",PR_TRUE) == 0) {
            PR_FREEIF(m_cancelID);
			m_cancelID = ToNewCString(value);
        }
        break;
    case 'N': case 'n':
        if (header.Find("Newsgroups",PR_TRUE) == 0) {
            PR_FREEIF(m_cancelNewsgroups);
			m_cancelNewsgroups = ToNewCString(value);
        }
        break;
     case 'D': case 'd':
        if (header.Find("Distributions",PR_TRUE) == 0) {
            PR_FREEIF(m_cancelDistribution);
			m_cancelDistribution = ToNewCString(value);
        }       
        break;
    }

  return;
}

PRInt32 nsNNTPProtocol::BeginAuthorization()
{
	char * command = 0;
    nsXPIDLCString username;
    nsresult rv = NS_OK;
	PRInt32 status = 0;
	nsXPIDLCString cachedUsername;

	if (!m_newsFolder && m_nntpServer) {
		nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_nntpServer);
		if (m_nntpServer) {
			nsCOMPtr<nsIFolder> rootFolder;
			rv = server->GetRootFolder(getter_AddRefs(rootFolder));
			if (NS_SUCCEEDED(rv) && rootFolder) {
				m_newsFolder = do_QueryInterface(rootFolder);
			}
		}
    }

    NS_ASSERTION(m_newsFolder, "no m_newsFolder");
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

	NS_MsgSACopy(&command, "AUTHINFO user ");
	if (cachedUsername) {
		PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) use %s as the username",this, (const char *)cachedUsername));
		NS_MsgSACat(&command, (const char *)cachedUsername);
	}
	else {
		PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) use %s as the username",this, (const char *)username));
		NS_MsgSACat(&command, (const char *)username);
	}
	NS_MsgSACat(&command, CRLF);

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
#ifdef HAVE_NNTP_EXTENSIONS
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_nntpServer->GetPushAuth(&pushAuth);
        
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
#else
        if (!TestFlag(NNTP_READER_PERFORMED))
			m_nextState = NNTP_SEND_MODE_READER;
        else
			m_nextState = SEND_FIRST_NNTP_COMMAND;
#endif /* HAVE_NNTP_EXTENSIONS */

		return(0); 
	  }
	else if (MK_NNTP_RESPONSE_AUTHINFO_CONT == m_responseCode)
	  {
		/* password required
		 */	
		char * command = 0;
        nsXPIDLCString password;
	    nsXPIDLCString cachedPassword;

        NS_ASSERTION(m_newsFolder, "no newsFolder");
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

            NS_ASSERTION(m_newsFolder, "no newsFolder");
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

		NS_MsgSACopy(&command, "AUTHINFO pass ");
		if (cachedPassword) {
			PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) use cached password", this));
			NS_MsgSACat(&command, (const char *)cachedPassword);
		}
		else {
			// *don't log the password!* PR_LOG(NNTP,PR_LOG_ALWAYS,("use %s as the password",(const char *)password)); 
			NS_MsgSACat(&command, (const char *)password);
		}
		NS_MsgSACat(&command, CRLF);
	
		nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
		if (mailnewsurl)
			status = SendData(mailnewsurl, command, PR_TRUE);

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

        NS_ASSERTION(m_newsFolder, "no newsFolder");
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
#ifdef HAVE_NNTP_EXTENSIONS
        PRBool pushAuth;
		/* If we're here because the host demanded authentication before we
		 * even sent a single command, then jump back to the beginning of everything
		 */
        rv = m_nntpServer->GetPushAuth(&pushAuth);
        
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
#else
        if (!TestFlag(NNTP_READER_PERFORMED))
			m_nextState = NNTP_SEND_MODE_READER;
        else
			m_nextState = SEND_FIRST_NNTP_COMMAND;
#endif /* HAVE_NNTP_EXTENSIONS */

        return(0);
	  }
	else
	{
        /* login failed */
		AlertError(MK_NNTP_AUTH_FAILED, m_responseText);

        NS_ASSERTION(m_newsFolder, "no newsFolder");
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

	PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) DisplayNewsgroups()",this));

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
    mBytesReceivedSinceLastStatusUpdate = 0;
    m_startTime = PR_Now();
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
		rv = m_nntpServer->QueryExtension("XACTIVE",&xactive);
		if (NS_SUCCEEDED(rv) && xactive)
		{
          nsXPIDLCString groupName;
          rv = m_nntpServer->GetFirstGroupNeedingExtraInfo(getter_Copies(groupName));
          if (NS_SUCCEEDED(rv)) {
                rv = m_nntpServer->FindGroup((const char *)groupName, getter_AddRefs(m_newsFolder));
                NS_ASSERTION(NS_SUCCEEDED(rv), "FindGroup failed");
				m_nextState = NNTP_LIST_XACTIVE;
				PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) listing xactive for %s", this, (const char *)groupName));
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
        mBytesReceivedSinceLastStatusUpdate += status;
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
    mBytesReceivedSinceLastStatusUpdate += status;
#endif

	NS_ASSERTION(m_nntpServer, "no nntp incoming server");
	if (m_nntpServer) {
		rv = m_nntpServer->AddNewsgroupToList(line);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to add to subscribe ds");
	}

    PRBool xactive=PR_FALSE;
    rv = m_nntpServer->QueryExtension("XACTIVE",&xactive);
    if (NS_SUCCEEDED(rv) && xactive)
	{
      m_nntpServer->SetGroupNeedsExtraInfo(line, PR_TRUE);
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
    mNumGroupsListed = 0;
    m_nextState = NNTP_READ_LIST;

    mBytesReceived = 0;
    mBytesReceivedSinceLastStatusUpdate = 0;
    m_startTime = PR_Now();

	PRInt32 status = 0;
#ifdef UNREADY_CODE
	NET_Progress(ce->window_id, XP_GetString(XP_PROGRESS_RECEIVE_NEWSGROUP));
#endif
	 
    return(status);
}

#define RATE_CONSTANT 976.5625      /* PR_USEC_PER_SEC / 1024 bytes */

static void ComputeRate(PRInt32 bytes, PRTime startTime, float *rate)
{
  // rate = (bytes / USECS since start) * RATE_CONSTANT

  // compute usecs since we started.
  PRTime timeSinceStart;
  PRTime now = PR_Now();
  LL_SUB(timeSinceStart, now, startTime);

  // convert PRTime to PRInt32
  PRInt32 delta;
  LL_L2I(delta, timeSinceStart);

  // compute rate
  if (delta > 0) {
    *rate = (float) ((bytes * RATE_CONSTANT) / delta);
  }
  else {
    *rate = 0.0;
  }
}

/* display a list of all or part of the newsgroups list
 * from the news server
 */
PRInt32 nsNNTPProtocol::ReadNewsList(nsIInputStream * inputStream, PRUint32 length)
{
	nsresult rv;
    PRInt32 i=0;
	PRUint32 status = 1;
	
	PRBool pauseForMoreData = PR_FALSE;
	char *line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData);
	char *orig_line = line;
 
	if (pauseForMoreData)
	{
		SetFlag(NNTP_PAUSE_FOR_READ);
		PR_FREEIF(orig_line);
		return 0;
	}

    if(!line) return(status);  /* no line yet */

            /* End of list? */
    if (line[0]=='.' && line[1]=='\0')
    {
		PRBool listpnames=PR_FALSE;
		rv = m_nntpServer->QueryExtension("LISTPNAMES",&listpnames);
		if (NS_SUCCEEDED(rv) && listpnames)
			m_nextState = NNTP_LIST_PRETTY_NAMES;
		else
			m_nextState = DISPLAY_NEWSGROUPS;
        ClearFlag(NNTP_PAUSE_FOR_READ);
		PR_FREEIF(orig_line);
        return 0;  
    }
	else if (line [0] == '.' && line [1] == '.') 
	{
	  if (line [2] == '.')
	  {
		  // some servers send "... 0000000001 0000000002 y".  
		  // just skip that that.
		  // see bug #69231
		  PR_FREEIF(orig_line);
		  return status;
	  }
	  /* The NNTP server quotes all lines beginning with "." by doubling it. */
	  line++;
	}

    /* almost correct
     */
    if(status > 1)
    {
		mBytesReceived += status;
        mBytesReceivedSinceLastStatusUpdate += status;
        
        if ((mBytesReceivedSinceLastStatusUpdate > UPDATE_THRESHHOLD) && m_msgWindow) {
                mBytesReceivedSinceLastStatusUpdate = 0;

        	    nsCOMPtr <nsIMsgStatusFeedback> msgStatusFeedback;

        	    rv = m_msgWindow->GetStatusFeedback(getter_AddRefs(msgStatusFeedback));
                NS_ENSURE_SUCCESS(rv, rv);

                nsXPIDLString statusString;
		
                nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
                NS_ENSURE_SUCCESS(rv, rv);

               	nsCOMPtr<nsIStringBundle> bundle;
                rv = bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
                NS_ENSURE_SUCCESS(rv, rv);

                nsAutoString bytesStr; 
                bytesStr.AppendInt(mBytesReceived / 1024);

                // compute the rate, and then convert it have one 
                // decimal precision.
                float rate = 0.0;
                ComputeRate(mBytesReceived, m_startTime, &rate);
                char rate_buf[RATE_STR_BUF_LEN];
                PR_snprintf(rate_buf,RATE_STR_BUF_LEN,"%.1f", rate);

                nsAutoString rateStr;
                rateStr.AppendWithConversion(rate_buf);

                nsAutoString numGroupsStr;
                numGroupsStr.AppendInt(mNumGroupsListed);

                const PRUnichar *formatStrings[3] = { numGroupsStr.get(), bytesStr.get(), rateStr.get() };
                rv = bundle->FormatStringFromName(NS_LITERAL_STRING("bytesReceived").get(),
                                                  formatStrings, 3,
                                                  getter_Copies(statusString));

        	    rv = msgStatusFeedback->ShowStatusString(statusString);
				if (NS_FAILED(rv)) {
					PR_FREEIF(orig_line);
					return rv;
				}
		}
    }
    
	 /* find whitespace seperator if it exits */
    for(i=0; line[i] != '\0' && !NET_IS_SPACE(line[i]); i++)
        ;  /* null body */

	char *description;
    if(line[i] == '\0')
        description = &line[i];
    else
        description = &line[i+1];

    line[i] = 0; /* terminate group name */

	/* store all the group names */
	NS_ASSERTION(m_nntpServer, "no nntp incoming server");
	if (m_nntpServer) {
		m_readNewsListCount++;
        mNumGroupsListed++;
		rv = m_nntpServer->AddNewsgroupToList(line);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to add to subscribe ds");
	}
	else {
		rv = NS_ERROR_FAILURE;
	}

	if (m_readNewsListCount == READ_NEWS_LIST_COUNT_MAX) {
		m_readNewsListCount = 0;
 	    if (mUpdateTimer) {
			mUpdateTimer->Cancel();
			mUpdateTimer = nsnull;
	    }
        mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create timer");
		if (NS_FAILED(rv)) {
			PR_FREEIF(orig_line);
			return -1;
		}

		mInputStream = inputStream;

		const PRUint32 kUpdateTimerDelay = READ_NEWS_LIST_TIMEOUT;
		rv = mUpdateTimer->Init(NS_STATIC_CAST(nsITimerCallback*,this), kUpdateTimerDelay);
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to init timer");
		if (NS_FAILED(rv)) {
			PR_FREEIF(orig_line);
			return -1;
		}

		m_nextState = NEWS_FINISHED;
    }

	PR_FREEIF(orig_line);
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
    
    rv = SetCurrentGroup();
    if (NS_FAILED(rv)) return -1;

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

    m_newsgroupList = do_CreateInstance(NS_NNTPNEWSGROUPLIST_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return -1;

    rv = m_newsgroupList->Initialize(m_runningURL, m_newsFolder);
    if (NS_FAILED(rv)) return -1;

    rv = m_newsFolder->UpdateSummaryFromNNTPInfo(m_firstPossibleArticle, m_lastPossibleArticle, count);
    if (NS_FAILED(rv)) return -1;

	m_numArticlesLoaded = 0;

    // if the user sets max_articles to a bogus value, get them everything
	m_numArticlesWanted = m_maxArticles > 0 ? m_maxArticles : 1L << 30; 

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
      PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) add to known articles:  %d - %d", this, m_firstArticle, m_lastArticle));

      if (NS_SUCCEEDED(rv) && m_newsgroupList) {
          rv = m_newsgroupList->AddToKnownArticles(m_firstArticle,
                                                 m_lastArticle);
      }

	  if (NS_FAILED(rv)) return status;
	}
										 
	if (m_numArticlesLoaded >= m_numArticlesWanted) 
	{
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}

    NS_ASSERTION(m_newsgroupList, "no newsgroupList");
    if (!m_newsgroupList) return -1;
        
    PRBool getOldMessages = PR_FALSE;
    if (m_runningURL) {
      rv = m_runningURL->GetGetOldMessages(&getOldMessages);
      if (NS_FAILED(rv)) return status;
    }

    rv = m_newsgroupList->SetGetOldMessages(getOldMessages);
    if (NS_FAILED(rv)) return status;
    
    rv = m_newsgroupList->GetRangeOfArtsToDownload(m_msgWindow,
      m_firstPossibleArticle,
      m_lastPossibleArticle,
      m_numArticlesWanted -
      m_numArticlesLoaded,
      &(m_firstArticle),
      &(m_lastArticle),
      &status);

	if (NS_FAILED(rv)) return status;

	if (m_firstArticle <= 0 || m_firstArticle > m_lastArticle) 
	{
	  /* Nothing more to get. */
	  m_nextState = NEWS_PROCESS_XOVER;
	  ClearFlag(NNTP_PAUSE_FOR_READ);
	  return 0;
	}

	PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) Chunk will be (%d-%d)", this, m_firstArticle, m_lastArticle));
    
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

	NNTP_LOG_WRITE(outputBuffer);

    m_nextState = NNTP_RESPONSE;
    m_nextStateAfterResponse = NNTP_XOVER_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

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
		mBytesReceived += status;
        mBytesReceivedSinceLastStatusUpdate += status;
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

    /* xover_parse_state stored in MSG_Pane cd->pane */
    NS_ASSERTION(m_newsgroupList, "no newsgroupList");
    if (!m_newsgroupList) return -1;

	PRInt32 status = 0;
    rv = m_newsgroupList->FinishXOVERLINE(0,&status);
    m_newsgroupList = nsnull;
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

      m_key = nsMsgKey_None;

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

  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) read_group_body: got line: %s|",this,line));

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
	nsresult rv;
	nsAutoString resultString(NS_LITERAL_STRING("???"));

	if (!m_stringBundle)
	{
		char*       propertyURL = NEWS_MSGS_URL;

        nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

		rv = bundleService->CreateBundle(propertyURL, getter_AddRefs(m_stringBundle));
        NS_ENSURE_SUCCESS(rv, rv);
	}

	if (m_stringBundle) {
		PRUnichar *ptrv = nsnull;
		rv = m_stringBundle->GetStringFromID(stringID, &ptrv);

		if (NS_FAILED(rv)) {
			resultString.Assign(NS_LITERAL_STRING("[StringID"));
			resultString.AppendInt(stringID, 10);
			resultString.Append(NS_LITERAL_STRING("?]"));
			*aString = ToNewUnicode(resultString);
		}
		else {
			*aString = ptrv;
		}
	}
	else {
		rv = NS_OK;
		*aString = ToNewUnicode(resultString);
	}
	return rv;
}

nsresult nsNNTPProtocol::GetNewsStringByName(const char *aName, PRUnichar **aString)
{
	nsresult rv;
	nsAutoString resultString(NS_LITERAL_STRING("???"));
	if (!m_stringBundle)
	{
		char*       propertyURL = NEWS_MSGS_URL;

        nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

		rv = bundleService->CreateBundle(propertyURL, getter_AddRefs(m_stringBundle));
	}

	if (m_stringBundle)
	{
		nsAutoString unicodeName; unicodeName.AssignWithConversion(aName);

		PRUnichar *ptrv = nsnull;
		rv = m_stringBundle->GetStringFromName(unicodeName.get(), &ptrv);

		if (NS_FAILED(rv)) 
		{
			resultString.Assign(NS_LITERAL_STRING("[StringName"));
			resultString.AppendWithConversion(aName);
			resultString.Append(NS_LITERAL_STRING("?]"));
			*aString = ToNewUnicode(resultString);
		}
		else
		{
			*aString = ptrv;
		}
	}
	else
	{
		rv = NS_OK;
		*aString = ToNewUnicode(resultString);
	}
	return rv;
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

#define NEWS_GROUP_DISPLAY_FREQ 1

nsresult
nsNNTPProtocol::SetCheckingForNewNewsStatus(PRInt32 current, PRInt32 total)
{
    nsresult rv;
    nsXPIDLString statusString;

    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_nntpServer, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
 
    nsXPIDLCString hostName;
    rv = server->GetHostName(getter_Copies(hostName));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString thisGroupStr; 
    thisGroupStr.AppendInt(current);

    nsAutoString totalGroupStr; 
    totalGroupStr.AppendInt(total);

    nsAutoString hostNameStr;
    hostNameStr.AssignWithConversion(hostName.get());
    
    const PRUnichar *formatStrings[] = { thisGroupStr.get(), totalGroupStr.get(), hostNameStr.get() };

    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("checkingForNewNews").get(),
                                                  formatStrings, 3,
                                                  getter_Copies(statusString));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetProgressStatus(statusString);
    NS_ENSURE_SUCCESS(rv, rv);

    SetProgressBarPercent(current, total);
    return NS_OK;
}

PRInt32 nsNNTPProtocol::DisplayNewsRC()
{
    PRInt32 status = 0;
    nsresult rv;

	if(!TestFlag(NNTP_NEWSRC_PERFORMED)) {
		SetFlag(NNTP_NEWSRC_PERFORMED);
        rv = m_nntpServer->GetNumGroupsNeedingCounts(&m_newsRCListCount);
        NS_ENSURE_SUCCESS(rv, rv);
	}
	
	nsCOMPtr <nsISupports> currChild;
	rv = m_nntpServer->GetFirstGroupNeedingCounts(getter_AddRefs(currChild));
	if (NS_FAILED(rv)) {
		ClearFlag(NNTP_NEWSRC_PERFORMED);
		return -1;
	}
    else if (!currChild) {
		ClearFlag(NNTP_NEWSRC_PERFORMED);
        m_nextState = NEWS_DONE;

		if (m_newsRCListCount) {
             // clear the status text.
            rv = SetProgressStatus(NS_LITERAL_STRING("").get());
            NS_ENSURE_SUCCESS(rv, rv);

			SetProgressBarPercent(0, -1);
			m_newsRCListCount = 0;
            status = 0;
		}
		else if (m_responseCode == MK_NNTP_RESPONSE_LIST_OK)  {
			/*
			 * 5-9-96 jefft 
			 * If for some reason the news server returns an empty 
			 * newsgroups list with a nntp response code MK_NNTP_RESPONSE_LIST_OK -- list of
			 * newsgroups follows. We set status to MK_EMPTY_NEWS_LIST
			 * to end the infinite dialog loop.
			 */
            status = MK_EMPTY_NEWS_LIST;
        }

        if(status > -1)
          return MK_DATA_LOADED; 
        else
          return(status);
    }

	nsCOMPtr<nsIFolder> currFolder = do_QueryInterface(currChild, &rv);
    if (NS_FAILED(rv)) return -1;
	if (!currFolder) return -1;

    m_newsFolder = do_QueryInterface(currFolder, &rv);
    if (NS_FAILED(rv)) return -1;
	if (!m_newsFolder) return -1;

	nsXPIDLCString name;
    rv = m_newsFolder->GetAsciiName(getter_Copies(name));
    if (NS_FAILED(rv)) return -1;
	if (!name) return -1;

	/* send group command to server */
	char outputBuffer[OUTPUT_BUFFER_SIZE];

	PR_snprintf(outputBuffer, OUTPUT_BUFFER_SIZE, "GROUP %.512s" CRLF, (const char *)name);
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl) {
		status = SendData(mailnewsurl, outputBuffer);
    }

	/* only update every NEWS_GROUP_DISPLAY_FREQ groups for speed */
	if ((m_newsRCListCount >= NEWS_GROUP_DISPLAY_FREQ) && ((m_newsRCListIndex % NEWS_GROUP_DISPLAY_FREQ) == 0 || ((m_newsRCListIndex+1) == m_newsRCListCount))) {
        rv = SetCheckingForNewNewsStatus(m_newsRCListIndex+1, m_newsRCListCount);
        if (NS_FAILED(rv)) return -1;
    }
		
	m_newsRCListIndex++;

	SetFlag(NNTP_PAUSE_FOR_READ);
	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NEWS_DISPLAY_NEWS_RC_RESPONSE;

	return status; /* keep going */
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

        // this might save us a GROUP command, if the user reads a message in the 
        // last group we update.
        m_currentGroup = group;

		// prevent crash when
		// if going offline in the middle of
                // updating the unread counts on a news server
                // (running a "news://host/*" url)
		NS_ASSERTION(m_nntpServer,"no server");
		if (!m_nntpServer) return -1;

        rv = m_nntpServer->DisplaySubscribedGroup(m_newsFolder,
                                              low ? atol(low) : 0,
                                              high ? atol(high) : 0,
                                              atol(num_arts));
		NS_ASSERTION(NS_SUCCEEDED(rv),"DisplaySubscribedGroup() failed");
		if (NS_FAILED(rv)) status = -1;

		if (status < 0) return status;
	  }
	  else if (m_responseCode == MK_NNTP_RESPONSE_GROUP_NO_GROUP)
	  {
          nsXPIDLCString name;
          rv = m_newsFolder->GetAsciiName(getter_Copies(name));

          if (NS_SUCCEEDED(rv)) {
            m_nntpServer->GroupNotFound(m_msgWindow, name.get(), PR_FALSE);
          }

          PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) NO_GROUP, so unset m_currentGroup", this));
          m_currentGroup = "";
	  }
	  /* it turns out subscribe ui depends on getting this displaysubscribedgroup call,
	     even if there was an error.
	  */
	  if(m_responseCode != MK_NNTP_RESPONSE_GROUP_SELECTED)
	  {
		/* only on news server error or when zero articles
		 */
#ifdef DEBUG_seth
		NS_ASSERTION(PR_FALSE,"check this code");
#endif
        rv = m_nntpServer->DisplaySubscribedGroup(m_newsFolder, 0, 0, 0);
		NS_ASSERTION(NS_SUCCEEDED(rv),"DisplaySubscribedGroup() failed");
        PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) error, so unset m_currentGroup", this));
        m_currentGroup = "";
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
	// already found a match, no need to go any further
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

    char *us = nsnull;
    char *them = nsnull;
    nsresult rv1 = parser->ExtractHeaderAddressMailboxes(nsnull, cancelInfo->from, &us);
    nsresult rv2 = parser->ExtractHeaderAddressMailboxes(nsnull, cancelInfo->old_from, &them);
    
	PR_LOG(NNTP,PR_LOG_ALWAYS,("us = %s, them = %s", us, them));

    if ((NS_FAILED(rv1) || NS_FAILED(rv2) || PL_strcasecmp(us, them))) {
        //no match.  don't set cancel email
        PR_FREEIF(cancelInfo->from);
        cancelInfo->from = nsnull;

        PR_FREEIF(us);
        PR_FREEIF(them);

        // keep going
        return PR_TRUE;
    }
    else {
        // we have a match, stop.
        return PR_FALSE;
    }          
}

PRInt32 nsNNTPProtocol::DoCancel()
{
    PRInt32 status = 0;
    PRBool failure = PR_FALSE;
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

  nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

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
  rv = m_nntpServer->QueryExtension("CANCELCHK",&cancelchk);
  if (NS_SUCCEEDED(rv) && !cancelchk) {
	  NNTP_LOG_NOTE("CANCELCHK not supported");
      
      // get the current identity from the news session....
      nsCOMPtr<nsIMsgAccountManager> accountManager = 
               do_GetService(kCMsgAccountManagerCID, &rv);
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
          failure = PR_TRUE;
          goto FAIL;
      }
      else {
		  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) CANCELCHK not supported, so post the cancel message as %s", this, cancelInfo.from));
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
      failure = PR_TRUE;
      goto FAIL;
  }  
  
  if (!subject || !other_random_headers || !body) {
	  status = MK_OUT_OF_MEMORY;
          failure = PR_TRUE;
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
		AlertError(MK_TCP_WRITE_ERROR, errorText.get());
                failure = PR_TRUE;
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
    NS_ASSERTION(NS_SUCCEEDED(rv) && m_newsFolder && (m_key != nsMsgKey_None), "need more to remove this message from the db");
    if ((m_key != nsMsgKey_None) && (m_newsFolder)) {
       rv = m_newsFolder->RemoveMessage(m_key);
    }
  }
   
FAIL:
  NS_ASSERTION(m_newsFolder,"no news folder");
  if (m_newsFolder) {
        if ( failure )
             rv = m_newsFolder->CancelFailed();
        else
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
		NS_MsgSACopy (&command, ++thisTerm);
		endOfTerm = PL_strchr(command, '/');
		if (endOfTerm)
			*endOfTerm = '\0';
		NS_MsgSACat(&command, CRLF);
	
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

    nsresult rv = m_newsFolder->GetAsciiName(getter_Copies(group_name));
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
              m_nntpServer->SetPrettyNameForGroup(line, prettyName);

			PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) adding pretty name %s", this, prettyName));
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
	rv = m_newsFolder->GetAsciiName(getter_Copies(group_name));
	if (NS_FAILED(rv)) return -1;

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
	nsresult rv;

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
        mBytesReceivedSinceLastStatusUpdate += status;
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


				NS_ASSERTION(m_nntpServer, "no nntp incoming server");
				if (m_nntpServer) {
					rv = m_nntpServer->AddNewsgroupToList(line);
					NS_ASSERTION(NS_SUCCEEDED(rv),"failed to add to subscribe ds");
				}
                
				/* we're either going to list prettynames first, or list
                   all prettynames every time, so we won't care so much
                   if it gets interrupted. */
				PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) got xactive for %s of %s", this, line, flags));
				/*	This isn't required, because the extra info is
                    initialized to false for new groups. And it's
                    an expensive call.
				*/
				/* MSG_SetGroupNeedsExtraInfo(cd->host, line, PR_FALSE); */
			}
		}
		else
		{
          PRBool xactive=PR_FALSE;
          rv = m_nntpServer->QueryExtension("XACTIVE",&xactive);
          if (m_typeWanted == NEW_GROUPS &&
              NS_SUCCEEDED(rv) && xactive)
			{
                nsCOMPtr <nsIMsgNewsFolder> old_newsFolder;
                old_newsFolder = m_newsFolder;
                nsXPIDLCString groupName;
                
                rv = m_nntpServer->GetFirstGroupNeedingExtraInfo(getter_Copies(groupName));
                if (NS_FAILED(rv)) return -1;
                rv = m_nntpServer->FindGroup(groupName, getter_AddRefs(m_newsFolder));
                if (NS_FAILED(rv)) return -1;

                // see if we got a different group
                if (old_newsFolder && m_newsFolder &&
                    (old_newsFolder.get() != m_newsFolder.get()))
                /* make sure we're not stuck on the same group */
                {
					PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) listing xactive for %s", this, (const char *)groupName));
					m_nextState = NNTP_LIST_XACTIVE;
			        ClearFlag(NNTP_PAUSE_FOR_READ); 
					PR_FREEIF(line);
					return 0;
				}
				else
                {
                    m_newsFolder = nsnull;
				}
			}
            PRBool listpname;
            rv = m_nntpServer->QueryExtension("LISTPNAME",&listpname);
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

PRInt32 nsNNTPProtocol::SendListGroup()
{
    nsresult rv;
	char outputBuffer[OUTPUT_BUFFER_SIZE];
	PRInt32 status = 0; 

    NS_ASSERTION(m_newsFolder,"no newsFolder");
    if (!m_newsFolder) return -1;
    nsXPIDLCString newsgroupName;

    rv = m_newsFolder->GetAsciiName(getter_Copies(newsgroupName));
    NS_ENSURE_SUCCESS(rv,rv);

	PR_snprintf(outputBuffer, 
			OUTPUT_BUFFER_SIZE, 
			"listgroup %.512s" CRLF,
            (const char *)newsgroupName);

    rv = nsComponentManager::CreateInstance(kNNTPArticleListCID,
                                            nsnull,
                                            NS_GET_IID(nsINNTPArticleList),
                                            getter_AddRefs(m_articleList));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = m_articleList->Initialize(m_newsFolder);
    NS_ENSURE_SUCCESS(rv,rv);
	
	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
	if (mailnewsurl)
		status = SendData(mailnewsurl, outputBuffer); 

	m_nextState = NNTP_RESPONSE;
	m_nextStateAfterResponse = NNTP_LIST_GROUP_RESPONSE;
    SetFlag(NNTP_PAUSE_FOR_READ);

	return status;
}

PRInt32 nsNNTPProtocol::SendListGroupResponse(nsIInputStream * inputStream, PRUint32 length)
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
            m_articleList->FinishAddingArticleKeys();
            m_articleList = nsnull;
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

    
	ClearFlag(NNTP_PAUSE_FOR_READ); 

    while(!TestFlag(NNTP_PAUSE_FOR_READ))
	{
		PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) Next state: %s",this, stateLabels[m_nextState]));
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
				status = SendListGroup();
				break;
			case NNTP_LIST_GROUP_RESPONSE:
				if (inputStream == nsnull)
					SetFlag(NNTP_PAUSE_FOR_READ);
				else
					status = SendListGroupResponse(inputStream, length);
				break;
	        case NEWS_DONE:
				m_nextState = NEWS_FREE;
	            break;
			case NEWS_POST_DONE:
				NNTP_LOG_NOTE("NEWS_POST_DONE");
				mailnewsurl->SetUrlState(PR_FALSE, NS_OK);
				m_nextState = NEWS_FREE;
				break;
	        case NEWS_ERROR:
				NNTP_LOG_NOTE("NEWS_ERROR"); 
        if (m_responseCode == MK_NNTP_RESPONSE_ARTICLE_NOTFOUND || m_responseCode == MK_NNTP_RESPONSE_ARTICLE_NONEXIST)
				  mailnewsurl->SetUrlState(PR_FALSE, NS_MSG_NEWS_ARTICLE_NOT_FOUND);
        else
          mailnewsurl->SetUrlState(PR_FALSE, NS_ERROR_FAILURE);
				m_nextState = NEWS_FREE;
	            break;
	        case NNTP_ERROR:
                // XXX do we really want to remove the connection from
                // the cache on error?
                /* check if this connection came from the cache or if it was
                 * a new connection.  If it was not new lets start it over
				 * again.  But only if we didn't have any successful protocol
				 * dialog at all.
                 */
				return CloseConnection();
            case NEWS_FREE:
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
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) ClosingConnection",this));
  SendData(nsnull, NNTP_CMD_QUIT); // this will cause OnStopRequest get called, which will call CloseSocket()
  // break some cycles
  CleanupNewsgroupList();

  if (m_nntpServer) {
    m_nntpServer->RemoveConnection(this);
    m_nntpServer = nsnull;
  }
  CloseSocket();
  m_newsFolder = nsnull;
  
  if (m_articleList) {
    m_articleList->FinishAddingArticleKeys();
    m_articleList = nsnull;
  }
  
  m_key = nsMsgKey_None;
  return NS_OK;
}

nsresult nsNNTPProtocol::CleanupNewsgroupList()
{
    nsresult rv;
    if (!m_newsgroupList) return NS_OK;
	PRInt32 status = 0;
    rv = m_newsgroupList->FinishXOVERLINE(0,&status);
    m_newsgroupList = nsnull;
    NS_ASSERTION(NS_SUCCEEDED(rv), "FinishXOVERLINE failed");
    return rv;
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
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) CleanupAfterRunningUrl()", this));

  // send StopRequest notification after we've cleaned up the protocol
  // because it can synchronously causes a new url to get run in the
  // protocol - truly evil, but we're stuck at the moment.
	if (m_channelListener)
		rv = m_channelListener->OnStopRequest(this, m_channelContext, NS_OK);

	if (m_loadGroup)
		m_loadGroup->RemoveRequest(NS_STATIC_CAST(nsIRequest *, this), nsnull, NS_OK);
    CleanupNewsgroupList();

  // clear out mem cache entry so we're not holding onto it.
  if (m_runningURL)
  {
  	nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningURL);
    if (mailnewsurl)
    {
      mailnewsurl->SetUrlState(PR_FALSE, NS_OK);
      mailnewsurl->SetMemCacheEntry(nsnull);
    }
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
  
  mDisplayInputStream = nsnull;
  mDisplayOutputStream = nsnull;
  mProgressEventSink = nsnull;
  SetOwner(nsnull);

  m_channelContext = nsnull;
  m_channelListener = nsnull;
  m_loadGroup = nsnull;
  mCallbacks = nsnull;

  // don't mark ourselves as not busy until we are done cleaning up the connection. it should be the
  // last thing we do.
  SetIsBusy(PR_FALSE);
 
  return NS_OK;
}

nsresult nsNNTPProtocol::CloseSocket()
{
  PR_LOG(NNTP,PR_LOG_ALWAYS,("(%p) ClosingSocket()",this));
  
  if (m_nntpServer) {
    m_nntpServer->RemoveConnection(this);
    m_nntpServer = nsnull;
  }
  
  CleanupAfterRunningUrl(); // is this needed?
  return nsMsgProtocol::CloseSocket();
}

void nsNNTPProtocol::SetProgressBarPercent(PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (mProgressEventSink)
    mProgressEventSink->OnProgress(this, m_channelContext, aProgress, aProgressMax);                                       
}

nsresult
nsNNTPProtocol::SetProgressStatus(const PRUnichar *aMessage)
{
  nsresult rv = NS_OK;
  if (mProgressEventSink) 
  {
    rv = mProgressEventSink->OnStatus(this, m_channelContext, NS_OK, aMessage);
  }
  return rv;
}

NS_IMETHODIMP nsNNTPProtocol::GetContentType(char * *aContentType)
{	
  if (!aContentType) return NS_ERROR_NULL_POINTER;

  // if we've been set with a content type, then return it....
  // this happens when we go through libmime now as it sets our new content type
  if (!m_ContentType.IsEmpty())
  {
    *aContentType = ToNewCString(m_ContentType);
    return NS_OK;
  }

  // otherwise do what we did before...  

  if (m_typeWanted == GROUP_WANTED)  
  {
    *aContentType = nsCRT::strdup("x-application-newsgroup");
  }
  else 
  {
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
  if (m_runningURL) {
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl (do_QueryInterface(m_runningURL));
    nsCOMPtr<nsIPrompt> dialog;
    rv = GetPromptDialogFromUrl(msgUrl, getter_AddRefs(dialog));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString alertText;
    nsXPIDLString str;
    rv = GetNewsStringByID(MK_NNTP_ERROR_MESSAGE, getter_Copies(str));
    NS_ENSURE_SUCCESS(rv,rv);
	alertText.Append(str);

	if (text) {
	    alertText.Append(NS_LITERAL_STRING(" ").get());
		alertText.AppendWithConversion(text);
    }

	rv = dialog->Alert(nsnull, alertText.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  return rv;
}

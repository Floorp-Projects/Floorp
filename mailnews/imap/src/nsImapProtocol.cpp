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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#define DOING_PSEUDO_MAILBOXES

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

// as does this
#define NS_IMPL_IDS
#include "msgCore.h"  // for pre-compiled headers

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIStringBundle.h"

#include "nsMsgImapCID.h"
#include "nsIEventQueueService.h"

#include "nsImapCore.h"
#include "nsImapProtocol.h"
#include "nsIMsgMailNewsUrl.h"
#include "nscore.h"
#include "nsImapProxyEvent.h"
#include "nsIMAPHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsImapMailFolder.h"
#include "nsImapServerResponseParser.h"
#include "nspr.h"
#include "plbase64.h"
#include "nsIImapService.h"
#include "nsISocketTransportService.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIPipe.h"
#include "nsIMsgFolder.h"
#include "nsImapStringBundle.h"
#include "nsICopyMsgStreamListener.h"
#include "nsTextFormatter.h"
#include "nsAutoLock.h"
#include "nsIDNSService.h"
#include "nsIMsgHdr.h"
#include "nsMsgUtf7Utils.h"
// for the memory cache...
#include "nsICacheEntryDescriptor.h"
#include "nsICacheSession.h"

#include "nsCOMPtr.h"
PRLogModuleInfo *IMAP;

// netlib required files
#include "nsIStreamListener.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapIncomingServer.h"
#include "nsIPref.h"
#include "nsImapUtils.h"
#include "nsIProxyObjectManager.h"
#include "nsIStreamConverterService.h"

#if 0
#include "nsIHashAlgorithm.h"
#endif

#define ONE_SECOND ((PRUint32)1000)    // one second

const char *kImapTrashFolderName = "Trash"; // **** needs to be localized ****

static NS_DEFINE_CID(kIStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);

#define OUTPUT_BUFFER_SIZE (4096*2) // mscott - i should be able to remove this if I can use nsMsgLineBuffer???

#define IMAP_ENV_HEADERS "From To Cc Subject Date Message-ID "
#define IMAP_DB_HEADERS "Priority X-Priority References Newsgroups"
#define IMAP_ENV_AND_DB_HEADERS IMAP_ENV_HEADERS IMAP_DB_HEADERS
static const PRIntervalTime kImapSleepTime = PR_MillisecondsToInterval(1000);
static PRInt32 gPromoteNoopToCheckCount = 0;

// **** helper class for downloading line ****
TLineDownloadCache::TLineDownloadCache()
{
    fLineInfo = (msg_line_info *) PR_CALLOC(sizeof( msg_line_info));
    fLineInfo->adoptedMessageLine = fLineCache;
    fLineInfo->uidOfMessage = 0;
    fBytesUsed = 0;
}

TLineDownloadCache::~TLineDownloadCache()
{
    PR_FREEIF( fLineInfo);
}

PRUint32 TLineDownloadCache::CurrentUID()
{
    return fLineInfo->uidOfMessage;
}

PRUint32 TLineDownloadCache::SpaceAvailable()
{
    return kDownLoadCacheSize - fBytesUsed;
}

msg_line_info *TLineDownloadCache::GetCurrentLineInfo()
{
    return fLineInfo;
}
    
void TLineDownloadCache::ResetCache()
{
    fBytesUsed = 0;
}
    
PRBool TLineDownloadCache::CacheEmpty()
{
    return fBytesUsed == 0;
}

void TLineDownloadCache::CacheLine(const char *line, PRUint32 uid)
{
    PRUint32 lineLength = PL_strlen(line);
    NS_ASSERTION((lineLength + 1) <= SpaceAvailable(), 
                 "Oops... line length greater than space available");
    
    fLineInfo->uidOfMessage = uid;
    
    PL_strcpy(fLineInfo->adoptedMessageLine + fBytesUsed, line);
    fBytesUsed += lineLength;
}

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_THREADSAFE_ADDREF(nsImapProtocol)
NS_IMPL_THREADSAFE_RELEASE(nsImapProtocol)

NS_INTERFACE_MAP_BEGIN(nsImapProtocol)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIImapProtocol)
   NS_INTERFACE_MAP_ENTRY(nsIRunnable)
   NS_INTERFACE_MAP_ENTRY(nsIImapProtocol)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
NS_INTERFACE_MAP_END_THREADSAFE

static PRInt32 gTooFastTime = 2;
static PRInt32 gIdealTime = 4;
static PRInt32 gChunkAddSize = 2048;
static PRInt32 gChunkSize = 10240;
static PRInt32 gChunkThreshold = 10240 + 4096;
static PRBool gFetchByChunks = PR_TRUE;
static PRInt32 gMaxChunkSize = 40960;
static PRBool gInitialized = PR_FALSE;
static PRBool gHideUnusedNamespaces = PR_TRUE;
static PRBool gHideOtherUsersFromList = PR_FALSE;
static PRBool gUseEnvelopeCmd = PR_FALSE;

nsresult nsImapProtocol::GlobalInitialization()
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
  if (NS_SUCCEEDED(rv) && prefs) 
  {
    prefs->GetIntPref("mail.imap.chunk_fast", &gTooFastTime);   // secs we read too little too fast
    prefs->GetIntPref("mail.imap.chunk_ideal", &gIdealTime);    // secs we read enough in good time
    prefs->GetIntPref("mail.imap.chunk_add", &gChunkAddSize);   // buffer size to add when wasting time
    prefs->GetIntPref("mail.imap.chunk_size", &gChunkSize);
    prefs->GetIntPref("mail.imap.min_chunk_size_threshold", &gChunkThreshold);
    prefs->GetIntPref("mail.imap.max_chunk_size", &gMaxChunkSize);
    prefs->GetBoolPref("mail.imap.hide_other_users",
                       &gHideOtherUsersFromList);
    prefs->GetBoolPref("mail.imap.hide_unused_namespaces",
                       &gHideUnusedNamespaces);
    prefs->GetIntPref("mail.imap.noop_check_count", &gPromoteNoopToCheckCount);
    prefs->GetBoolPref("mail.imap.use_envelope_cmd",
                       &gUseEnvelopeCmd);
  }
  gInitialized = PR_TRUE;
  return rv;
}

nsImapProtocol::nsImapProtocol() : 
    m_parser(*this)
{
  NS_INIT_REFCNT();
  m_flags = 0;
  m_urlInProgress = PR_FALSE;
  m_socketIsOpen = PR_FALSE;
  m_gotFEEventCompletion = PR_FALSE;
  m_connectionStatus = 0;
  m_hostSessionList = nsnull;
  m_flagState = nsnull;
  m_fetchBodyIdList = nsnull;
    
  if (!gInitialized)
    GlobalInitialization();

    // ***** Thread support *****
  m_thread = nsnull;
  m_dataAvailableMonitor = nsnull;
  m_urlReadyToRunMonitor = nsnull;
  m_pseudoInterruptMonitor = nsnull;
  m_dataMemberMonitor = nsnull;
  m_threadDeathMonitor = nsnull;
  m_eventCompletionMonitor = nsnull;
  m_waitForBodyIdsMonitor = nsnull;
  m_fetchMsgListMonitor = nsnull;
  m_fetchBodyListMonitor = nsnull;
  m_imapThreadIsRunning = PR_FALSE;
  m_currentServerCommandTagNumber = 0;
  m_active = PR_FALSE;
  m_folderNeedsSubscribing = PR_FALSE;
  m_folderNeedsACLRefreshed = PR_FALSE;
  m_threadShouldDie = PR_FALSE;
  m_pseudoInterrupted = PR_FALSE;
  m_nextUrlReadyToRun = PR_FALSE;
  m_trackingTime = PR_FALSE;
  LL_I2L(m_startTime, 0);
  LL_I2L(m_endTime, 0);
  LL_I2L(m_lastActiveTime, 0);
  LL_I2L(m_lastProgressTime, 0);
  ResetProgressInfo();

  m_tooFastTime = 0;
  m_idealTime = 0;
  m_chunkAddSize = 0;
  m_chunkStartSize = 0;
  m_maxChunkSize = 0;
  m_fetchByChunks = PR_TRUE;
  m_chunkSize = 0;
  m_chunkThreshold = 0;
  m_fromHeaderSeen = PR_FALSE;
  m_closeNeededBeforeSelect = PR_FALSE;
  m_needNoop = PR_FALSE;
  m_noopCount = 0;
  m_promoteNoopToCheckCount = 0;
  m_mailToFetch = PR_FALSE;
  m_fetchMsgListIsNew = PR_FALSE;
  m_fetchBodyListIsNew = PR_FALSE;

  m_checkForNewMailDownloadsHeaders = PR_TRUE;  // this should be on by default
  m_hierarchyNameState = kNoOperationInProgress;
  m_onlineBaseFolderExists = PR_FALSE;
  m_discoveryStatus = eContinue;

  m_overRideUrlConnectionInfo = PR_FALSE;
  // m_dataOutputBuf is used by Send Data
  m_dataOutputBuf = (char *) PR_CALLOC(sizeof(char) * OUTPUT_BUFFER_SIZE);
  m_allocatedSize = OUTPUT_BUFFER_SIZE;

  // used to buffer incoming data by ReadNextLineFromInput
  m_inputStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE /* allocate new lines */, PR_FALSE /* leave CRLFs on the returned string */);
  m_currentBiffState = nsIMsgFolder::nsMsgBiffState_Unknown;

  m_userName = nsnull;
  m_hostName = nsnull;
  m_serverKey = nsnull;

  m_progressStringId = 0;

  // subscription
  m_autoSubscribe = PR_TRUE;
  m_autoUnsubscribe = PR_TRUE;
  m_autoSubscribeOnOpen = PR_TRUE;
  m_deletableChildren = nsnull;

  Configure(gTooFastTime, gIdealTime, gChunkAddSize, gChunkSize,
                    gChunkThreshold, gFetchByChunks, gMaxChunkSize);

  // where should we do this? Perhaps in the factory object?
  if (!IMAP)
    IMAP = PR_NewLogModule("IMAP");
}

nsresult nsImapProtocol::Configure(PRInt32 TooFastTime, PRInt32 IdealTime,
                  PRInt32 ChunkAddSize, PRInt32 ChunkSize, PRInt32 ChunkThreshold,
                  PRBool FetchByChunks, PRInt32 MaxChunkSize)
{
  m_tooFastTime = TooFastTime;    // secs we read too little too fast
  m_idealTime = IdealTime;    // secs we read enough in good time
  m_chunkAddSize = ChunkAddSize;    // buffer size to add when wasting time
  m_chunkStartSize = m_chunkSize = ChunkSize;
  m_chunkThreshold = ChunkThreshold;
  m_fetchByChunks = FetchByChunks;
  m_maxChunkSize = MaxChunkSize;

  return NS_OK;
}


nsresult nsImapProtocol::Initialize(nsIImapHostSessionList * aHostSessionList, nsIEventQueue * aSinkEventQueue)
{
  NS_PRECONDITION(aSinkEventQueue && aHostSessionList, 
             "oops...trying to initalize with a null sink event queue!");
  if (!aSinkEventQueue || !aHostSessionList)
        return NS_ERROR_NULL_POINTER;

   m_flagState = new nsImapFlagAndUidState(kImapFlagAndUidStateSize, PR_FALSE);
   if (!m_flagState)
	   return NS_ERROR_OUT_OF_MEMORY;

   NS_ADDREF(m_flagState);

    m_sinkEventQueue = dont_QueryInterface(aSinkEventQueue);
    m_hostSessionList = aHostSessionList; // no ref count...host session list has life time > connection
    m_parser.SetHostSessionList(aHostSessionList);
    m_parser.SetFlagState(m_flagState);

  // Now initialize the thread for the connection and create appropriate monitors
  if (m_thread == nsnull)
  {
    m_dataAvailableMonitor = PR_NewMonitor();
    m_urlReadyToRunMonitor = PR_NewMonitor();
    m_pseudoInterruptMonitor = PR_NewMonitor();
    m_dataMemberMonitor = PR_NewMonitor();
    m_threadDeathMonitor = PR_NewMonitor();
    m_eventCompletionMonitor = PR_NewMonitor();
    m_waitForBodyIdsMonitor = PR_NewMonitor();
    m_fetchMsgListMonitor = PR_NewMonitor();
    m_fetchBodyListMonitor = PR_NewMonitor();

    SetFlag(IMAP_FIRST_PASS_IN_THREAD);
 
    nsresult rv = NS_NewThread(getter_AddRefs(m_iThread), this);
    if (NS_FAILED(rv)) 
    {
      NS_ASSERTION(m_iThread, "Unable to create imap thread.\n");
      return rv;
    }
    m_iThread->GetPRThread(&m_thread);

  }
  return NS_OK;
}

nsImapProtocol::~nsImapProtocol()
{
  PR_FREEIF(m_userName);
  PR_FREEIF(m_serverKey);
  PR_FREEIF(m_fetchBodyIdList);

  NS_IF_RELEASE(m_flagState);

  nsCRT::free(m_hostName);

  PR_FREEIF(m_dataOutputBuf);
  if (m_inputStreamBuffer)
    delete m_inputStreamBuffer;

  // **** We must be out of the thread main loop function
  NS_ASSERTION(m_imapThreadIsRunning == PR_FALSE, "Oops, thread is still running.\n");

  if (m_dataAvailableMonitor)
  {
    PR_DestroyMonitor(m_dataAvailableMonitor);
    m_dataAvailableMonitor = nsnull;
  }

  if (m_urlReadyToRunMonitor)
  {
    PR_DestroyMonitor(m_urlReadyToRunMonitor);
    m_urlReadyToRunMonitor = nsnull;
  }
  if (m_pseudoInterruptMonitor)
  {
    PR_DestroyMonitor(m_pseudoInterruptMonitor);
    m_pseudoInterruptMonitor = nsnull;
  }
  if (m_dataMemberMonitor)
  {
    PR_DestroyMonitor(m_dataMemberMonitor);
    m_dataMemberMonitor = nsnull;
  }
  if (m_threadDeathMonitor)
  {
    PR_DestroyMonitor(m_threadDeathMonitor);
    m_threadDeathMonitor = nsnull;
  }
  if (m_eventCompletionMonitor)
  {
      PR_DestroyMonitor(m_eventCompletionMonitor);
      m_eventCompletionMonitor = nsnull;
  }
  if (m_waitForBodyIdsMonitor)
  {
    PR_DestroyMonitor(m_waitForBodyIdsMonitor);
    m_waitForBodyIdsMonitor = nsnull;
  }
  if (m_fetchMsgListMonitor)
  {
    PR_DestroyMonitor(m_fetchMsgListMonitor);
    m_fetchMsgListMonitor = nsnull;
  }
  if (m_fetchBodyListMonitor)
  {
    PR_DestroyMonitor(m_fetchBodyListMonitor);
    m_fetchBodyListMonitor = nsnull;
  }
}

const char*
nsImapProtocol::GetImapHostName()
{
  if (m_runningUrl && !m_hostName)
  {
    nsCOMPtr<nsIURI> url = do_QueryInterface(m_runningUrl);
    url->GetHost(&m_hostName);
  }

  return m_hostName;
}

const char*
nsImapProtocol::GetImapUserName()
{
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(m_server);
  if (!m_userName && server)
    server->GetUsername(&m_userName);
  return m_userName;
}

const char*
nsImapProtocol::GetImapServerKey()
{
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(m_server);
  if (!m_serverKey && server)
    server->GetKey(&m_serverKey);
  return m_serverKey;
}

void
nsImapProtocol::SetupSinkProxy()
{
  nsresult res = NS_ERROR_FAILURE;
//  NS_ASSERTION(!m_imapMiscellaneousSink, "shouldn't be non-null here");

  if (m_runningUrl)
  {
    NS_ASSERTION(m_sinkEventQueue && m_thread, "fatal... null sink event queue or thread");

    nsCOMPtr<nsIProxyObjectManager> proxyManager(do_GetService(kProxyObjectManagerCID, &res));
    if (proxyManager) // if we don't get one of these are as good as dead...
    {
      if (!m_imapMailFolderSink)
      {
        nsCOMPtr<nsIImapMailFolderSink> aImapMailFolderSink;
        res = m_runningUrl->GetImapMailFolderSink(getter_AddRefs(aImapMailFolderSink));
        if (NS_SUCCEEDED(res) && aImapMailFolderSink)
          res = proxyManager->GetProxyForObject(m_sinkEventQueue,
                                             NS_GET_IID(nsIImapMailFolderSink),
                                             aImapMailFolderSink,
                                             PROXY_SYNC | PROXY_ALWAYS,
                                             getter_AddRefs(m_imapMailFolderSink));
      }
      
      if (!m_imapMessageSink)
      {
        nsCOMPtr<nsIImapMessageSink> aImapMessageSink;
        res = m_runningUrl->GetImapMessageSink(getter_AddRefs(aImapMessageSink));
        if (NS_SUCCEEDED(res) && aImapMessageSink)
          res = proxyManager->GetProxyForObject(m_sinkEventQueue,
                                             NS_GET_IID(nsIImapMessageSink),
                                             aImapMessageSink,
                                             PROXY_SYNC | PROXY_ALWAYS,
                                             getter_AddRefs(m_imapMessageSink));
      }
      if (!m_imapExtensionSink)
      {
        nsCOMPtr<nsIImapExtensionSink> aImapExtensionSink;
        res = m_runningUrl->GetImapExtensionSink(getter_AddRefs(aImapExtensionSink));
        if(NS_SUCCEEDED(res) && aImapExtensionSink)
        {
          nsImapExtensionSinkProxy * extensionSink = new nsImapExtensionSinkProxy(aImapExtensionSink,
                                                     this,
                                                     m_sinkEventQueue,
                                                     m_thread);
          m_imapExtensionSink = do_QueryInterface(extensionSink);
        }
      }
      if (!m_imapMiscellaneousSink)
      {
        nsCOMPtr<nsIImapMiscellaneousSink> aImapMiscellaneousSink;
        res = m_runningUrl->GetImapMiscellaneousSink(getter_AddRefs(aImapMiscellaneousSink));
        if (NS_SUCCEEDED(res) && aImapMiscellaneousSink)
        {
           nsImapMiscellaneousSinkProxy * miscSink = new nsImapMiscellaneousSinkProxy(aImapMiscellaneousSink,
                                                                                      this,
                                                                                      m_sinkEventQueue,
                                                                                      m_thread);
           m_imapMiscellaneousSink = do_QueryInterface(miscSink);
        }
        NS_ASSERTION(NS_SUCCEEDED(res), "couldn't get proxies");
      }
      if (!m_imapServerSink)
      {
         nsCOMPtr<nsIImapServerSink> aImapServerSink;
         res = m_runningUrl->GetImapServerSink(getter_AddRefs(aImapServerSink));
         if (NS_SUCCEEDED(res) && aImapServerSink)
            res = proxyManager->GetProxyForObject(  m_sinkEventQueue,
                             NS_GET_IID(nsIImapServerSink),
                             aImapServerSink,
                             PROXY_SYNC | PROXY_ALWAYS,
                             getter_AddRefs(m_imapServerSink));
        NS_ASSERTION(NS_SUCCEEDED(res), "couldn't get proxies");
      }
    }
    else
      NS_ASSERTION(PR_FALSE, "can't get proxy service");
  }
  NS_ASSERTION(NS_SUCCEEDED(res), "couldn't get proxies");
}

// Setup With Url is intended to set up data which is held on a PER URL basis and not
// a per connection basis. If you have data which is independent of the url we are currently
// running, then you should put it in Initialize(). 
nsresult nsImapProtocol::SetupWithUrl(nsIURI * aURL, nsISupports* aConsumer)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_PRECONDITION(aURL, "null URL passed into Imap Protocol");
  if (aURL)
  {
        rv = aURL->QueryInterface(NS_GET_IID(nsIImapUrl), getter_AddRefs(m_runningUrl));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(m_server);
        if (!server)
        {
            nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl);
            rv = mailnewsUrl->GetServer(getter_AddRefs(server));
            m_server = getter_AddRefs(NS_GetWeakReference(server));
        }
        nsCOMPtr<nsIImapIncomingServer> imapServer = do_QueryInterface(server);

        nsCOMPtr<nsIStreamListener> aRealStreamListener = do_QueryInterface(aConsumer);
        m_runningUrl->GetMockChannel(getter_AddRefs(m_mockChannel));
        if (m_mockChannel)
        {   
            // if we have a listener from a mock channel, over-ride the consumer that was passed in
            nsCOMPtr<nsIStreamListener> channelListener;
            m_mockChannel->GetChannelListener(getter_AddRefs(channelListener));
            if (channelListener) // only over-ride if we have a non null channel listener
                aRealStreamListener = channelListener;
            m_mockChannel->GetChannelContext(getter_AddRefs(m_channelContext));
        }

        // since we'll be making calls directly from the imap thread to the channel listener,
        // we need to turn it into a proxy object....we'll assume that the listener is on the same thread
        // as the event sink queue
        if (aRealStreamListener)
        {
            rv = NS_NewAsyncStreamListener(getter_AddRefs(m_channelListener), aRealStreamListener, m_sinkEventQueue);
        }

    PRUint32 capability = kCapabilityUndefined;

    m_hostSessionList->GetCapabilityForHost(GetImapServerKey(), capability);
    GetServerStateParser().SetCapabilityFlag(capability);

    if (imapServer)
      imapServer->GetFetchByChunks(&m_fetchByChunks);

    if ( m_runningUrl && !m_channel /* and we don't have a transport yet */)
    {
      // extract the file name and create a file transport...
      PRInt32 port=-1;
      server->GetPort(&port);

      if (port <= 0)
      {
        PRBool isSecure = PR_FALSE;
        // Be a bit smarter about setting the default port
        if (NS_SUCCEEDED(server->GetIsSecure(&isSecure)) && isSecure) 
          port = SECURE_IMAP_PORT;
        else
          port = IMAP_PORT;
      }
      
      nsXPIDLCString hostName;
            
      nsCOMPtr<nsISocketTransportService> socketService = 
               do_GetService(kSocketTransportServiceCID, &rv);
      if (NS_SUCCEEDED(rv) && aURL)
      {
        aURL->GetPort(&port);
        server->GetRealHostName(getter_Copies(hostName));

        ClearFlag(IMAP_CONNECTION_IS_OPEN); 
        PRBool isSecure = PR_FALSE;
        const char *connectionType = nsnull;
        
        if (NS_SUCCEEDED(server->GetIsSecure(&isSecure)) && isSecure) 
          connectionType = "ssl-forcehandshake";

        if (m_overRideUrlConnectionInfo)
            rv = socketService->CreateTransportOfType(connectionType, m_logonHost.get(), m_logonPort, nsnull, 0, 0, getter_AddRefs(m_channel));
        else
            rv = socketService->CreateTransportOfType(connectionType, hostName, port, nsnull, 0, 0, getter_AddRefs(m_channel));
        
        // Ensure that the socket can get the notification callbacks
        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        m_mockChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));
        if (m_channel)
        {
          m_channel->SetNotificationCallbacks(callbacks, PR_FALSE);
        
          if (NS_SUCCEEDED(rv))
            rv = m_channel->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(m_outputStream));
        }
      }
    } // if m_runningUrl

    if (m_channel && m_mockChannel)
    {
      // set the security info for the mock channel to be the security status for our underlying transport.
      nsCOMPtr<nsISupports> securityInfo;
      m_channel->GetSecurityInfo(getter_AddRefs(securityInfo));
      m_mockChannel->SetSecurityInfo(securityInfo);

      // and if we have a cache entry that we are saving the message to, set the security info on it too.
      // since imap only uses the memory cache, passing this on is the right thing to do.
      nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl);
      if (mailnewsUrl)
      {
        nsCOMPtr<nsICacheEntryDescriptor> cacheEntry;
        mailnewsUrl->GetMemCacheEntry(getter_AddRefs(cacheEntry));
        if (cacheEntry)
          cacheEntry->SetSecurityInfo(securityInfo);
      }
    }
  } // if aUR
    
  return rv;
}


// when the connection is done processing the current state, free any per url state data...
void nsImapProtocol::ReleaseUrlState()
{
  if (m_mockChannel)
  {
    if (m_imapMailFolderSink)
      m_imapMailFolderSink->CloseMockChannel(m_mockChannel);
    else
      m_mockChannel->Close();
     m_mockChannel = nsnull;
  }
  if (m_runningUrl)
  {
    nsCOMPtr<nsIMsgMailNewsUrl>  mailnewsurl = do_QueryInterface(m_runningUrl);
    if (m_imapServerSink)  
      m_imapServerSink->RemoveChannelFromUrl(mailnewsurl, NS_OK);
    m_runningUrl = nsnull; // force us to release our last reference on the url

    // we want to make sure the imap protocol's last reference to the url gets released
    // back on the UI thread. This ensures that the objects the imap url hangs on to
    // properly get released back on the UI thread. In order to do this, we need a
    // a fancy texas two step where we first give the ui thread the url we want to
    // release, then we forget about our copy. Then we tell it to release the url
    // for real.
    if (m_imapMailFolderSink)
    {
      m_imapMailFolderSink->PrepareToReleaseUrl(mailnewsurl);
      mailnewsurl = nsnull;
      // at this point in time, we MUST have released all of our references to 
      // the url from the imap protocol. otherwise this whole exercise is moot.
      m_imapMailFolderSink->ReleaseUrl();
    }
  }

  m_imapMailFolderSink = nsnull;
  m_imapMessageSink = nsnull;
  m_imapExtensionSink = nsnull;
  m_imapMiscellaneousSink = nsnull;
  m_channelListener = nsnull;
  m_channelContext = nsnull;
  
  m_channelInputStream = nsnull;
  m_channelOutputStream = nsnull;
}


NS_IMETHODIMP nsImapProtocol::Run()
{
  nsImapProtocol *me = this;
  nsresult result = NS_OK;
  NS_ASSERTION(me, "Yuk, me is null.\n");
    
  PR_CEnterMonitor(this);
  NS_ASSERTION(me->m_imapThreadIsRunning == PR_FALSE, 
                 "Oh. oh. thread is already running. What's wrong here?");
    if (me->m_imapThreadIsRunning)
    {
        PR_CExitMonitor(me);
        return NS_OK;
    }


  nsCOMPtr<nsIEventQueueService> pEventQService = 
           do_GetService(kEventQueueServiceCID, &result); 
  
  if (NS_FAILED(result)) return result;

    result = pEventQService->CreateMonitoredThreadEventQueue();
	
	pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,getter_AddRefs(me->m_eventQueue));

    NS_ASSERTION(me->m_eventQueue, 
                 "Unable to create imap thread event queue.\n");
    if (NS_FAILED(result) || !me->m_eventQueue)
    {
        PR_CExitMonitor(me);
        return result;
    }
    me->m_imapThreadIsRunning = PR_TRUE;
    PR_CExitMonitor(me);

    // call the platform specific main loop ....
    me->ImapThreadMainLoop();

    me->m_eventQueue->StopAcceptingEvents();
    me->m_eventQueue->ProcessPendingEvents();
    me->m_eventQueue = nsnull;

    nsCOMPtr<nsIMsgIncomingServer> me_server = do_QueryReferent(m_server);
    if (me_server)
    {
        nsCOMPtr<nsIImapIncomingServer>
            aImapServer(do_QueryInterface(me_server, &result));
        if (NS_SUCCEEDED(result))
            aImapServer->RemoveConnection(me);
        me_server = nsnull;
    }
        
    me->m_runningUrl = nsnull;
    me->m_channel = nsnull;
    me->m_outputStream = nsnull;
    me->m_channelListener = nsnull;
    me->m_channelContext = nsnull;
    if (me->m_mockChannel)
    {
        me->m_mockChannel->Close();
        me->m_mockChannel = nsnull;
    }
    me->m_channelInputStream = nsnull;
    me->m_channelOutputStream = nsnull;
    me->m_sinkEventQueue = nsnull;
    me->m_eventQueue = nsnull;
    me->m_server = nsnull;
    me->m_imapMailFolderSink = nsnull;
    me->m_imapExtensionSink = nsnull;
    me->m_imapMessageSink = nsnull;
    me->m_imapMiscellaneousSink = nsnull;
    m_iThread = nsnull;

    result = pEventQService->DestroyThreadEventQueue(); 
    return NS_OK;
}

PRBool
nsImapProtocol::ImapThreadIsRunning()
{
    PRBool retValue = PR_FALSE;
    retValue = m_imapThreadIsRunning;
    return retValue;
}

NS_IMETHODIMP
nsImapProtocol::GetThreadEventQueue(nsIEventQueue **aEventQueue)
{
    // *** should subclassing PLEventQueue and ref count it ***
    // *** need to find a way to prevent dangling pointer ***
    // *** a callback mechanism or a semaphor control thingy ***
    
  PR_CEnterMonitor(this);
    if (aEventQueue)
  {
        *aEventQueue = m_eventQueue;
    NS_IF_ADDREF(*aEventQueue);
  }
    PR_CExitMonitor(this);
    return NS_OK;
}

NS_IMETHODIMP 
nsImapProtocol::NotifyFEEventCompletion()
{
    PR_EnterMonitor(m_eventCompletionMonitor);
    PR_Notify(m_eventCompletionMonitor);
    m_gotFEEventCompletion = PR_TRUE;
    PR_ExitMonitor(m_eventCompletionMonitor);
    return NS_OK;
}

void
nsImapProtocol::WaitForFEEventCompletion()
{
  PR_EnterMonitor(m_eventCompletionMonitor);
  if (!m_gotFEEventCompletion)
    PR_Wait(m_eventCompletionMonitor, PR_INTERVAL_NO_TIMEOUT);
  m_gotFEEventCompletion = PR_FALSE;
  PR_ExitMonitor(m_eventCompletionMonitor);
}

NS_IMETHODIMP
nsImapProtocol::TellThreadToDie(PRBool isSaveToClose)
{
    // **** jt - This routine should only be called by imap service.
  nsAutoCMonitor(this);

  m_urlInProgress = PR_TRUE;  // let's say it's busy so no one tries to use
                              // this about to die connection.
  PRBool closeNeeded = GetServerStateParser().GetIMAPstate() ==
  nsImapServerResponseParser::kFolderSelected && isSaveToClose;
  nsCString command;
  nsresult rv = NS_OK;
  PRUint32 writeCount;

  if (closeNeeded && GetDeleteIsMoveToTrash() &&
      TestFlag(IMAP_CONNECTION_IS_OPEN) && m_outputStream)
  {
    IncrementCommandTagNumber();
    command = GetServerCommandTag();
    command.Append(" close" CRLF);
    rv = m_outputStream->Write(command.get(), command.Length(),
                               &writeCount);
    Log("SendData", "TellThreadToDie", command.get());
  }

  if (NS_SUCCEEDED(rv) && TestFlag(IMAP_CONNECTION_IS_OPEN) && m_outputStream)
  {
    IncrementCommandTagNumber();
    command = GetServerCommandTag();
    command.Append(" logout" CRLF);
    rv = m_outputStream->Write(command.get(), command.Length(),
                               &writeCount);
    Log("SendData", "TellThreadToDie", command.get());
  }

  if (mAsyncReadRequest)
    mAsyncReadRequest->Cancel(NS_BINDING_ABORTED);
  PR_EnterMonitor(m_threadDeathMonitor);
  m_threadShouldDie = PR_TRUE;
  PR_ExitMonitor(m_threadDeathMonitor);

  PR_EnterMonitor(m_eventCompletionMonitor);
  PR_NotifyAll(m_eventCompletionMonitor);
  PR_ExitMonitor(m_eventCompletionMonitor);


  PR_EnterMonitor(m_dataAvailableMonitor);
  PR_Notify(m_dataAvailableMonitor);
  PR_ExitMonitor(m_dataAvailableMonitor);

  PR_EnterMonitor(m_urlReadyToRunMonitor);
  m_imapThreadIsRunning = PR_FALSE;
  PR_NotifyAll(m_urlReadyToRunMonitor);
  PR_ExitMonitor(m_urlReadyToRunMonitor);
  return rv;
}

NS_IMETHODIMP
nsImapProtocol::GetLastActiveTimeStamp(PRTime* aTimeStamp)
{
  nsAutoCMonitor(this);
  if (aTimeStamp)
      *aTimeStamp = m_lastActiveTime;
  return NS_OK;
}

NS_IMETHODIMP
nsImapProtocol::PseudoInterruptMsgLoad(nsIImapUrl *aImapUrl, PRBool *interrupted)
{
  NS_ENSURE_ARG (interrupted);

  *interrupted = PR_FALSE;

  nsAutoCMonitor(this);

  if (m_runningUrl && !TestFlag(IMAP_CLEAN_UP_URL_STATE))
  {
    nsImapAction imapAction;
    m_runningUrl->GetImapAction(&imapAction);

    if (imapAction == nsIImapUrl::nsImapMsgFetch)
    {
      nsresult rv = NS_OK;
      nsCOMPtr<nsIImapUrl> runningImapURL;

      rv = GetRunningImapURL(getter_AddRefs(runningImapURL));
      if (NS_SUCCEEDED(rv) && runningImapURL)
      {
        nsXPIDLCString runningImapUrlSourceFolder;
        nsXPIDLCString newImapUrlSourceFolder;

        runningImapURL->CreateServerSourceFolderPathString(getter_Copies(runningImapUrlSourceFolder));
        aImapUrl->CreateServerSourceFolderPathString(getter_Copies(newImapUrlSourceFolder));
        if (!nsCRT::strcmp(runningImapUrlSourceFolder, newImapUrlSourceFolder))
        {
          PseudoInterrupt(PR_TRUE);
          *interrupted = PR_TRUE;
        }
      }
    }
  }
#ifdef DEBUG_bienvenu
  printf("interrupt msg load : %s\n", (*interrupted) ? "TRUE" : "FALSE");
#endif
  return NS_OK;
}

void
nsImapProtocol::ImapThreadMainLoop()
{
  PRIntervalTime sleepTime = kImapSleepTime;
    // ****** please implement PR_LOG 'ing ******
  while (ImapThreadIsRunning() && !DeathSignalReceived())
  {
  // if we are making our first pass through this loop and
  // we already have a url to process then jump right in and
  // process the current url. Don't try to wait for the monitor
  // the first time because it may have already been signaled.
  // But make sure we have a channel first, or ProcessCurrentUrl will fail.
    if (TestFlag(IMAP_FIRST_PASS_IN_THREAD) && m_runningUrl && m_channel)
    {
      // if we launched another url, just loop around and process it.
      if (ProcessCurrentURL())
        continue;
      ClearFlag(IMAP_FIRST_PASS_IN_THREAD);
    }

    if (DeathSignalReceived()) break;

    PR_EnterMonitor(m_urlReadyToRunMonitor);

    PRStatus err;

    err = PR_Wait(m_urlReadyToRunMonitor, sleepTime);

    PR_ExitMonitor(m_urlReadyToRunMonitor);
    if (err == PR_FAILURE && PR_PENDING_INTERRUPT_ERROR == PR_GetError()) 
    {
      printf("error waiting for monitor\n");
      break;
    }

  //    m_eventQueue->ProcessPendingEvents();
  //    m_sinkEventQueue->ProcessPendingEvents();

    if (m_nextUrlReadyToRun && m_runningUrl)
    {
      m_nextUrlReadyToRun = PR_FALSE;
      ProcessCurrentURL();
    }
  }
  m_imapThreadIsRunning = PR_FALSE;
}

void nsImapProtocol::EstablishServerConnection()
{
  char * serverResponse = CreateNewLineFromSocket(); // read in the greeting

  // record the fact that we've received a greeting for this connection so we don't ever
  // try to do it again..
  if (serverResponse) {
    SetFlag(IMAP_RECEIVED_GREETING);
  }

  if (!nsCRT::strncasecmp(serverResponse, "* OK", 4))
  {
    SetConnectionStatus(0);
  }
  else if (!nsCRT::strncasecmp(serverResponse, "* PREAUTH", 9))
  {
    // we've been pre-authenticated.
    // we can skip the whole password step, right into the
    // kAuthenticated state
    GetServerStateParser().PreauthSetAuthenticatedState();

    if (GetServerStateParser().GetCapabilityFlag() == kCapabilityUndefined)
      Capability();

    if ( !(GetServerStateParser().GetCapabilityFlag() & 
          (kIMAP4Capability | kIMAP4rev1Capability | kIMAP4other) ) )
    {
      // AlertUserEvent_UsingId(MK_MSG_IMAP_SERVER_NOT_IMAP4);
      SetConnectionStatus(-1);        // stop netlib
    }
    else
    {
      // let's record the user as authenticated.
      m_imapServerSink->SetUserAuthenticated(PR_TRUE);

      ProcessAfterAuthenticated();
      // the connection was a success
      SetConnectionStatus(0);
     }
  }
  
  PR_FREEIF(serverResponse); // we don't care about the greeting yet...
}

// returns PR_TRUE if another url was run, PR_FALSE otherwise.
PRBool nsImapProtocol::ProcessCurrentURL()
{
  PRBool  logonFailed = PR_FALSE;
  PRBool anotherUrlRun = PR_FALSE;


  PseudoInterrupt(PR_FALSE);  // clear this if left over from previous url.

  if (!TestFlag(IMAP_CONNECTION_IS_OPEN) && m_channel)
  {
    m_channel->AsyncRead(this /* stream listener */, nsnull, 0, PRUint32(-1), 0,
                         getter_AddRefs(mAsyncReadRequest));
    SetFlag(IMAP_CONNECTION_IS_OPEN);
  }
#ifdef DEBUG_bienvenu   
  NS_ASSERTION(m_imapMiscellaneousSink, "null sink");
#endif
  if (!m_imapMiscellaneousSink)
    SetupSinkProxy(); // try this again. Evil, but I'm desperate.

  // we used to check if the current running url was 
  // Reinitialize the parser
  GetServerStateParser().InitializeState();
  GetServerStateParser().SetConnected(PR_TRUE);

  // acknowledge that we are running the url now..
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_runningUrl, &rv);
#ifdef DEBUG_bienvenu1
    nsXPIDLCString urlSpec;
    mailnewsurl->GetSpec(getter_Copies(urlSpec));
    printf("processing url %s\n", (const char *) urlSpec);
#endif
  if (NS_SUCCEEDED(rv) && mailnewsurl && m_imapMailFolderSink)
  {
    m_imapMailFolderSink->SetUrlState(this, mailnewsurl, PR_TRUE,
                                             NS_OK);
  }

    // if we are set up as a channel, we should notify our channel listener that we are starting...
  // so pass in ourself as the channel and not the underlying socket or file channel the protocol
  // happens to be using
  if (m_channelListener) {
      nsCOMPtr<nsIRequest> request = do_QueryInterface(m_mockChannel);
      m_channelListener->OnStartRequest(request, m_channelContext);
  }
  // mscott - I believe whenever we get here that we will also have
  // a connection. Why? Because when we load the url we open the 
  // connection if it isn't open already....However, if we haven't received
  // the greeting yet, we need to make sure we strip it out of the input
  // before we start to do useful things...
  if (!TestFlag(IMAP_RECEIVED_GREETING))
    EstablishServerConnection();

  // Update the security status for this URL
#ifdef UNREADY_CODE
  UpdateSecurityStatus();
#endif

  // Step 1: If we have not moved into the authenticated state yet then do so
  // by attempting to logon.
  if (!DeathSignalReceived() && (GetConnectionStatus() >= 0) &&
            (GetServerStateParser().GetIMAPstate() == 
       nsImapServerResponseParser::kNonAuthenticated))
    {
      /* if we got here, the server's greeting should not have been PREAUTH */
      if (GetServerStateParser().GetCapabilityFlag() == kCapabilityUndefined)
          Capability();
      
      if ( !(GetServerStateParser().GetCapabilityFlag() & (kIMAP4Capability | kIMAP4rev1Capability | 
             kIMAP4other) ) )
      {
        AlertUserEventUsingId(IMAP_SERVER_NOT_IMAP4);

        SetConnectionStatus(-1);        // stop netlib
      }
      else
      {
        logonFailed = !TryToLogon();
      }
    } // if death signal not received

    if (!DeathSignalReceived() && (GetConnectionStatus() >= 0))
    {
      // if the server supports a language extension then we should
      // attempt to issue the language extension.
      if ( GetServerStateParser().GetCapabilityFlag() & kHasLanguageCapability)
        Language();

      if (m_runningUrl)
        FindMailboxesIfNecessary();
      
      nsImapState imapState;      
      if (m_runningUrl)
        m_runningUrl->GetRequiredImapState(&imapState);
      
      if (imapState == nsIImapUrl::nsImapAuthenticatedState)
        ProcessAuthenticatedStateURL();
      else   // must be a url that requires us to be in the selected stae 
        ProcessSelectedStateURL();

    // The URL has now been processed
        if (!logonFailed && GetConnectionStatus() < 0)
           HandleCurrentUrlError();
        if (DeathSignalReceived())
        {
          HandleCurrentUrlError();
        }
        else
        {
        }
    }
    else if (!logonFailed)
        HandleCurrentUrlError(); 

    if (mailnewsurl && m_imapMailFolderSink)
    {
        rv = GetServerStateParser().LastCommandSuccessful() ? NS_OK :
             NS_ERROR_FAILURE;
        m_imapMailFolderSink->SetUrlState(this, mailnewsurl, PR_FALSE,
                                             rv);  // we are done with this
                                                      // url.
    }
    else
      NS_ASSERTION(PR_FALSE, "missing url or sink");

  // if we are set up as a channel, we should notify our channel listener that we are starting...
  // so pass in ourself as the channel and not the underlying socket or file channel the protocol
  // happens to be using
    if (m_channelListener) 
    {
        nsCOMPtr<nsIRequest> request = do_QueryInterface(m_mockChannel);
        if (!request) return NS_ERROR_FAILURE;

        rv = m_channelListener->OnStopRequest(request, m_channelContext, NS_OK);
    }
  m_lastActiveTime = PR_Now(); // ** jt -- is this the best place for time stamp
  SetFlag(IMAP_CLEAN_UP_URL_STATE);
  if (GetConnectionStatus() >= 0 && m_imapMiscellaneousSink && m_runningUrl)
  {
      m_imapMiscellaneousSink->CopyNextStreamMessage(this, m_runningUrl, GetServerStateParser().LastCommandSuccessful());
      WaitForFEEventCompletion();
  }

#ifdef DEBUG_bienvenu1
    mailnewsurl->GetSpec(getter_Copies(urlSpec));
    printf("end processing url %s\n", (const char *) urlSpec);
#endif
  // this is so hokey...we MUST clear any local references to the url 
  // BEFORE calling ReleaseUrlState
  mailnewsurl = nsnull;

  // release the url as we are done with it...
  ReleaseUrlState();
  ResetProgressInfo();
  m_urlInProgress = PR_FALSE;
  ClearFlag(IMAP_CLEAN_UP_URL_STATE);

  // now try queued urls, now that we've released this connection.
  if (m_imapServerSink && GetConnectionStatus() >= 0)
  {
    rv = m_imapServerSink->LoadNextQueuedUrl(&anotherUrlRun);
    SetFlag(IMAP_FIRST_PASS_IN_THREAD);
  }

  // if we didn't run another url, release the server sink to
  // cut circular refs.
  if (!anotherUrlRun)
      m_imapServerSink = nsnull;
  
  if (GetConnectionStatus() < 0 || !GetServerStateParser().Connected())
  {
    nsCOMPtr<nsIImapIncomingServer> imapServer  = do_QueryReferent(m_server, &rv);
    if (NS_SUCCEEDED(rv))
      imapServer->RemoveConnection(this);

    if (!DeathSignalReceived()) {
        TellThreadToDie(PR_FALSE);
    }
  }
  return anotherUrlRun;
}

// ignoreBadAndNOResponses --> don't throw a error dialog if this command results in a NO or Bad response
// from the server..in other words the command is "exploratory" and we don't really care if it succeeds or fails.
void nsImapProtocol::ParseIMAPandCheckForNewMail(const char* commandString, PRBool aIgnoreBadAndNOResponses)
{
    if (commandString)
        GetServerStateParser().ParseIMAPServerResponse(commandString, aIgnoreBadAndNOResponses);
    else
        GetServerStateParser().ParseIMAPServerResponse(m_currentCommand.get(), aIgnoreBadAndNOResponses);
    // **** fix me for new mail biff state *****
}


/////////////////////////////////////////////////////////////////////////////////////////////
// we suppport the nsIStreamListener interface 
////////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsImapProtocol::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *aIStream, PRUint32 aSourceOffset, PRUint32 aLength)
{
    nsresult res = NS_OK;

    if(NS_SUCCEEDED(res) && aLength > 0)
    {
    // make sure m_inputStream is set to the right input stream...
    if (!m_inputStream)
      m_inputStream = dont_QueryInterface(aIStream);

	if (TestFlag(IMAP_WAITING_FOR_DATA))
	{
        // if we received data, we need to signal the data available monitor...
	    // Read next line from input will actually read the data out of the stream
		ClearFlag(IMAP_WAITING_FOR_DATA); // we are no longer waiting for data
		PR_EnterMonitor(m_dataAvailableMonitor);
			PR_Notify(m_dataAvailableMonitor);
		PR_ExitMonitor(m_dataAvailableMonitor);
	}
  }

  return res;
}

NS_IMETHODIMP nsImapProtocol::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  // we used to change the url state here......but OnStartRequest only gets called
  // once....when the connnection is first build from necko...So we'll set the url
  // state in ProcessCurrentUrl instead. 
  //
  // If you actually add code back to this method, be careful as we ran
  // into some problems pumping events on the imap event queue. When this method
  // gets called, it is in response to an event getting placed by the socket thread.
  // If you turn around and call a method like WaitForFEEventCompletion which 
  // blocks until another event gets pumped on the imap event queue you will be
  // very sorry as we'll be blocked...

  return NS_OK;
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsImapProtocol::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult aStatus)
{

  PRBool killThread = PR_FALSE;

  if (NS_FAILED(aStatus)) 
  {
    switch (aStatus) 
    {
        case NS_ERROR_UNKNOWN_HOST:
            AlertUserEventUsingId(IMAP_UNKNOWN_HOST_ERROR);
            killThread = PR_TRUE;
            break;
        case NS_ERROR_CONNECTION_REFUSED:
            AlertUserEventUsingId(IMAP_CONNECTION_REFUSED_ERROR);
            killThread = PR_TRUE;
            break;
        case NS_ERROR_NET_TIMEOUT:
            AlertUserEventUsingId(IMAP_NET_TIMEOUT_ERROR);
            killThread = PR_TRUE;
            break;
        default:
            break;
    }

  }

  PR_CEnterMonitor(this);
  mAsyncReadRequest = nsnull; // don't need to cache this anymore, it's going away
  if (killThread == PR_TRUE) 
  {
    ClearFlag(IMAP_CONNECTION_IS_OPEN);
    TellThreadToDie(PR_FALSE);
  }
  m_channel = nsnull;
  m_outputStream = nsnull;
  m_inputStream = nsnull;
  PR_CExitMonitor(this);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// End of nsIStreamListenerSupport
//////////////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapProtocol::GetStreamConsumer (nsISupports **result)
{
    // mscott - i'm going to make this method obsolete...
    NS_NOTREACHED("nsImapProtocol::GetStreamConsumer");
    return NS_ERROR_NOT_IMPLEMENTED;
#if 0
  if (result)
  {
    *result = nsnull;
    NS_IF_ADDREF(*result);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
#endif
}

NS_IMETHODIMP
nsImapProtocol::GetRunningUrl(nsIURI **result)
{
    if (result && m_runningUrl)
        return m_runningUrl->QueryInterface(NS_GET_IID(nsIURI), (void**)
                                            result);
    else
        return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsImapProtocol::GetRunningImapURL(nsIImapUrl **aImapUrl)
{
    if (aImapUrl && m_runningUrl)
  {
        return m_runningUrl->QueryInterface(NS_GET_IID(nsIImapUrl), (void**) aImapUrl);
  }
  else
    return NS_ERROR_NULL_POINTER;

}

/*
 * Writes the data contained in dataBuffer into the current output stream. It also informs
 * the transport layer that this data is now available for transmission.
 * Returns a positive number for success, 0 for failure (not all the bytes were written to the
 * stream, etc). We need to make another pass through this file to install an error system (mscott)
 */

nsresult nsImapProtocol::SendData(const char * dataBuffer, PRBool aSuppressLogging)
{
  PRUint32 writeCount = 0; 
  nsresult rv = NS_ERROR_NULL_POINTER;

  NS_ENSURE_TRUE(m_channel, NS_ERROR_FAILURE);

  if (dataBuffer && m_outputStream)
  {
    m_currentCommand = dataBuffer;
    if (!aSuppressLogging)
      Log("SendData", nsnull, dataBuffer);
    else
      Log("SendData", nsnull, "Logging suppressed for this command (it probably contained authentication information)");
    rv = m_outputStream->Write(dataBuffer, PL_strlen(dataBuffer), &writeCount);
    if (NS_FAILED(rv))
    {
      // the connection died unexpectedly! so clear the open connection flag
      ClearFlag(IMAP_CONNECTION_IS_OPEN); 
      TellThreadToDie(PR_FALSE);
    }
  }

  return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Begin protocol state machine functions...
//////////////////////////////////////////////////////////////////////////////////////////////

// LoadUrl takes a url, initializes all of our url specific data by calling SetupUrl.
// If we don't have a connection yet, we open the connection. Finally, we signal the 
// url to run monitor to let the imap main thread loop process the current url (it is waiting
// on this monitor). There is a contract that the imap thread has already been started b4 we
// attempt to load a url....
nsresult nsImapProtocol::LoadUrl(nsIURI * aURL, nsISupports * aConsumer)
{
  nsresult rv = NS_OK;
  if (aURL)
  {
#ifdef DEBUG_bienvenu1
    nsXPIDLCString urlSpec;
    aURL->GetSpec(getter_Copies(urlSpec));
    printf("loading url %s\n", (const char *) urlSpec);
#endif
    m_urlInProgress = PR_TRUE;
    rv = SetupWithUrl(aURL, aConsumer); 
    NS_ASSERTION(NS_SUCCEEDED(rv), "error setting up imap url");
        if (NS_FAILED(rv)) return rv;
      SetupSinkProxy(); // generate proxies for all of the event sinks in the url
        m_lastActiveTime = PR_Now();
    if (m_channel && m_runningUrl)
    {
      nsImapAction imapAction;
      m_runningUrl->GetImapAction(&imapAction);

      // if we're running a select or delete all, do a noop first.
      // this should really be in the connection cache code when we know
      // we're pulling out a selected state connection, but maybe we
      // can get away with this.
      m_needNoop = (imapAction == nsIImapUrl::nsImapSelectFolder || imapAction == nsIImapUrl::nsImapDeleteAllMsgs);

      // We now have a url to run so signal the monitor for url ready to be processed...
      PR_EnterMonitor(m_urlReadyToRunMonitor);
      m_nextUrlReadyToRun = PR_TRUE;
      PR_Notify(m_urlReadyToRunMonitor);
      PR_ExitMonitor(m_urlReadyToRunMonitor);

    } // if we have an imap url and a transport
    else
      NS_ASSERTION(PR_FALSE, "missing channel or running url");

  } // if we received a url!

  return rv;
}

NS_IMETHODIMP nsImapProtocol::IsBusy(PRBool *aIsConnectionBusy,
                                     PRBool *isInboxConnection)
{
	if (!aIsConnectionBusy || !isInboxConnection)
		return NS_ERROR_NULL_POINTER;
  NS_LOCK_INSTANCE();
  nsresult rv = NS_OK;
  *aIsConnectionBusy = PR_FALSE;
  *isInboxConnection = PR_FALSE;
  if (!m_channel)
  {
    // ** jt -- something is really wrong kill the thread
    TellThreadToDie(PR_FALSE);
    rv = NS_ERROR_FAILURE;
  }
  else
  {
    if (m_urlInProgress) // do we have a url? That means we're working on it... 
      *aIsConnectionBusy = PR_TRUE;

    if (GetServerStateParser().GetIMAPstate() ==
        nsImapServerResponseParser::kFolderSelected && GetServerStateParser().GetSelectedMailboxName() && 
        PL_strcasecmp(GetServerStateParser().GetSelectedMailboxName(),
                      "Inbox") == 0)
      *isInboxConnection = PR_TRUE;
      
  }
  NS_UNLOCK_INSTANCE();
  return rv;
}

#define IS_SUBSCRIPTION_RELATED_ACTION(action) (action == nsIImapUrl::nsImapSubscribe\
|| action == nsIImapUrl::nsImapUnsubscribe || action == nsIImapUrl::nsImapDiscoverAllBoxesUrl || action == nsIImapUrl::nsImapListFolder)


// canRunUrl means the connection is not busy, and is in the selcted state
// for the desired folder (or authenticated).
// has to wait means it's in the right selected state, but busy.
NS_IMETHODIMP nsImapProtocol::CanHandleUrl(nsIImapUrl * aImapUrl, 
                                           PRBool * aCanRunUrl,
                                           PRBool * hasToWait)
{
  if (!aCanRunUrl || !hasToWait || !aImapUrl)
    return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  NS_LOCK_INSTANCE();

  *aCanRunUrl = PR_FALSE; // assume guilty until proven otherwise...
  *hasToWait = PR_FALSE;

  PRBool isBusy = PR_FALSE;
  PRBool isInboxConnection = PR_FALSE;

  if (!m_channel)
  {
    // *** jt -- something is really wrong; it could be the dialer gave up
    // the connection or ip binding has been release by the operating
    // system; tell thread to die and return error failure
    TellThreadToDie(PR_FALSE);
    rv =  NS_ERROR_FAILURE;
  }
  else
  {
    IsBusy(&isBusy, &isInboxConnection);
    
    PRBool inSelectedState = GetServerStateParser().GetIMAPstate() ==
        nsImapServerResponseParser::kFolderSelected;
    
    nsCString curUrlFolderName;
    if (inSelectedState)
    {
        curUrlFolderName = 
            GetServerStateParser().GetSelectedMailboxName();
    }
    else if (isBusy)
    {
        nsImapState curUrlImapState;
		NS_ASSERTION(m_runningUrl,"isBusy, but no running url.");
		if (m_runningUrl) {
			m_runningUrl->GetRequiredImapState(&curUrlImapState);
			if (curUrlImapState == nsIImapUrl::nsImapSelectedState) {
				char *folderName = OnCreateServerSourceFolderPathString();
        curUrlFolderName.Assign(folderName);
				inSelectedState = PR_TRUE;
        PR_FREEIF(folderName);
			}
        }
    }

    nsImapState imapState;
    nsImapAction actionForProposedUrl;
    aImapUrl->GetImapAction(&actionForProposedUrl);
    aImapUrl->GetRequiredImapState(&imapState);

    // OK, this is a bit of a hack - we're going to pretend that
    // a delete or rename folder url requires a selected state connection on
    // the folder to be deleted. This isn't technically true,
    // but we would much rather use that connection for several reasons,
    // one is that some UW servers require us to use that connection
    // the other is that we don't want to leave a connection dangling in
    // the selected state for the deleted folder.
    // If we don't find a connection in that selected state,
    // we'll fall back to the first free connection.
    PRBool isSelectedStateUrl = imapState == nsIImapUrl::nsImapSelectedState 
      || actionForProposedUrl == nsIImapUrl::nsImapDeleteFolder || actionForProposedUrl == nsIImapUrl::nsImapRenameFolder
      || actionForProposedUrl == nsIImapUrl::nsImapMoveFolderHierarchy;
    
    nsCOMPtr<nsIMsgMailNewsUrl> msgUrl = do_QueryInterface(aImapUrl);
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = msgUrl->GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv))
    {
      // compare host/user between url and connection.
      char * urlHostName = nsnull;
      char * urlUserName = nsnull;
      rv = server->GetHostName(&urlHostName);
      if (NS_FAILED(rv)) return rv;
      rv = server->GetUsername(&urlUserName);
      if (NS_FAILED(rv)) return rv;
      if ((!GetImapHostName() || 
           PL_strcasecmp(urlHostName, GetImapHostName()) == 0) &&
          (!GetImapUserName() || 
           PL_strcasecmp(urlUserName, GetImapUserName()) == 0))
      {
          if (isSelectedStateUrl)
          {
              if (inSelectedState)
              {
                  // *** jt - in selected state can only run url with
                  // matching foldername
                  char *folderNameForProposedUrl = nsnull;
                  rv = aImapUrl->CreateServerSourceFolderPathString(
                      &folderNameForProposedUrl);
                  if (NS_SUCCEEDED(rv) && folderNameForProposedUrl)
                  {
                      PRBool isInbox = 
                          PL_strcasecmp("Inbox", folderNameForProposedUrl) == 0;
                      if (curUrlFolderName.Length() > 0)
                      {
                          PRBool matched = isInbox ?
                              PL_strcasecmp(curUrlFolderName.get(),
                                            folderNameForProposedUrl) == 0 : 
                              PL_strcmp(curUrlFolderName.get(),
                                        folderNameForProposedUrl) == 0;
                          if (matched)
                          {
                            if (isBusy)
                                *hasToWait = PR_TRUE;
                            else
                                *aCanRunUrl = PR_TRUE;
                          }
                      }
                  }
#ifdef DEBUG_bienvenu1
                  printf("proposed url = %s folder for connection %s has To Wait = %s can run = %s\n",
                    folderNameForProposedUrl, curUrlFolderName.get(),
                    (*hasToWait) ? "TRUE" : "FALSE", (*aCanRunUrl) ? "TRUE" : "FALSE");
#endif
                  PR_FREEIF(folderNameForProposedUrl);
              }
          }
          else // *** jt - an authenticated state url can be run in either
              // authenticated or selected state
          {
            nsImapAction actionForRunningUrl;

            // If proposed url is subscription related, and we are currently running
            // a subscription url, then we want to queue the proposed url after the current url.
            // Otherwise, we can run this url if we're not busy.
            // If we never find a running subscription-related url, the caller will
            // just use whatever free connection it can find, which is what we want.
            if (IS_SUBSCRIPTION_RELATED_ACTION(actionForProposedUrl))
            {
              if (isBusy && m_runningUrl)
              {
                m_runningUrl->GetImapAction(&actionForRunningUrl);
                if (IS_SUBSCRIPTION_RELATED_ACTION(actionForRunningUrl))
                {
                  *aCanRunUrl = PR_FALSE;
                  *hasToWait = PR_TRUE;
                }
              }
            }
            else
            {
              if (!isBusy)
                  *aCanRunUrl = PR_TRUE;
            }
          }
          
          PR_FREEIF(urlHostName);
          PR_FREEIF(urlUserName);
      }
    }
  }
  NS_UNLOCK_INSTANCE();
  return rv;
}


// Command tag handling stuff
void nsImapProtocol::IncrementCommandTagNumber()
{
    sprintf(m_currentServerCommandTag,"%ld", (long) ++m_currentServerCommandTagNumber);
}

char *nsImapProtocol::GetServerCommandTag()
{
    return m_currentServerCommandTag;
}

void nsImapProtocol::ProcessSelectedStateURL()
{
  nsXPIDLCString mailboxName;
  PRBool          bMessageIdsAreUids = PR_TRUE;
  imapMessageFlagsType  msgFlags = 0;
  const char          *hostName = GetImapHostName();
  nsCString       urlHost;

  // this can't fail, can it?
  nsresult res;
  res = m_runningUrl->GetImapAction(&m_imapAction);
  m_runningUrl->MessageIdsAreUids(&bMessageIdsAreUids);
  m_runningUrl->GetMsgFlags(&msgFlags);

  res = CreateServerSourceFolderPathString(getter_Copies(mailboxName));

    if (NS_SUCCEEDED(res) && !DeathSignalReceived())
    {
    // OK, code here used to check explicitly for multiple connections to the inbox,
    // but the connection pool stuff should handle this now.
      PRBool selectIssued = PR_FALSE;
        if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
        {
            if (GetServerStateParser().GetSelectedMailboxName() && 
                PL_strcmp(GetServerStateParser().GetSelectedMailboxName(),
                          mailboxName))
            {       // we are selected in another folder
              if (m_closeNeededBeforeSelect)
                  Close();
                if (GetServerStateParser().LastCommandSuccessful()) 
                {
                  selectIssued = PR_TRUE;
                  AutoSubscribeToMailboxIfNecessary(mailboxName);
                  SelectMailbox(mailboxName);
                }
            }
            else if (!GetServerStateParser().GetSelectedMailboxName())
            {       // why are we in the selected state with no box name?
                SelectMailbox(mailboxName);
                selectIssued = PR_TRUE;
            }
            else
            {
              // get new message counts, if any, from server
        if (m_needNoop)
        {
          m_noopCount++;
          if (gPromoteNoopToCheckCount > 0 && (m_noopCount % gPromoteNoopToCheckCount) == 0)
            Check();
          else
            Noop(); // I think this is needed when we're using a cached connection
          m_needNoop = PR_FALSE;
        }
            }
        }
        else
        {
            // go to selected state
            AutoSubscribeToMailboxIfNecessary(mailboxName);
            SelectMailbox(mailboxName);
            selectIssued = PR_TRUE;
        }

    if (selectIssued)
    {
      RefreshACLForFolderIfNecessary(mailboxName);
    }
        
        PRBool uidValidityOk = PR_TRUE;
        if (GetServerStateParser().LastCommandSuccessful() && selectIssued && 
           (m_imapAction != nsIImapUrl::nsImapSelectFolder) && (m_imapAction != nsIImapUrl::nsImapLiteSelectFolder))
        {
          uid_validity_info *uidStruct = (uid_validity_info *) PR_Malloc(sizeof(uid_validity_info));
          if (uidStruct)
          {
            uidStruct->returnValidity = kUidUnknown;
            uidStruct->hostName = hostName;
            m_runningUrl->CreateCanonicalSourceFolderPathString(&uidStruct->canonical_boxname);
        if (m_imapMiscellaneousSink)
        {
          m_imapMiscellaneousSink->GetStoredUIDValidity(this, uidStruct);

              WaitForFEEventCompletion();
              
              // error on the side of caution, if the fe event fails to set uidStruct->returnValidity, then assume that UIDVALIDITY
              // did not role.  This is a common case event for attachments that are fetched within a browser context.
          if (!DeathSignalReceived())
            uidValidityOk = (uidStruct->returnValidity == kUidUnknown) || (uidStruct->returnValidity == GetServerStateParser().FolderUID());
          }
          else
              HandleMemoryFailure();
          
          PR_FREEIF(uidStruct->canonical_boxname);
          PR_Free(uidStruct);
          }
          else
            HandleMemoryFailure();
        }
            
    if (GetServerStateParser().LastCommandSuccessful() && !DeathSignalReceived() && (uidValidityOk || m_imapAction == nsIImapUrl::nsImapDeleteAllMsgs))
    {

      if (GetServerStateParser().CurrentFolderReadOnly() && 
        (m_imapAction == nsIImapUrl::nsImapExpungeFolder || m_imapAction == nsIImapUrl::nsImapDeleteMsg ||
         m_imapAction == nsIImapUrl::nsImapDeleteAllMsgs || m_imapAction == nsIImapUrl::nsImapAddMsgFlags ||
         m_imapAction == nsIImapUrl::nsImapSubtractMsgFlags))
        return;

      switch (m_imapAction)
      {
      case nsIImapUrl::nsImapLiteSelectFolder:
        if (GetServerStateParser().LastCommandSuccessful() && m_imapMiscellaneousSink)
        {
          m_imapMiscellaneousSink->LiteSelectUIDValidity(this, GetServerStateParser().FolderUID());

          WaitForFEEventCompletion();
          // need to update the mailbox count - is this a good place?
          ProcessMailboxUpdate(PR_FALSE); // handle uidvalidity change
        }
        break;
        case nsIImapUrl::nsImapSaveMessageToDisk:
        case nsIImapUrl::nsImapMsgFetch:
        case nsIImapUrl::nsImapMsgDownloadForOffline:
            {
              nsXPIDLCString messageIdString;
              m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
              // we dont want to send the flags back in a group
              // GetServerStateParser().ResetFlagInfo(0);
              if (HandlingMultipleMessages(messageIdString) || m_imapAction == nsIImapUrl::nsImapMsgDownloadForOffline)
              {
                // multiple messages, fetch them all
                SetProgressString(IMAP_FOLDER_RECEIVING_MESSAGE_OF);
          
                m_progressIndex = 0;
                m_progressCount = CountMessagesInIdString(messageIdString);
          
                FetchMessage(messageIdString, 
                             kEveryThingRFC822Peek,
                             bMessageIdsAreUids);
                SetProgressString(0);
               }
          else
          {
            // A single message ID

            // First, let's see if we're requesting a specific MIME part
            char *imappart = nsnull;
            m_runningUrl->GetImapPartToFetch(&imappart);
            if (imappart)
            {
              if (bMessageIdsAreUids)
              {
                // We actually want a specific MIME part of the message.
                // The Body Shell will generate it, even though we haven't downloaded it yet.

                IMAP_ContentModifiedType modType = IMAP_CONTENT_MODIFIED_VIEW_INLINE ;

                nsIMAPBodyShell *foundShell = nsnull;
                res = m_hostSessionList->FindShellInCacheForHost(GetImapServerKey(),
                    GetServerStateParser().GetSelectedMailboxName(), messageIdString, modType, &foundShell);
                if (!foundShell)
                {
                  // The shell wasn't in the cache.  Deal with this case later.
                  Log("SHELL",NULL,"Loading part, shell not found in cache!");
                  //PR_LOG(IMAP, out, ("BODYSHELL: Loading part, shell not found in cache!"));
                  // The parser will extract the part number from the current URL.
                  SetContentModified(modType);
                  Bodystructure(messageIdString, bMessageIdsAreUids);
                }
                else
                {
                  Log("SHELL", NULL, "Loading Part, using cached shell.");
                  //PR_LOG(IMAP, out, ("BODYSHELL: Loading part, using cached shell."));
                  SetContentModified(modType);
                  foundShell->SetConnection(this);
                  GetServerStateParser().UseCachedShell(foundShell);
                  foundShell->Generate(imappart);
                  GetServerStateParser().UseCachedShell(NULL);
                }
              }
              else
              {
                // Message IDs are not UIDs.
                NS_ASSERTION(PR_FALSE, "message ids aren't uids");
              }
              PR_Free(imappart);
            }
            else
            {
              // downloading a single message: try to do it by bodystructure, and/or do it by chunks
              PRUint32 messageSize = GetMessageSize(messageIdString, bMessageIdsAreUids);
              // We need to check the format_out bits to see if we are allowed to leave out parts,
              // or if we are required to get the whole thing.  Some instances where we are allowed
              // to do it by parts:  when viewing a message, replying to a message, or viewing its source
              // Some times when we're NOT allowed:  when forwarding a message, saving it, moving it, etc.
              // need to set a flag in the url, I guess, equiv to allow_content_changed.
              PRBool allowedToBreakApart = PR_TRUE; // (ce  && !DeathSignalReceived()) ? ce->URL_s->allow_content_change : PR_FALSE;
              PRBool mimePartSelectorDetected;
              PRBool urlOKToFetchByParts = PR_FALSE;
              m_runningUrl->GetMimePartSelectorDetected(&mimePartSelectorDetected);
              m_runningUrl->GetFetchPartsOnDemand(&urlOKToFetchByParts);

              if (urlOKToFetchByParts &&
                allowedToBreakApart && 
                !GetShouldFetchAllParts() &&
                GetServerStateParser().ServerHasIMAP4Rev1Capability() /* &&
                  !mimePartSelectorDetected */)  // if a ?part=, don't do BS.
              {
                // OK, we're doing bodystructure

                // Before fetching the bodystructure, let's check our body shell cache to see if
                // we already have it around.
                nsIMAPBodyShell *foundShell = NULL;
                IMAP_ContentModifiedType modType = IMAP_CONTENT_MODIFIED_VIEW_INLINE;

                nsCOMPtr<nsIMsgMailNewsUrl> mailurl = do_QueryInterface(m_runningUrl);
                if (mailurl)
                  mailurl->SetAddToMemoryCache(PR_FALSE);
                SetContentModified(modType);  // This will be looked at by the cache
                if (bMessageIdsAreUids)
                {
                  res = m_hostSessionList->FindShellInCacheForHost(GetImapServerKey(),
                    GetServerStateParser().GetSelectedMailboxName(), messageIdString, modType, &foundShell);
                  if (foundShell)
                  {
                    Log("SHELL",NULL,"Loading message, using cached shell.");
                    //PR_LOG(IMAP, out, ("BODYSHELL: Loading message, using cached shell."));
                    foundShell->SetConnection(this);
                    GetServerStateParser().UseCachedShell(foundShell);
                    foundShell->Generate(NULL);
                    GetServerStateParser().UseCachedShell(NULL);
                  }
                }

                if (!foundShell)
                  Bodystructure(messageIdString, bMessageIdsAreUids);
              }
              else
              {
                // Not doing bodystructure.  Fetch the whole thing, and try to do
                // it in chunks.
                SetContentModified(IMAP_CONTENT_NOT_MODIFIED);
                FetchTryChunking(messageIdString, kEveryThingRFC822,
                  bMessageIdsAreUids, NULL, messageSize, PR_TRUE);
              }
            }
          }
            }
      break;
      case nsIImapUrl::nsImapExpungeFolder:
        Expunge();
        // note fall through to next cases.
      case nsIImapUrl::nsImapSelectFolder:
      case nsIImapUrl::nsImapSelectNoopFolder:
           ProcessMailboxUpdate(PR_TRUE);
           break;
      case nsIImapUrl::nsImapMsgHeader:
        {
          nsXPIDLCString messageIds;
          m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIds));

           // we don't want to send the flags back in a group
           //        GetServerStateParser().ResetFlagInfo(0);
          FetchMessage(messageIds, 
                       kHeadersRFC822andUid,
                       bMessageIdsAreUids);
        }
        break;
      case nsIImapUrl::nsImapSearch:
        {
            nsXPIDLCString searchCriteriaString;
            m_runningUrl->CreateSearchCriteriaString(getter_Copies(searchCriteriaString));
            Search(searchCriteriaString, bMessageIdsAreUids);
                // drop the results on the floor for now
        }
        break;
      case nsIImapUrl::nsImapDeleteMsg:
        {
          nsXPIDLCString messageIdString;
    
          m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
          if (HandlingMultipleMessages(messageIdString))
            ProgressEventFunctionUsingId (IMAP_DELETING_MESSAGES);
          else
            ProgressEventFunctionUsingId(IMAP_DELETING_MESSAGE);
          
          Store(messageIdString, "+FLAGS (\\Deleted)",  bMessageIdsAreUids);
          
          if (GetServerStateParser().LastCommandSuccessful())
          {
              //delete_message_struct *deleteMsg = (delete_message_struct *) PR_Malloc (sizeof(delete_message_struct));
            // convert name back from utf7
            utf_name_struct *nameStruct = (utf_name_struct *) PR_Malloc(sizeof(utf_name_struct));
            char *canonicalName = NULL;
            if (nameStruct)
            {
              const char *selectedMailboxName = GetServerStateParser().GetSelectedMailboxName();
              if (selectedMailboxName)
              {
                m_runningUrl->AllocateCanonicalPath(selectedMailboxName, 
                        kOnlineHierarchySeparatorUnknown, &canonicalName);
              }
            }

            if (m_imapMessageSink)
                m_imapMessageSink->NotifyMessageDeleted(canonicalName, PR_FALSE, messageIdString);
                // notice we don't wait for this to finish...
          }
          else
              HandleMemoryFailure();
        }
        break;
      case nsIImapUrl::nsImapDeleteAllMsgs:
        {
          uint32 numberOfMessages = GetServerStateParser().NumberOfMessages();
          if (numberOfMessages)
          {
                  
            Store("1:*", "+FLAGS.SILENT (\\Deleted)",
                            PR_FALSE);  // use sequence #'s  
              
            if (GetServerStateParser().LastCommandSuccessful())
              Expunge();      // expunge messages with deleted flag
            if (GetServerStateParser().LastCommandSuccessful())
            {
              // convert name back from utf7
              utf_name_struct *nameStruct = (utf_name_struct *) PR_Malloc(sizeof(utf_name_struct));
              char *canonicalName = NULL;
              if (nameStruct)
              {
                const char *selectedMailboxName = GetServerStateParser().GetSelectedMailboxName();
                if (selectedMailboxName )
                {
                  m_runningUrl->AllocateCanonicalPath(selectedMailboxName,
                    kOnlineHierarchySeparatorUnknown, &canonicalName);
                }
              }

              if (m_imapMessageSink)
                  m_imapMessageSink->NotifyMessageDeleted(canonicalName, PR_TRUE, nsnull);
            }
              
          }
          DeleteSubFolders(mailboxName);
        }
        break;
      case nsIImapUrl::nsImapAppendDraftFromFile:
        {
          OnAppendMsgFromFile();
        }
        break;
      case nsIImapUrl::nsImapAddMsgFlags:
        {
          nsXPIDLCString messageIdString;
          m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
                
          ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                            msgFlags, PR_TRUE);
        
              /*
              if ( !DeathSignalReceived() &&
                  GetServerStateParser().Connected() &&
                   !GetServerStateParser().SyntaxError())
              {
                  //if (msgFlags & kImapMsgDeletedFlag)
                  //    Expunge();      // expunge messages with deleted flag
                  Check();        // flush servers flag state
              }
              */
        }
        break;
      case nsIImapUrl::nsImapSubtractMsgFlags:
        {
          nsXPIDLCString messageIdString;
          m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
              
          ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                            msgFlags, PR_FALSE);
            
        }
        break;
      case nsIImapUrl::nsImapSetMsgFlags:
        {
            nsXPIDLCString messageIdString;
            m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
                  
            ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                              msgFlags, PR_TRUE);
            ProcessStoreFlags(messageIdString, bMessageIdsAreUids,
                              ~msgFlags, PR_FALSE);
        }
        break;
      case nsIImapUrl::nsImapBiff:
          PeriodicBiff(); 
          break;
      case nsIImapUrl::nsImapOnlineCopy:
      case nsIImapUrl::nsImapOnlineMove:
          {
            nsXPIDLCString messageIdString;
            m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
            char *destinationMailbox = OnCreateServerDestinationFolderPathString();

            if (destinationMailbox)
            {
              if (m_imapAction == nsIImapUrl::nsImapOnlineMove) 
              {
                if (HandlingMultipleMessages(messageIdString))
                  ProgressEventFunctionUsingIdWithString (IMAP_MOVING_MESSAGES_TO, destinationMailbox);
                else
                  ProgressEventFunctionUsingIdWithString (IMAP_MOVING_MESSAGE_TO, destinationMailbox); 
              }
              else {
                if (HandlingMultipleMessages(messageIdString))
                  ProgressEventFunctionUsingIdWithString (IMAP_COPYING_MESSAGES_TO, destinationMailbox);
                else
                  ProgressEventFunctionUsingIdWithString (IMAP_COPYING_MESSAGE_TO, destinationMailbox); 
              }

              Copy(messageIdString, destinationMailbox, bMessageIdsAreUids);
              PR_FREEIF( destinationMailbox);
              ImapOnlineCopyState copyState;
              if (DeathSignalReceived())
                copyState = ImapOnlineCopyStateType::kInterruptedState;
              else
                copyState = GetServerStateParser().LastCommandSuccessful() ? 
                                (ImapOnlineCopyState) ImapOnlineCopyStateType::kSuccessfulCopy : 
              (ImapOnlineCopyState) ImapOnlineCopyStateType::kFailedCopy;
              if (m_imapMailFolderSink)
                m_imapMailFolderSink->OnlineCopyCompleted(this, copyState);
                    
           if (GetServerStateParser().LastCommandSuccessful() &&
                        (m_imapAction == nsIImapUrl::nsImapOnlineMove))
           {
             Store(messageIdString, "+FLAGS (\\Deleted)",
                            bMessageIdsAreUids); 
             PRBool storeSuccessful = GetServerStateParser().LastCommandSuccessful();
                          
              if (m_imapMailFolderSink)
			       {
               copyState = storeSuccessful ? (ImapOnlineCopyState) ImapOnlineCopyStateType::kSuccessfulDelete 
                : (ImapOnlineCopyState) ImapOnlineCopyStateType::kFailedDelete;
               m_imapMailFolderSink->OnlineCopyCompleted(this, copyState);
			       }
           }
         }
         else
           HandleMemoryFailure();
      }
      break;
    case nsIImapUrl::nsImapOnlineToOfflineCopy:
    case nsIImapUrl::nsImapOnlineToOfflineMove:
      {
        nsXPIDLCString messageIdString;
        nsresult rv = m_runningUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
        if (NS_SUCCEEDED(rv))
        {
          SetProgressString(IMAP_FOLDER_RECEIVING_MESSAGE_OF);
          m_progressIndex = 0;
          m_progressCount = CountMessagesInIdString(messageIdString);
          
          FetchMessage(messageIdString, 
                                 kEveryThingRFC822Peek,
                                 bMessageIdsAreUids);
                      
          SetProgressString(0);
					if (m_imapMailFolderSink)
					{
						ImapOnlineCopyState copyStatus;
						if (GetServerStateParser().LastCommandSuccessful())
							copyStatus = ImapOnlineCopyStateType::kSuccessfulCopy;
						else
							copyStatus = ImapOnlineCopyStateType::kFailedCopy;
					  m_imapMailFolderSink->OnlineCopyCompleted(this, copyStatus);
                      if (GetServerStateParser().LastCommandSuccessful() &&
                        (m_imapAction == nsIImapUrl::nsImapOnlineToOfflineMove))
					  {
              Store(messageIdString, "+FLAGS (\\Deleted)",bMessageIdsAreUids); 
              if (GetServerStateParser().LastCommandSuccessful())
							 copyStatus = ImapOnlineCopyStateType::kSuccessfulDelete;
						  else
							 copyStatus = ImapOnlineCopyStateType::kFailedDelete;

               m_imapMailFolderSink->OnlineCopyCompleted(this,  copyStatus);
					  }
					}
        }
        else
         HandleMemoryFailure();
      }
      
      default:
        if (GetServerStateParser().LastCommandSuccessful() && !uidValidityOk)
              ProcessMailboxUpdate(PR_FALSE); // handle uidvalidity change
        break;
    }
   }
  }
  else if (!DeathSignalReceived())
    HandleMemoryFailure();
}

void nsImapProtocol::AutoSubscribeToMailboxIfNecessary(const char *mailboxName)
{
#ifdef HAVE_PORT
  if (m_folderNeedsSubscribing)  // we don't know about this folder - we need to subscribe to it / list it.
  {
    fHierarchyNameState = kListingForInfoOnly;
    List(mailboxName, PR_FALSE);
    fHierarchyNameState = kNoOperationInProgress;

    // removing and freeing it as we go.
    TIMAPMailboxInfo *mb = NULL;
    int total = XP_ListCount(fListedMailboxList);
    do
    {
      mb = (TIMAPMailboxInfo *) XP_ListRemoveTopObject(fListedMailboxList);
      delete mb;
    } while (mb);

    // if the mailbox exists (it was returned from the LIST response)
    if (total > 0)
    {
      // Subscribe to it, if the pref says so
      if (TIMAPHostInfo::GetHostIsUsingSubscription(fCurrentUrl->GetUrlHost()) && m_autoSubscribeOnOpen)
      {
        XP_Bool lastReportingErrors = GetServerStateParser().GetReportingErrors();
        GetServerStateParser().SetReportingErrors(PR_FALSE);
        Subscribe(mailboxName);
        GetServerStateParser().SetReportingErrors(lastReportingErrors);
      }

      // Always LIST it anyway, to get it into the folder lists,
      // so that we can continue to perform operations on it, at least
      // for this session.
      fHierarchyNameState = kNoOperationInProgress;
      List(mailboxName, PR_FALSE);
    }

    // We should now be subscribed to it, and have it in our folder lists
    // and panes.  Even if something failed, we don't want to try this again.
    m_folderNeedsSubscribing = PR_FALSE;

  }
#endif
}


nsresult nsImapProtocol::BeginMessageDownLoad(
    PRUint32 total_message_size, // for user, headers and body
    const char *content_type)
{
  nsresult rv = NS_OK;
  char *sizeString = PR_smprintf("OPEN Size: %ld", total_message_size);
  Log("STREAM",sizeString,"Begin Message Download Stream");
  PR_FREEIF(sizeString);
    //total_message_size)); 
  if (content_type)
  {
    if (GetServerStateParser().GetDownloadingHeaders())
    {
      if (m_imapMailFolderSink)
      {
        m_imapMailFolderSink->SetupHeaderParseStream(this, total_message_size, content_type, nsnull);
      }
    }

    // if we have a mock channel, that means we have a channel listener who wants the
    // message. So set up a pipe. We'll write the messsage into one end of the pipe
    // and they will read it out of the other end.
    else if (m_channelListener)
    {
     // create a pipe to pump the message into...the output will go to whoever
     // is consuming the message display
        rv = NS_NewPipe(getter_AddRefs(m_channelInputStream), getter_AddRefs(m_channelOutputStream));
        NS_ASSERTION(NS_SUCCEEDED(rv), "NS_NewPipe failed!");
    }
    // else, if we are saving the message to disk!
    else if (m_imapMessageSink /* && m_imapAction == nsIImapUrl::nsImapSaveMessageToDisk */) 
    {
        nsCOMPtr<nsIFileSpec> fileSpec;
        PRBool addDummyEnvelope = PR_TRUE;
        nsCOMPtr<nsIMsgMessageUrl> msgurl = do_QueryInterface(m_runningUrl);
        msgurl->GetMessageFile(getter_AddRefs(fileSpec));
        msgurl->GetAddDummyEnvelope(&addDummyEnvelope);
//                m_imapMessageSink->SetupMsgWriteStream(fileSpec, addDummyEnvelope);
        nsXPIDLCString nativePath;
//        NS_ASSERTION(fileSpec, "no fileSpec!");
        if (fileSpec) 
	      {
           fileSpec->GetNativePath(getter_Copies(nativePath));
           rv = m_imapMessageSink->SetupMsgWriteStream(nativePath, addDummyEnvelope);
	       }
	}
	if (m_imapMailFolderSink && m_runningUrl)
	{
    nsCOMPtr<nsIMsgMailNewsUrl> mailurl = do_QueryInterface(m_runningUrl);
    m_imapMailFolderSink->StartMessage(mailurl);
  }
  
  }
  else
    HandleMemoryFailure();
  return rv;
}

void
nsImapProtocol::GetShouldDownloadArbitraryHeaders(PRBool *aResult)
{
  if (m_imapServerSink)
    m_imapServerSink->GetShouldDownloadArbitraryHeaders(aResult);
}

void
nsImapProtocol::GetArbitraryHeadersToDownload(char **aResult)
{
  if (m_imapServerSink)
    m_imapServerSink->GetArbitraryHeaders(aResult);
}

static void PRTime2Seconds(PRTime prTime, PRInt32 *seconds)
{
	PRInt64 microSecondsPerSecond, intermediateResult;
	
	LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
	LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2I((*seconds), intermediateResult);
}

void
nsImapProtocol::AdjustChunkSize()
{
  PRTime deltaTime;
  PRInt32 deltaInSeconds;

  m_endTime = PR_Now();
  LL_SUB(deltaTime, m_endTime, m_startTime);
  PRTime2Seconds(deltaTime, &deltaInSeconds);
  m_trackingTime = PR_FALSE;
  if (deltaInSeconds < 0)
    return;             // bogus for some reason
  
  if (deltaInSeconds <= m_tooFastTime) {
    m_chunkSize += m_chunkAddSize;
    m_chunkThreshold = m_chunkSize + (m_chunkSize / 2);
    if (m_chunkSize > m_maxChunkSize)
      m_chunkSize = m_maxChunkSize;
  }
  else if (deltaInSeconds <= m_idealTime)
    return;
  else {
    if (m_chunkSize > m_chunkStartSize)
      m_chunkSize = m_chunkStartSize;
    else if (m_chunkSize > (m_chunkAddSize * 2))
      m_chunkSize -= m_chunkAddSize;
    m_chunkThreshold = m_chunkSize + (m_chunkSize / 2);
  }
}

// authenticated state commands 
// escape any backslashes or quotes.  Backslashes are used a lot with our NT server
char *nsImapProtocol::CreateEscapedMailboxName(const char *rawName)
{
  nsCString escapedName(rawName);

  for (PRInt32 strIndex = 0; *rawName; strIndex++)
  {
    char currentChar = *rawName++;
    if ((currentChar == '\\') || (currentChar == '\"'))
    {
      escapedName.Insert('\\', strIndex++);
    }
  }
  return ToNewCString(escapedName);
}

void nsImapProtocol::SelectMailbox(const char *mailboxName)
{
    ProgressEventFunctionUsingId (IMAP_STATUS_SELECTING_MAILBOX);
    IncrementCommandTagNumber();
    
    m_closeNeededBeforeSelect = PR_FALSE;   // initial value
  GetServerStateParser().ResetFlagInfo(0);    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    nsCString commandBuffer(GetServerCommandTag());
  commandBuffer.Append(" select \"");
  commandBuffer.Append(escapedName);
  commandBuffer.Append("\"" CRLF);

  nsMemory::Free(escapedName);
  nsresult res;       
  res = SendData(commandBuffer.get());
  if (NS_FAILED(res)) return;
  ParseIMAPandCheckForNewMail();

  PRInt32 numOfMessagesInFlagState = 0;
  nsImapAction imapAction; 
  m_flagState->GetNumberOfMessages(&numOfMessagesInFlagState);
  res = m_runningUrl->GetImapAction(&imapAction);
  // if we've selected a mailbox, and we're not going to do an update because of the
  // url type, but don't have the flags, go get them!
  if (NS_SUCCEEDED(res) &&
    imapAction != nsIImapUrl::nsImapSelectFolder && imapAction != nsIImapUrl::nsImapExpungeFolder 
    && imapAction != nsIImapUrl::nsImapLiteSelectFolder &&
    imapAction != nsIImapUrl::nsImapDeleteAllMsgs && 
    ((GetServerStateParser().NumberOfMessages() != numOfMessagesInFlagState) && (numOfMessagesInFlagState == 0))) 
  {
      ProcessMailboxUpdate(PR_FALSE); 
  }
}

// Please call only with a single message ID
void nsImapProtocol::Bodystructure(const char *messageId, PRBool idIsUid)
{
  IncrementCommandTagNumber();
  
  nsCString commandString(GetServerCommandTag());
  if (idIsUid)
    commandString.Append(" UID");
  commandString.Append(" fetch ");

  commandString.Append(messageId);
  commandString.Append(" (BODYSTRUCTURE)" CRLF);
            
  nsresult rv = SendData(commandString.get());
  if (NS_SUCCEEDED(rv))
      ParseIMAPandCheckForNewMail(commandString.get());
}

void nsImapProtocol::PipelinedFetchMessageParts(const char *uid, nsIMAPMessagePartIDArray *parts)
{
  // assumes no chunking

  // build up a string to fetch
  nsCString stringToFetch, what;
  int32 currentPartNum = 0;
  while ((parts->GetNumParts() > currentPartNum) && !DeathSignalReceived())
  {
    nsIMAPMessagePartID *currentPart = parts->GetPart(currentPartNum);
    if (currentPart)
    {
      // Do things here depending on the type of message part
      // Append it to the fetch string
      if (currentPartNum > 0)
        stringToFetch.Append(" ");

      switch (currentPart->GetFields())
      {
      case kMIMEHeader:
        what = "BODY[";
        what.Append(currentPart->GetPartNumberString());
        what.Append(".MIME]");
        stringToFetch.Append(what);
        break;
      case kRFC822HeadersOnly:
        if (currentPart->GetPartNumberString())
        {
          what = "BODY[";
          what.Append(currentPart->GetPartNumberString());
          what.Append(".HEADER]");
          stringToFetch.Append(what);
        }
        else
        {
          // headers for the top-level message
          stringToFetch.Append("BODY[HEADER]");
        }
        break;
      default:
        NS_ASSERTION(PR_FALSE, "we should only be pipelining MIME headers and Message headers");
        break;
      }

    }
    currentPartNum++;
  }

  // Run the single, pipelined fetch command
  if ((parts->GetNumParts() > 0) && !DeathSignalReceived() && !GetPseudoInterrupted() && stringToFetch.get())
  {
      IncrementCommandTagNumber();

    nsCString commandString(GetServerCommandTag()); 
    commandString.Append(" UID fetch ");
    commandString.Append(uid, 10);
    commandString.Append(" (");
    commandString.Append(stringToFetch);
    commandString.Append(")" CRLF);
    nsresult rv = SendData(commandString.get());
        if (NS_SUCCEEDED(rv))
            ParseIMAPandCheckForNewMail(commandString.get());
  }
}




// this routine is used to fetch a message or messages, or headers for a
// message...

void
nsImapProtocol::FetchMessage(const char * messageIds, 
                             nsIMAPeFetchFields whatToFetch,
                             PRBool idIsUid,
                             PRUint32 startByte, PRUint32 endByte,
                             char *part)
{
    IncrementCommandTagNumber();
    
    nsCString commandString;
    if (idIsUid)
      commandString = "%s UID fetch";
    else
      commandString = "%s fetch";
    
    switch (whatToFetch) {
        case kEveryThingRFC822:
          GetServerStateParser().SetFetchingEverythingRFC822(PR_TRUE);
          if (m_trackingTime)
            AdjustChunkSize();      // we started another segment
          m_startTime = PR_Now();     // save start of download time
          m_trackingTime = PR_TRUE;
          if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
          {
            if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
              commandString.Append(" %s (XSENDER UID RFC822.SIZE BODY[]");
            else
              commandString.Append(" %s (UID RFC822.SIZE BODY[]");
          }
          else
          {
            if (GetServerStateParser().GetCapabilityFlag() & kHasXSenderCapability)
              commandString.Append(" %s (XSENDER UID RFC822.SIZE RFC822");
            else
              commandString.Append(" %s (UID RFC822.SIZE RFC822");
          }
          if (endByte > 0)
          {
            // if we are retrieving chunks
            char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
            if (byterangeString)
            {
              commandString.Append(byterangeString);
              PR_Free(byterangeString);
            }
           }
           commandString.Append(")");

           break;

        case kEveryThingRFC822Peek:
          {
            const char *formatString = "";
            PRUint32 server_capabilityFlags = GetServerStateParser().GetCapabilityFlag();
            
            GetServerStateParser().SetFetchingEverythingRFC822(PR_TRUE);
            if (server_capabilityFlags & kIMAP4rev1Capability)
            {
              // use body[].peek since rfc822.peek is not in IMAP4rev1
              if (server_capabilityFlags & kHasXSenderCapability)
                formatString = " %s (XSENDER UID RFC822.SIZE BODY.PEEK[])";
              else
                formatString = " %s (UID RFC822.SIZE BODY.PEEK[])";
            }
            else
            {
              if (server_capabilityFlags & kHasXSenderCapability)
                formatString = " %s (XSENDER UID RFC822.SIZE RFC822.peek)";
              else
                formatString = " %s (UID RFC822.SIZE RFC822.peek)";
            }
          
        commandString.Append(formatString);
      }
            break;
        case kHeadersRFC822andUid:
      if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
      {
        PRUint32 server_capabilityFlags = GetServerStateParser().GetCapabilityFlag();
		    PRBool aolImapServer = ((server_capabilityFlags & kAOLImapCapability) != 0);
        PRBool useArbitraryHeaders = PR_FALSE;
        GetShouldDownloadArbitraryHeaders(&useArbitraryHeaders); // checks filter headers, etc.
        if (/***** Fix me *** gOptimizedHeaders &&  */// preference -- able to turn it off
          useArbitraryHeaders)  // if it's ok -- no filters on any header, etc.
        {
          char *headersToDL = nsnull;
          char *what = nsnull;
          const char *dbHeaders = (gUseEnvelopeCmd) ? IMAP_DB_HEADERS : IMAP_ENV_AND_DB_HEADERS;
          nsXPIDLCString arbitraryHeaders;
          GetArbitraryHeadersToDownload(getter_Copies(arbitraryHeaders));
          if (arbitraryHeaders)
            headersToDL = PR_smprintf("%s %s",dbHeaders, arbitraryHeaders.get());
          else
            headersToDL = nsCRT::strdup(dbHeaders);

          if (aolImapServer)
            what = nsCRT::strdup(" XAOL-ENVELOPE INTERNALDATE)");
          else if (gUseEnvelopeCmd)
            what = PR_smprintf(" ENVELOPE BODY.PEEK[HEADER.FIELDS (%s)])", headersToDL);
          else
            what = PR_smprintf(" BODY.PEEK[HEADER.FIELDS (%s)])",headersToDL);
          nsCRT::free(headersToDL);
          if (what)
          {
            commandString.Append(" %s (UID ");
            if (aolImapServer)
              commandString.Append(" XAOL.SIZE") ;
            else
              commandString.Append("RFC822.SIZE");
            commandString.Append(" FLAGS");
            commandString.Append(what);
            PR_Free(what);
          }
          else 
          {
            commandString.Append(" %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
          }
        }
        else
          commandString.Append(" %s (UID RFC822.SIZE BODY.PEEK[HEADER] FLAGS)");
      }
      else
        commandString.Append(" %s (UID RFC822.SIZE RFC822.HEADER FLAGS)");
            break;
        case kUid:
      commandString.Append(" %s (UID)");
            break;
        case kFlags:
          GetServerStateParser().SetFetchingFlags(PR_TRUE);
      commandString.Append(" %s (FLAGS)");
            break;
        case kRFC822Size:
      commandString.Append(" %s (RFC822.SIZE)");
      break;
    case kRFC822HeadersOnly:
      if (GetServerStateParser().ServerHasIMAP4Rev1Capability())
      {
        if (part)
        {
          commandString.Append(" %s (BODY[");
          char *what = PR_smprintf("%s.HEADER])", part);
          if (what)
          {
            commandString.Append(what);
            PR_Free(what);
          }
          else
            HandleMemoryFailure();
        }
        else
        {
          // headers for the top-level message
          commandString.Append(" %s (BODY[HEADER])");
        }
      }
      else
        commandString.Append(" %s (RFC822.HEADER)");
      break;
    case kMIMEPart:
      commandString.Append(" %s (BODY[%s]");
      if (endByte > 0)
      {
        // if we are retrieving chunks
        char *byterangeString = PR_smprintf("<%ld.%ld>",startByte,endByte);
        if (byterangeString)
        {
          commandString.Append(byterangeString);
          PR_Free(byterangeString);
        }
      }
      commandString.Append(")");
      break;
    case kMIMEHeader:
      commandString.Append(" %s (BODY[%s.MIME])");
        break;
  };

  commandString.Append(CRLF);

    // since messageIds can be infinitely long, use a dynamic buffer rather than the fixed one
  const char *commandTag = GetServerCommandTag();
  int protocolStringSize = commandString.Length() + nsCRT::strlen(messageIds) + PL_strlen(commandTag) + 1 +
    (part ? PL_strlen(part) : 0);
  char *protocolString = (char *) PR_CALLOC( protocolStringSize );
    
    if (protocolString)
    {
    char *cCommandStr = ToNewCString(commandString);
    if ((whatToFetch == kMIMEPart) ||
      (whatToFetch == kMIMEHeader))
    {
      PR_snprintf(protocolString,                                      // string to create
          protocolStringSize,                                      // max size
          cCommandStr,                                   // format string
          commandTag,                          // command tag
          messageIds,
          part);
    }
    else
    {
      PR_snprintf(protocolString,                                      // string to create
          protocolStringSize,                                      // max size
          cCommandStr,                                   // format string
          commandTag,                          // command tag
          messageIds);
    }
              
    nsresult rv = SendData(protocolString);
      
    nsMemory::Free(cCommandStr);
    if (NS_SUCCEEDED(rv))
       ParseIMAPandCheckForNewMail(protocolString);
    PR_Free(protocolString);
    GetServerStateParser().SetFetchingFlags(PR_FALSE);
    GetServerStateParser().SetFetchingEverythingRFC822(PR_FALSE); // always clear this flag after every fetch....
    }
    else
        HandleMemoryFailure();
}

void nsImapProtocol::FetchTryChunking(const char * messageIds,
                                      nsIMAPeFetchFields whatToFetch,
                                      PRBool idIsUid,
                                      char *part,
                                      PRUint32 downloadSize,
                                      PRBool tryChunking)
{
  GetServerStateParser().SetTotalDownloadSize(downloadSize);
  if (m_fetchByChunks && tryChunking &&
        GetServerStateParser().ServerHasIMAP4Rev1Capability() &&
    (downloadSize > (PRUint32) m_chunkThreshold))
  {
    PRUint32 startByte = 0;
    GetServerStateParser().ClearLastFetchChunkReceived();
    while (!DeathSignalReceived() && !GetPseudoInterrupted() && 
      !GetServerStateParser().GetLastFetchChunkReceived() &&
      GetServerStateParser().ContinueParse())
    {
      PRUint32 sizeToFetch = startByte + m_chunkSize > downloadSize ?
        downloadSize - startByte : m_chunkSize;
      FetchMessage(messageIds, 
             whatToFetch,
             idIsUid,
             startByte, sizeToFetch,
             part);
      startByte += sizeToFetch;
      PRUint32 newMsgSize = GetServerStateParser().SizeOfMostRecentMessage();
      if (newMsgSize > 0 && newMsgSize != downloadSize)
        downloadSize = newMsgSize;
    }

    // Only abort the stream if this is a normal message download
    // Otherwise, let the body shell abort the stream.
    if ((whatToFetch == kEveryThingRFC822)
      &&
      ((startByte > 0 && (startByte < downloadSize) &&
      (DeathSignalReceived() || GetPseudoInterrupted())) ||
      !GetServerStateParser().ContinueParse()))
    {
      AbortMessageDownLoad();
      PseudoInterrupt(PR_FALSE);
    }
  }
  else
  {
    // small message, or (we're not chunking and not doing bodystructure),
    // or the server is not rev1.
    // Just fetch the whole thing.
    FetchMessage(messageIds, whatToFetch,idIsUid, 0, 0, part);
  }
}


void nsImapProtocol::PipelinedFetchMessageParts(nsCString &uid, nsIMAPMessagePartIDArray *parts)
{
  // assumes no chunking

  // build up a string to fetch
  nsCString stringToFetch;
  nsCString what;

  int32 currentPartNum = 0;
  while ((parts->GetNumParts() > currentPartNum) && !DeathSignalReceived())
  {
    nsIMAPMessagePartID *currentPart = parts->GetPart(currentPartNum);
    if (currentPart)
    {
      // Do things here depending on the type of message part
      // Append it to the fetch string
      if (currentPartNum > 0)
        stringToFetch += " ";

      switch (currentPart->GetFields())
      {
      case kMIMEHeader:
        what = "BODY[";
        what += currentPart->GetPartNumberString();
        what += ".MIME]";
        stringToFetch += what;
        break;
      case kRFC822HeadersOnly:
        if (currentPart->GetPartNumberString())
        {
          what = "BODY[";
          what += currentPart->GetPartNumberString();
          what += ".HEADER]";
          stringToFetch += what;
        }
        else
        {
          // headers for the top-level message
          stringToFetch += "BODY[HEADER]";
        }
        break;
      default:
        NS_ASSERTION(PR_FALSE, "we should only be pipelining MIME headers and Message headers");
        break;
      }

    }
    currentPartNum++;
  }

  // Run the single, pipelined fetch command
  if ((parts->GetNumParts() > 0) && !DeathSignalReceived() && !GetPseudoInterrupted() && stringToFetch.get())
  {
      IncrementCommandTagNumber();

    char *commandString = PR_smprintf("%s UID fetch %s (%s)%s",
                                          GetServerCommandTag(), uid.get(),
                                          stringToFetch.get(), CRLF);

    if (commandString)
    {
      nsresult rv = SendData(commandString);
            if (NS_SUCCEEDED(rv))
                ParseIMAPandCheckForNewMail(commandString);
      PR_Free(commandString);
    }
    else
      HandleMemoryFailure();
  }
}

void
nsImapProtocol::AddXMozillaStatusLine(uint16 /* flags */) // flags not use now
{
  static char statusLine[] = "X-Mozilla-Status: 0201\r\n";
  HandleMessageDownLoadLine(statusLine, PR_FALSE);
}

void
nsImapProtocol::PostLineDownLoadEvent(msg_line_info *downloadLineDontDelete)
{
    NS_ASSERTION(downloadLineDontDelete, 
                 "Oops... null msg line info not good");

  if (GetServerStateParser().GetDownloadingHeaders())
  {
    if (m_imapMailFolderSink && downloadLineDontDelete)
    {
      m_imapMailFolderSink->ParseAdoptedHeaderLine(this, downloadLineDontDelete->adoptedMessageLine, downloadLineDontDelete->uidOfMessage);
    }
  }

  else 
  {
    PRBool echoLineToMessageSink = PR_TRUE;
    // if we have a channel listener, then just spool the message
    // directly to the listener
    if (m_channelListener)
    {
      PRUint32 count = 0;
      char * line = downloadLineDontDelete->adoptedMessageLine;
      if (m_channelOutputStream)
      {
        nsresult rv = m_channelOutputStream->Write(line, PL_strlen(line), &count);
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr<nsIRequest> request = do_QueryInterface(m_mockChannel);
          m_channelListener->OnDataAvailable(request, m_channelContext, m_channelInputStream, 0, count); 
        }
          // here is where we should echo the line to the local folder copy of an online message
      }
      if (m_imapMessageSink)
        m_imapMessageSink->GetNotifyDownloadedLines(&echoLineToMessageSink);
    }
    if (m_imapMessageSink && downloadLineDontDelete && echoLineToMessageSink)
    {
      m_imapMessageSink->ParseAdoptedMsgLine(downloadLineDontDelete->adoptedMessageLine, 
      downloadLineDontDelete->uidOfMessage);
    }
  }
  // ***** We need to handle the psuedo interrupt here *****
}

// well, this is what the old code used to look like to handle a line seen by the parser.
// I'll leave it mostly #ifdef'ed out, but I suspect it will look a lot like this.
// Perhaps we'll send the line to a messageSink...
void nsImapProtocol::HandleMessageDownLoadLine(const char *line, PRBool chunkEnd)
{
    // when we duplicate this line, whack it into the native line
    // termination.  Do not assume that it really ends in CRLF
    // to start with, even though it is supposed to be RFC822

  // If we are fetching by chunks, we can make no assumptions about
  // the end-of-line terminator, and we shouldn't mess with it.
    
    // leave enough room for two more chars. (CR and LF)
    char *localMessageLine = (char *) PR_CALLOC(strlen(line) + 3);
    if (localMessageLine)
        strcpy(localMessageLine,line);
    char *endOfLine = localMessageLine + strlen(localMessageLine);
    PRBool canonicalLineEnding = PR_FALSE;
    nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(m_runningUrl);
    
    if (m_imapAction == nsIImapUrl::nsImapSaveMessageToDisk && msgUrl)
        msgUrl->GetCanonicalLineEnding(&canonicalLineEnding);

	if (!chunkEnd)
	{
        if (MSG_LINEBREAK_LEN == 1 && !canonicalLineEnding)
        {
            if ((endOfLine - localMessageLine) >= 2 &&
                endOfLine[-2] == nsCRT::CR &&
                endOfLine[-1] == nsCRT::LF)
			{
                /* CRLF -> CR or LF */
                endOfLine[-2] = MSG_LINEBREAK[0];
                endOfLine[-1] = '\0';
			}
            else if (endOfLine > localMessageLine + 1 &&
                     endOfLine[-1] != MSG_LINEBREAK[0] &&
                     ((endOfLine[-1] == nsCRT::CR) || (endOfLine[-1] == nsCRT::LF)))
			{
                /* CR -> LF or LF -> CR */
                endOfLine[-1] = MSG_LINEBREAK[0];
			}
            else // no eol characters at all
            {
                endOfLine[0] = MSG_LINEBREAK[0]; // CR or LF
                endOfLine[1] = '\0';
            }
        }
        else
        {
            if (((endOfLine - localMessageLine) >= 2 && endOfLine[-2] != nsCRT::CR) ||
                ((endOfLine - localMessageLine) >= 1 && endOfLine[-1] != nsCRT::LF))
			{
                if ((endOfLine[-1] == nsCRT::CR) || (endOfLine[-1] == nsCRT::LF))
                {
				  /* LF -> CRLF or CR -> CRLF */
                    endOfLine[-1] = MSG_LINEBREAK[0];
                    endOfLine[0]  = MSG_LINEBREAK[1];
                    endOfLine[1]  = '\0';
                }
                else	// no eol characters at all
                {
                    endOfLine[0] = MSG_LINEBREAK[0];	// CR
                    endOfLine[1] = MSG_LINEBREAK[1];	// LF
                    endOfLine[2] = '\0';
			  }
			}
        }
	}

	const char *xSenderInfo = GetServerStateParser().GetXSenderInfo();

	if (xSenderInfo && *xSenderInfo && !m_fromHeaderSeen)
	{
		if (!PL_strncmp("From: ", localMessageLine, 6))
		{
			m_fromHeaderSeen = PR_TRUE;
			if (PL_strstr(localMessageLine, xSenderInfo) != NULL)
				AddXMozillaStatusLine(0);
			GetServerStateParser().FreeXSenderInfo();
		}
	}

    // if this line is for a different message, or the incoming line is too big
    if (((m_downloadLineCache.CurrentUID() != GetServerStateParser().CurrentResponseUID()) && !m_downloadLineCache.CacheEmpty()) ||
        (m_downloadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1)) )
    {
    if (!m_downloadLineCache.CacheEmpty())
    {
      msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
      // post ODA lines....
      PostLineDownLoadEvent(downloadLineDontDelete);
    }
    m_downloadLineCache.ResetCache();
  }
     
    // so now the cache is flushed, but this string might still be to big
    if (m_downloadLineCache.SpaceAvailable() < (PL_strlen(localMessageLine) + 1) )
    {
        // has to be dynamic to pass to other win16 thread
        msg_line_info *downLoadInfo = (msg_line_info *) PR_CALLOC(sizeof(msg_line_info));
        if (downLoadInfo)
        {
            downLoadInfo->adoptedMessageLine = localMessageLine;
            downLoadInfo->uidOfMessage = GetServerStateParser().CurrentResponseUID();
            PostLineDownLoadEvent(downLoadInfo);
            if (!DeathSignalReceived())
              PR_Free(downLoadInfo);
            else
            {
              // this is very rare, interrupt while waiting to display a huge single line
              // Net_InterruptIMAP will read this line so leak the downLoadInfo              
              // set localMessageLine to NULL so the FREEIF( localMessageLine) leaks also
              localMessageLine = NULL;
            }
        }
  }
  else
    m_downloadLineCache.CacheLine(localMessageLine, GetServerStateParser().CurrentResponseUID());

  PR_FREEIF( localMessageLine);
}



void nsImapProtocol::NormalMessageEndDownload()
{
  Log("STREAM", "CLOSE", "Normal Message End Download Stream");

  if (m_trackingTime)
    AdjustChunkSize();
  if (!m_downloadLineCache.CacheEmpty())
  {
      msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
      PostLineDownLoadEvent(downloadLineDontDelete);
      m_downloadLineCache.ResetCache();
    }

  if (GetServerStateParser().GetDownloadingHeaders())
  {
    if (m_imapMailFolderSink)
      m_imapMailFolderSink->NormalEndHeaderParseStream(this);
  }
  else 
  {
    if (m_channelListener)
    {
//      PRUint32 inlength = 0;
//      m_channelInputStream->Available(&inlength);
      //if (inlength > 0) // broadcast our batched up ODA changes
//        m_channelListener->OnDataAvailable(m_mockChannel, m_channelContext, m_channelInputStream, 0, inlength);   
    }

    // need to know if we're downloading for display or not. We'll use action == nsImapMsgFetch for now 
    nsImapAction imapAction = nsIImapUrl::nsImapSelectFolder; // just set it to some legal value
    if (m_runningUrl)
      m_runningUrl->GetImapAction(&imapAction);

    if (m_imapMessageSink)
      m_imapMessageSink->NormalEndMsgWriteStream(m_downloadLineCache.CurrentUID(), imapAction == nsIImapUrl::nsImapMsgFetch);
  
    if (m_runningUrl && m_imapMailFolderSink)
    {
      nsCOMPtr<nsIMsgMailNewsUrl> mailUrl (do_QueryInterface(m_runningUrl));
      m_imapMailFolderSink->EndMessage(mailUrl, m_downloadLineCache.CurrentUID());
    }
  }
}

void nsImapProtocol::AbortMessageDownLoad()
{
  Log("STREAM", "CLOSE", "Abort Message  Download Stream");

  if (m_trackingTime)
    AdjustChunkSize();
  if (!m_downloadLineCache.CacheEmpty())
  {
      msg_line_info *downloadLineDontDelete = m_downloadLineCache.GetCurrentLineInfo();
      PostLineDownLoadEvent(downloadLineDontDelete);
      m_downloadLineCache.ResetCache();
    }

  if (GetServerStateParser().GetDownloadingHeaders())
  {
    if (m_imapMailFolderSink)
      m_imapMailFolderSink->AbortHeaderParseStream(this);
  }
  else if (m_imapMessageSink)
        m_imapMessageSink->AbortMsgWriteStream();

}


void nsImapProtocol::ProcessMailboxUpdate(PRBool handlePossibleUndo)
{
  if (DeathSignalReceived())
    return;
    // fetch the flags and uids of all existing messages or new ones
    if (!DeathSignalReceived() && GetServerStateParser().NumberOfMessages())
    {
      if (handlePossibleUndo)
      {
        // undo any delete flags we may have asked to
        nsXPIDLCString undoIdsStr;
        nsCAutoString undoIds;
      
      GetCurrentUrl()->CreateListOfMessageIdsString(getter_Copies(undoIdsStr));
      undoIds.Assign(undoIdsStr);
      if (undoIds.Length() > 0)
      {
        char firstChar = (char) undoIds.CharAt(0);
        undoIds.Cut(0, 1);  // remove first character
          // if this string started with a '-', then this is an undo of a delete
          // if its a '+' its a redo
          if (firstChar == '-')
          Store(undoIds.get(), "-FLAGS (\\Deleted)", PR_TRUE);  // most servers will fail silently on a failure, deal with it?
          else  if (firstChar == '+')
          Store(undoIds.get(), "+FLAGS (\\Deleted)", PR_TRUE);  // most servers will fail silently on a failure, deal with it?
        else 
          NS_ASSERTION(PR_FALSE, "bogus undo Id's");
      }
    }
      
        // make the parser record these flags
    nsCString fetchStr;
    PRInt32 added = 0, deleted = 0;

    m_flagState->GetNumberOfMessages(&added);
    deleted = m_flagState->GetNumberOfDeletedMessages();

    if (!added || (added == deleted))
    {
      nsCString idsToFetch("1:*");
      FetchMessage(idsToFetch.get(), kFlags, PR_TRUE);  // id string shows uids
      // lets see if we should expunge during a full sync of flags.
      if (!DeathSignalReceived()) // only expunge if not reading messages manually and before fetching new
      {
        // ### TODO read gExpungeThreshhold from prefs.
        if ((m_flagState->GetNumberOfDeletedMessages() >= 20/* gExpungeThreshold */)  && GetDeleteIsMoveToTrash() )
          Expunge();  // might be expensive, test for user cancel
      }

    }
    else 
    {
      fetchStr.AppendInt(GetServerStateParser().HighestRecordedUID() + 1, 10);
      fetchStr.Append(":*");

      // sprintf(fetchStr, "%ld:*", GetServerStateParser().HighestRecordedUID() + 1);
      FetchMessage(fetchStr.get(), kFlags, PR_TRUE);      // only new messages please
    }
    }
    else if (!DeathSignalReceived())
      GetServerStateParser().ResetFlagInfo(0);

	if (!DeathSignalReceived())
	{
      nsImapAction imapAction; 
      nsresult res = m_runningUrl->GetImapAction(&imapAction);
	  if (NS_SUCCEEDED(res) && imapAction == nsIImapUrl::nsImapLiteSelectFolder)
          return;
	}
        
    nsImapMailboxSpec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec();
  if (new_spec && !DeathSignalReceived())
  {
    if (!DeathSignalReceived())
    {
      nsImapAction imapAction; 
      nsresult res = m_runningUrl->GetImapAction(&imapAction);
      if (NS_SUCCEEDED(res) && imapAction == nsIImapUrl::nsImapExpungeFolder)
        new_spec->box_flags |= kJustExpunged;
      PR_EnterMonitor(m_waitForBodyIdsMonitor);
      UpdatedMailboxSpec(new_spec);
    }
  }
  else if (!new_spec)
    HandleMemoryFailure();
    
    // Block until libmsg decides whether to download headers or not.
    PRUint32 *msgIdList = NULL;
    PRUint32 msgCount = 0;
    
  if (!DeathSignalReceived())
  {
    WaitForPotentialListOfMsgsToFetch(&msgIdList, msgCount);

    if (new_spec)
      PR_ExitMonitor(m_waitForBodyIdsMonitor);

    if (msgIdList && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    {
      FolderHeaderDump(msgIdList, msgCount);
      PR_FREEIF( msgIdList);
    }
    HeaderFetchCompleted();
      // this might be bogus, how are we going to do pane notification and stuff when we fetch bodies without
      // headers!
  }
    // wait for a list of bodies to fetch. 
    if (!DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
    {
        WaitForPotentialListOfBodysToFetch(&msgIdList, msgCount);
        if ( msgCount && !DeathSignalReceived() && GetServerStateParser().LastCommandSuccessful())
      {
        FolderMsgDump(msgIdList, msgCount, kEveryThingRFC822Peek);
      }
  }
  if (DeathSignalReceived())
    GetServerStateParser().ResetFlagInfo(0);
  PR_FREEIF(new_spec->allocatedPathName); 
  PR_FREEIF(new_spec->hostName);
  NS_IF_RELEASE(new_spec);
}

void nsImapProtocol::UpdatedMailboxSpec(nsImapMailboxSpec *aSpec)
{
  if (m_imapMailFolderSink)
    m_imapMailFolderSink->UpdateImapMailboxInfo(this, aSpec);
}

void nsImapProtocol::FolderHeaderDump(PRUint32 *msgUids, PRUint32 msgCount)
{
  FolderMsgDump(msgUids, msgCount, kHeadersRFC822andUid);
}

void nsImapProtocol::FolderMsgDump(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields)
{
  // lets worry about this progress stuff later.
  switch (fields) {
  case kHeadersRFC822andUid:
    SetProgressString(IMAP_RECEIVING_MESSAGE_HEADERS_OF);
    break;
  case kFlags:
    SetProgressString(IMAP_RECEIVING_MESSAGE_FLAGS_OF);
    break;
  default:
    SetProgressString(IMAP_FOLDER_RECEIVING_MESSAGE_OF);
    break;
  }
  
  m_progressIndex = 0;
  m_progressCount = msgCount;
  FolderMsgDumpLoop(msgUids, msgCount, fields);
  
  SetProgressString(0);
}

void nsImapProtocol::WaitForPotentialListOfMsgsToFetch(PRUint32 **msgIdList, PRUint32 &msgCount)
{
  PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(m_fetchMsgListMonitor);
    while(!m_fetchMsgListIsNew && !DeathSignalReceived())
        PR_Wait(m_fetchMsgListMonitor, sleepTime);
    m_fetchMsgListIsNew = PR_FALSE;

    *msgIdList = m_fetchMsgIdList;
    msgCount   = m_fetchCount;
    
    PR_ExitMonitor(m_fetchMsgListMonitor);
}

void nsImapProtocol::WaitForPotentialListOfBodysToFetch(PRUint32 **msgIdList, PRUint32 &msgCount)
{
  PRIntervalTime sleepTime = kImapSleepTime;

    PR_EnterMonitor(m_fetchBodyListMonitor);
    while(!m_fetchBodyListIsNew && !DeathSignalReceived())
        PR_Wait(m_fetchBodyListMonitor, sleepTime);
    m_fetchBodyListIsNew = PR_FALSE;

    *msgIdList = m_fetchBodyIdList;
    msgCount   = m_fetchBodyCount;
    
    PR_ExitMonitor(m_fetchBodyListMonitor);
}

// libmsg uses this to notify a running imap url about the message headers it should
// download while opening a folder. Generally, the imap thread will be blocked 
// in WaitForPotentialListOfMsgsToFetch waiting for this notification.
NS_IMETHODIMP nsImapProtocol::NotifyHdrsToDownload(PRUint32 *keys, PRUint32 keyCount)
{
    PR_EnterMonitor(m_fetchMsgListMonitor);
    m_fetchMsgIdList = keys;
    m_fetchCount    = keyCount;
    m_fetchMsgListIsNew = PR_TRUE;
    PR_Notify(m_fetchMsgListMonitor);
    PR_ExitMonitor(m_fetchMsgListMonitor);
  return NS_OK;
}

// libmsg uses this to notify a running imap url about message bodies it should download.
// why not just have libmsg explicitly download the message bodies?
NS_IMETHODIMP nsImapProtocol::NotifyBodysToDownload(PRUint32 *keys, PRUint32 keyCount)
{
    PR_EnterMonitor(m_fetchBodyListMonitor);
    PR_FREEIF(m_fetchBodyIdList);
    m_fetchBodyIdList = (PRUint32 *) PR_MALLOC(keyCount * sizeof(PRUint32));
    if (m_fetchBodyIdList)
      nsCRT::memcpy(m_fetchBodyIdList, keys, keyCount * sizeof(PRUint32));
    m_fetchBodyCount    = keyCount;
    m_fetchBodyListIsNew = PR_TRUE;
    PR_Notify(m_fetchBodyListMonitor);
    PR_ExitMonitor(m_fetchBodyListMonitor);
  return NS_OK;
}

NS_IMETHODIMP nsImapProtocol::GetFlagsForUID(PRUint32 uid, PRBool *foundIt, imapMessageFlagsType *resultFlags)
{
  PRInt32 i;

  imapMessageFlagsType flags = m_flagState->GetMessageFlagsFromUID(uid, foundIt, &i);
  if (*foundIt)
    *resultFlags = flags;
  return NS_OK;
}

NS_IMETHODIMP nsImapProtocol::GetSupportedUserFlags(PRUint16 *supportedFlags)
{
  if (!supportedFlags)
    return NS_ERROR_NULL_POINTER;

  *supportedFlags = m_flagState->GetSupportedUserFlags();
  return NS_OK;
}
void nsImapProtocol::FolderMsgDumpLoop(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields)
{

  PRInt32 msgCountLeft = msgCount;
  PRUint32 msgsDownloaded = 0;
  do 
  {
    nsCString idString;

    PRUint32 msgsToDownload = (msgCountLeft > 200) ? 200 : msgCountLeft;
      AllocateImapUidString(msgUids + msgsDownloaded, msgsToDownload, idString);  // 20 * 200

    // except I don't think this works ### DB
    FetchMessage(idString.get(),  fields, PR_TRUE);                  // msg ids are uids                 

    msgsDownloaded += msgsToDownload;
    msgCountLeft -= msgsToDownload;

    }
  while (msgCountLeft > 0 && !DeathSignalReceived());
}     
    

void nsImapProtocol::HeaderFetchCompleted()
{
    if (m_imapMiscellaneousSink)
  {
        m_imapMiscellaneousSink->HeaderFetchCompleted(this);
    WaitForFEEventCompletion();
  }
}


// Use the noop to tell the server we are still here, and therefore we are willing to receive
// status updates. The recent or exists response from the server could tell us that there is
// more mail waiting for us, but we need to check the flags of the mail and the high water mark
// to make sure that we do not tell the user that there is new mail when perhaps they have
// already read it in another machine.

void nsImapProtocol::PeriodicBiff()
{
  
  nsMsgBiffState startingState = m_currentBiffState;
  
  if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
    {
    Noop(); // check the latest number of messages
    PRInt32 numMessages = 0;
    m_flagState->GetNumberOfMessages(&numMessages);
        if (GetServerStateParser().NumberOfMessages() != numMessages)
        {
      PRUint32 id = GetServerStateParser().HighestRecordedUID() + 1;
      nsCString fetchStr;           // only update flags
      PRUint32 added = 0, deleted = 0;
      
      deleted = m_flagState->GetNumberOfDeletedMessages();
      added = numMessages;
      if (!added || (added == deleted)) // empty keys, get them all
        id = 1;

      //sprintf(fetchStr, "%ld:%ld", id, id + GetServerStateParser().NumberOfMessages() - fFlagState->GetNumberOfMessages());
      fetchStr.AppendInt(id, 10);
      fetchStr.Append(":*"); 
      FetchMessage(fetchStr.get(), kFlags, PR_TRUE);

      if (((PRUint32) m_flagState->GetHighestNonDeletedUID() >= id) && m_flagState->IsLastMessageUnseen())
        m_currentBiffState = nsIMsgFolder::nsMsgBiffState_NewMail;
      else
        m_currentBiffState = nsIMsgFolder::nsMsgBiffState_NoMail;
        }
        else
            m_currentBiffState = nsIMsgFolder::nsMsgBiffState_NoMail;
    }
    else
      m_currentBiffState = nsIMsgFolder::nsMsgBiffState_Unknown;
    
    if (startingState != m_currentBiffState)
      SendSetBiffIndicatorEvent(m_currentBiffState);
}

void nsImapProtocol::SendSetBiffIndicatorEvent(nsMsgBiffState newState)
{
    m_imapMiscellaneousSink->SetBiffStateAndUpdate(this, newState);

  if (newState == nsIMsgFolder::nsMsgBiffState_NewMail)
    m_mailToFetch = PR_TRUE;
  else
    m_mailToFetch = PR_FALSE;
    WaitForFEEventCompletion();
}



// We get called to see if there is mail waiting for us at the server, even if it may have been
// read elsewhere. We just want to know if we should download headers or not.

PRBool nsImapProtocol::CheckNewMail()
{
  return m_checkForNewMailDownloadsHeaders;
}




// log info including current state...
void nsImapProtocol::Log(const char *logSubName, const char *extraInfo, const char *logData)
{
  if (PR_LOG_TEST(IMAP, PR_LOG_ALWAYS))
  {
    static const char nonAuthStateName[] = "NA";
    static const char authStateName[] = "A";
    static const char selectedStateName[] = "S";
      //  static const char waitingStateName[] = "W";
    const char *stateName = NULL;
      const char *hostName = GetImapHostName();  // initilize to empty string
    switch (GetServerStateParser().GetIMAPstate())
    {
    case nsImapServerResponseParser::kFolderSelected:
      if (m_runningUrl)
      {
        if (extraInfo)
          PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s-%s:%s:%s: %s", hostName,selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, extraInfo, logData));
        else
          PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s-%s:%s: %s", hostName,selectedStateName, GetServerStateParser().GetSelectedMailboxName(), logSubName, logData));
      }
      return;
      break;
    case nsImapServerResponseParser::kNonAuthenticated:
      stateName = nonAuthStateName;
      break;
    case nsImapServerResponseParser::kAuthenticated:
      stateName = authStateName;
      break;
  #if 0 // *** this isn't a server state; its a status ***
    case nsImapServerResponseParser::kWaitingForMoreClientInput:
      stateName = waitingStateName;
      break;
  #endif 
    }

    if (m_runningUrl)
    {
      if (extraInfo)
        PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s:%s:%s: %s", hostName,stateName,logSubName,extraInfo,logData));
      else
        PR_LOG(IMAP, PR_LOG_ALWAYS, ("%s:%s:%s: %s",hostName,stateName,logSubName,logData));
    }
  }
}

// In 4.5, this posted an event back to libmsg and blocked until it got a response.
// We may still have to do this.It would be nice if we could preflight this value,
// but we may not always know when we'll need it.
PRUint32 nsImapProtocol::GetMessageSize(const char * messageId, 
                                        PRBool idsAreUids)
{
  const char *folderFromParser = GetServerStateParser().GetSelectedMailboxName(); 
  if (folderFromParser && messageId)
  {
    char *id = (char *)PR_CALLOC(nsCRT::strlen(messageId) + 1);
    char *folderName;
    PRUint32 size;

    PL_strcpy(id, messageId);

    nsIMAPNamespace *nsForMailbox = nsnull;
        m_hostSessionList->GetNamespaceForMailboxForHost(GetImapServerKey(), folderFromParser,
            nsForMailbox);


    if (nsForMailbox)
          m_runningUrl->AllocateCanonicalPath(
              folderFromParser, nsForMailbox->GetDelimiter(),
              &folderName);
    else
       m_runningUrl->AllocateCanonicalPath(
          folderFromParser,kOnlineHierarchySeparatorUnknown,
          &folderName);

    if (id && folderName)
    {
      if (m_imapMessageSink)
          m_imapMessageSink->GetMessageSizeFromDB(id, idsAreUids, &size);
    }
    PR_FREEIF(id);
    PR_FREEIF(folderName);

    PRUint32 rv = 0;
    if (!DeathSignalReceived())
      rv = size;
    return rv;
  }
  return 0;
}

// message id string utility functions
/* static */PRBool nsImapProtocol::HandlingMultipleMessages(const char *messageIdString)
{
  return (PL_strchr(messageIdString,',') != nsnull ||
        PL_strchr(messageIdString,':') != nsnull);
}

PRUint32 nsImapProtocol::CountMessagesInIdString(const char *idString)
{
  PRUint32 numberOfMessages = 0;
  char *uidString = PL_strdup(idString);

  if (uidString)
  {
    // This is in the form <id>,<id>, or <id1>:<id2>
    char curChar = *uidString;
    PRBool isRange = PR_FALSE;
    PRInt32 curToken;
    PRInt32 saveStartToken=0;

    for (char *curCharPtr = uidString; curChar && *curCharPtr;)
    {
      char *currentKeyToken = curCharPtr;
      curChar = *curCharPtr;
      while (curChar != ':' && curChar != ',' && curChar != '\0')
        curChar = *curCharPtr++;
      *(curCharPtr - 1) = '\0';
      curToken = atol(currentKeyToken);
      if (isRange)
      {
        while (saveStartToken < curToken)
        {
          numberOfMessages++;
          saveStartToken++;
        }
      }

      numberOfMessages++;
      isRange = (curChar == ':');
      if (isRange)
        saveStartToken = curToken + 1;
    }
    PR_Free(uidString);
  }
  return numberOfMessages;
}


PRMonitor *nsImapProtocol::GetDataMemberMonitor()
{
    return m_dataMemberMonitor;
}

// It would be really nice not to have to use this method nearly as much as we did
// in 4.5 - we need to think about this some. Some of it may just go away in the new world order
PRBool nsImapProtocol::DeathSignalReceived()
{
	nsresult returnValue = NS_OK;
  // ignore mock channel status if we've been pseudo interrupted
  // ### need to make sure we clear pseudo interrupted status appropriately.
	if (!GetPseudoInterrupted() && m_mockChannel) 
    { 
        nsCOMPtr<nsIRequest> request = do_QueryInterface(m_mockChannel);
        if (request) 
            request->GetStatus(&returnValue);
    }
	if (NS_SUCCEEDED(returnValue))	// check the other way of cancelling.
	{
		PR_EnterMonitor(m_threadDeathMonitor);
		returnValue = m_threadShouldDie;
		PR_ExitMonitor(m_threadDeathMonitor);
	}
	return returnValue;
}

NS_IMETHODIMP nsImapProtocol::ResetToAuthenticatedState()
{
    GetServerStateParser().PreauthSetAuthenticatedState();
    return NS_OK;
}


NS_IMETHODIMP nsImapProtocol::GetSelectedMailboxName(char ** folderName)
{
    if (!folderName) return NS_ERROR_NULL_POINTER;
    if (GetServerStateParser().GetSelectedMailboxName())
        *folderName =
            PL_strdup((GetServerStateParser().GetSelectedMailboxName())); 
    return NS_OK;
}

PRBool nsImapProtocol::GetPseudoInterrupted()
{
  PRBool rv = PR_FALSE;
  PR_EnterMonitor(m_pseudoInterruptMonitor);
  rv = m_pseudoInterrupted;
  PR_ExitMonitor(m_pseudoInterruptMonitor);
  return rv;
}

void nsImapProtocol::PseudoInterrupt(PRBool the_interrupt)
{
  PR_EnterMonitor(m_pseudoInterruptMonitor);
  m_pseudoInterrupted = the_interrupt;
  if (the_interrupt)
  {
    Log("CONTROL", NULL, "PSEUDO-Interrupted");
  }
  PR_ExitMonitor(m_pseudoInterruptMonitor);
}

void  nsImapProtocol::SetActive(PRBool active)
{
  PR_EnterMonitor(GetDataMemberMonitor());
  m_active = active;
  PR_ExitMonitor(GetDataMemberMonitor());
}

PRBool  nsImapProtocol::GetActive()
{
  PRBool ret;
  PR_EnterMonitor(GetDataMemberMonitor());
  ret = m_active;
  PR_ExitMonitor(GetDataMemberMonitor());
  return ret;
}

void nsImapProtocol::SetContentModified(IMAP_ContentModifiedType modified)
{
  if (m_runningUrl && m_imapMessageSink)
    m_imapMessageSink->SetContentModified(m_runningUrl, modified);
}


PRBool	nsImapProtocol::GetShouldFetchAllParts()
{
	if (m_runningUrl  && !DeathSignalReceived())
	{
		nsImapContentModifiedType contentModified;
		if (NS_SUCCEEDED(m_runningUrl->GetContentModified(&contentModified)))
			return (contentModified == IMAP_CONTENT_FORCE_CONTENT_NOT_MODIFIED);
	}
	return PR_TRUE;
}

PRInt32 nsImapProtocol::OpenTunnel (PRInt32 maxNumberOfBytesToRead)
{
  return 0;
}

PRInt32 nsImapProtocol::GetTunnellingThreshold()
{
  return 0;
//  return gTunnellingThreshold;
}

PRBool nsImapProtocol::GetIOTunnellingEnabled()
{
  return PR_FALSE;
//  return gIOTunnelling;
}

// Adds a set of rights for a given user on a given mailbox on the current host.
// if userName is NULL, it means "me," or MYRIGHTS.
void nsImapProtocol::AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights)
{
    nsIMAPACLRightsInfo *aclRightsInfo = new nsIMAPACLRightsInfo();
  if (aclRightsInfo)
  {
    nsIMAPNamespace *namespaceForFolder = nsnull;
        const char *userName = GetImapUserName();
        NS_ASSERTION (m_hostSessionList, "fatal ... null host session list");
        if (m_hostSessionList)
            m_hostSessionList->GetNamespaceForMailboxForHost(
                GetImapServerKey(), mailboxName,
                namespaceForFolder);

    aclRightsInfo->hostName = PL_strdup(GetImapHostName());

    if (namespaceForFolder)
            m_runningUrl->AllocateCanonicalPath(
                mailboxName,
                namespaceForFolder->GetDelimiter(), 
                &aclRightsInfo->mailboxName);
    else
            m_runningUrl->AllocateCanonicalPath(mailboxName,
                          kOnlineHierarchySeparatorUnknown, 
                          &aclRightsInfo->mailboxName);

    if (userName)
      aclRightsInfo->userName = PL_strdup(userName);
    else
      aclRightsInfo->userName = NULL;
    aclRightsInfo->rights = PL_strdup(rights);
    

    if (aclRightsInfo->hostName && 
            aclRightsInfo->mailboxName && aclRightsInfo->rights && 
      userName ? (aclRightsInfo->userName != NULL) : PR_TRUE)
    {
            if (m_imapExtensionSink)
            {
                m_imapExtensionSink->AddFolderRights(this, aclRightsInfo);
                WaitForFEEventCompletion();
            }
        }
        PR_FREEIF(aclRightsInfo->hostName);
        PR_FREEIF(aclRightsInfo->mailboxName);
        PR_FREEIF(aclRightsInfo->rights);
        PR_FREEIF(aclRightsInfo->userName);

    delete aclRightsInfo;
  }
  else
    HandleMemoryFailure();
}

void nsImapProtocol::SetCopyResponseUid(nsMsgKeyArray* aKeyArray,
                                        const char *msgIdString)
{
  if (m_imapExtensionSink)
  {
    m_imapExtensionSink->SetCopyResponseUid(this,aKeyArray, msgIdString,
                                            m_runningUrl);
    WaitForFEEventCompletion();
  }
}

void nsImapProtocol::CommitNamespacesForHostEvent()
{
    if (m_imapServerSink)
        m_imapServerSink->CommitNamespaces();
}

// notifies libmsg that we have new capability data for the current host
void nsImapProtocol::CommitCapability()
{
    if (m_imapServerSink)
    {
        m_imapServerSink->SetCapability(GetServerStateParser().GetCapabilityFlag());
    }
}

// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
// Clears all rights for a given folder, for all users.
void nsImapProtocol::ClearAllFolderRights(const char *mailboxName,
                                          nsIMAPNamespace *nsForMailbox)
{
  NS_ASSERTION (nsForMailbox, "Oops ... null name space");
    nsIMAPACLRightsInfo *aclRightsInfo = new nsIMAPACLRightsInfo();
  if (aclRightsInfo)
  {
        const char *hostName = GetImapHostName();

    aclRightsInfo->hostName = PL_strdup(hostName);
    if (nsForMailbox)
            m_runningUrl->AllocateCanonicalPath(mailboxName,
                                                nsForMailbox->GetDelimiter(),
                                                &aclRightsInfo->mailboxName); 
    else
            m_runningUrl->AllocateCanonicalPath(
                mailboxName, kOnlineHierarchySeparatorUnknown,
                &aclRightsInfo->mailboxName);


    aclRightsInfo->rights = NULL;
    aclRightsInfo->userName = NULL;

    if (aclRightsInfo->hostName && aclRightsInfo->mailboxName)
    {
            if (m_imapExtensionSink)
            {
                m_imapExtensionSink->ClearFolderRights(this, aclRightsInfo);
            WaitForFEEventCompletion();
            }
        }
        PR_FREEIF(aclRightsInfo->hostName);
        PR_FREEIF(aclRightsInfo->mailboxName);

    delete aclRightsInfo;
  }
  else
    HandleMemoryFailure();
}


char* nsImapProtocol::CreateNewLineFromSocket()
{
  PRBool needMoreData = PR_FALSE;
  char * newLine = nsnull;
  PRUint32 numBytesInLine = 0;
  do
  {
    m_eventQueue->ProcessPendingEvents();
    newLine = m_inputStreamBuffer->ReadNextLine(m_inputStream, numBytesInLine, needMoreData); 
    if (needMoreData)
    {
      SetFlag(IMAP_WAITING_FOR_DATA);
      // we want to put this thread to rest until the on data available monitor goes off..
      // so sleep until the timeout hits, then pump some events. This may cause the IMAP_WAITING_FOR_DATA
      // flag to get cleared which means data has arrived so we can kick out of the loop or the
      // death signal may have been received which means we should still kick out of the loop.
      do
      {
//		  printf("waiting for data\n");
        // wait on the data available monitor!!
        PR_EnterMonitor(m_dataAvailableMonitor);
        // wait for data arrival
        PR_Wait(m_dataAvailableMonitor, /* PR_INTERVAL_NO_TIMEOUT */ PR_MillisecondsToInterval(50));
        PR_ExitMonitor(m_dataAvailableMonitor);

        // now that we are awake...process some events
        m_eventQueue->ProcessPendingEvents();
      } while (TestFlag(IMAP_WAITING_FOR_DATA) && !DeathSignalReceived());
    }
  } while (!newLine && !DeathSignalReceived()); // until we get the next line and haven't been interrupted
  
  Log("CreateNewLineFromSocket", nsnull, newLine);
  SetConnectionStatus(newLine && numBytesInLine ? 1 : -1); // set > 0 if string is not null or empty
  return newLine;
}

PRInt32
nsImapProtocol::GetConnectionStatus()
{
    // ***?? I am not sure we really to guard with monitor for 5.0 ***
    PRInt32 status;
  // mscott -- do we need these monitors? as i was debuggin this I continually
  // locked when entering this monitor...control would never return from this
  // function...
//    PR_CEnterMonitor(this);
    status = m_connectionStatus;
//    PR_CExitMonitor(this);
    return status;
}

void
nsImapProtocol::SetConnectionStatus(PRInt32 status)
{
//    PR_CEnterMonitor(this);
    m_connectionStatus = status;
//    PR_CExitMonitor(this);
}

void
nsImapProtocol::NotifyMessageFlags(imapMessageFlagsType flags, nsMsgKey key)
{
    if (m_imapMessageSink)
        m_imapMessageSink->NotifyMessageFlags(flags, key);
}

void
nsImapProtocol::NotifySearchHit(const char * hitLine)
{
    nsresult rv;
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl, &rv);
    if (m_imapMailFolderSink)
        m_imapMailFolderSink->NotifySearchHit(mailnewsUrl, hitLine);
}

void nsImapProtocol::SetMailboxDiscoveryStatus(EMailboxDiscoverStatus status)
{
    PR_EnterMonitor(GetDataMemberMonitor());
  m_discoveryStatus = status;
    PR_ExitMonitor(GetDataMemberMonitor());
}

EMailboxDiscoverStatus nsImapProtocol::GetMailboxDiscoveryStatus( )
{
  EMailboxDiscoverStatus returnStatus;
    PR_EnterMonitor(GetDataMemberMonitor());
  returnStatus = m_discoveryStatus;
    PR_ExitMonitor(GetDataMemberMonitor());
    
    return returnStatus;
}

PRBool
nsImapProtocol::GetSubscribingNow()
{
    // ***** code me *****
    return PR_FALSE;// ***** for now
}

void
nsImapProtocol::DiscoverMailboxSpec(nsImapMailboxSpec * adoptedBoxSpec)  
{
  // IMAP_LoadTrashFolderName(); **** needs to work on localization issues
  nsIMAPNamespace *ns = nsnull;

    NS_ASSERTION (m_hostSessionList, "fatal null host session list");
    if (!m_hostSessionList) return;

    m_hostSessionList->GetDefaultNamespaceOfTypeForHost(
        GetImapServerKey(), kPersonalNamespace, ns);
  const char *nsPrefix = ns ? ns->GetPrefix() : 0;

  nsCString canonicalSubDir;
  if (nsPrefix)
  {
        PRUnichar slash = '/';
    canonicalSubDir = nsPrefix;
    if (canonicalSubDir.Length() && canonicalSubDir.Last() == slash)
        canonicalSubDir.SetLength((PRUint32) canonicalSubDir.Length()-1);
  }
    
  switch (m_hierarchyNameState)
  {
    case kListingForCreate:
    case kNoOperationInProgress:
    case kDiscoverTrashFolderInProgress:
    case kListingForInfoAndDiscovery:
    {
            if (canonicalSubDir.Length() &&
                PL_strstr(adoptedBoxSpec->allocatedPathName,
                          canonicalSubDir.get()))
                m_onlineBaseFolderExists = PR_TRUE;

            if (ns && nsPrefix) // if no personal namespace, there can be no Trash folder
      {
                PRBool onlineTrashFolderExists = PR_FALSE;
                if (m_hostSessionList)
                    m_hostSessionList->GetOnlineTrashFolderExistsForHost(
                        GetImapServerKey(), onlineTrashFolderExists);

        if (GetDeleteIsMoveToTrash() && // don't set the Trash flag
                                        // if not using the Trash model
            !onlineTrashFolderExists && 
                    PL_strstr(adoptedBoxSpec->allocatedPathName, 
                              kImapTrashFolderName))
        {
          PRBool trashExists = PR_FALSE;
                    nsCString trashMatch;
                    trashMatch = nsPrefix;
                    trashMatch += kImapTrashFolderName;
          {
            char *serverTrashName = nsnull;
                        m_runningUrl->AllocateCanonicalPath(
                            trashMatch.get(),
                            ns->GetDelimiter(), &serverTrashName); 
            if (serverTrashName)
            {
              if (!PL_strcasecmp(nsPrefix, "INBOX.")) // need to special-case this because it should be case-insensitive
              {
#ifdef DEBUG
                NS_ASSERTION (PL_strlen(serverTrashName) > 6,
                                              "Oops.. less that 6");
#endif
                trashExists = ((PL_strlen(serverTrashName) > 6 /* nsCRT::strlen("INBOX.") */) &&
                  (PL_strlen(adoptedBoxSpec->allocatedPathName) > 6 /* nsCRT::strlen("INBOX.") */) &&
                  !PL_strncasecmp(adoptedBoxSpec->allocatedPathName, serverTrashName, 6) && /* "INBOX." */
                  !PL_strcmp(adoptedBoxSpec->allocatedPathName + 6, serverTrashName + 6));
              }
              else
              {
                trashExists = (PL_strcmp(serverTrashName, adoptedBoxSpec->allocatedPathName) == 0);
              }
                            if (m_hostSessionList)
              m_hostSessionList->
                                SetOnlineTrashFolderExistsForHost(
                                    GetImapServerKey(), trashExists);
              PR_Free(serverTrashName);
            }
          }
          
          if (trashExists)
                    adoptedBoxSpec->box_flags |= kImapTrash;
        }
      }

      // Discover the folder (shuttle over to libmsg, yay)
      // Do this only if the folder name is not empty (i.e. the root)
      if (adoptedBoxSpec->allocatedPathName &&
                *adoptedBoxSpec->allocatedPathName)
      {
                nsCString boxNameCopy; 
                
        boxNameCopy = adoptedBoxSpec->allocatedPathName;
        if (m_hierarchyNameState == kListingForCreate)
          adoptedBoxSpec->box_flags |= kNewlyCreatedFolder;

                if (m_imapServerSink)
                {
                    m_imapServerSink->PossibleImapMailbox(boxNameCopy.get(), 
									adoptedBoxSpec->hierarchySeparator,
                                    adoptedBoxSpec->box_flags);
                
                    PRBool useSubscription = PR_FALSE;

                    if (m_hostSessionList)
                        m_hostSessionList->GetHostIsUsingSubscription(
                            GetImapServerKey(),
                            useSubscription);

          if ((GetMailboxDiscoveryStatus() != eContinue) && 
            (GetMailboxDiscoveryStatus() != eContinueNew) &&
            (GetMailboxDiscoveryStatus() != eListMyChildren))
          {
                      SetConnectionStatus(-1);
          }
          else if (boxNameCopy.Length() && 
                             (GetMailboxDiscoveryStatus() == 
                              eListMyChildren) &&
                             (!useSubscription || GetSubscribingNow()))
          {
            NS_ASSERTION (PR_FALSE, 
                                      "we should never get here anymore");
                      SetMailboxDiscoveryStatus(eContinue);
          }
          else if (GetMailboxDiscoveryStatus() == eContinueNew)
          {
            if (m_hierarchyNameState ==
                            kListingForInfoAndDiscovery &&
                            boxNameCopy.Length() && 
                            !(adoptedBoxSpec->box_flags & kNameSpace))
            {
              // remember the info here also
              nsIMAPMailboxInfo *mb = new
                                nsIMAPMailboxInfo(boxNameCopy.get(),
                                    adoptedBoxSpec->hierarchySeparator); 
                            m_listedMailboxList.AppendElement((void*) mb);
            }
            SetMailboxDiscoveryStatus(eContinue);
          }
        }
      }
        }
        NS_IF_RELEASE( adoptedBoxSpec);
        break;
    case kDiscoverBaseFolderInProgress:
        {
            if (canonicalSubDir.Length() &&
                PL_strstr(adoptedBoxSpec->allocatedPathName,
                          canonicalSubDir.get()))
                m_onlineBaseFolderExists = PR_TRUE;
        }
        break;
    case kDeleteSubFoldersInProgress:
        {
            NS_ASSERTION(m_deletableChildren, 
                         "Oops .. null m_deletableChildren\n");
            m_deletableChildren->AppendElement((void*)
                nsCRT::strdup(adoptedBoxSpec->allocatedPathName));
            PR_FREEIF(adoptedBoxSpec->hostName);
            NS_IF_RELEASE( adoptedBoxSpec);
        }
        break;
  case kListingForInfoOnly:
    {
      //UpdateProgressWindowForUpgrade(adoptedBoxSpec->allocatedPathName);
      ProgressEventFunctionUsingIdWithString(IMAP_DISCOVERING_MAILBOX,
                adoptedBoxSpec->allocatedPathName);
      nsIMAPMailboxInfo *mb = new
                nsIMAPMailboxInfo(adoptedBoxSpec->allocatedPathName,
                                  adoptedBoxSpec->hierarchySeparator);
      m_listedMailboxList.AppendElement((void*) mb);
            PR_FREEIF(adoptedBoxSpec->allocatedPathName);
            NS_IF_RELEASE(adoptedBoxSpec);
    }
    break;
  case kDiscoveringNamespacesOnly:
    {
            PR_FREEIF(adoptedBoxSpec->allocatedPathName);
            NS_IF_RELEASE(adoptedBoxSpec);
    }
    break;
    default:
        NS_ASSERTION (PR_FALSE, "we aren't supposed to be here");
        break;
  }
}

void
nsImapProtocol::AlertUserEventUsingId(PRUint32 aMessageId)
{
  if (m_imapServerSink)
  {
    PRBool suppressErrorMsg = PR_FALSE;

    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_runningUrl);
    if (mailnewsUrl)
      mailnewsUrl->GetSuppressErrorMsgs(&suppressErrorMsg);

    if (!suppressErrorMsg)
    {
      PRUnichar *progressString = nsnull;
      m_imapServerSink->GetImapStringByID(aMessageId, &progressString);
      nsCOMPtr<nsIMsgWindow> msgWindow;
      GetMsgWindow(getter_AddRefs(msgWindow));
      m_imapServerSink->FEAlert(progressString, msgWindow);
      PR_FREEIF(progressString);
    } 
  }
}

void
nsImapProtocol::AlertUserEvent(const char * message)
{
  if (m_imapServerSink)
  {
    nsCOMPtr<nsIMsgWindow> msgWindow;
    GetMsgWindow(getter_AddRefs(msgWindow));
    m_imapServerSink->FEAlert(NS_ConvertASCIItoUCS2(message).get(), msgWindow);
  }
}

void
nsImapProtocol::AlertUserEventFromServer(const char * aServerEvent)
{
    if (m_imapServerSink)
    {
      nsCOMPtr<nsIMsgWindow> msgWindow;
      GetMsgWindow(getter_AddRefs(msgWindow));
      m_imapServerSink->FEAlertFromServer(aServerEvent, msgWindow);
    }
}

void nsImapProtocol::ResetProgressInfo()
{
  LL_I2L(m_lastProgressTime, 0);
  m_lastPercent = -1;
  m_lastProgressStringId = (PRUint32) -1;
}

void nsImapProtocol::SetProgressString(PRInt32 stringId)
{
  m_progressStringId = stringId;
  if (m_progressStringId && m_imapServerSink)
    m_imapServerSink->GetImapStringByID(stringId, getter_Copies(m_progressString));
}

void
nsImapProtocol::ShowProgress()
{
    if (m_progressString && m_progressStringId)
    {
      PRUnichar *progressString = NULL;
      nsCAutoString cProgressString; cProgressString.AssignWithConversion(m_progressString);
      const char *mailboxName = GetServerStateParser().GetSelectedMailboxName();

	    nsXPIDLString unicodeMailboxName;

	    nsresult rv = CreateUnicodeStringFromUtf7(mailboxName, getter_Copies(unicodeMailboxName));
	    if (NS_SUCCEEDED(rv))
	    {
		    // ### should convert mailboxName to PRUnichar and change %s to %S in msg text
		    progressString = nsTextFormatter::smprintf(m_progressString, (const PRUnichar *) unicodeMailboxName, ++m_progressIndex, m_progressCount);
		    if (progressString)
		    {
			  PercentProgressUpdateEvent(progressString, m_progressIndex,m_progressCount);
			  nsTextFormatter::smprintf_free(progressString);
		    }
	    }
    }
}

void
nsImapProtocol::ProgressEventFunctionUsingId(PRUint32 aMsgId)
{
    if (m_imapMiscellaneousSink && aMsgId != m_lastProgressStringId)
  {
    m_imapMiscellaneousSink->ProgressStatus(this, aMsgId, nsnull);
    m_lastProgressStringId = aMsgId;
    // who's going to free this? Does ProgressStatus complete synchronously?
  }
}

void
nsImapProtocol::ProgressEventFunctionUsingIdWithString(PRUint32 aMsgId, const
                                                       char * aExtraInfo)
{
    if (m_imapMiscellaneousSink)
  {

	    nsXPIDLString unicodeStr;

	    nsresult rv = CreateUnicodeStringFromUtf7(aExtraInfo, getter_Copies(unicodeStr));
	    if (NS_SUCCEEDED(rv))
        m_imapMiscellaneousSink->ProgressStatus(this, aMsgId, unicodeStr);
  }
}

void
nsImapProtocol::PercentProgressUpdateEvent(PRUnichar *message, PRInt32 currentProgress, PRInt32 maxProgress)
{

  PRInt64 nowMS = LL_ZERO;
  PRInt32 percent = (100 * currentProgress) / maxProgress;
  if (percent == m_lastPercent)
    return; // hasn't changed, right? So just return. Do we need to clear this anywhere?

  if (percent < 100)  // always need to do 100%
  {
    int64 minIntervalBetweenProgress;

    LL_I2L(minIntervalBetweenProgress, 250);
    int64 diffSinceLastProgress;
    LL_I2L(nowMS, PR_IntervalToMilliseconds(PR_IntervalNow()));
    LL_SUB(diffSinceLastProgress, nowMS, m_lastProgressTime); // r = a - b
    LL_SUB(diffSinceLastProgress, diffSinceLastProgress, minIntervalBetweenProgress); // r = a - b
    if (!LL_GE_ZERO(diffSinceLastProgress))
      return;
  }

  ProgressInfo aProgressInfo;
  aProgressInfo.message = message;
  aProgressInfo.currentProgress = currentProgress;
  aProgressInfo.maxProgress = maxProgress;
  m_lastPercent = percent;
  m_lastProgressTime = nowMS;

  if (m_imapMiscellaneousSink)
      m_imapMiscellaneousSink->PercentProgress(this, &aProgressInfo);
}


PRUnichar * nsImapProtocol::CreatePRUnicharStringFromUTF7(const char * aSourceString)
{
  PRUnichar *convertedString = NULL;
  nsresult res;
  
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsAutoString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    // convert utf7 to unicode
    nsIUnicodeDecoder* decoder = nsnull;

    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
    {
      PRInt32 srcLen = PL_strlen(aSourceString);
      res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
      // temporary buffer to hold unicode string
      unichars = new PRUnichar[unicharLength + 1];
      if (unichars == nsnull) 
      {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else 
      {
        res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
        unichars[unicharLength] = 0;
      }
      NS_IF_RELEASE(decoder);
      // convert the unicode to 8 bit ascii.
      nsString unicodeStr(unichars);
      convertedString = ToNewUnicode(unicodeStr);
    }
    }
    return convertedString;
}


  // imap commands issued by the parser
void
nsImapProtocol::Store(const char * messageList, const char * messageData,
                      PRBool idsAreUid)
{
  IncrementCommandTagNumber();
    
  const char *formatString;
  if (idsAreUid)
      formatString = "%s uid store %s %s\015\012";
  else
      formatString = "%s store %s %s\015\012";
        
    // we might need to close this mailbox after this
  m_closeNeededBeforeSelect = GetDeleteIsMoveToTrash() &&
        (PL_strcasestr(messageData, "\\Deleted"));

  const char *commandTag = GetServerCommandTag();
  int protocolStringSize = PL_strlen(formatString) +
        PL_strlen(messageList) + PL_strlen(messageData) +
        PL_strlen(commandTag) + 1;
  char *protocolString = (char *) PR_CALLOC( protocolStringSize );

  if (protocolString)
  {
    PR_snprintf(protocolString, // string to create
                  protocolStringSize, // max size
                  formatString, // format string
                  commandTag, // command tag
                  messageList,
                  messageData);
      
    nsresult rv = SendData(protocolString);
    if (NS_SUCCEEDED(rv))
      ParseIMAPandCheckForNewMail(protocolString);
    PR_Free(protocolString);
  }
  else
    HandleMemoryFailure();
}

void
nsImapProtocol::UidExpunge(const char* messageSet)
{
    IncrementCommandTagNumber();
    nsCString command(GetServerCommandTag());
    command.Append(" uid expunge ");
    command.Append(messageSet);
    command.Append(CRLF);
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void
nsImapProtocol::Expunge()
{
    ProgressEventFunctionUsingId (IMAP_STATUS_EXPUNGING_MAILBOX);
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
    command.Append(" expunge"CRLF);
    
  nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void
nsImapProtocol::HandleMemoryFailure()
{
    PR_CEnterMonitor(this);
    // **** jefft fix me!!!!!! ******
    // m_imapThreadIsRunning = PR_FALSE;
    // SetConnectionStatus(-1);
    PR_CExitMonitor(this);
}

void nsImapProtocol::HandleCurrentUrlError()
{
#ifdef UNREADY_CODE
  if (fCurrentUrl->GetIMAPurlType() == TIMAPUrl::kSelectFolder)
  {
    // let the front end know the select failed so they
    // don't leave the view without a database.
    nsImapMailboxSpec *notSelectedSpec = new nsImapMailboxSpec;
    if (notSelectedSpec)
    {
	  NS_ADDREF(notSelectedSpec);
      notSelectedSpec->allocatedPathName = fCurrentUrl->CreateCanonicalSourceFolderPathString();
      notSelectedSpec->hostName = fCurrentUrl->GetUrlHost();
      notSelectedSpec->folderSelected = PR_FALSE;
      notSelectedSpec->flagState = NULL;
      notSelectedSpec->onlineVerified = PR_FALSE;
      UpdatedMailboxSpec(notSelectedSpec);
    }
  }
  else 
#endif
    // this is to handle a move/copy failing, especially because the user
    // cancelled the password prompt.
  nsresult res;
  res = m_runningUrl->GetImapAction(&m_imapAction);
    if (m_imapAction == nsIImapUrl::nsImapOfflineToOnlineMove || m_imapAction == nsIImapUrl::nsImapAppendMsgFromFile
      || m_imapAction == nsIImapUrl::nsImapAppendDraftFromFile)
  {
    if (m_imapMailFolderSink)
      m_imapMailFolderSink->OnlineCopyCompleted(this, ImapOnlineCopyStateType::kFailedCopy);
  }
}

void nsImapProtocol::Capability()
{

    ProgressEventFunctionUsingId (IMAP_STATUS_CHECK_COMPAT);
    IncrementCommandTagNumber();
    nsCString command(GetServerCommandTag());

    command.Append(" capability" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Language()
{
  // only issue the language request if we haven't done so already...
  if (!TestFlag(IMAP_ISSUED_LANGUAGE_REQUEST))
  {
    SetFlag(IMAP_ISSUED_LANGUAGE_REQUEST);
    ProgressEventFunctionUsingId (IMAP_STATUS_CHECK_COMPAT);
    IncrementCommandTagNumber();
    nsCString command(GetServerCommandTag());

    // extract the desired language attribute from prefs
    nsresult rv = NS_OK; 
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    nsXPIDLString acceptLanguages;
    if (NS_SUCCEEDED(rv) && prefs) 
        prefs->GetLocalizedUnicharPref("intl.accept_languages", getter_Copies(acceptLanguages));

    // we need to parse out the first language out of this comma separated list....
    // i.e if we have en,ja we only want to send en to the server.
    if (acceptLanguages)
    {
      nsCAutoString extractedLanguage;
      extractedLanguage.AssignWithConversion(acceptLanguages);
      PRInt32 pos = extractedLanguage.FindChar(',', PR_TRUE);
      if (pos > 0) // we have a comma separated list of languages...
        extractedLanguage.Truncate(pos); // truncate everything after the first comma (including the comma)
      
      if (extractedLanguage.IsEmpty())
        return;

      command.Append(" LANGUAGE ");
      command.Append(extractedLanguage); 
      command.Append(CRLF);
            
      rv = SendData(command.get());
      if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail(nsnull, PR_TRUE /* ignore bad or no result from the server for this command */);
    }
  }
}

void nsImapProtocol::EscapeUserNamePasswordString(const char *strToEscape, nsCString *resultStr)
{
  if (strToEscape) 
  {
    PRUint32 i = 0;
    PRUint32 escapeStrlen = nsCRT::strlen(strToEscape);
    for (i=0; i<escapeStrlen; i++)
    {
        if (strToEscape[i] == '\\' || strToEscape[i] == '\"') 
        {
            resultStr->Append('\\');
        }
        resultStr->Append(strToEscape[i]);
    }
  }
}

void nsImapProtocol::InsecureLogin(const char *userName, const char *password)
{

  ProgressEventFunctionUsingId (IMAP_STATUS_SENDING_LOGIN);
  IncrementCommandTagNumber();
  nsCString command (GetServerCommandTag());
  nsCAutoString escapedUserName;
  command.Append(" login \"");
  EscapeUserNamePasswordString(userName, &escapedUserName);
  command.Append(escapedUserName);
  command.Append("\" \"");

  // if the password contains a \, login will fail
  // turn foo\bar into foo\\bar
  nsCAutoString correctedPassword;
  EscapeUserNamePasswordString(password, &correctedPassword);
  command.Append(correctedPassword);
  command.Append("\""CRLF);

  nsresult rv = SendData(command.get(), PR_TRUE /* suppress logging */);
  
  if (NS_SUCCEEDED(rv))
     ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::AuthLogin(const char *userName, const char *password, eIMAPCapabilityFlag flag)
{
  ProgressEventFunctionUsingId (IMAP_STATUS_SENDING_AUTH_LOGIN);
  IncrementCommandTagNumber();

  char * currentCommand=nsnull;
  nsresult rv;

#if 0
  if (flag & kHasCRAMCapability)
  {
    nsCOMPtr<nsIKeyedHashAlgorithm> keyedHash (do_GetService(NS_KEYEDHASHALGORITHM_CONTRACTID_PREFIX "MD5"));
    if (keyedHash)
    {
      // inform the server that we want to begin a CRAM authentication procedure...
      nsCAutoString command (GetServerCommandTag());
      command.Append(" authenticate CRAM-MD5" CRLF);
      rv = SendData(command);
      ParseIMAPandCheckForNewMail();
      if (GetServerStateParser().LastCommandSuccessful()) 
      {
          unsigned char * digest;
          char * decodedChallenge = PL_Base64Decode(GetServerStateParser().fCRAMDigest, 
                                                    nsCRT::strlen(GetServerStateParser().fCRAMDigest), nsnull);

          rv = keyedHash->KeyedHash((unsigned char *) decodedChallenge, 
                                  nsCRT::strlen(decodedChallenge), 
                                  (unsigned char *) password, nsCRT::strlen(password), 
                                  &digest);
          PR_FREEIF(decodedChallenge);
          if (NS_SUCCEEDED(rv) && digest)
          {
            nsCAutoString encodedDigest;
            PRUint32 digestLength = nsCRT::strlen((const char *) digest);
            char hexVal[8];

            for (PRUint32 j=0; j<digestLength; j++) 
            {
              PR_snprintf (hexVal,8, "%.2x", 0x0ff & (unsigned short)digest[j]);
              encodedDigest.Append(hexVal); 
            }

            PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s %s", userName, encodedDigest.get());
            char *base64Str = PL_Base64Encode(m_dataOutputBuf, nsCRT::strlen(m_dataOutputBuf), nsnull);
            PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
            PR_FREEIF(base64Str);
            PR_FREEIF(digest);
            rv = SendData(m_dataOutputBuf);
            if (NS_SUCCEEDED(rv))
               ParseIMAPandCheckForNewMail(command);
            if (GetServerStateParser().LastCommandSuccessful())
              return;
          }
      } // if CRAM response was received
    } // if keyed hash
  }
  else 
#endif
  if (flag & kHasAuthPlainCapability)
  {
    PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s authenticate plain" CRLF, GetServerCommandTag());
    rv = SendData(m_dataOutputBuf);
    if (NS_FAILED(rv)) return;
    currentCommand = PL_strdup(m_dataOutputBuf); /* StrAllocCopy(currentCommand, GetOutputBuffer()); */
    ParseIMAPandCheckForNewMail();
    if (GetServerStateParser().LastCommandSuccessful()) 
    {
      char plainstr[512]; // placeholder for "<NUL>userName<NUL>password"
      int len = 1; // count for first <NUL> char
      nsCRT::memset(plainstr, 0, 512);
      PR_snprintf(&plainstr[1], 510, "%s", userName);
      len += PL_strlen(userName);
      len++;  // count for second <NUL> char
      PR_snprintf(&plainstr[len], 511-len, "%s", password);
      len += PL_strlen(password);
    
      char *base64Str = PL_Base64Encode(plainstr, len, nsnull);
      if (base64Str)
      {
        PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
        PR_Free(base64Str);
        rv = SendData(m_dataOutputBuf, PR_TRUE /* suppress logging */);
        if (NS_SUCCEEDED(rv))
            ParseIMAPandCheckForNewMail(currentCommand);
        if (GetServerStateParser().LastCommandSuccessful())
        {
          PR_FREEIF(currentCommand);
          return;
        } // if the last command succeeded
      } // if we got a base 64 encoded string
    } // if the last command succeeded
  } // if auth plain capability

  else if (flag & kHasAuthLoginCapability)
  {
    PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s authenticate login" CRLF, GetServerCommandTag());
    rv = SendData(m_dataOutputBuf);
    if (NS_FAILED(rv)) return;
    currentCommand = PL_strdup(m_dataOutputBuf);
    ParseIMAPandCheckForNewMail();

    if (GetServerStateParser().LastCommandSuccessful()) 
    {
      char *base64Str = PL_Base64Encode(userName, PL_strlen(userName), nsnull);
      if(base64Str)
      {
        PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
        PR_Free(base64Str);
        rv = SendData(m_dataOutputBuf, PR_TRUE /* suppress logging */);
        if (NS_SUCCEEDED(rv))
            ParseIMAPandCheckForNewMail(currentCommand);
      }
      if (GetServerStateParser().LastCommandSuccessful()) 
      {
        base64Str = PL_Base64Encode((char*)password, PL_strlen(password), nsnull);
        PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE, "%s" CRLF, base64Str);
        PR_FREEIF(base64Str);
        rv = SendData(m_dataOutputBuf, PR_TRUE /* suppress logging */);
        if (NS_SUCCEEDED(rv))
           ParseIMAPandCheckForNewMail(currentCommand);
        if (GetServerStateParser().LastCommandSuccessful())
        {
          PR_FREEIF(currentCommand);
          return;
        }
      } // if last command successful
    } // if last command successful
  } // if has auth login capability

  // fall back to use InsecureLogin()
  InsecureLogin(userName, password);
  PR_FREEIF(currentCommand);
}

void nsImapProtocol::OnLSubFolders()
{
  // **** use to find out whether Drafts, Sent, & Templates folder
  // exists or not even the user didn't subscribe to it
  char *mailboxName = OnCreateServerSourceFolderPathString();
  if (mailboxName)
  {
    ProgressEventFunctionUsingId(IMAP_STATUS_LOOKING_FOR_MAILBOX);
    IncrementCommandTagNumber();
    PR_snprintf(m_dataOutputBuf, OUTPUT_BUFFER_SIZE,"%s list \"\" \"%s\"" CRLF, GetServerCommandTag(), mailboxName);
    nsresult rv = SendData(m_dataOutputBuf);
#ifdef UNREADY_CODE
    TimeStampListNow();
#endif
        if (NS_SUCCEEDED(rv))
            ParseIMAPandCheckForNewMail();  
    PR_Free(mailboxName);
  }
  else
  {
    HandleMemoryFailure();
  }

}

void nsImapProtocol::OnGetMailAccount()
{
  NS_ASSERTION(0, "unimplemented feature");
#ifdef UNREADY_CODE
  if (GetServerStateParser().GetCapabilityFlag() & kHasXNetscapeCapability) 
  {
    Netscape();
    if (GetServerStateParser().LastCommandSuccessful()) 
    {
      TImapFEEvent *alertEvent = 
        new TImapFEEvent(msgSetMailAccountURL,  // function to call
                 this,                // access to current entry
                 (void *) fCurrentUrl->GetUrlHost(),
                 PR_TRUE);
      if (alertEvent)
      {
        fFEEventQueue->AdoptEventToEnd(alertEvent);
        // WaitForFEEventCompletion();
      }
      else
        HandleMemoryFailure();
    }
  }
#endif
}

void nsImapProtocol::OnOfflineToOnlineMove()
{
  NS_ASSERTION(0, "unimplemented feature");
#ifdef UNREADY_CODE
    char *destinationMailbox = OnCreateServerDestinationFolderPathString();
    
  if (destinationMailbox)
  {
        uint32 appendSize = 0;
        do {
            WaitForNextAppendMessageSize();
            appendSize = GetAppendSize();
            if (!DeathSignalReceived() && appendSize)
            {
                char messageSizeString[100];
                sprintf(messageSizeString, "%ld",(long) appendSize);
                AppendMessage(destinationMailbox, messageSizeString, GetAppendFlags());
            }
        } while (appendSize && GetServerStateParser().LastCommandSuccessful());
        PR_FREEIF( destinationMailbox);
    }
    else
        HandleMemoryFailure();
#endif
}

void nsImapProtocol::OnAppendMsgFromFile()
{
    nsCOMPtr<nsIFileSpec> fileSpec;
    nsresult rv = NS_OK;
    rv = m_runningUrl->GetMsgFileSpec(getter_AddRefs(fileSpec));
    if (NS_SUCCEEDED(rv) && fileSpec)
    {
        char *mailboxName =  
            OnCreateServerSourceFolderPathString();
        if (mailboxName)
        {
          imapMessageFlagsType flagsToSet = kImapMsgSeenFlag;
          nsCOMPtr <nsISupports> copyState;
          m_runningUrl->GetCopyState(getter_AddRefs(copyState));
          if (copyState)
          {
            nsCOMPtr<nsImapMailCopyState> mailCopyState = do_QueryInterface(copyState, &rv);
            if (mailCopyState)
            {
              nsCOMPtr <nsIMsgDBHdr> curMsg = mailCopyState->m_message;
              PRUint32 flags;

              if (curMsg)
              {
                curMsg->GetFlags(&flags);
                if (! (flags & MSG_FLAG_READ))
                  flagsToSet &= ~MSG_FLAG_READ;
              }

            }
          }
          UploadMessageFromFile(fileSpec, mailboxName, flagsToSet);
          PR_Free( mailboxName );
        }
        else
        {
            HandleMemoryFailure();
        }
    }
}

void nsImapProtocol::UploadMessageFromFile (nsIFileSpec* fileSpec,
                                            const char* mailboxName,
                                            imapMessageFlagsType flags)
{
  if (!fileSpec || !mailboxName) return;
  IncrementCommandTagNumber();
  
  PRUint32 fileSize = 0;
  PRInt32 totalSize;
  PRUint32 readCount;
  char *dataBuffer = nsnull;
  nsCString command(GetServerCommandTag());
  char* escapedName = CreateEscapedMailboxName(mailboxName);
  nsresult rv;
  PRBool isOpen = PR_FALSE;
  PRBool eof = PR_FALSE;
  nsCString flagString;
  PRBool hasLiteralPlus = (GetServerStateParser().GetCapabilityFlag() &
    kLiteralPlusCapability);
  
  nsCOMPtr <nsIInputStream> fileInputStream;
  
  if (escapedName)
  {
    command.Append(" append \"");
    command.Append(escapedName);
    command.Append("\" (");
    
    SetupMessageFlagsString(flagString, flags,
      GetServerStateParser().SupportsUserFlags());
    command.Append(flagString);
    command.Append(") {");
    
    dataBuffer = (char*) PR_CALLOC(COPY_BUFFER_SIZE+1);
    if (!dataBuffer) goto done;
    rv = fileSpec->GetFileSize(&fileSize);
    if (NS_FAILED(rv)) goto done;
    rv = fileSpec->GetInputStream(getter_AddRefs(fileInputStream));
    if (NS_FAILED(rv) || !fileInputStream) goto done;
    command.AppendInt((PRInt32)fileSize);
    if (hasLiteralPlus)
      command.Append("+}" CRLF);
    else
      command.Append("}" CRLF);
    
    rv = SendData(command.get());
    if (NS_FAILED(rv)) goto done;
    
    if (!hasLiteralPlus)
      ParseIMAPandCheckForNewMail();
    
    totalSize = fileSize;
    readCount = 0;
    while(NS_SUCCEEDED(rv) && !eof && totalSize > 0)
    {
      rv = fileInputStream->Read(dataBuffer, COPY_BUFFER_SIZE, &readCount);
      if (NS_SUCCEEDED(rv))
      {
        dataBuffer[readCount] = 0;
        rv = SendData(dataBuffer);
        totalSize -= readCount;
        rv = fileSpec->Eof(&eof);
      }
    }
    if (NS_SUCCEEDED(rv))
    {
      rv = SendData(CRLF); // complete the append
      ParseIMAPandCheckForNewMail(command.get());
      
      nsImapAction imapAction;
      m_runningUrl->GetImapAction(&imapAction);
      
      if (GetServerStateParser().LastCommandSuccessful() &&  (
        imapAction == nsIImapUrl::nsImapAppendDraftFromFile || imapAction == nsIImapUrl::nsImapAppendMsgFromFile ))
      {
        if (GetServerStateParser().GetCapabilityFlag() &
          kUidplusCapability)
        {
          nsMsgKey newKey =
            GetServerStateParser().CurrentResponseUID();
          if (m_imapExtensionSink)
          {
            m_imapExtensionSink->SetAppendMsgUid(this, newKey,
              m_runningUrl);
            WaitForFEEventCompletion();
          }
          nsXPIDLCString oldMsgId;
          rv = m_runningUrl->CreateListOfMessageIdsString(getter_Copies(oldMsgId));
          if (NS_SUCCEEDED(rv) && nsCRT::strlen(oldMsgId) > 0)
          {
            PRBool idsAreUids = PR_TRUE;
            m_runningUrl->MessageIdsAreUids(&idsAreUids);
            Store(oldMsgId, "+FLAGS (\\Deleted)", idsAreUids);
            UidExpunge(oldMsgId);
          }
        }
        else if (m_imapExtensionSink)
        {   // *** code me to search for the newly appended message
          // go to selected state
          AutoSubscribeToMailboxIfNecessary(mailboxName);
          SelectMailbox(mailboxName);
          
          nsCString messageId;
          rv = m_imapExtensionSink->GetMessageId(this, &messageId,
            m_runningUrl);
          WaitForFEEventCompletion();
          if (NS_SUCCEEDED(rv) && messageId.Length() > 0 &&
            GetServerStateParser().LastCommandSuccessful())
          {
            command = "SEARCH SEEN HEADER Message-ID ";
            command.Append(messageId);
            
            // Clean up result sequence before issuing the cmd.
            GetServerStateParser().ResetSearchResultSequence();
            
            Search(command.get(), PR_TRUE, PR_FALSE);
            if (GetServerStateParser().LastCommandSuccessful())
            {
              nsMsgKey newkey = nsMsgKey_None;
              nsImapSearchResultIterator *searchResult = 
                GetServerStateParser().CreateSearchResultIterator();
              newkey = searchResult->GetNextMessageNumber();
              delete searchResult;
              if (newkey != nsMsgKey_None)
              {
                m_imapExtensionSink->SetAppendMsgUid
                  (this, newkey, m_runningUrl);
                WaitForFEEventCompletion();
              }
            }
          }
        }
      }
    }
    }
done:
  PR_FREEIF(dataBuffer);
  fileSpec->CloseStream();
  nsMemory::Free(escapedName);
}

//caller must free using PR_Free
char * nsImapProtocol::OnCreateServerSourceFolderPathString()
{
  char *sourceMailbox = nsnull;
  char hierarchyDelimiter = 0;
  char *onlineDelimiter = nsnull;
  m_runningUrl->GetOnlineSubDirSeparator(&hierarchyDelimiter);
  if (m_imapMailFolderSink)
      m_imapMailFolderSink->GetOnlineDelimiter(&onlineDelimiter);
  if (onlineDelimiter && *onlineDelimiter != kOnlineHierarchySeparatorUnknown
      && *onlineDelimiter != hierarchyDelimiter)
      m_runningUrl->SetOnlineSubDirSeparator (*onlineDelimiter);
  if (onlineDelimiter)
      nsCRT::free(onlineDelimiter);

  m_runningUrl->CreateServerSourceFolderPathString(&sourceMailbox);

  return sourceMailbox;
}

nsresult nsImapProtocol::CreateServerSourceFolderPathString(char **result)
{
  NS_ENSURE_ARG(result);
  *result = OnCreateServerSourceFolderPathString();
  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

//caller must free using PR_Free
char * nsImapProtocol::OnCreateServerDestinationFolderPathString()
{
  char *destinationMailbox = nsnull;
  char hierarchyDelimiter = 0;
  char *onlineDelimiter = nsnull;
  m_runningUrl->GetOnlineSubDirSeparator(&hierarchyDelimiter);
  if (m_imapMailFolderSink)
      m_imapMailFolderSink->GetOnlineDelimiter(&onlineDelimiter);
  if (onlineDelimiter && *onlineDelimiter != kOnlineHierarchySeparatorUnknown
      && *onlineDelimiter != hierarchyDelimiter)
      m_runningUrl->SetOnlineSubDirSeparator (*onlineDelimiter);
  if (onlineDelimiter)
      nsCRT::free(onlineDelimiter);

  m_runningUrl->CreateServerDestinationFolderPathString(&destinationMailbox);

  return destinationMailbox;
}

void nsImapProtocol::OnCreateFolder(const char * aSourceMailbox)
{
  PRBool created = CreateMailboxRespectingSubscriptions(aSourceMailbox);
  if (created)
  {
    m_hierarchyNameState = kListingForCreate;
    List(aSourceMailbox, PR_FALSE);
    m_hierarchyNameState = kNoOperationInProgress;
  }
  else
    FolderNotCreated(aSourceMailbox);
}

void nsImapProtocol::OnEnsureExistsFolder(const char * aSourceMailbox)
{

  List(aSourceMailbox, PR_FALSE); // how to tell if that succeeded?
  PRBool exists = PR_FALSE;

  // try converting aSourceMailbox to canonical format

  nsIMAPNamespace *nsForMailbox = nsnull;
  m_hostSessionList->GetNamespaceForMailboxForHost(GetImapServerKey(),
                                                     aSourceMailbox, nsForMailbox);
  // NS_ASSERTION (nsForMailbox, "Oops .. null nsForMailbox\n");

  nsXPIDLCString name;

  if (nsForMailbox)
    m_runningUrl->AllocateCanonicalPath(aSourceMailbox,
                                            nsForMailbox->GetDelimiter(),
                                            getter_Copies(name));
  else
    m_runningUrl->AllocateCanonicalPath(aSourceMailbox,
                                            kOnlineHierarchySeparatorUnknown, 
                                            getter_Copies(name));

  if (m_imapServerSink)
    m_imapServerSink->FolderVerifiedOnline(name, &exists);

  if (exists)
  {
      Subscribe(aSourceMailbox);
  }
  else
  {
    PRBool created = CreateMailboxRespectingSubscriptions(aSourceMailbox);
    if (created)
    {
        List(aSourceMailbox, PR_FALSE);
    }
  }
  if (!GetServerStateParser().LastCommandSuccessful())
        FolderNotCreated(aSourceMailbox);
}


void nsImapProtocol::OnSubscribe(const char * sourceMailbox)
{
  Subscribe(sourceMailbox);
}

void nsImapProtocol::OnUnsubscribe(const char * sourceMailbox)
{
  // When we try to auto-unsubscribe from \Noselect folders,
  // some servers report errors if we were already unsubscribed
  // from them.
  PRBool lastReportingErrors = GetServerStateParser().GetReportingErrors();
  GetServerStateParser().SetReportingErrors(PR_FALSE);
  Unsubscribe(sourceMailbox);
  GetServerStateParser().SetReportingErrors(lastReportingErrors);
}

void nsImapProtocol::RefreshACLForFolderIfNecessary(const char *mailboxName)
{
    if (GetServerStateParser().ServerHasACLCapability())
    {
        if (!m_folderNeedsACLRefreshed && m_imapMailFolderSink)
            m_imapMailFolderSink->GetFolderNeedsACLListed(&m_folderNeedsACLRefreshed);
        if (m_folderNeedsACLRefreshed)
        {
            OnRefreshACLForFolder(mailboxName);
            m_folderNeedsACLRefreshed = PR_FALSE;
        }
    }
}

void nsImapProtocol::OnRefreshACLForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    command.Append(" getacl \"");
  command.Append(escapedName);
  command.Append("\"" CRLF);
            
    nsMemory::Free(escapedName);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::OnRefreshAllACLs()
{
	m_hierarchyNameState = kListingForInfoOnly;
	nsIMAPMailboxInfo *mb = NULL;

	// This will fill in the list
	List("*", PR_TRUE);

	PRInt32 total = m_listedMailboxList.Count(), count = 0;
	GetServerStateParser().SetReportingErrors(PR_FALSE);
	do
	{
		mb = (nsIMAPMailboxInfo *) m_listedMailboxList.ElementAt(0);
        m_listedMailboxList.RemoveElementAt(0);
		if (mb)
		{
			char *onlineName = nsnull;
            m_runningUrl->AllocateServerPath(mb->GetMailboxName(),
                                             mb->GetDelimiter(), &onlineName);
			if (onlineName)
			{
				OnRefreshACLForFolder(onlineName);
				nsCRT::free(onlineName);
			}
			PercentProgressUpdateEvent(NULL, count, total);
			delete mb;
			count++;
		}
	} while (mb);
	
	PercentProgressUpdateEvent(NULL, 100, 100);
	GetServerStateParser().SetReportingErrors(PR_TRUE);
	m_hierarchyNameState = kNoOperationInProgress;
}

// any state commands
void nsImapProtocol::Logout()
{
  ProgressEventFunctionUsingId (IMAP_STATUS_LOGGING_OUT);

/******************************************************************
 * due to the undo functionality we cannot issule a close when logout; there
 * is no way to do an undo if the message has been permanently expunge
 * jt - 07/12/1999

    PRBool closeNeeded = GetServerStateParser().GetIMAPstate() ==
        nsImapServerResponseParser::kFolderSelected;

    if (closeNeeded && GetDeleteIsMoveToTrash())
        Close();
********************/

  IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());

  command.Append(" logout" CRLF);

  nsresult rv = SendData(command.get());
  // the socket may be dead before we read the response, so drop it.
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Noop()
{
    //ProgressUpdateEvent("noop...");
    IncrementCommandTagNumber();
  nsCString command(GetServerCommandTag());
    
  command.Append(" noop" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::XServerInfo()
{

    ProgressEventFunctionUsingId (IMAP_GETTING_SERVER_INFO);
    IncrementCommandTagNumber();
    nsCString command(GetServerCommandTag());
  
  command.Append(" XSERVERINFO MANAGEACCOUNTURL MANAGELISTSURL MANAGEFILTERSURL" CRLF);
          
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Netscape()
{
    ProgressEventFunctionUsingId (IMAP_GETTING_SERVER_INFO);
    IncrementCommandTagNumber();
    
    nsCString command(GetServerCommandTag());
  
  command.Append(" netscape" CRLF);
          
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}



void nsImapProtocol::XMailboxInfo(const char *mailboxName)
{

    ProgressEventFunctionUsingId (IMAP_GETTING_MAILBOX_INFO);
    IncrementCommandTagNumber();
    nsCString command(GetServerCommandTag());

  command.Append(" XMAILBOXINFO \"");
  command.Append(mailboxName);
  command.Append("\"  MANAGEURL POSTURL" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Namespace()
{

    ProgressEventFunctionUsingId (IMAP_STATUS_GETTING_NAMESPACE);
    IncrementCommandTagNumber();
    
  nsCString command(GetServerCommandTag());
  command.Append(" namespace" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}


void nsImapProtocol::MailboxData()
{
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
  command.Append(" mailboxdata" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}


void nsImapProtocol::GetMyRightsForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
  command.Append(" myrights \"");
  command.Append(escapedName);
  command.Append("\"" CRLF);
            
    nsMemory::Free(escapedName);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::OnStatusForFolder(const char *mailboxName)
{
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
  command.Append(" STATUS \"");
  command.Append(escapedName);
  command.Append("\" (UIDNEXT MESSAGES UNSEEN)" CRLF);
            
    nsMemory::Free(escapedName);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();

    nsImapMailboxSpec *new_spec = GetServerStateParser().CreateCurrentMailboxSpec(mailboxName);
  if (new_spec && m_imapMailFolderSink)
    m_imapMailFolderSink->UpdateImapMailboxStatus(this, new_spec);
}


void nsImapProtocol::OnListFolder(const char * aSourceMailbox, PRBool aBool)
{
  List(aSourceMailbox, aBool);
}


// Returns PR_TRUE if the mailbox is a NoSelect mailbox.
// If we don't know about it, returns PR_FALSE.
PRBool nsImapProtocol::MailboxIsNoSelectMailbox(const char *mailboxName)
{
  PRBool rv = PR_FALSE;

  nsIMAPNamespace *nsForMailbox = nsnull;
    m_hostSessionList->GetNamespaceForMailboxForHost(GetImapServerKey(),
                                                     mailboxName, nsForMailbox);
  // NS_ASSERTION (nsForMailbox, "Oops .. null nsForMailbox\n");

  char *name;

  if (nsForMailbox)
    m_runningUrl->AllocateCanonicalPath(mailboxName,
                                            nsForMailbox->GetDelimiter(),
                                            &name);
  else
    m_runningUrl->AllocateCanonicalPath(mailboxName,
                                            kOnlineHierarchySeparatorUnknown, 
                                            &name);

  if (!name)
    return PR_FALSE;

  m_imapServerSink->FolderIsNoSelect(name, &rv);

  PL_strfree(name);
  return rv;
}

// returns PR_TRUE is the delete succeeded (regardless of subscription changes)
PRBool nsImapProtocol::DeleteMailboxRespectingSubscriptions(const char *mailboxName)
{
  PRBool rv = PR_TRUE;
  if (!MailboxIsNoSelectMailbox(mailboxName))
  {
    // Only try to delete it if it really exists
    DeleteMailbox(mailboxName);
    rv = GetServerStateParser().LastCommandSuccessful();
  }

  // We can unsubscribe even if the mailbox doesn't exist.
  if (rv && m_autoUnsubscribe) // auto-unsubscribe is on
  {
    PRBool reportingErrors = GetServerStateParser().GetReportingErrors();
    GetServerStateParser().SetReportingErrors(PR_FALSE);
    Unsubscribe(mailboxName);
    GetServerStateParser().SetReportingErrors(reportingErrors);

  }
  return (rv);
}

// returns PR_TRUE is the rename succeeded (regardless of subscription changes)
// reallyRename tells us if we should really do the rename (PR_TRUE) or if we should just move subscriptions (PR_FALSE)
PRBool nsImapProtocol::RenameMailboxRespectingSubscriptions(const char *existingName, const char *newName, PRBool reallyRename)
{
  PRBool rv = PR_TRUE;
  if (reallyRename && !MailboxIsNoSelectMailbox(existingName))
  {
    RenameMailbox(existingName, newName);
    rv = GetServerStateParser().LastCommandSuccessful();
  }
  
  if (rv)
  {
    if (m_autoSubscribe)  // if auto-subscribe is on
    {
      PRBool reportingErrors = GetServerStateParser().GetReportingErrors();
      GetServerStateParser().SetReportingErrors(PR_FALSE);
      Subscribe(newName);
      GetServerStateParser().SetReportingErrors(reportingErrors);
    }
    if (m_autoUnsubscribe) // if auto-unsubscribe is on
    {
      PRBool reportingErrors = GetServerStateParser().GetReportingErrors();
      GetServerStateParser().SetReportingErrors(PR_FALSE);
      Unsubscribe(existingName);
      GetServerStateParser().SetReportingErrors(reportingErrors);
    }
  }
  return (rv);
}

PRBool nsImapProtocol::RenameHierarchyByHand(const char *oldParentMailboxName,
                                             const char *newParentMailboxName)
{
  PRBool renameSucceeded = PR_TRUE;
    char onlineDirSeparator = kOnlineHierarchySeparatorUnknown;
  m_deletableChildren = new nsVoidArray();

  PRBool nonHierarchicalRename = 
        ((GetServerStateParser().GetCapabilityFlag() & kNoHierarchyRename)
         || MailboxIsNoSelectMailbox(oldParentMailboxName));

  if (m_deletableChildren)
  {
    m_hierarchyNameState = kDeleteSubFoldersInProgress;
    nsIMAPNamespace *ns = nsnull;
        m_hostSessionList->GetNamespaceForMailboxForHost(GetImapServerKey(), 
                                                         oldParentMailboxName,
                                                         ns); // for delimiter
    if (!ns)
    {
      if (!PL_strcasecmp(oldParentMailboxName, "INBOX"))
                m_hostSessionList->GetDefaultNamespaceOfTypeForHost(GetImapServerKey(), 
                                                                    kPersonalNamespace,
                                                                    ns);
    }
    if (ns)
    {
            nsCString pattern(oldParentMailboxName);
            pattern += ns->GetDelimiter();
            pattern += "*";
            PRBool isUsingSubscription = PR_FALSE;
            m_hostSessionList->GetHostIsUsingSubscription(GetImapServerKey(),
                                                          isUsingSubscription);

            if (isUsingSubscription)
                Lsub(pattern.get(), PR_FALSE);
            else
                List(pattern.get(), PR_FALSE);
    }
    m_hierarchyNameState = kNoOperationInProgress;
    
    if (GetServerStateParser().LastCommandSuccessful())
      renameSucceeded = // rename this, and move subscriptions 
                RenameMailboxRespectingSubscriptions(oldParentMailboxName,
                                                     newParentMailboxName, PR_TRUE);
    
    PRInt32 numberToDelete = m_deletableChildren->Count();
        PRInt32 childIndex;
    
    for (childIndex = 0; 
             (childIndex < numberToDelete) && renameSucceeded; childIndex++)
    {
      // the imap parser has already converted to a non UTF7 string in the canonical
      // format so convert it back
      char *currentName = (char *) m_deletableChildren->ElementAt(childIndex);
        if (currentName)
        {
          char *serverName = nsnull;
          m_runningUrl->AllocateServerPath(currentName,
                                           onlineDirSeparator,
                                           &serverName);
          char *convertedName = serverName ? 
              CreateUtf7ConvertedString(serverName, PR_TRUE) : (char *)NULL;
          PR_FREEIF(serverName);
          PR_FREEIF(currentName);
          currentName = convertedName;
        }
        
        // calculate the new name and do the rename
        nsCString newChildName(newParentMailboxName);
        newChildName += (currentName + PL_strlen(oldParentMailboxName));
        RenameMailboxRespectingSubscriptions(currentName,
                                             newChildName.get(), 
                                             nonHierarchicalRename);  
        // pass in xNonHierarchicalRename to determine if we should really
        // reanme, or just move subscriptions
        renameSucceeded = GetServerStateParser().LastCommandSuccessful();
        PR_FREEIF(currentName);
    }
                
    delete m_deletableChildren;
    m_deletableChildren = nsnull;
  }
  
  return renameSucceeded;
}

PRBool nsImapProtocol::DeleteSubFolders(const char* selectedMailbox)
{
  PRBool deleteSucceeded = PR_TRUE;
  m_deletableChildren = new nsVoidArray();
  
  if (m_deletableChildren)
  {
    m_hierarchyNameState = kDeleteSubFoldersInProgress;
        nsCString pattern(selectedMailbox);
        char onlineDirSeparator = kOnlineHierarchySeparatorUnknown;
        m_runningUrl->GetOnlineSubDirSeparator(&onlineDirSeparator);
        pattern.Append(onlineDirSeparator);
        pattern.Append('*');

    if (pattern.Length())
    {
      List(pattern.get(), PR_FALSE);
    }
    m_hierarchyNameState = kNoOperationInProgress;
    
    // this should be a short list so perform a sequential search for the
    // longest name mailbox.  Deleting the longest first will hopefully
        // prevent the server from having problems about deleting parents
        // ** jt - why? I don't understand this.
    PRInt32 numberToDelete = m_deletableChildren->Count();
    PRInt32 outerIndex, innerIndex;
		
    deleteSucceeded = GetServerStateParser().LastCommandSuccessful();
    for (outerIndex = 0; 
         (outerIndex < numberToDelete) && deleteSucceeded;
         outerIndex++)
    {
        char* longestName = nsnull;
        for (innerIndex = 0; 
             innerIndex < m_deletableChildren->Count();
             innerIndex++)
        {
            char *currentName = 
                (char *) m_deletableChildren->ElementAt(innerIndex);
            if (!longestName || 
                PL_strlen(longestName) < PL_strlen(currentName))
            {
                longestName = currentName;
            }
        }
        m_deletableChildren->RemoveElement(longestName);
      
        // the imap parser has already converted to a non UTF7 string in
        // the canonical format so convert it back
        if (longestName)
        {
            char *serverName = nsnull;
            m_runningUrl->AllocateServerPath(longestName,
                                             onlineDirSeparator,
                                             &serverName);
            PR_FREEIF(longestName);
            longestName = serverName;
        }
      
      // some imap servers include the selectedMailbox in the list of 
      // subfolders of the selectedMailbox.  Check for this so we don't
            // delete the selectedMailbox (usually the trash and doing an
            // empty trash)       
      // The Cyrus imap server ignores the "INBOX.Trash" constraining
            // string passed to the list command.  Be defensive and make sure
            // we only delete children of the trash
      if (longestName && 
        PL_strcmp(selectedMailbox, longestName) &&
        !PL_strncmp(selectedMailbox, longestName,
                            PL_strlen(selectedMailbox)))
      {
          nsCOMPtr<nsIImapIncomingServer> imapServer =
              do_QueryReferent(m_server);
          if (imapServer)
              imapServer->ResetConnection(longestName);
          PRBool deleted =
              DeleteMailboxRespectingSubscriptions(longestName);
          if (deleted)
              FolderDeleted(longestName);
          deleteSucceeded = deleted;
      }
      PR_FREEIF(longestName);
    }
  
    delete m_deletableChildren;
    m_deletableChildren = nsnull;
  }
  return deleteSucceeded;
}

void nsImapProtocol::FolderDeleted(const char *mailboxName)
{
    char onlineDelimiter = kOnlineHierarchySeparatorUnknown;
    char *orphanedMailboxName = nsnull;

    if (mailboxName)
    {
        m_runningUrl->AllocateCanonicalPath(mailboxName, onlineDelimiter,
                                            &orphanedMailboxName);
    if (m_imapServerSink)
      m_imapServerSink->OnlineFolderDelete(orphanedMailboxName);
    }

    PR_FREEIF(orphanedMailboxName);
}

void nsImapProtocol::FolderNotCreated(const char *folderName)
{
    if (folderName && m_imapServerSink)
        m_imapServerSink->OnlineFolderCreateFailed(folderName);
}

void nsImapProtocol::FolderRenamed(const char *oldName,
                                   const char *newName)
{
    char onlineDelimiter = kOnlineHierarchySeparatorUnknown;

    if ((m_hierarchyNameState == kNoOperationInProgress) ||
    (m_hierarchyNameState == kListingForInfoAndDiscovery))

    {
		nsXPIDLCString canonicalOldName, canonicalNewName;
            m_runningUrl->AllocateCanonicalPath(oldName,
                                                onlineDelimiter,
                                                getter_Copies(canonicalOldName));
            m_runningUrl->AllocateCanonicalPath(newName,
                                                onlineDelimiter,
                                                getter_Copies(canonicalNewName));
            nsCOMPtr<nsIMsgWindow> msgWindow;
            GetMsgWindow(getter_AddRefs(msgWindow));
            m_imapServerSink->OnlineFolderRename(msgWindow, canonicalOldName, canonicalNewName);
    }
}

void nsImapProtocol::OnDeleteFolder(const char * sourceMailbox)
{
    PRBool deleted = DeleteSubFolders(sourceMailbox);
    if (deleted)
        deleted = DeleteMailboxRespectingSubscriptions(sourceMailbox);
  if (deleted)
    FolderDeleted(sourceMailbox);
}

void nsImapProtocol::OnRenameFolder(const char * sourceMailbox)
{
  char *destinationMailbox = OnCreateServerDestinationFolderPathString();

  if (destinationMailbox)
  {
    PRBool renamed = RenameHierarchyByHand(sourceMailbox, destinationMailbox);
    if (renamed)
      FolderRenamed(sourceMailbox, destinationMailbox);
        
    PR_Free( destinationMailbox);
  }
  else
    HandleMemoryFailure();
}

void nsImapProtocol::OnMoveFolderHierarchy(const char * sourceMailbox)
{
  char *destinationMailbox = OnCreateServerDestinationFolderPathString();

    if (destinationMailbox)
    {
        nsCString newBoxName;
        char onlineDirSeparator = kOnlineHierarchySeparatorUnknown;

        m_runningUrl->GetOnlineSubDirSeparator(&onlineDirSeparator);
        newBoxName = destinationMailbox;

        nsCString oldBoxName(sourceMailbox);
        PRInt32 leafStart = oldBoxName.RFindChar(onlineDirSeparator);
        PRInt32 length = oldBoxName.Length();
        nsCString leafName;

        if (-1 == leafStart)
            leafName = oldBoxName;  // this is a root level box
        else
            oldBoxName.Right(leafName, length-(leafStart+1));

        if ( newBoxName.Length() > 0 )
             newBoxName.Append(onlineDirSeparator);
        newBoxName.Append(leafName);
        PRBool  renamed = RenameHierarchyByHand(sourceMailbox,
                                                newBoxName.get());
        if (renamed)
            FolderRenamed(sourceMailbox, newBoxName.get());
    }
    else
      HandleMemoryFailure();
}

void nsImapProtocol::FindMailboxesIfNecessary()
{
    //PR_EnterMonitor(fFindingMailboxesMonitor);
    // biff should not discover mailboxes
  PRBool foundMailboxesAlready = PR_FALSE;
  nsImapAction imapAction;
  nsresult rv = NS_OK;

  // need to do this for every connection in order to see folders.
#ifdef DOING_PSEUDO_MAILBOXES
  // check if this is an aol web mail server by checking for the host name the account wizard sets
  // up for an aol web mail account - the host name itself is not used, but that's what we set it to, 
  // so compare against it. A better solution would be to have the wizard set a special pref property on the
  // server and perhaps we should do that for RTM
  if (GetServerStateParser().ServerIsAOLServer() && GetImapHostName() && !nsCRT::strcmp(GetImapHostName(), "imap.mail.aol.com"))
    XAOL_Option("+READMBOX");
#endif

  rv = m_runningUrl->GetImapAction(&imapAction);
  rv = m_hostSessionList->GetHaveWeEverDiscoveredFoldersForHost(GetImapServerKey(), foundMailboxesAlready);
    if (NS_SUCCEEDED(rv) && !foundMailboxesAlready &&
    (imapAction != nsIImapUrl::nsImapBiff) &&
    (imapAction != nsIImapUrl::nsImapDiscoverAllBoxesUrl) &&
    (imapAction != nsIImapUrl::nsImapUpgradeToSubscription) &&
    !GetSubscribingNow())
    {
    DiscoverMailboxList();

    // If we decide to do it, here is where we should check to see if
    // a namespace exists (personal namespace only?) and possibly
    // create it if it doesn't exist.
  }
    //PR_ExitMonitor(fFindingMailboxesMonitor);
}

void nsImapProtocol::DiscoverAllAndSubscribedBoxes()
{
	// used for subscribe pane
	// iterate through all namespaces
    PRUint32 count = 0;
    m_hostSessionList->GetNumberOfNamespacesForHost(GetImapServerKey(), count);

	for (PRUint32 i = 0; i < count; i++ )
	{
		nsIMAPNamespace *ns = nsnull;

        m_hostSessionList->GetNamespaceNumberForHost(GetImapServerKey(), i,
                                                     ns);
		if (ns &&
			gHideOtherUsersFromList ? (ns->GetType() != kOtherUsersNamespace)
            : PR_TRUE)
		{
			const char *prefix = ns->GetPrefix();
			if (prefix)
			{
				if (!gHideUnusedNamespaces && *prefix &&
                    PL_strcasecmp(prefix, "INBOX.")) /* only do it for
                     non-empty namespace prefixes */
				{
					// Explicitly discover each Namespace, just so they're
                    // there in the subscribe UI 
					nsImapMailboxSpec *boxSpec = new nsImapMailboxSpec;
					if (boxSpec)
					{
                        NS_ADDREF(boxSpec);
						boxSpec->folderSelected = PR_FALSE;
						boxSpec->hostName = nsCRT::strdup(GetImapHostName());
						boxSpec->connection = this;
						boxSpec->flagState = nsnull;
						boxSpec->discoveredFromLsub = PR_TRUE;
						boxSpec->onlineVerified = PR_TRUE;
						boxSpec->box_flags = kNoselect;
						boxSpec->hierarchySeparator = ns->GetDelimiter();
						m_runningUrl->AllocateCanonicalPath(
                            ns->GetPrefix(), ns->GetDelimiter(),
                            &boxSpec->allocatedPathName);
						boxSpec->namespaceForFolder = ns;
                        boxSpec->box_flags |= kNameSpace;

						switch (ns->GetType())
						{
						case kPersonalNamespace:
							boxSpec->box_flags |= kPersonalMailbox;
							break;
						case kPublicNamespace:
							boxSpec->box_flags |= kPublicMailbox;
							break;
						case kOtherUsersNamespace:
							boxSpec->box_flags |= kOtherUsersMailbox;
							break;
						default:	// (kUnknownNamespace)
							break;
						}

						DiscoverMailboxSpec(boxSpec);
					}
					else
						HandleMemoryFailure();
				}

                nsCAutoString allPattern(prefix);
                allPattern += '*';

                nsCAutoString topLevelPattern(prefix);
                topLevelPattern += '%';

                nsCAutoString secondLevelPattern;

				char delimiter = ns->GetDelimiter();
				if (delimiter)
				{
					// Hierarchy delimiter might be NIL, in which case there's no hierarchy anyway
                    secondLevelPattern = prefix;
                    secondLevelPattern += '%';
                    secondLevelPattern += delimiter;
                    secondLevelPattern += '%';
				}

				nsresult rv;
    			nsCOMPtr<nsIImapIncomingServer> imapServer = do_QueryReferent(m_server, &rv);
				if (NS_FAILED(rv) || !imapServer) return;

				if (allPattern.Length())
				{
					imapServer->SetDoingLsub(PR_TRUE);
					Lsub(allPattern.get(), PR_TRUE);	// LSUB all the subscribed
				}
				if (topLevelPattern.Length())
				{
					imapServer->SetDoingLsub(PR_FALSE);
					List(topLevelPattern.get(), PR_TRUE);	// LIST the top level
				}
				if (secondLevelPattern.Length())
				{
					imapServer->SetDoingLsub(PR_FALSE);
					List(secondLevelPattern.get(), PR_TRUE);	// LIST the second level
				}
			}
		}
	}
}

// DiscoverMailboxList() is used to actually do the discovery of folders
// for a host.  This is used both when we initially start up (and re-sync)
// and also when the user manually requests a re-sync, by collapsing and
// expanding a host in the folder pane.  This is not used for the subscribe
// pane.
// DiscoverMailboxList() also gets the ACLs for each newly discovered folder
void nsImapProtocol::DiscoverMailboxList()
{
  PRBool usingSubscription = PR_FALSE;

  SetMailboxDiscoveryStatus(eContinue);
  if (GetServerStateParser().ServerHasACLCapability())
    m_hierarchyNameState = kListingForInfoAndDiscovery;
  else
    m_hierarchyNameState = kNoOperationInProgress;

  // Pretend that the Trash folder doesn't exist, so we will rediscover it if we need to.
  m_hostSessionList->SetOnlineTrashFolderExistsForHost(GetImapServerKey(), PR_FALSE);
  m_hostSessionList->GetHostIsUsingSubscription(GetImapServerKey(), usingSubscription);

  // iterate through all namespaces and LSUB them.
  PRUint32 count = 0;
  m_hostSessionList->GetNumberOfNamespacesForHost(GetImapServerKey(), count);
  for (PRUint32 i = 0; i < count; i++ )
  {
    nsIMAPNamespace * ns = nsnull;
    m_hostSessionList->GetNamespaceNumberForHost(GetImapServerKey(),i,ns);
    if (ns)
    {
      const char *prefix = ns->GetPrefix();
      if (prefix)
      {
        // static PRBool gHideUnusedNamespaces = PR_TRUE;
        // mscott -> WARNING!!! i where are we going to get this
                // global variable for unusued name spaces from??? *wince*
        // dmb - we should get this from a per-host preference,
        // I'd say. But for now, just make it PR_TRUE;
        if (!gHideUnusedNamespaces && *prefix &&
                    PL_strcasecmp(prefix, "INBOX."))  // only do it for
                    // non-empty namespace prefixes, and for non-INBOX prefix 
        {
          // Explicitly discover each Namespace, so that we can
                    // create subfolders of them,
          nsImapMailboxSpec *boxSpec = new nsImapMailboxSpec; 
          if (boxSpec)
          {
			NS_ADDREF(boxSpec);
            boxSpec->folderSelected = PR_FALSE;
            boxSpec->hostName = nsCRT::strdup(GetImapHostName());
            boxSpec->connection = this;
            boxSpec->flagState = nsnull;
            boxSpec->discoveredFromLsub = PR_TRUE;
            boxSpec->onlineVerified = PR_TRUE;
            boxSpec->box_flags = kNoselect;
            boxSpec->hierarchySeparator = ns->GetDelimiter();
            m_runningUrl->AllocateCanonicalPath(
                            ns->GetPrefix(), ns->GetDelimiter(), 
                            &boxSpec->allocatedPathName);  
            boxSpec->namespaceForFolder = ns;
            boxSpec->box_flags |= kNameSpace;

            switch (ns->GetType())
            {
            case kPersonalNamespace:
              boxSpec->box_flags |= kPersonalMailbox;
              break;
            case kPublicNamespace:
              boxSpec->box_flags |= kPublicMailbox;
              break;
            case kOtherUsersNamespace:
              boxSpec->box_flags |= kOtherUsersMailbox;
              break;
            default:  // (kUnknownNamespace)
              break;
            }

            DiscoverMailboxSpec(boxSpec);
          }
          else
            HandleMemoryFailure();
        }

        // now do the folders within this namespace
        nsCString pattern;
        nsCString pattern2;
        if (usingSubscription)
        {
          pattern.Append(prefix);
          pattern.Append("*");
        }
        else
        {
          pattern.Append(prefix);
          pattern.Append("%"); // mscott just need one percent right?
          // pattern = PR_smprintf("%s%%", prefix);
          char delimiter = ns->GetDelimiter();
          if (delimiter)
          {
            // delimiter might be NIL, in which case there's no hierarchy anyway
            pattern2 = prefix;
            pattern2 += "%";
            pattern2 += delimiter;
            pattern2 += "%";
            // pattern2 = PR_smprintf("%s%%%c%%", prefix, delimiter);
          }
        }


        if (usingSubscription) // && !GetSubscribingNow())  should never get here from subscribe pane
          Lsub(pattern.get(), PR_TRUE);
        else
        {
          List(pattern.get(), PR_TRUE);
          List(pattern2.get(), PR_TRUE);
        }
      }
    }
  }

  // explicitly LIST the INBOX if (a) we're not using subscription, or (b) we are using subscription and
  // the user wants us to always show the INBOX.
  PRBool listInboxForHost = PR_FALSE;
  m_hostSessionList->GetShouldAlwaysListInboxForHost(GetImapServerKey(), listInboxForHost);
  if (!usingSubscription || listInboxForHost) 
    List("INBOX", PR_TRUE);

  m_hierarchyNameState = kNoOperationInProgress;

  MailboxDiscoveryFinished();

  // Get the ACLs for newly discovered folders
  if (GetServerStateParser().ServerHasACLCapability())
  {
    PRInt32 total = m_listedMailboxList.Count(), cnt = 0;
    // Let's not turn this off here, since we don't turn it on after
    // GetServerStateParser().SetReportingErrors(PR_FALSE);
    if (total)
    {
      ProgressEventFunctionUsingId(IMAP_GETTING_ACL_FOR_FOLDER);
      nsIMAPMailboxInfo * mb = nsnull;
      do
      {
        mb = (nsIMAPMailboxInfo *) m_listedMailboxList[0]; // get top element
        m_listedMailboxList.RemoveElementAt(0); // XP_ListRemoveTopObject(fListedMailboxList);
        if (mb)
        {
          if (FolderNeedsACLInitialized(mb->GetMailboxName()))
          {
            char *onlineName = nsnull;
            m_runningUrl->AllocateServerPath(mb->GetMailboxName(), mb->GetDelimiter(), &onlineName);
            if (onlineName)
            {
              OnRefreshACLForFolder(onlineName);
              PR_Free(onlineName);
            }
          }
          PercentProgressUpdateEvent(NULL, cnt, total);
          delete mb;  // this is the last time we're using the list, so delete the entries here
          cnt++;
        }
      } while (mb && !DeathSignalReceived());
    }
  }
}

PRBool nsImapProtocol::FolderNeedsACLInitialized(const char *folderName)
{
  PRBool rv = PR_FALSE;

  char *name = PL_strdup(folderName);
  if (!name)
    return PR_FALSE;
  // mscott - big hack...where do we get a IMAPACLRights object from??????
//  m_imapServerSink->FolderNeedsACLInitialized(name, nsnull);

  PR_Free(name);
  return rv;
}

void nsImapProtocol::MailboxDiscoveryFinished()
{
    if (!DeathSignalReceived() && !GetSubscribingNow() &&
    ((m_hierarchyNameState == kNoOperationInProgress) || 
     (m_hierarchyNameState == kListingForInfoAndDiscovery)))
    {
    nsIMAPNamespace *ns = nsnull;
    m_hostSessionList->GetDefaultNamespaceOfTypeForHost(GetImapServerKey(), kPersonalNamespace, ns);
    const char *personalDir = ns ? ns->GetPrefix() : 0;
    
    PRBool trashFolderExists = PR_FALSE;
    PRBool usingSubscription = PR_FALSE;
    m_hostSessionList->GetOnlineTrashFolderExistsForHost(GetImapServerKey(), trashFolderExists);
    m_hostSessionList->GetHostIsUsingSubscription(GetImapServerKey(),usingSubscription);
    if (!trashFolderExists && GetDeleteIsMoveToTrash() && usingSubscription)
    {
      // maybe we're not subscribed to the Trash folder
      if (personalDir)
      {
        char *originalTrashName = CreatePossibleTrashName(personalDir);
        m_hierarchyNameState = kDiscoverTrashFolderInProgress;
        List(originalTrashName, PR_TRUE);
        m_hierarchyNameState = kNoOperationInProgress;
      }
    }

    // There is no Trash folder (either LIST'd or LSUB'd), and we're using the
    // Delete-is-move-to-Trash model, and there is a personal namespace
    if (!trashFolderExists && GetDeleteIsMoveToTrash() && ns)
      {
        char *trashName = CreatePossibleTrashName(ns->GetPrefix());
        if (trashName)
      {
        char *onlineTrashName = nsnull;
        m_runningUrl->AllocateServerPath(trashName, ns->GetDelimiter(), &onlineTrashName);
        if (onlineTrashName)
        {
            GetServerStateParser().SetReportingErrors(PR_FALSE);
            PRBool created = CreateMailboxRespectingSubscriptions(onlineTrashName);
            GetServerStateParser().SetReportingErrors(PR_TRUE);
            
            // force discovery of new trash folder.
            if (created)
          {
            m_hierarchyNameState = kDiscoverTrashFolderInProgress;
              List(onlineTrashName, PR_FALSE);
            m_hierarchyNameState = kNoOperationInProgress;
          }
            else
            m_hostSessionList->SetOnlineTrashFolderExistsForHost(GetImapServerKey(), PR_TRUE);
          PR_Free(onlineTrashName);
        }
          PR_FREEIF(trashName);
      } // if trash name
    } //if trashg folder doesn't exist
    m_hostSessionList->SetHaveWeEverDiscoveredFoldersForHost(GetImapServerKey(), PR_TRUE);

    // notify front end that folder discovery is complete....
    if (m_imapServerSink)
        m_imapServerSink->DiscoveryDone();
    }
}

// returns PR_TRUE is the create succeeded (regardless of subscription changes)
PRBool nsImapProtocol::CreateMailboxRespectingSubscriptions(const char *mailboxName)
{
  CreateMailbox(mailboxName);
  PRBool rv = GetServerStateParser().LastCommandSuccessful();
  if (rv)
  {
    if (m_autoSubscribe) // auto-subscribe is on
    {
      // create succeeded - let's subscribe to it
      PRBool reportingErrors = GetServerStateParser().GetReportingErrors();
      GetServerStateParser().SetReportingErrors(PR_FALSE);
      OnSubscribe(mailboxName);
      GetServerStateParser().SetReportingErrors(reportingErrors);
    }
  }
  return (rv);
}

void nsImapProtocol::CreateMailbox(const char *mailboxName)
{
    ProgressEventFunctionUsingId (IMAP_STATUS_CREATING_MAILBOX);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
  nsCString command(GetServerCommandTag());
  command += " create \"";
  command += escapedName;
  command += "\""CRLF;
               
    nsMemory::Free(escapedName);

  nsresult rv = SendData(command.get());
    if(NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::DeleteMailbox(const char *mailboxName)
{

  // check if this connection currently has the folder to be deleted selected.
  // If so, we should close it because at least some UW servers don't like you deleting
  // a folder you have open.
  if (GetServerStateParser().GetIMAPstate() ==
      nsImapServerResponseParser::kFolderSelected && GetServerStateParser().GetSelectedMailboxName() && 
      PL_strcmp(GetServerStateParser().GetSelectedMailboxName(),
                    mailboxName) == 0)
    Close();
  
  
  ProgressEventFunctionUsingIdWithString (IMAP_STATUS_DELETING_MAILBOX, mailboxName);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    nsCString command(GetServerCommandTag());
    command += " delete \"";
    command += escapedName;
    command += "\"" CRLF;
    nsMemory::Free(escapedName);
    
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::RenameMailbox(const char *existingName, 
                                   const char *newName)
{
    // just like DeleteMailbox; Some UW servers don't like it.
    if (GetServerStateParser().GetIMAPstate() ==
      nsImapServerResponseParser::kFolderSelected && GetServerStateParser().GetSelectedMailboxName() && 
      PL_strcmp(GetServerStateParser().GetSelectedMailboxName(),
                    existingName) == 0)
      Close();

    ProgressEventFunctionUsingIdWithString (IMAP_STATUS_RENAMING_MAILBOX, existingName);
    
  IncrementCommandTagNumber();
    
    char *escapedExistingName = CreateEscapedMailboxName(existingName);
    char *escapedNewName = CreateEscapedMailboxName(newName);
    nsCString command(GetServerCommandTag());
    command += " rename \"";
    command += escapedExistingName;
    command += "\" \"";
    command += escapedNewName;
    command += "\"" CRLF;
    nsMemory::Free(escapedExistingName);
    nsMemory::Free(escapedNewName);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

char * nsImapProtocol::CreatePossibleTrashName(const char *prefix)
{
  // mscott we used to have a localized global string for the trash name...
  // I haven't don't localization stuff yet so I'm going to do a bad thing and just
  // use a string literal....(only temporary!!!!! =))...

//  IMAP_LoadTrashFolderName();
  nsCString returnTrash(prefix);

  returnTrash += "Trash";
  return ToNewCString(returnTrash);
}

void nsImapProtocol::Lsub(const char *mailboxPattern, PRBool addDirectoryIfNecessary)
{
    ProgressEventFunctionUsingId (IMAP_STATUS_LOOKING_FOR_MAILBOX);

    IncrementCommandTagNumber();

  char *boxnameWithOnlineDirectory = nsnull;
  if (addDirectoryIfNecessary)
    m_runningUrl->AddOnlineDirectoryIfNecessary(mailboxPattern, &boxnameWithOnlineDirectory);

    char *escapedPattern = CreateEscapedMailboxName(boxnameWithOnlineDirectory ? 
                          boxnameWithOnlineDirectory :
                          mailboxPattern);

  nsCString command (GetServerCommandTag());
  command += " lsub \"\" \"";
  command += escapedPattern;
  command += "\""CRLF;

//    PR_snprintf(GetOutputBuffer(),                              // string to create
//            kOutputBufferSize,                              // max size
//            "%s lsub \"\" \"%s\"" CRLF,                   // format string
//            GetServerCommandTag(),                  // command tag
//            escapedPattern);
            
  nsMemory::Free(escapedPattern);
  PR_FREEIF(boxnameWithOnlineDirectory);

  nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::List(const char *mailboxPattern, PRBool addDirectoryIfNecessary)
{
    ProgressEventFunctionUsingId (IMAP_STATUS_LOOKING_FOR_MAILBOX);

    IncrementCommandTagNumber();

  char *boxnameWithOnlineDirectory = nsnull;
  if (addDirectoryIfNecessary)
    m_runningUrl->AddOnlineDirectoryIfNecessary(mailboxPattern, &boxnameWithOnlineDirectory);

    char *escapedPattern = CreateEscapedMailboxName(boxnameWithOnlineDirectory ? 
                          boxnameWithOnlineDirectory :
                          mailboxPattern);

  nsCString command (GetServerCommandTag());
  command += " list \"\" \"";
  command += escapedPattern;
  command += "\""CRLF;

//    PR_snprintf(GetOutputBuffer(),                              // string to create
//            kOutputBufferSize,                              // max size
//            "%s list \"\" \"%s\"" CRLF,                   // format string
//            GetServerCommandTag(),                  // command tag
//            escapedPattern);
            
    nsMemory::Free(escapedPattern);
  PR_FREEIF(boxnameWithOnlineDirectory);

  nsresult rv = SendData(command.get());  
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Subscribe(const char *mailboxName)
{
    ProgressEventFunctionUsingIdWithString (IMAP_STATUS_SUBSCRIBE_TO_MAILBOX, mailboxName);

    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);

  nsCString command (GetServerCommandTag());
  command += " subscribe \"";
  command += escapedName;
  command += "\""CRLF;
    nsMemory::Free(escapedName);

  nsresult rv = SendData(command.get());  
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Unsubscribe(const char *mailboxName)
{
    ProgressEventFunctionUsingIdWithString (IMAP_STATUS_UNSUBSCRIBE_MAILBOX, mailboxName);
    IncrementCommandTagNumber();
    
    char *escapedName = CreateEscapedMailboxName(mailboxName);
    
  nsCString command (GetServerCommandTag());
  command += " unsubscribe \"";
  command += escapedName;
  command += "\""CRLF;
    nsMemory::Free(escapedName);

  nsresult rv = SendData(command.get());  
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}


void nsImapProtocol::Search(const char * searchCriteria, 
                            PRBool useUID, 
                            PRBool notifyHit /* PR_TRUE */)
{
  m_notifySearchHit = notifyHit;
  ProgressEventFunctionUsingId (IMAP_STATUS_SEARCH_MAILBOX);
  IncrementCommandTagNumber();
    
  nsCString protocolString(GetServerCommandTag());
  // the searchCriteria string contains the 'search ....' string
  if (useUID)
     protocolString.Append(" uid");
  protocolString.Append(" ");
  protocolString.Append(searchCriteria);
  protocolString.Append(CRLF);;

  nsresult rv = SendData(protocolString.get());
  if (NS_SUCCEEDED(rv))
     ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Copy(const char * messageList,
                          const char *destinationMailbox, 
                          PRBool idsAreUid)
{
  IncrementCommandTagNumber();
  
  char *escapedDestination = CreateEscapedMailboxName(destinationMailbox);

  nsCAutoString protocolString(GetServerCommandTag());
  if (idsAreUid)
    protocolString.Append(" uid");
  protocolString.Append(" copy ");
  protocolString.Append(messageList);
  protocolString.Append(" \"");
  protocolString.Append(escapedDestination);
  protocolString.Append("\"" CRLF);
      
  nsresult rv = SendData(protocolString.get());
  if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail(protocolString.get());
        
  nsMemory::Free(escapedDestination);
}

void nsImapProtocol::NthLevelChildList(const char* onlineMailboxPrefix,
                                       PRInt32 depth)
{
  NS_ASSERTION (depth >= 0, 
                  "Oops ... depth must be equal or greater than 0");
  if (depth < 0) return;

  nsCString truncatedPrefix (onlineMailboxPrefix);
    PRUnichar slash = '/';
  if (truncatedPrefix.Last() == slash)
        truncatedPrefix.SetLength(truncatedPrefix.Length()-1);
    
    char *utf7ListArg = 
        CreateUtf7ConvertedString(truncatedPrefix.get(),PR_TRUE);
    if (utf7ListArg)
    {
        nsCString pattern(utf7ListArg);
        nsCString suffix;
        int count = 0;
        char separator = 0;
        m_runningUrl->GetOnlineSubDirSeparator(&separator);
        suffix.Assign(separator);
        suffix += '%';
        
        while (count < depth)
        {
            pattern += suffix;
            count++;
            List(pattern.get(), PR_FALSE);
        }
        PR_Free(utf7ListArg);
    }
}

void nsImapProtocol::ProcessAuthenticatedStateURL()
{
  nsImapAction imapAction;
  char * sourceMailbox = nsnull;
  m_runningUrl->GetImapAction(&imapAction);

  // switch off of the imap url action and take an appropriate action
  switch (imapAction)
  {
    case nsIImapUrl::nsImapLsubFolders:
      OnLSubFolders();
      return;
      break;
    case nsIImapUrl::nsImapGetMailAccountUrl:
      OnGetMailAccount();
      return;
      break;
    default: 
      break;
  }

#if 0
  // all of the other states require the following extra step...
    // even though we don't have to to be legal protocol, Close any select mailbox
    // so we don't miss any info these urls might change for the selected folder
    // (e.g. msg count after append)
  if (GetServerStateParser().GetIMAPstate() == nsImapServerResponseParser::kFolderSelected)
  {
    // now we should be avoiding an implicit Close because it performs an implicit Expunge
    // authenticated state urls should not use a cached connection that is in the selected state
    // However, append message can happen either in a selected state or authenticated state
    if (imapAction != nsIImapUrl::nsImapAppendMsgFromFile /* kAppendMsgFromFile */)
    {
      PR_ASSERT(PR_FALSE);
    }
  }
#endif 

  switch (imapAction)
  {
    case nsIImapUrl::nsImapOfflineToOnlineMove:
      OnOfflineToOnlineMove();
      break;
    case nsIImapUrl::nsImapAppendMsgFromFile:
      OnAppendMsgFromFile();
      break;
    case nsIImapUrl::nsImapDiscoverAllBoxesUrl:
      NS_ASSERTION (!GetSubscribingNow(),
                      "Oops ... should not get here from subscribe UI");
      DiscoverMailboxList();
      break;
    case nsIImapUrl::nsImapDiscoverAllAndSubscribedBoxesUrl:
      DiscoverAllAndSubscribedBoxes();
      break;
    case nsIImapUrl::nsImapCreateFolder:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnCreateFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapEnsureExistsFolder:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnEnsureExistsFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapDiscoverChildrenUrl:
        {
            char *canonicalParent = nsnull;
            m_runningUrl->CreateServerSourceFolderPathString(&canonicalParent);
            if (canonicalParent)
            {
        NthLevelChildList(canonicalParent, 2);
                PR_Free(canonicalParent);
            }
      break;
        }
    case nsIImapUrl::nsImapDiscoverLevelChildrenUrl:
        {
            char *canonicalParent = nsnull;
            m_runningUrl->CreateServerSourceFolderPathString(&canonicalParent);
      PRInt32 depth = 0;
            m_runningUrl->GetChildDiscoveryDepth(&depth);
        if (canonicalParent)
      {
        NthLevelChildList(canonicalParent, depth);
        if (GetServerStateParser().LastCommandSuccessful() &&
                    m_imapMailFolderSink)
          m_imapMailFolderSink->ChildDiscoverySucceeded(this);
        PR_Free(canonicalParent);
      }
      break;
        }
    case nsIImapUrl::nsImapSubscribe:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnSubscribe(sourceMailbox); // used to be called subscribe
      break;
    case nsIImapUrl::nsImapUnsubscribe: 
      sourceMailbox = OnCreateServerSourceFolderPathString();     
      OnUnsubscribe(sourceMailbox);
      break;
    case nsIImapUrl::nsImapRefreshACL:
      sourceMailbox = OnCreateServerSourceFolderPathString(); 
      OnRefreshACLForFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapRefreshAllACLs:
      OnRefreshAllACLs();
      break;
    case nsIImapUrl::nsImapListFolder:
      sourceMailbox = OnCreateServerSourceFolderPathString();  
      OnListFolder(sourceMailbox, PR_FALSE);
      break;
    case nsIImapUrl::nsImapFolderStatus:
      sourceMailbox = OnCreateServerSourceFolderPathString(); 
      OnStatusForFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapRefreshFolderUrls:
      sourceMailbox = OnCreateServerSourceFolderPathString();
#ifdef UNREADY_CODE
      XMailboxInfo(sourceMailbox);
      if (GetServerStateParser().LastCommandSuccessful()) 
        InitializeFolderUrl(sourceMailbox);
#endif
      break;
    case nsIImapUrl::nsImapDeleteFolder:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnDeleteFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapRenameFolder:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnRenameFolder(sourceMailbox);
      break;
    case nsIImapUrl::nsImapMoveFolderHierarchy:
      sourceMailbox = OnCreateServerSourceFolderPathString();
      OnMoveFolderHierarchy(sourceMailbox);
      break;
    default:
      break;
  }

  PR_FREEIF(sourceMailbox);
}

void nsImapProtocol::ProcessAfterAuthenticated()
{
  // mscott: ignore admin url stuff for now...
  // if we're a netscape server, and we haven't got the admin url, get it
  PRBool hasAdminUrl = PR_TRUE;

  if (NS_SUCCEEDED(m_hostSessionList->GetHostHasAdminURL(GetImapServerKey(), hasAdminUrl)) 
    && !hasAdminUrl)
  {
    if (GetServerStateParser().ServerHasServerInfo())
    {
      XServerInfo();
      if (GetServerStateParser().LastCommandSuccessful() && m_imapServerSink) 
      {
        m_imapServerSink->SetMailServerUrls(GetServerStateParser().GetMailAccountUrl(),
          GetServerStateParser().GetManageListsUrl(),
          GetServerStateParser().GetManageFiltersUrl());
      }
    }
    else if (GetServerStateParser().ServerIsNetscape3xServer())
    {
      Netscape();
      if (GetServerStateParser().LastCommandSuccessful() && m_imapServerSink) 
      {
        m_imapServerSink->SetMailServerUrls(GetServerStateParser().GetMailAccountUrl(), nsnull, nsnull);
      }
    }
  }

  if (GetServerStateParser().ServerHasNamespaceCapability())
  {
    PRBool nameSpacesOverridable = PR_FALSE;
    PRBool haveNameSpacesForHost = PR_FALSE;
    m_hostSessionList->GetNamespacesOverridableForHost(GetImapServerKey(), nameSpacesOverridable);
    m_hostSessionList->GetGotNamespacesForHost(GetImapServerKey(), haveNameSpacesForHost); 

  // mscott: VERIFY THIS CLAUSE!!!!!!!
    if (nameSpacesOverridable && !haveNameSpacesForHost)
    {
      Namespace();
    }
  }
}

void nsImapProtocol::SetupMessageFlagsString(nsCString& flagString,
                                             imapMessageFlagsType flags,
                                             PRUint16 userFlags)
{
    if (flags & kImapMsgSeenFlag)
        flagString.Append("\\Seen ");
    if (flags & kImapMsgAnsweredFlag)
        flagString.Append("\\Answered ");
    if (flags & kImapMsgFlaggedFlag)
        flagString.Append("\\Flagged ");
    if (flags & kImapMsgDeletedFlag)
        flagString.Append("\\Deleted ");
    if (flags & kImapMsgDraftFlag)
        flagString.Append("\\Draft ");
    if (flags & kImapMsgRecentFlag)
        flagString.Append("\\Recent ");
    if ((flags & kImapMsgForwardedFlag) && 
        (userFlags & kImapMsgSupportForwardedFlag))
        flagString.Append("$Forwarded "); // Not always available
    if ((flags & kImapMsgMDNSentFlag) && (
        userFlags & kImapMsgSupportMDNSentFlag))
        flagString.Append("$MDNSent "); // Not always available
    
    // eat the last space
    if (flagString.Length() > 0)
        flagString.SetLength(flagString.Length()-1);
}

void nsImapProtocol::ProcessStoreFlags(const char * messageIdsString,
                                                 PRBool idsAreUids,
                                                 imapMessageFlagsType flags,
                                                 PRBool addFlags)
{
  if (!flags)
    return;
    
  nsCString flagString;

  uint16 userFlags = GetServerStateParser().SupportsUserFlags();
  uint16 settableFlags = GetServerStateParser().SettablePermanentFlags();

  if (!(flags & userFlags) && !(flags & settableFlags))
    return;         // if cannot set any of the flags bail out
    
  if (addFlags)
      flagString = "+Flags (";
  else
      flagString = "-Flags (";
    
  if (flags & kImapMsgSeenFlag && kImapMsgSeenFlag & settableFlags)
      flagString .Append("\\Seen ");
  if (flags & kImapMsgAnsweredFlag && kImapMsgAnsweredFlag & settableFlags)
      flagString .Append("\\Answered ");
  if (flags & kImapMsgFlaggedFlag && kImapMsgFlaggedFlag & settableFlags)
      flagString .Append("\\Flagged ");
  if (flags & kImapMsgDeletedFlag && kImapMsgDeletedFlag & settableFlags)
      flagString .Append("\\Deleted ");
  if (flags & kImapMsgDraftFlag && kImapMsgDraftFlag & settableFlags)
      flagString .Append("\\Draft ");
  if (flags & kImapMsgForwardedFlag && kImapMsgSupportForwardedFlag & userFlags)
        flagString .Append("$Forwarded ");  // if supported
  if (flags & kImapMsgMDNSentFlag && kImapMsgSupportMDNSentFlag & userFlags)
        flagString .Append("$MDNSent ");  // if supported

    // replace the final space with ')'
    flagString.SetCharAt(')',flagString.Length() - 1);;
    
    Store(messageIdsString, flagString.get(), idsAreUids);
}


void nsImapProtocol::Close()
{
    IncrementCommandTagNumber();

  nsCString command(GetServerCommandTag());
  command.Append(" close" CRLF);

    ProgressEventFunctionUsingId (IMAP_STATUS_CLOSE_MAILBOX);

  GetServerStateParser().ResetFlagInfo(0);
    
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::XAOL_Option(const char *option)
{
    IncrementCommandTagNumber();
    
  nsCString command(GetServerCommandTag());
  command.Append(" XAOL-OPTION ");
  command.Append(option);
  command.Append(CRLF);
            
  nsresult rv = SendData(command.get());
  if (NS_SUCCEEDED(rv))
      ParseIMAPandCheckForNewMail();
}

void nsImapProtocol::Check()
{
    //ProgressUpdateEvent("Checking mailbox...");
    IncrementCommandTagNumber();
    
  nsCString command(GetServerCommandTag());
  command.Append(" check" CRLF);
            
    nsresult rv = SendData(command.get());
    if (NS_SUCCEEDED(rv))
        ParseIMAPandCheckForNewMail();
}

nsresult nsImapProtocol::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl =
        do_QueryInterface(m_runningUrl, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = mailnewsUrl->GetMsgWindow(aMsgWindow);
    return rv;
}

PRBool nsImapProtocol::TryToLogon()
{
  PRInt32 logonTries = 0;
  PRBool loginSucceeded = PR_FALSE;
  char * password = nsnull;
  char * userName = nsnull;
  nsresult rv = NS_OK;

  // get the password and user name for the current incoming server...
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryReferent(m_server);
  if (server)
  {
    // we are in the imap thread so *NEVER* try to extract the password with UI
	// if logon redirection has changed the password, use the cookie as the password
	if (m_overRideUrlConnectionInfo)
		password = ToNewCString(m_logonCookie);
	else
		rv = server->GetPassword(&password);
    rv = server->GetRealUsername(&userName);

  }
      
  nsCOMPtr<nsIMsgWindow> aMsgWindow;

  do
  {
    if (userName && (!password || !*password) && m_imapServerSink)
    {
      if (!aMsgWindow)
      {
          rv = GetMsgWindow(getter_AddRefs(aMsgWindow));
          if (NS_FAILED(rv)) return rv;
      }
      m_imapServerSink->PromptForPassword(&password, aMsgWindow);
    }
      PRBool imapPasswordIsNew = PR_FALSE;

      if (userName && password)
      {
      PRBool prefBool = PR_TRUE;

      PRBool lastReportingErrors = GetServerStateParser().GetReportingErrors();
      GetServerStateParser().SetReportingErrors(PR_FALSE);  // turn off errors - we'll put up our own.
        nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
        if (NS_SUCCEEDED(rv) && prefs) 
        prefs->GetBoolPref("mail.auth_login", &prefBool);

      if (prefBool) 
      {
        if (GetServerStateParser().GetCapabilityFlag() == kCapabilityUndefined)
          Capability();
        
        // for now, disable CRAM auth...we currently rely on PSM being present for this to work
        // in order to get an MD5 hash routine. I'm in the process of adding this routine to 
        // xpcom/ds so until then I'm going to leave CRAM turned off (although it does now work!)
#if 0
        // try to use CRAM before we fall back to plain or auth login....
        if (GetServerStateParser().GetCapabilityFlag() & kHasCRAMCapability)
        {
          AuthLogin (userName, password, kHasCRAMCapability);
          logonTries++;
        }
        else 
#endif
        if (GetServerStateParser().GetCapabilityFlag() & kHasAuthPlainCapability)
        {
          AuthLogin (userName, password, kHasAuthPlainCapability);
          logonTries++;
        }
        else if (GetServerStateParser().GetCapabilityFlag() & kHasAuthLoginCapability)
        {
          AuthLogin (userName, password,  kHasAuthLoginCapability); 
          logonTries++; // I think this counts against most
                  // servers as a logon try 
        }
        else
          InsecureLogin(userName, password);
      }
      else
        InsecureLogin(userName, password);

      if (!GetServerStateParser().LastCommandSuccessful())
      {
              // login failed!
              // if we failed because of an interrupt, then do not bother the user
              if (server)
                  rv = server->ForgetPassword();

              if (!DeathSignalReceived())
              {
                AlertUserEventUsingId(IMAP_LOGIN_FAILED);
                m_hostSessionList->SetPasswordForHost(GetImapServerKey(), nsnull);
                PR_FREEIF(password);
                m_currentBiffState = nsIMsgFolder::nsMsgBiffState_Unknown;
                SendSetBiffIndicatorEvent(m_currentBiffState);
            } // if we didn't receive the death signal...
          } // if login failed
      else  // login succeeded
      {
        rv = m_hostSessionList->SetPasswordForHost(GetImapServerKey(), password);
        rv = m_hostSessionList->GetPasswordVerifiedOnline(GetImapServerKey(), imapPasswordIsNew);
        if (NS_SUCCEEDED(rv) && imapPasswordIsNew)
          m_hostSessionList->SetPasswordVerifiedOnline(GetImapServerKey());
#ifdef UNREADY_CODE
        NET_SetPopPassword2(passwordForHost); // bug 53380
#endif
        if (imapPasswordIsNew) 
        {
                  if (m_currentBiffState == nsIMsgFolder::nsMsgBiffState_Unknown)
                  {
                    m_currentBiffState = nsIMsgFolder::nsMsgBiffState_NoMail;
                      SendSetBiffIndicatorEvent(m_currentBiffState);
                  }
#ifdef UNREADY_CODE
                  LIBNET_LOCK();
                  if (!DeathSignalReceived())
                  {
                    setUserAuthenticatedIsSafe = GetActiveEntry()->URL_s->msg_pane != NULL;
            MWContext *context = GetActiveEntry()->window_id;
            if (context)
              FE_RememberPopPassword(context, SECNAV_MungeString(passwordForHost));
          }
          LIBNET_UNLOCK();
#endif
        }

        loginSucceeded = PR_TRUE;
      } // else if login succeeded
      
      GetServerStateParser().SetReportingErrors(lastReportingErrors);  // restore it

      if (loginSucceeded && imapPasswordIsNew)
      {
        // let's record the user as authenticated.
        m_imapServerSink->SetUserAuthenticated(PR_TRUE);
      }

      if (loginSucceeded)
      {
        ProcessAfterAuthenticated();
      }
      }
      else
      {
      // The user hit "Cancel" on the dialog box
          //AlertUserEvent("Login cancelled.");
      HandleCurrentUrlError();
#ifdef UNREADY_CODE
      SetCurrentEntryStatus(-1);
          SetConnectionStatus(-1);        // stop netlib
#endif
      break;
      }
  }

  while (!loginSucceeded && ++logonTries < 4);

  PR_FREEIF(password);
  PR_FREEIF(userName);
  if (!loginSucceeded)
  {
    m_currentBiffState = nsIMsgFolder::nsMsgBiffState_Unknown;
    SendSetBiffIndicatorEvent(m_currentBiffState);
    HandleCurrentUrlError();
    SetConnectionStatus(-1);        // stop netlib
  }

  return loginSucceeded;
}

PRBool
nsImapProtocol::GetDeleteIsMoveToTrash()
{
    PRBool rv = PR_FALSE;
    NS_ASSERTION (m_hostSessionList, "fatal... null host session list");
    if (m_hostSessionList)
        m_hostSessionList->GetDeleteIsMoveToTrashForHost(GetImapServerKey(), rv);
    return rv;
}

NS_IMETHODIMP nsImapProtocol::OverrideConnectionInfo(const PRUnichar *pHost, PRUint16 pPort, const char *pCookieData)
{
	m_logonHost.AssignWithConversion(pHost);
	m_logonPort = pPort;
	m_logonCookie = pCookieData;
	m_overRideUrlConnectionInfo = PR_TRUE;
	return NS_OK;
}

nsIMAPMailboxInfo::nsIMAPMailboxInfo(const char *name, char delimiter)
{
  m_mailboxName = name;
  m_delimiter = delimiter;
  m_childrenListed = PR_FALSE;
}

nsIMAPMailboxInfo::~nsIMAPMailboxInfo()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////
// The following is the implementation of nsImapMockChannel and an intermediary 
// imap steam listener. The stream listener is used to make a clean binding between the
// imap mock channel and the memory cache channel (if we are reading from the cache)
//////////////////////////////////////////////////////////////////////////////////////////////

// WARNING: the cache stream listener is intended to be accessed from the UI thread!
// it will NOT create another proxy for the stream listener that gets passed in...
class nsImapCacheStreamListener : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsImapCacheStreamListener ();
  virtual ~nsImapCacheStreamListener();

  nsresult Init(nsIStreamListener * aStreamListener, nsIChannel * aMockChannelToUse);
protected:
  nsCOMPtr<nsIChannel> mChannelToUse;
  nsCOMPtr<nsIStreamListener> mListener;
};

NS_IMPL_ADDREF(nsImapCacheStreamListener);
NS_IMPL_RELEASE(nsImapCacheStreamListener);

NS_INTERFACE_MAP_BEGIN(nsImapCacheStreamListener)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END

nsImapCacheStreamListener::nsImapCacheStreamListener()
{
  NS_INIT_ISUPPORTS();
}

nsImapCacheStreamListener::~nsImapCacheStreamListener()
{}

nsresult nsImapCacheStreamListener::Init(nsIStreamListener * aStreamListener, nsIChannel * aMockChannelToUse)
{
  NS_ENSURE_ARG(aStreamListener);
  NS_ENSURE_ARG(aMockChannelToUse);

  mChannelToUse = aMockChannelToUse;
  mListener = aStreamListener;

  return NS_OK;
}

NS_IMETHODIMP
nsImapCacheStreamListener::OnStartRequest(nsIRequest *request, nsISupports * aCtxt)
{
  nsCOMPtr <nsILoadGroup> loadGroup;
  mChannelToUse->GetLoadGroup(getter_AddRefs(loadGroup));
  nsCOMPtr<nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);
  if (loadGroup)
    loadGroup->AddRequest(ourRequest, nsnull /* context isupports */);
  return mListener->OnStartRequest(ourRequest, aCtxt);
}

NS_IMETHODIMP
nsImapCacheStreamListener::OnStopRequest(nsIRequest *request, nsISupports * aCtxt, nsresult aStatus)
{
  nsCOMPtr<nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);
  nsresult rv = mListener->OnStopRequest(ourRequest, aCtxt, aStatus);
  nsCOMPtr <nsILoadGroup> loadGroup;
  mChannelToUse->GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
			loadGroup->RemoveRequest(ourRequest, nsnull, aStatus);

  mListener = nsnull;
  mChannelToUse = nsnull;
  return rv;
}

NS_IMETHODIMP
nsImapCacheStreamListener::OnDataAvailable(nsIRequest *request, nsISupports * aCtxt, nsIInputStream * aInStream, PRUint32 aSourceOffset, PRUint32 aCount)
{
  nsCOMPtr<nsIRequest> ourRequest = do_QueryInterface(mChannelToUse);
  return mListener->OnDataAvailable(ourRequest, aCtxt, aInStream, aSourceOffset, aCount);
}

NS_IMPL_THREADSAFE_ADDREF(nsImapMockChannel)
NS_IMPL_THREADSAFE_RELEASE(nsImapMockChannel)

NS_INTERFACE_MAP_BEGIN(nsImapMockChannel)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIImapMockChannel)
   NS_INTERFACE_MAP_ENTRY(nsIImapMockChannel)
   NS_INTERFACE_MAP_ENTRY(nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIRequest)
   NS_INTERFACE_MAP_ENTRY(nsICacheListener)
NS_INTERFACE_MAP_END_THREADSAFE


nsImapMockChannel::nsImapMockChannel()
{
  NS_INIT_REFCNT();
  m_channelContext = nsnull;
  m_cancelStatus = NS_OK;
  mLoadFlags = 0;
  mChannelClosed = PR_FALSE;
  mReadingFromCache = PR_FALSE;
}

nsImapMockChannel::~nsImapMockChannel()
{
  // if we're offline, we may not get to close the channel correctly.
  // we need to do this to send the url state change notification in
  // the case of mem and disk cache reads.
  if (!mChannelClosed)
    Close();
}

nsresult nsImapMockChannel::NotifyStartEndReadFromCache(PRBool start)
{
  nsresult rv = NS_OK;
  mReadingFromCache = start;
  nsCOMPtr <nsIImapUrl> imapUrl = do_QueryInterface(m_url, &rv);
  if (imapUrl)
  {
    nsCOMPtr <nsIImapMailFolderSink> folderSink;
    rv = imapUrl->GetImapMailFolderSink(getter_AddRefs(folderSink));
    if (folderSink)
    {
      nsCOMPtr <nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(m_url);
      rv = folderSink->SetUrlState(nsnull /* we don't know the protocol */, mailUrl, start, NS_OK); 
    }
  }
  return rv;
}

NS_IMETHODIMP nsImapMockChannel::Close()
{
  if (mReadingFromCache)
    NotifyStartEndReadFromCache(PR_FALSE);

  m_channelListener = nsnull;
  mCacheRequest = nsnull;
  m_url = nsnull;

  mChannelClosed = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetProgressEventSink(nsIProgressEventSink ** aProgressEventSink)
{
  *aProgressEventSink = mProgressEventSink;
  NS_IF_ADDREF(*aProgressEventSink);
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::SetProgressEventSink(nsIProgressEventSink * aProgressEventSink)
{
  mProgressEventSink = aProgressEventSink;
  return NS_OK;
}

NS_IMETHODIMP  nsImapMockChannel::GetChannelListener(nsIStreamListener **aChannelListener)
{
    *aChannelListener = m_channelListener;
    NS_IF_ADDREF(*aChannelListener);
    return NS_OK;
}

NS_IMETHODIMP  nsImapMockChannel::GetChannelContext(nsISupports **aChannelContext)
{
    *aChannelContext = m_channelContext;
    NS_IF_ADDREF(*aChannelContext);
    return NS_OK;
}

// now implement our mock implementation of the channel interface...we forward all calls to the real
// channel if we have one...otherwise we return something bogus...

NS_IMETHODIMP nsImapMockChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  m_loadGroup = dont_QueryInterface(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = m_loadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetOriginalURI(nsIURI* *aURI)
{
// IMap does not seem to have the notion of an original URI :-(
//  *aURI = m_originalUrl ? m_originalUrl : m_url;
    *aURI = m_url;
    NS_IF_ADDREF(*aURI);
    return NS_OK; 
}
 
NS_IMETHODIMP nsImapMockChannel::SetOriginalURI(nsIURI* aURI)
{
// IMap does not seem to have the notion of an original URI :-(
//    NS_NOTREACHED("nsImapMockChannel::SetOriginalURI");
//    return NS_ERROR_NOT_IMPLEMENTED; 
    return NS_OK;       // ignore
}
 
NS_IMETHODIMP nsImapMockChannel::GetURI(nsIURI* *aURI)
{
    *aURI = m_url;
    NS_IF_ADDREF(*aURI);
    return NS_OK ; 
}
 
NS_IMETHODIMP nsImapMockChannel::SetURI(nsIURI* aURI)
{
    m_url = aURI;
#ifdef DEBUG_bienvenu
    if (!aURI)
      printf("Clearing URI\n");
#endif
  if (m_url)
  {
    // if we don't have a progress event sink yet, get it from the url for now...
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url);
    if (mailnewsUrl && !mProgressEventSink)
    {
      nsCOMPtr<nsIMsgStatusFeedback> statusFeedback;
      mailnewsUrl->GetStatusFeedback(getter_AddRefs(statusFeedback));
      mProgressEventSink = do_QueryInterface(statusFeedback);
    }
  }
  return NS_OK; 
}
 
NS_IMETHODIMP nsImapMockChannel::Open(nsIInputStream **_retval)
{
    NS_NOTREACHED("nsImapMockChannel::Open");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsImapMockChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry, nsCacheAccessMode access, nsresult status)
{
  nsresult rv = NS_OK;

  // make sure we didn't close the channel before the async call back came in...
  // hmmm....if we had write access and we canceled this mock channel then I wonder if we should
  // be invalidating the cache entry before kicking out...
  if (mChannelClosed) return NS_OK;

  NS_ENSURE_ARG(m_url); // kick out if m_url is null for some reason. 

  if (NS_SUCCEEDED(status)) 
  {
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
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
      NotifyStartEndReadFromCache(PR_TRUE);
      if (access & nsICache::ACCESS_WRITE)
        entry->MarkValid();
      if (NS_SUCCEEDED(rv)) return NS_OK; // kick out if reading from the cache succeeded...
      mailnewsUrl->SetMemCacheEntry(nsnull); // we aren't going to be reading from the cache

    }
  } // if we got a valid entry back from the cache...

  // if reading from the cache failed or if we are writing into the cache, default to ReadFromImapConnection.
  return ReadFromImapConnection();
}

nsresult nsImapMockChannel::OpenCacheEntry()
{
  nsresult rv = NS_OK;
  // get the cache session from our imap service...
  nsCOMPtr<nsIImapService> imapService = do_GetService(kCImapService, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsICacheSession> cacheSession;
  rv = imapService->GetCacheSession(getter_AddRefs(cacheSession));
  NS_ENSURE_SUCCESS(rv, rv);

  // Open a cache entry with key = url
  nsXPIDLCString urlSpec;
  m_url->GetSpec(getter_Copies(urlSpec));
  // for now, truncate of the query part so we don't duplicate urls in the cache...
  char * anchor = PL_strrchr(urlSpec, '?');
  if (anchor)
    *anchor = '\0';
  return cacheSession->AsyncOpenCacheEntry(urlSpec, nsICache::ACCESS_READ_WRITE, this);
}

nsresult nsImapMockChannel::ReadFromMemCache(nsICacheEntryDescriptor *entry)
{
  NS_ENSURE_ARG(entry);

  PRUint32 annotationLength = 0;
  nsXPIDLCString annotation;
  nsresult rv = NS_OK;

  rv = entry->GetMetaDataElement("ContentModified", getter_Copies(annotation));
  if (NS_SUCCEEDED(rv) && (annotation.get()))
  {
    annotationLength = nsCRT::strlen(annotation.get());
    if (annotationLength == nsCRT::strlen("Not Modified") && !nsCRT::strncmp(annotation, "Not Modified", annotationLength))
    {
      nsCOMPtr<nsITransport> cacheTransport;
      rv = entry->GetTransport(getter_AddRefs(cacheTransport));
      if (NS_SUCCEEDED(rv))
      {
        // if we are going to read from the cache, then create a mock stream listener class and use it
        nsImapCacheStreamListener * cacheListener = new nsImapCacheStreamListener();
        NS_ADDREF(cacheListener);
        cacheListener->Init(m_channelListener, NS_STATIC_CAST(nsIChannel *, this));
        rv = cacheTransport->AsyncRead(cacheListener, m_channelContext, 0, PRUint32(-1), 0, getter_AddRefs(mCacheRequest));
        NS_RELEASE(cacheListener);

        if (NS_SUCCEEDED(rv)) // ONLY if we succeeded in actually starting the read should we return
        {
          nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(m_url);

          // if the msg is unread, we should mark it read on the server. This lets
          // the code running this url we're loading from the cache, if it cares.
          imapUrl->SetMsgLoadingFromCache(PR_TRUE);

          // and force the url to remove its reference on the mock channel...this is to solve
          // a nasty reference counting problem...
          imapUrl->SetMockChannel(nsnull);

          // be sure to set the cache entry's security info status as our security info status...
          nsCOMPtr<nsISupports> securityInfo;
          entry->GetSecurityInfo(getter_AddRefs(securityInfo));
          SetSecurityInfo(securityInfo);
          return NS_OK;
        } // if AsyncRead succeeded.
      } // if get transport succeeded
    } // if contnet is not modified
    else
      rv = NS_ERROR_FAILURE; // content is modified so return an error so we try to open it the old fashioned way
  } // if we got an annotation

  return rv;
}

// the requested url isn't in any of our caches so create an imap connection
// to process it.
nsresult nsImapMockChannel::ReadFromImapConnection()
{
  nsresult rv = NS_OK;  
  nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(m_url);
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url);

  // okay, add the mock channel to the load group..
  imapUrl->AddChannelToLoadGroup();
  // loading the url consists of asking the server to add the url to it's imap event queue....
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = mailnewsUrl->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIImapIncomingServer> imapServer (do_QueryInterface(server, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Assume AsyncRead is always called from the UI thread.....
  nsCOMPtr<nsIEventQueue> queue;
  // get the Event Queue for this thread...
  nsCOMPtr<nsIEventQueueService> pEventQService (do_GetService(kEventQueueServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(queue));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = imapServer->GetImapConnectionAndLoadUrl(queue, imapUrl, nsnull);
  return rv;
}

// for messages stored in our offline cache, we have special code to handle that...
// If it's in the local cache, we return true and we can abort the download because
// this method does the rest of the work.
PRBool nsImapMockChannel::ReadFromLocalCache()
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIImapUrl> imapUrl = do_QueryInterface(m_url);
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);

  PRBool useLocalCache = PR_FALSE;
  mailnewsUrl->GetMsgIsInLocalCache(&useLocalCache);
  if (useLocalCache)
  {
    nsXPIDLCString messageIdString;
    imapUrl->CreateListOfMessageIdsString(getter_Copies(messageIdString));
    nsCOMPtr <nsIMsgFolder> folder;
    rv = imapUrl->GetImapFolder(getter_AddRefs(folder));
    if (folder && NS_SUCCEEDED(rv))
    {
      // we want to create a file channel and read the msg from there.
      nsCOMPtr<nsITransport> fileChannel;
      nsMsgKey msgKey = atoi(messageIdString);
      PRUint32 size, offset;
      rv = folder->GetOfflineFileTransport(msgKey, &offset, &size, getter_AddRefs(fileChannel));
      // get the file channel from the folder, somehow (through the message or
      // folder sink?) We also need to set the transfer offset to the message offset
      if (fileChannel && NS_SUCCEEDED(rv))
      {
        // dougt - This may break the ablity to "cancel" a read from offline mail reading.
        // fileChannel->SetLoadGroup(m_loadGroup);
        
        // force the url to remove its reference on the mock channel...this is to solve
        // a nasty reference counting problem...
        imapUrl->SetMockChannel(nsnull);

        nsImapCacheStreamListener * cacheListener = new nsImapCacheStreamListener();
        NS_ADDREF(cacheListener);
        cacheListener->Init(m_channelListener, NS_STATIC_CAST(nsIChannel *, this));
        nsCOMPtr<nsIRequest> request;
        rv = fileChannel->AsyncRead(cacheListener, m_channelContext, offset, size, 0, getter_AddRefs(request));
        NS_RELEASE(cacheListener);

        if (NS_SUCCEEDED(rv)) // ONLY if we succeeded in actually starting the read should we return
        {
          // if the msg is unread, we should mark it read on the server. This lets
          // the code running this url we're loading from the cache, if it cares.
          imapUrl->SetMsgLoadingFromCache(PR_TRUE);
          return PR_TRUE;
        }
      } // if we got an offline file transport
    } // if we got the folder for this url
  } // if use local cache

  return PR_FALSE;
}

NS_IMETHODIMP nsImapMockChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  nsresult rv = NS_OK;
  
  PRInt32 port;
  if (!m_url)
    return NS_ERROR_NULL_POINTER;
  rv = m_url->GetPort(&port);
  if (NS_FAILED(rv))
      return rv;
 
  rv = NS_CheckPortSafety(port, "imap");
  if (NS_FAILED(rv))
      return rv;
    
  // set the stream listener and then load the url
  m_channelContext = ctxt;
  m_channelListener = listener;
  nsCOMPtr<nsIImapUrl> imapUrl  (do_QueryInterface(m_url));

  SetupPartExtractorListener(imapUrl, m_channelListener);
  
  if (ReadFromLocalCache())
  {
    (void) NotifyStartEndReadFromCache(PR_TRUE);
    return NS_OK;
  }

  nsImapAction imapAction;
  imapUrl->GetImapAction(&imapAction);
  // okay, it's not in the local cache, now check the memory cache...
  // but we can't download for offline use from the memory cache
  if (imapAction != nsIImapUrl::nsImapMsgDownloadForOffline)
  {
    rv = OpenCacheEntry();
    if (NS_SUCCEEDED(rv))
      return rv;
  }
  // if for some reason open cache entry failed then just default to opening an imap connection for the url
  return ReadFromImapConnection();
}

nsresult nsImapMockChannel::SetupPartExtractorListener(nsIImapUrl * aUrl, nsIStreamListener * aConsumer)
{
  // if the url we are loading refers to a specific part then we need 
  // libmime to extract that part from the message for us.
  PRBool refersToPart = PR_FALSE;
  aUrl->GetMimePartSelectorDetected(&refersToPart);
  if (refersToPart)
  {
    nsCOMPtr<nsIStreamConverterService> converter = do_GetService(kIStreamConverterServiceCID);
    if (converter && aConsumer)
    {
      nsCOMPtr<nsIStreamListener> newConsumer;
      converter->AsyncConvertData(NS_LITERAL_STRING("message/rfc822").get(), NS_LITERAL_STRING("*/*").get(),
           aConsumer, NS_STATIC_CAST(nsIChannel *, this), getter_AddRefs(newConsumer));
      if (newConsumer)
        m_channelListener = newConsumer;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  //*aLoadFlags = nsIRequest::LOAD_NORMAL;
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;       // don't fail when trying to set this
}

NS_IMETHODIMP nsImapMockChannel::GetContentType(char * *aContentType)
{
  if (m_ContentType.IsEmpty())
    *aContentType = nsCRT::strdup("message/rfc822");
  else
    *aContentType = ToNewCString(m_ContentType);
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::SetContentType(const char *aContentType)
{
  m_ContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsImapMockChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsImapMockChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsImapMockChannel::GetOwner(nsISupports * *aPrincipal)
{
  *aPrincipal = mOwner;
  NS_IF_ADDREF(*aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::SetOwner(nsISupports * aPrincipal)
{
    mOwner = aPrincipal;
    return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
    return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::SetSecurityInfo(nsISupports *aSecurityInfo)
{
    mSecurityInfo = aSecurityInfo;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapMockChannel::GetName(PRUnichar* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsImapMockChannel::IsPending(PRBool *result)
{
    *result = PR_TRUE;
    return NS_OK; 
}

NS_IMETHODIMP nsImapMockChannel::GetStatus(nsresult *status)
{
    *status = m_cancelStatus;
    return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::Cancel(nsresult status)
{
    m_cancelStatus = status;
    return NS_OK;
}

NS_IMETHODIMP nsImapMockChannel::Suspend()
{
    NS_NOTREACHED("nsImapMockChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsImapMockChannel::Resume()
{
    NS_NOTREACHED("nsImapMockChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsImapMockChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsImapMockChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;

  // Verify that the event sink is http
  if (mCallbacks) 
  {
      nsCOMPtr<nsIProgressEventSink> progressSink;
     (void)mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink),
                                   getter_AddRefs(progressSink));
     // only replace our current progress event sink if we were given a new one..
     if (progressSink) mProgressEventSink  = progressSink;
  }
  
  return NS_OK;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsImapProtocol_h___
#define nsImapProtocol_h___

#include "nsIImapProtocol.h"
#include "nsIImapUrl.h"

#include "nsIEventQueue.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsImapCore.h"
#include "nsString.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransport.h"

// imap event sinks
#include "nsIImapMailFolderSink.h"
#include "nsIImapServerSink.h"
#include "nsIImapMessageSink.h"
#include "nsIImapExtensionSink.h"
#include "nsIImapMiscellaneousSink.h"

#include "nsImapServerResponseParser.h"
#include "nsImapProxyEvent.h"
#include "nsImapFlagAndUidState.h"
#include "nsIMAPNamespace.h"
#include "nsVoidArray.h"
#include "nsWeakPtr.h"
#include "nsMsgLineBuffer.h" // we need this to use the nsMsgLineStreamBuffer helper class...
#include "nsIInputStream.h"
#include "nsIMsgIncomingServer.h"
#include "nsISupportsArray.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsIImapMockChannel.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIImapIncomingServer.h"
#include "nsXPIDLString.h"
#include "nsIMsgWindow.h"
#include "nsIMsgLogonRedirector.h"
#include "nsICacheListener.h"

class nsIMAPMessagePartIDArray;
class nsIMsgIncomingServer;

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state)
// Use these flags in conjunction with SetFlag/TestFlag/ClearFlag instead
// of creating PRBools for everything....

#define IMAP_RECEIVED_GREETING		      0x00000001  /* should we pause for the next read */
#define IMAP_FIRST_PASS_IN_THREAD       0x00000002  /* entering thread for the first time? */
#define	IMAP_CONNECTION_IS_OPEN		      0x00000004  /* is the connection currently open? */
#define IMAP_WAITING_FOR_DATA		        0x00000008
#define IMAP_CLEAN_UP_URL_STATE         0x00000010 // processing clean up url state
#define IMAP_ISSUED_LANGUAGE_REQUEST		0x00000020 // make sure we only issue the language request once per connection...

class nsImapProtocol : public nsIImapProtocol, public nsIRunnable
{
public:

	NS_DECL_ISUPPORTS

	nsImapProtocol();
	
	virtual ~nsImapProtocol();

    // nsIRunnable method
    NS_IMETHOD Run();

	//////////////////////////////////////////////////////////////////////////////////
	// we support the nsIImapProtocol interface
	//////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD LoadUrl(nsIURI * aURL, nsISupports * aConsumer);
	NS_IMETHOD IsBusy(PRBool * aIsConnectionBusy, PRBool *isInboxConnection);
	NS_IMETHOD CanHandleUrl(nsIImapUrl * aImapUrl, PRBool * aCanRunUrl,
                            PRBool * hasToWait);
	NS_IMETHOD Initialize(nsIImapHostSessionList * aHostSessionList, nsIEventQueue * aSinkEventQueue);
    NS_IMETHOD GetThreadEventQueue(nsIEventQueue **aEventQueue);
    // Notify FE Event has been completed
    NS_IMETHOD NotifyFEEventCompletion();

	NS_IMETHOD GetRunningImapURL(nsIImapUrl **aImapUrl);
	////////////////////////////////////////////////////////////////////////////////////////
	// we suppport the nsIStreamListener interface 
	////////////////////////////////////////////////////////////////////////////////////////
	
	// Whenever data arrives from the connection, core netlib notifies the protocol by calling
	// OnDataAvailable. We then read and process the incoming data from the input stream. 
	// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER


	// This is evil, I guess, but this is used by libmsg to tell a running imap url
	// about headers it should download to update a local database.
	NS_IMETHOD NotifyHdrsToDownload(PRUint32 *keys, PRUint32 keyCount);
	NS_IMETHOD NotifyBodysToDownload(PRUint32 *keys, PRUint32 keyCount);

	NS_IMETHOD GetFlagsForUID(PRUint32 uid, PRBool *foundIt, imapMessageFlagsType *flags);
	NS_IMETHOD GetSupportedUserFlags(PRUint16 *flags);

	NS_IMETHOD GetStreamConsumer (nsISupports **aSupport);
  NS_IMETHOD GetRunningUrl(nsIURI **aUrl);
  
  // Tell thread to die. This can only be called by imap service
  // 
  NS_IMETHOD TellThreadToDie(PRBool isSafeToClose);

  // Get last active time stamp
  NS_IMETHOD GetLastActiveTimeStamp(PRTime *aTimeStamp);

	NS_IMETHOD PseudoInterruptMsgLoad(nsIImapUrl *aImapUrl, PRBool
                                      *interrupted);
  NS_IMETHOD GetSelectedMailboxName(char ** folderName);
  NS_IMETHOD ResetToAuthenticatedState();
  NS_IMETHOD OverrideConnectionInfo(const PRUnichar *pHost, PRUint16 pPort, const char *pCookieData);
	//////////////////////////////////////////////////////////////////////////////////////
	// End of nsIStreamListenerSupport
	////////////////////////////////////////////////////////////////////////////////////////

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

	// message id string utilities.
	PRUint32		CountMessagesInIdString(const char *idString);
	static	PRBool	HandlingMultipleMessages(const char *messageIdString);

  // escape slashes and double quotes in username/passwords for insecure login.
  static void EscapeUserNamePasswordString(const char *strToEscape, nsCString *resultStr);

	// used to start fetching a message.
  PRBool GetShouldDownloadArbitraryHeaders();
  char *GetArbitraryHeadersToDownload();
  virtual void AdjustChunkSize();
  virtual void FetchMessage(const char * messageIds, 
                            nsIMAPeFetchFields whatToFetch,
                            PRBool idAreUid,
							              PRUint32 startByte = 0, PRUint32 endByte = 0,
							              char *part = 0);
	void FetchTryChunking(const char * messageIds,
                        nsIMAPeFetchFields whatToFetch,
                        PRBool idIsUid,
											  char *part,
											  PRUint32 downloadSize,
											  PRBool tryChunking);
	virtual void PipelinedFetchMessageParts(nsCString &uid, nsIMAPMessagePartIDArray *parts);

	// used when streaming a message fetch
  virtual nsresult BeginMessageDownLoad(PRUint32 totalSize, // for user, headers and body
                                    const char *contentType);     // some downloads are header only
  virtual void HandleMessageDownLoadLine(const char *line, PRBool chunkEnd);
  virtual void NormalMessageEndDownload();
  virtual void AbortMessageDownLoad();
  virtual void PostLineDownLoadEvent(msg_line_info *downloadLineDontDelete);
	virtual void AddXMozillaStatusLine(uint16 flags);	// for XSender auth info
    
  virtual void SetMailboxDiscoveryStatus(EMailboxDiscoverStatus status);
  virtual EMailboxDiscoverStatus GetMailboxDiscoveryStatus();
    
	virtual void ProcessMailboxUpdate(PRBool handlePossibleUndo);
	// Send log output...
	void	Log(const char *logSubName, const char *extraInfo, const char *logData);

	// Comment from 4.5: We really need to break out the thread synchronizer from the
	// connection class...Not sure what this means
	PRBool  GetPseudoInterrupted();
	void	PseudoInterrupt(PRBool the_interrupt);

	PRUint32 GetMessageSize(const char * messageId, PRBool idsAreUids);
  PRBool GetSubscribingNow();

	PRBool	DeathSignalReceived();
	void	ResetProgressInfo();
	void	SetActive(PRBool active);
	PRBool	GetActive();

	// Sets whether or not the content referenced by the current ActiveEntry has been modified.
	// Used for MIME parts on demand.
	void	SetContentModified(IMAP_ContentModifiedType modified);
	PRBool	GetShouldFetchAllParts();

	// Generic accessors required by the imap parser
	char * CreateNewLineFromSocket();
	PRInt32 GetConnectionStatus();
  void SetConnectionStatus(PRInt32 status);
    
	const char* GetImapHostName(); // return the host name from the url for the
                                   // current connection
  const char* GetImapUserName(); // return the user name from the identity;
  const char* GetImapServerKey(); // return the user name from the incoming server; 
	
	// state set by the imap parser...
	void NotifyMessageFlags(imapMessageFlagsType flags, nsMsgKey key);
	void NotifySearchHit(const char * hitLine);

	// Event handlers for the imap parser. 
	void DiscoverMailboxSpec(nsImapMailboxSpec * adoptedBoxSpec);
	void AlertUserEventUsingId(PRUint32 aMessageId);
	void AlertUserEvent(const char * message);
	void AlertUserEventFromServer(const char * aServerEvent);

	void ProgressEventFunctionUsingId(PRUint32 aMsgId);
	void ProgressEventFunctionUsingIdWithString(PRUint32 aMsgId, const char *
                                                aExtraInfo);
	void PercentProgressUpdateEvent(PRUnichar *message, PRInt32 currentProgress, PRInt32 maxProgress);
	void ShowProgress();

	// utility function calls made by the server

	PRUnichar * CreatePRUnicharStringFromUTF7(const char * aSourceString);

	void Copy(const char * messageList, const char *destinationMailbox, 
                                    PRBool idsAreUid);
	void Search(const char * searchCriteria,  PRBool useUID, 
									  PRBool notifyHit = PR_TRUE);
	// imap commands issued by the parser
	void Store(const char * aMessageList, const char * aMessageData, PRBool
               aIdsAreUid);
	void ProcessStoreFlags(const char * messageIds,
                         PRBool idsAreUids,
                         imapMessageFlagsType flags,
                         PRBool addFlags);
	void Expunge();
  void UidExpunge(const char* messageSet);
	void Close();
	void Check();
	void SelectMailbox(const char *mailboxName);
	// more imap commands
	void Logout();
	void Noop();
	void XServerInfo();
	void Netscape();
	void XMailboxInfo(const char *mailboxName);
  void XAOL_Option(const char *option);
	void MailboxData();
	void GetMyRightsForFolder(const char *mailboxName);
	void AutoSubscribeToMailboxIfNecessary(const char *mailboxName);
	void Bodystructure(const char *messageId, PRBool idIsUid);
	void PipelinedFetchMessageParts(const char *uid, nsIMAPMessagePartIDArray *parts);


	// this function does not ref count!!! be careful!!!
	nsIImapUrl		*GetCurrentUrl() {return m_runningUrl;}

	// Tunnels
	virtual PRInt32 OpenTunnel (PRInt32 maxNumberOfBytesToRead);
	PRBool GetIOTunnellingEnabled();
	PRInt32	GetTunnellingThreshold();

	// acl and namespace stuff
	// notifies libmsg that we have a new personal/default namespace that we're using
	void CommitNamespacesForHostEvent();
	// notifies libmsg that we have new capability data for the current host
	void CommitCapability();

	// Adds a set of rights for a given user on a given mailbox on the current host.
	// if userName is NULL, it means "me," or MYRIGHTS.
	// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
	void AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights);
	// Clears all rights for a given folder, for all users.
	void ClearAllFolderRights(const char *mailboxName, nsIMAPNamespace *nsForMailbox);

  void WaitForFEEventCompletion();
  void HandleMemoryFailure();
	void HandleCurrentUrlError();

    // UIDPLUS extension
  void SetCopyResponseUid(nsMsgKeyArray* aKeyArray,
                          const char* msgIdString);

private:
	// the following flag is used to determine when a url is currently being run. It is cleared when we 
	// finish processng a url and it is set whenever we call Load on a url
	PRBool			m_urlInProgress;	
	PRBool			m_socketIsOpen;
	PRBool			m_gotFEEventCompletion;
	PRUint32		m_flags;	   // used to store flag information
	nsCOMPtr<nsIImapUrl>		m_runningUrl; // the nsIImapURL that is currently running
	nsImapAction	m_imapAction;  // current imap action associated with this connnection...

	char			*m_userName;
	char			*m_hostName;
	char			*m_serverKey;
	char			*m_dataOutputBuf;
	nsMsgLineStreamBuffer * m_inputStreamBuffer;
  PRUint32		m_allocatedSize; // allocated size
  PRUint32        m_totalDataSize; // total data size
  PRUint32        m_curReadIndex;  // current read index

	// Ouput stream for writing commands to the socket
	nsCOMPtr<nsITransport>		m_channel; 
	nsCOMPtr<nsIOutputStream>	m_outputStream;   // this will be obtained from the transport interface
	nsCOMPtr<nsIInputStream>    m_inputStream;

  nsCOMPtr<nsIInputStream>  m_channelInputStream;
	nsCOMPtr<nsIOutputStream> m_channelOutputStream;
	nsCOMPtr<nsIStreamListener>	    m_channelListener; // if we are displaying an article this is the rfc-822 display sink...
  nsCOMPtr<nsISupports>           m_channelContext;
  nsCOMPtr<nsIImapMockChannel>    m_mockChannel;   // this is the channel we should forward to people
  nsCOMPtr<nsIRequest> mAsyncReadRequest; // we're going to cancel this when we're done with the conn.


	// this is a method designed to buffer data coming from the input stream and efficiently extract out 
	// a line on each call. We read out as much of the stream as we can and store the extra that doesn't
	// for the next line in aDataBuffer. We always check the buffer for a complete line first, 
	// if it doesn't have a line, we read in data from the stream and try again. If we still don't have
	// a complete line, we WAIT for more data to arrive by waiting onthe m_dataAvailable monitor. So this function
	// BLOCKS until we get a new line. Eventually I'd like to move this method out into a utiliity method
	// so I can resuse it for the other mail protocols...
	char * ReadNextLineFromInput(char * aDataBuffer,char *& aStartPos, PRUint32 aDataBufferSize, nsIInputStream * aInputStream);

  // ******* Thread support *******
  nsCOMPtr<nsIEventQueue>		m_sinkEventQueue;
  nsCOMPtr<nsIEventQueue>		m_eventQueue;
	nsCOMPtr<nsIThread>			m_iThread;
  PRThread     *m_thread;
  PRMonitor    *m_dataAvailableMonitor;   // used to notify the arrival of data from the server
	PRMonitor    *m_urlReadyToRunMonitor;	// used to notify the arrival of a new url to be processed
	PRMonitor	   *m_pseudoInterruptMonitor;
  PRMonitor	   *m_dataMemberMonitor;
	PRMonitor	   *m_threadDeathMonitor;
  PRMonitor    *m_eventCompletionMonitor;
	PRMonitor	   *m_waitForBodyIdsMonitor;
	PRMonitor	   *m_fetchMsgListMonitor;
	PRMonitor	   *m_fetchBodyListMonitor;

  PRBool       m_imapThreadIsRunning;
  void ImapThreadMainLoop(void);
  PRBool ImapThreadIsRunning();
  PRInt32				 m_connectionStatus;

	PRBool			m_nextUrlReadyToRun;
  nsWeakPtr   m_server;

  nsCOMPtr<nsIImapMailFolderSink> m_imapMailFolderSink;
  nsCOMPtr<nsIImapMessageSink>	m_imapMessageSink;

  nsCOMPtr<nsIImapExtensionSink>		m_imapExtensionSink;
  nsCOMPtr<nsIImapMiscellaneousSink>	m_imapMiscellaneousSink;
  nsCOMPtr<nsIImapServerSink>				m_imapServerSink;
  
  // helper function to setup imap sink interface proxies
  void SetupSinkProxy();
	// End thread support stuff

  PRBool GetDeleteIsMoveToTrash();
  PRMonitor *GetDataMemberMonitor();
  nsCString m_currentCommand;
  nsImapServerResponseParser m_parser;
  nsImapServerResponseParser& GetServerStateParser() { return m_parser; };

  virtual PRBool ProcessCurrentURL();
	void EstablishServerConnection();
  virtual void ParseIMAPandCheckForNewMail(const char* commandString =
                                             nsnull, PRBool ignoreBadNOResponses = PR_FALSE);
	// biff
	void	PeriodicBiff();
	void	SendSetBiffIndicatorEvent(nsMsgBiffState newState);
	PRBool	CheckNewMail();

	// folder opening and listing header functions
	void UpdatedMailboxSpec(nsImapMailboxSpec *aSpec);
	void FolderHeaderDump(PRUint32 *msgUids, PRUint32 msgCount);
	void FolderMsgDump(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields);
	void FolderMsgDumpLoop(PRUint32 *msgUids, PRUint32 msgCount, nsIMAPeFetchFields fields);
	void WaitForPotentialListOfMsgsToFetch(PRUint32 **msgIdList, PRUint32 &msgCount);
	void WaitForPotentialListOfBodysToFetch(PRUint32 **msgIdList, PRUint32 &msgCount);
	void HeaderFetchCompleted();
  void UploadMessageFromFile(nsIFileSpec* fileSpec, const char* mailboxName,
                               imapMessageFlagsType flags);

	// mailbox name utilities.
	char *CreateEscapedMailboxName(const char *rawName);
  void SetupMessageFlagsString(nsCString & flagString,
                                 imapMessageFlagsType flags,
                                 PRUint16 userFlags);
                                 
	// header listing data
	PRBool		m_fetchMsgListIsNew;
	PRUint32	m_fetchCount;
	PRUint32	*m_fetchMsgIdList;
	PRBool		m_fetchBodyListIsNew;
	PRUint32	m_fetchBodyCount;
	PRUint32	*m_fetchBodyIdList;

	// initialization function given a new url and transport layer
	nsresult  SetupWithUrl(nsIURI * aURL, nsISupports* aConsumer);
	void ReleaseUrlState(); // release any state that is stored on a per action basis.

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	// SendData not only writes the NULL terminated data in dataBuffer to our output stream
	// but it also informs the consumer that the data has been written to the stream.
  // aSuppressLogging --> set to true if you wish to suppress logging for this particular command.
  // this is useful for making sure we don't log authenication information like the user's password (which was 
  // encoded anyway), but still we shouldn't add that information to the log.
	nsresult SendData(const char * dataBuffer, PRBool aSuppressLogging = PR_FALSE);

	// state ported over from 4.5
	PRBool					m_pseudoInterrupted;
	PRBool					m_active;
  PRBool                  m_folderNeedsSubscribing;
  PRBool                  m_folderNeedsACLRefreshed;
	PRBool					m_threadShouldDie;
	nsImapFlagAndUidState	*m_flagState;
	nsMsgBiffState			m_currentBiffState;
  // manage the IMAP server command tags
  char m_currentServerCommandTag[10];   // enough for a billion
  int  m_currentServerCommandTagNumber;
  void IncrementCommandTagNumber();
  char *GetServerCommandTag();  

	// login related methods. All of these methods actually issue protocol
	void Capability(); // query host for capabilities.
  void Language(); // set the language on the server if it supports it
	void Namespace();
	void InsecureLogin(const char *userName, const char *password);
	void AuthLogin(const char *userName, const char *password, eIMAPCapabilityFlag flag);
	void ProcessAuthenticatedStateURL();
	void ProcessAfterAuthenticated();
	void ProcessSelectedStateURL();
	PRBool TryToLogon();

	// Process Authenticated State Url used to be one giant if statement. I've broken out a set of actions
	// based on the imap action passed into the url. The following functions are imap protocol handlers for
	// each action. They are called by ProcessAuthenticatedStateUrl.
	void OnLSubFolders();
	void OnGetMailAccount();
	void OnOfflineToOnlineMove();
	void OnAppendMsgFromFile();
	char * OnCreateServerSourceFolderPathString();
  char * OnCreateServerDestinationFolderPathString();
  nsresult CreateServerSourceFolderPathString(char **result);
	void OnCreateFolder(const char * aSourceMailbox);
	void OnEnsureExistsFolder(const char * aSourceMailbox);
	void OnSubscribe(const char * aSourceMailbox);
	void OnUnsubscribe(const char * aSourceMailbox);
  void RefreshACLForFolderIfNecessary(const char * mailboxName);
	void OnRefreshACLForFolder(const char * aSourceMailbox);
	void OnRefreshAllACLs();
	void OnListFolder(const char * aSourceMailbox, PRBool aBool);
	void OnStatusForFolder(const char * sourceMailbox);
	void OnDeleteFolder(const char * aSourceMailbox);
	void OnRenameFolder(const char * aSourceMailbox);
	void OnMoveFolderHierarchy(const char * aSourceMailbox);
	void FindMailboxesIfNecessary();
	void CreateMailbox(const char *mailboxName);
  void DeleteMailbox(const char *mailboxName);
  void RenameMailbox(const char *existingName, const char *newName);
	PRBool CreateMailboxRespectingSubscriptions(const char *mailboxName);
  PRBool DeleteMailboxRespectingSubscriptions(const char *mailboxName);
  PRBool  RenameMailboxRespectingSubscriptions(const char *existingName, 
                                                 const char *newName, 
                                                 PRBool reallyRename);
  // notify the fe that a folder was deleted
  void FolderDeleted(const char *mailboxName);
	// notify the fe that a folder creation failed
	void FolderNotCreated(const char *mailboxName);
  // notify the fe that a folder was deleted
  void FolderRenamed(const char *oldName,
                       const char *newName);
    
	PRBool	MailboxIsNoSelectMailbox(const char *mailboxName);
	char * CreatePossibleTrashName(const char *prefix);
	PRBool FolderNeedsACLInitialized(const char *folderName);
	void DiscoverMailboxList();
	void DiscoverAllAndSubscribedBoxes();
	void MailboxDiscoveryFinished();
  void NthLevelChildList(const char *onlineMailboxPrefix, PRInt32 depth);
	void Lsub(const char *mailboxPattern, PRBool addDirectoryIfNecessary);
	void List(const char *mailboxPattern, PRBool addDirectoryIfNecessary);
	void Subscribe(const char *mailboxName);
	void Unsubscribe(const char *mailboxName);
  PRBool DeleteSubFolders(const char* mailboxName);
	PRBool  RenameHierarchyByHand(const char *oldParentMailboxName, 
                                  const char *newParentMailboxName);

	nsresult GlobalInitialization();
	nsresult Configure(PRInt32 TooFastTime, PRInt32 IdealTime,
									PRInt32 ChunkAddSize, PRInt32 ChunkSize, PRInt32 ChunkThreshold,
									PRBool FetchByChunks, PRInt32 MaxChunkSize);
  nsresult GetMsgWindow(nsIMsgWindow ** aMsgWindow);
	// End Process AuthenticatedState Url helper methods

  PRBool		m_trackingTime;
  PRTime		m_startTime;
  PRTime		m_endTime;
  PRTime    m_lastActiveTime;
  PRInt32		m_tooFastTime;
  PRInt32		m_idealTime;
  PRInt32		m_chunkAddSize;
  PRInt32		m_chunkStartSize;
  PRInt32		m_maxChunkSize;
  PRBool		m_fetchByChunks;
  PRInt32		m_chunkSize;
  PRInt32		m_chunkThreshold;
  TLineDownloadCache m_downloadLineCache;

	nsIImapHostSessionList * m_hostSessionList;

  PRBool m_fromHeaderSeen;

  // these settings allow clients to override various pieces of the connection info from the url
  PRBool m_overRideUrlConnectionInfo;

	nsCString m_logonHost;
  nsCString m_logonCookie;
	PRInt16 m_logonPort;

	// progress stuff
	void SetProgressString(PRInt32 stringId);

	nsXPIDLString m_progressString;
	PRInt32	m_progressStringId;
	PRInt32	m_progressIndex;
	PRInt32 m_progressCount;
	PRUint32 m_lastProgressStringId;
	PRInt32 m_lastPercent;
	PRInt64 m_lastProgressTime;				

	PRBool m_notifySearchHit;
	PRBool m_mailToFetch;
	PRBool m_checkForNewMailDownloadsHeaders;
	PRBool m_needNoop;
	PRInt32 m_noopCount;
	PRInt32 m_promoteNoopToCheckCount;
  PRBool  m_autoSubscribe, m_autoUnsubscribe, m_autoSubscribeOnOpen;
  PRBool m_closeNeededBeforeSelect;
    enum EMailboxHierarchyNameState {
        kNoOperationInProgress,
        kDiscoverBaseFolderInProgress,
	kDiscoverTrashFolderInProgress,
        kDeleteSubFoldersInProgress,
	kListingForInfoOnly,
	kListingForInfoAndDiscovery,
	kDiscoveringNamespacesOnly,
        kListingForCreate
    };
    EMailboxHierarchyNameState m_hierarchyNameState;
  PRBool m_onlineBaseFolderExists;
  EMailboxDiscoverStatus m_discoveryStatus;
  nsVoidArray m_listedMailboxList;
  nsVoidArray* m_deletableChildren;
};

// This small class is a "mock" channel because it is a mockery of the imap channel's implementation...
// it's a light weight channel that we can return to necko when they ask for a channel on a url before
// we actually have an imap protocol instance around which can run the url. Please see my comments in
// nsIImapMockChannel.idl for more details..
//
// Threading concern: This class lives entirely in the UI thread.

class nsICacheEntryDescriptor;

class nsImapMockChannel : public nsIImapMockChannel, public nsICacheListener
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMAPMOCKCHANNEL
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUEST
  NS_DECL_NSICACHELISTENER
	
  nsImapMockChannel();
  virtual ~nsImapMockChannel();
  static nsresult Create (const nsIID& iid, void **result);

protected:
  // we must break this circular reference between the imap url 
  // and the mock channel when we've finished running the url,
  // or we'll leak like crazy. The idea is that when nsImapUrl::RemoveChannel is called,
  // it will null out the url's pointer to the mock channel
  nsCOMPtr <nsIURI> m_url;

  nsCOMPtr<nsIURI> m_originalUrl;
  nsCOMPtr<nsILoadGroup> m_loadGroup;
  nsCOMPtr<nsIStreamListener> m_channelListener;
  // non owning ref of the context in order to fix a circular ref count
  // because the context is already the uri...
  nsISupports * m_channelContext;
  nsresult m_cancelStatus;
  nsLoadFlags mLoadFlags;
  nsCOMPtr<nsIProgressEventSink> mProgressEventSink;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIRequest> mCacheRequest; // the request associated with a read from the cache
  nsCString m_ContentType;

  PRBool mChannelClosed;
  PRBool mReadingFromCache;
  // cache related helper methods
  nsresult OpenCacheEntry(); // makes a request to the cache service for a cache entry for a url
  PRBool ReadFromLocalCache(); // attempts to read the url out of our local (offline) cache....
  nsresult ReadFromImapConnection(); // creates a new imap connection to read the url 
  nsresult ReadFromMemCache(nsICacheEntryDescriptor *entry); // attempts to read the url out of our memory cache
  nsresult NotifyStartEndReadFromCache(PRBool start);

  // we end up daisy chaining multiple nsIStreamListeners into the load process. 
  nsresult SetupPartExtractorListener(nsIImapUrl * aUrl, nsIStreamListener * aConsumer);
};

#endif  // nsImapProtocol_h___

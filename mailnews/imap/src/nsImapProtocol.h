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

#ifndef nsImapProtocol_h___
#define nsImapProtocol_h___

#include "nsIImapProtocol.h"
#include "nsIImapUrl.h"

#include "nsIStreamListener.h"
#include "nsITransport.h"

#include "nsIOutputStream.h"
#include "nsImapCore.h"

class nsIMAPMessagePartIDArray;

class nsImapProtocol : public nsIImapProtocol
{
public:
	
    enum ImapState { 
        NOT_CONNECTED,
        NON_AUTHENTICATED_STATE, 
        AUTHENTICATED_STATE, 
        SELECTED_STATE,
        LOGOUT_STATE 
    };

	NS_DECL_ISUPPORTS

	nsImapProtocol();
	
	virtual ~nsImapProtocol();

	//////////////////////////////////////////////////////////////////////////////////
	// we support the nsIImapProtocol interface
	//////////////////////////////////////////////////////////////////////////////////
	NS_IMETHOD LoadUrl(nsIURL * aURL, nsISupports * aConsumer);
	NS_IMETHOD Initialize(PLEventQueue * aSinkEventQueue);
    NS_IMETHOD GetThreadEventQueue(PLEventQueue **aEventQueue);
    NS_IMETHOD SetMessageDownloadOutputStream(nsIOutputStream* aOutputStream);
    // Notify FE Event has been completed
    NS_IMETHOD NotifyFEEventCompletion();


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

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

	// used to start fetching a message.
	void FetchTryChunking(const char *messageIds,
                                            nsIMAPeFetchFields whatToFetch,
                                            PRBool idIsUid,
											char *part,
											PRUint32 downloadSize);
	virtual void PipelinedFetchMessageParts(const char *uid, nsIMAPMessagePartIDArray *parts);

	// used when streaming a message fetch
    virtual void BeginMessageDownLoad(PRUint32 totalSize, // for user, headers and body
                                      const char *contentType);     // some downloads are header only
    virtual void HandleMessageDownLoadLine(const char *line, PRBool chunkEnd);
    virtual void NormalMessageEndDownload();
    virtual void AbortMessageDownLoad();
	// Send log output...
	void	Log(const char *logSubName, const char *extraInfo, const char *logData);

	// Comment from 4.5: We really need to break out the thread synchronizer from the
	// connection class...Not sure what this means
	PRBool	GetShowAttachmentsInline();
	PRBool GetPseudoInterrupted();
	void	PseudoInterrupt(PRBool the_interrupt);

	PRUint32 GetMessageSize(const char *messageId, PRBool idsAreUids);

	PRBool	DeathSignalReceived();
	void	ResetProgressInfo();
	void	SetActive(PRBool active);
	PRBool	GetActive();

	// Sets whether or not the content referenced by the current ActiveEntry has been modified.
	// Used for MIME parts on demand.
	void	SetContentModified(PRBool modified);
	PRBool	GetShouldFetchAllParts();

	// Generic accessors required by the imap parser
	char * CreateNewLineFromSocket() { return nsnull; }
	PRUint32 GetConnectionStatus() { return NS_OK;} // mscott - could we make this an nsresult and subclass imap error values?
	const char * GetHostName() { return nsnull;} // return the host name from the url for the current connection
	
	// state set by the imap parser...
	void NotifyMessageFlags(imapMessageFlagsType flags, nsMsgKey key) {}
	void NotifySearchHit(const char * hitLine) {}

	// Event handlers for the imap parser. 
	void DiscoverMailboxSpec(mailbox_spec * adoptedBoxSpec) {} // mscott: need mailbox_spec type..
	void AlertUserEventUsingId(PRUint32 aMessageId) {}
	void AlertUserEvent(char * message) {} // mscott --> can this be a const char * ? 
	void AlertUserEventFromServer(char * aServerEvent) {} // mscott --> const????
	void ShowProgress() {}
	void ProgressEventFunctionUsingId(PRUint32 aMsgId) {}
	void ProgressEventFunctionUsingIdWithString(PRUint32 aMsgId, const char * aExtraInfo) {}
	void PercentProgressUpdateEvent(char *message, PRInt32 percent) {};

	// utility function calls made by the server
	char * CreateUtf7ConvertedString(const char * aSourceString, PRBool aConvertToUtf7Imap) { return nsnull;}

	// imap commands issued by the parser
	void Store(const char * aMessageList, const char * aMessageData, PRBool aIdsAreUid) {}
	void Expunge() {}

	nsIImapUrl		*GetCurrentUrl() {return m_runningUrl;}
	// Tunnels
	virtual PRInt32 OpenTunnel (PRInt32 maxNumberOfBytesToRead);
	PRBool GetIOTunnellingEnabled();
	PRInt32	GetTunnellingThreshold();

	// acl and namespace stuff
	// notifies libmsg that we have a new personal/default namespace that we're using
	void CommitNamespacesForHostEvent();
	// notifies libmsg that we have new capability data for the current host
	void CommitCapabilityForHostEvent();

	// Adds a set of rights for a given user on a given mailbox on the current host.
	// if userName is NULL, it means "me," or MYRIGHTS.
	// rights is a single string of rights, as specified by RFC2086, the IMAP ACL extension.
	void AddFolderRightsForUser(const char *mailboxName, const char *userName, const char *rights);
	// Clears all rights for a given folder, for all users.
	void ClearAllFolderRights(const char *mailboxName);

    void WaitForFEEventCompletion();

private:
	// the following flag is used to determine when a url is currently being run. It is cleared on calls
	// to ::StopBinding and it is set whenever we call Load on a url
	PRBool			m_urlInProgress;	
	PRBool			m_socketIsOpen;
	PRUint32		m_flags;	   // used to store flag information
	nsIImapUrl		*m_runningUrl; // the nsIImapURL that is currently running
	nsIImapUrl::nsImapAction	m_imapAction;  // current imap action associated with this connnection...

	char			*m_dataBuf;
    PRUint32		m_dataBufSize;

	// Ouput stream for writing commands to the socket
	nsITransport			* m_transport; 
	nsIOutputStream			* m_outputStream;   // this will be obtained from the transport interface
	nsIStreamListener	    * m_outputConsumer; // this will be obtained from the transport interface

    // ******* Thread support *******
    PLEventQueue *m_sinkEventQueue;
    PLEventQueue *m_eventQueue;
    PRThread     *m_thread;
    PRMonitor    *m_dataAvailableMonitor;
	PRMonitor	 *m_pseudoInterruptMonitor;
    PRMonitor	 *m_dataMemberMonitor;
	PRMonitor	 *m_threadDeathMonitor;
    PRMonitor    *m_eventCompletionMonitor;

    PRBool       m_imapThreadIsRunning;
    static void ImapThreadMain(void *aParm);
    void ImapThreadMainLoop(void);
    PRBool ImapThreadIsRunning();
    nsISupports* m_consumer;

    nsIOutputStream *m_messageDownloadOutputStream;
    
    PRMonitor *GetDataMemberMonitor();
    // **** current protocol instance state ****
    ImapState m_imapState;

    virtual void ProcessCurrentURL();

	// initialization function given a new url and transport layer
	void SetupWithUrl(nsIURL * aURL);

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	PRInt32 ReadLine(nsIInputStream * inputStream, PRUint32 length, char ** line);

	// SendData not only writes the NULL terminated data in dataBuffer to our output stream
	// but it also informs the consumer that the data has been written to the stream.
	PRInt32 SendData(const char * dataBuffer);

	// state ported over from 4.5
	PRBool m_pseudoInterrupted;
	PRBool m_active;
	PRBool m_threadShouldDie;

    // manage the IMAP server command tags
    char m_currentServerCommandTag[10];   // enough for a billion
    int  m_currentServerCommandTagNumber;
    void IncrementCommandTagNumber();
    char *GetServerCommandTag();  

};

#endif  // nsImapProtocol_h___

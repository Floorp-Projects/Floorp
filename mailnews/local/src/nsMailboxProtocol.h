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

#ifndef nsMailboxProtocol_h___
#define nsMailboxProtocol_h___

#include "nsIStreamListener.h"
#include "nsITransport.h"
#include "rosetta.h"
#include HG40855

#include "nsIOutputStream.h"
#include "nsIMailboxUrl.h"

#include "nsIWebShell.h"  // mscott - this dependency should only be temporary!

#ifdef XP_UNIX
#define MESSAGE_PATH "/usr/tmp/tempMessage.eml"
#endif

#ifdef XP_PC
#define MESSAGE_PATH  "c:\\temp\\tempMessage.eml"
#endif

#ifdef XP_MAC
#define MESSAGE_PATH  "tempMessage.eml"
#endif

// State Flags (Note, I use the word state in terms of storing 
// state information about the connection (authentication, have we sent
// commands, etc. I do not intend it to refer to protocol state)

#define MAILBOX_PAUSE_FOR_READ			0x00000001  /* should we pause for the next read */

/* states of the machine
 */
typedef enum _MailboxStatesEnum {
	MAILBOX_READ_FOLDER,
	MAILBOX_FINISH_OPEN_FOLDER,
	MAILBOX_OPEN_MESSAGE,
	MAILBOX_OPEN_STREAM,
	MAILBOX_READ_MESSAGE,
	MAILBOX_COMPRESS_FOLDER,
	MAILBOX_FINISH_COMPRESS_FOLDER,
	MAILBOX_BACKGROUND,
	MAILBOX_NULL,
	MAILBOX_NULL2,
	MAILBOX_DELIVER_QUEUED,
	MAILBOX_FINISH_DELIVER_QUEUED,
	MAILBOX_DONE,
	MAILBOX_ERROR_DONE,
	MAILBOX_FREE,
	MAILBOX_COPY_MESSAGES,
	MAILBOX_FINISH_COPY_MESSAGES
} MailboxStatesEnum;

class nsMsgLineStreamBuffer;

class nsMailboxProtocol : public nsIStreamListener
{
public:
	// Creating a protocol instance requires the URL which needs to be run AND it requires
	// a transport layer. 
	nsMailboxProtocol(nsIURL * aURL);
	
	virtual ~nsMailboxProtocol();

	// the consumer of the url might be something like an nsIWebShell....
	PRInt32 LoadURL(nsIURL * aURL, nsISupports * aConsumer);
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

	// Flag manipulators
	PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
	void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
	void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

private:
	// the following flag is used to determine when a url is currently being run. It is cleared on calls
	// to ::StopBinding and it is set whenever we call Load on a url
	PRBool			m_urlInProgress;	
	PRBool			m_socketIsOpen;
	PRUint32		m_flags; // used to store flag information
	nsIMailboxUrl	*m_runningUrl; // the nsIMailboxURL that is currently running
	nsMailboxAction m_mailboxAction; // current mailbox action associated with this connnection...
	PRInt32			m_originalContentLength; /* the content length at the time of calling graph progress */

	// Event sink handles
	nsIStreamListener *m_mailboxParser;
	nsIStreamListener *m_mailboxCopyHandler;

	// Local state for the current operation

	// Ouput stream for writing commands to the socket
	nsITransport			* m_transport; 
	nsIOutputStream			* m_outputStream;   // this will be obtained from the transport interface
	nsIStreamListener	    * m_outputConsumer; // this will be obtained from the transport interface
	nsMsgLineStreamBuffer   * m_lineStreamBuffer; // used to efficiently extract lines from the incoming data stream

	// Generic state information -- What state are we in? What state do we want to go to
	// after the next response? What was the last response code? etc. 
	MailboxStatesEnum  m_nextState;
    MailboxStatesEnum  m_initialState;

	PRUint32	m_messageID;

	PRFileDesc* m_tempMessageFile;
	nsIWebShell				* m_displayConsumer; // if we are displaying an article this is the rfc-822 display sink...
	

	PRInt32	  ProcessMailboxState(nsIURL * url, nsIInputStream * inputStream, PRUint32 length);
	PRInt32	  CloseConnection(); // releases and closes down this protocol instance...

	// initialization function given a new url and transport layer
	void Initialize(nsIURL * aURL);
	PRInt32 SetupMessageExtraction();

	////////////////////////////////////////////////////////////////////////////////////////
	// Communication methods --> Reading and writing protocol
	////////////////////////////////////////////////////////////////////////////////////////

	// SendData not only writes the NULL terminated data in dataBuffer to our output stream
	// but it also informs the consumer that the data has been written to the stream.
	PRInt32 SendData(const char * dataBuffer);

	////////////////////////////////////////////////////////////////////////////////////////
	// Protocol Methods --> This protocol is state driven so each protocol method is 
	//						designed to re-act to the current "state". I've attempted to 
	//						group them together based on functionality. 
	////////////////////////////////////////////////////////////////////////////////////////

	// When parsing a mailbox folder in chunks, this protocol state reads in the current chunk
	// and forwards it to the mailbox parser. 
	PRInt32 ReadFolderResponse(nsIInputStream * inputStream, PRUint32 length);
	PRInt32 ReadMessageResponse(nsIInputStream * inputStream, PRUint32 length);

	////////////////////////////////////////////////////////////////////////////////////////
	// End of Protocol Methods
	////////////////////////////////////////////////////////////////////////////////////////
};

#endif  // nsMailboxProtocol_h___

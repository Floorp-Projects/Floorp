/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMsgProtocol_h__
#define nsMsgProtocol_h__

#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIChannel.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsITransport.h"
#include "nsIAsyncOutputStream.h"
#include "nsIEventQueue.h"
#include "nsIAuthModule.h"

#define UNKNOWN_ERROR             101
#define UNKNOWN_HOST_ERROR        102
#define CONNECTION_REFUSED_ERROR  103
#define NET_TIMEOUT_ERROR         104

class nsIPrompt;
class nsIMsgMailNewsUrl;
class nsMsgFilePostHelper;
class nsIProxyInfo;

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_DEFAULT

// This is a helper class used to encapsulate code shared between all of the
// mailnews protocol objects (imap, news, pop, smtp, etc.) In particular,
// it unifies the core networking code for the protocols. My hope is that
// this will make unification with Necko easier as we'll only have to change
// this class and not all of the mailnews protocols.
class NS_MSG_BASE nsMsgProtocol : public nsIStreamListener
                                , public nsIChannel
                                , public nsITransportEventSink
{
public:
  nsMsgProtocol(nsIURI * aURL);
  virtual ~nsMsgProtocol();

  NS_DECL_ISUPPORTS
  // nsIChannel support
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUEST
  
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSITRANSPORTEVENTSINK

  // LoadUrl -- A protocol typically overrides this function, sets up any local state for the url and
  // then calls the base class which opens the socket if it needs opened. If the socket is 
  // already opened then we just call ProcessProtocolState to start the churning process.
  // aConsumer is the consumer for the url. It can be null if this argument is not appropriate
  virtual nsresult LoadUrl(nsIURI * aURL, nsISupports * aConsumer = nsnull);

  virtual nsresult SetUrl(nsIURI * aURL); // sometimes we want to set the url before we load it

  // Flag manipulators
  virtual PRBool TestFlag  (PRUint32 flag) {return flag & m_flags;}
  virtual void   SetFlag   (PRUint32 flag) { m_flags |= flag; }
  virtual void   ClearFlag (PRUint32 flag) { m_flags &= ~flag; }

protected:
  // methods for opening and closing a socket with core netlib....
  // mscott -okay this is lame. I should break this up into a file protocol and a socket based
  // protocool class instead of cheating and putting both methods here...

  // open a connection on this url
  virtual nsresult OpenNetworkSocket(nsIURI * aURL,
                                     const char *connectionType,
                                     nsIInterfaceRequestor* callbacks);

  // open a connection with a specific host and port
  // aHostName must be UTF-8 encoded.
  virtual nsresult OpenNetworkSocketWithInfo(const char * aHostName,
                                             PRInt32 aGetPort,
                                             const char *connectionType,
                                             nsIProxyInfo *aProxyInfo,
                                             nsIInterfaceRequestor* callbacks);
  // helper routine
  nsresult GetFileFromURL(nsIURI * aURL, nsIFile **aResult);
  virtual nsresult OpenFileSocket(nsIURI * aURL, PRUint32 aStartPosition, PRInt32 aReadCount); // used to open a file socket connection

  // a Protocol typically overrides this method. They free any of their own connection state and then
  // they call up into the base class to free the generic connection objects
  virtual nsresult CloseSocket(); 

  virtual nsresult SetupTransportState(); // private method used by OpenNetworkSocket and OpenFileSocket

  // ProcessProtocolState - This is the function that gets churned by calls to OnDataAvailable. 
  // As data arrives on the socket, OnDataAvailable calls ProcessProtocolState.
  
  virtual nsresult ProcessProtocolState(nsIURI * url, nsIInputStream * inputStream, 
									PRUint32 sourceOffset, PRUint32 length) = 0;

  // SendData -- Writes the data contained in dataBuffer into the current output stream. 
  // It also informs the transport layer that this data is now available for transmission.
  // Returns a positive number for success, 0 for failure (not all the bytes were written to the
  // stream, etc). 
    // aSuppressLogging is a hint that sensitive data is being sent and should not be logged
  virtual PRInt32 SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging = PR_FALSE);

  virtual nsresult PostMessage(nsIURI* url, nsIFileSpec * fileSpec);

  virtual nsresult InitFromURI(nsIURI *aUrl);

  nsresult DoNtlmStep1(const char *username, const char *password, nsCString &response);
  nsresult DoNtlmStep2(nsCString &commandResponse, nsCString &response);

  // Ouput stream for writing commands to the socket	
  nsCOMPtr<nsIOutputStream>   m_outputStream;   // this will be obtained from the transport interface
  nsCOMPtr<nsIInputStream>    m_inputStream;

  // Ouput stream for writing commands to the socket
  nsCOMPtr<nsITransport>  m_transport; 
  nsCOMPtr<nsIRequest>    m_request;

  PRBool        m_socketIsOpen; // mscott: we should look into keeping this state in the nsSocketTransport...
                                  // I'm using it to make sure I open the socket the first time a URL is loaded into the connection
  PRUint32      m_flags; // used to store flag information
  //PRUint32	m_startPosition;
  PRInt32       m_readCount;

  nsFileSpec	m_tempMsgFileSpec;  // we currently have a hack where displaying a msg involves writing it to a temp file first

  // auth module for access to NTLM functions
  nsCOMPtr<nsIAuthModule> m_authModule;

  // the following is a catch all for nsIChannel related data
  nsCOMPtr<nsIURI>            m_originalUrl;  // the original url
  nsCOMPtr<nsIURI>            m_url;          // the running url
  nsCOMPtr<nsIStreamListener> m_channelListener;
  nsCOMPtr<nsISupports>	      m_channelContext;
  nsCOMPtr<nsILoadGroup>      m_loadGroup;
  nsLoadFlags                 mLoadFlags;
  nsCOMPtr<nsIProgressEventSink> mProgressEventSink;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports>       mOwner;
  nsCString                   m_ContentType;

  // private helper routine used by subclasses to quickly get a reference to the correct prompt dialog
  // for a mailnews url. 
  nsresult GetPromptDialogFromUrl(nsIMsgMailNewsUrl * aMsgUrl, nsIPrompt ** aPromptDialog);

  // if a url isn't going to result in any content then we want to suppress calls to
  // OnStartRequest, OnDataAvailable and OnStopRequest
  PRBool mSuppressListenerNotifications;
};


// This is is a subclass of nsMsgProtocol extends the parent class with AsyncWrite support. Protocols like smtp
// and news want to leverage aysnc write. We don't want everyone who inherits from nsMsgProtocol to have to
// pick up the extra overhead.
class NS_MSG_BASE nsMsgAsyncWriteProtocol : public nsMsgProtocol
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Cancel(nsresult status);

  nsMsgAsyncWriteProtocol(nsIURI * aURL);
  virtual ~nsMsgAsyncWriteProtocol(); 
  
  // temporary over ride...
  virtual nsresult PostMessage(nsIURI* url, nsIFileSpec *fileSpec);
  
  // over ride the following methods from the base class
  virtual nsresult SetupTransportState();
  virtual PRInt32 SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging = PR_FALSE);
   
  // if we suspended the asynch write while waiting for more data to write then this will be TRUE
  PRBool mSuspendedWrite;
  nsCOMPtr<nsIRequest>     m_WriteRequest;
  nsCOMPtr<nsIAsyncOutputStream>    mAsyncOutStream;
  nsCOMPtr<nsIOutputStreamCallback> mProvider;
  nsCOMPtr<nsIEventQueue>           mProviderEventQ;

  // because we are reading the post data in asychronously, it's possible that we aren't sending it 
  // out fast enough and the reading gets blocked. The following set of state variables are used to 
  // track this.
  PRBool  mSuspendedRead;
  PRBool  mInsertPeriodRequired; // do we need to insert a '.' as part of the unblocking process
   
  nsresult ProcessIncomingPostData(nsIInputStream *inStr, PRUint32 count);
  nsresult UnblockPostReader();
  nsresult UpdateSuspendedReadBytes(PRUint32 aNewBytes, PRBool aAddToPostPeriodByteCount);
  nsresult PostDataFinished(); // this is so we'll send out a closing '.' and release any state related to the post


  // these two routines are used to pause and resume our loading of the file containing the contents
  // we are trying to post. We call these routines when we aren't sending the bits out fast enough
  // to keep up with the file read. 
  nsresult SuspendPostFileRead();
  nsresult ResumePostFileRead(); 
  nsresult UpdateSuspendedReadBytes(PRUint32 aNewBytes); 
  void UpdateProgress(PRUint32 aNewBytes);
  nsMsgFilePostHelper * mFilePostHelper; // needs to be a weak reference
protected:
  // the streams for the pipe used to queue up data for the async write calls to the server.
  // we actually re-use the same mOutStream variable in our parent class for the output
  // stream to the socket channel. So no need for a new variable here.
  nsCOMPtr<nsIInputStream>  mInStream;    
  nsCOMPtr<nsIInputStream>  mPostDataStream;
  PRUint32                  mSuspendedReadBytes;   // remaining # of bytes we need to read before   
                                                   // the input stream becomes unblocked
  PRUint32                  mSuspendedReadBytesPostPeriod; // # of bytes which need processed after we insert a '.' before 
                                                           // the input stream becomes unblocked.
  PRUint32  mFilePostSize; // used for determining progress on posting files.
  PRUint32  mNumBytesPosted; // used for deterimining progress on posting files 
  PRBool    mGenerateProgressNotifications; // set during a post operation after we've started sending the post data...

  virtual nsresult CloseSocket(); 
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

#endif /* nsMsgProtocol_h__ */

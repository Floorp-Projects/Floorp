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
#ifndef nsStreamConverter_h_
#define nsStreamConverter_h_

#include "nsCOMPtr.h"
#include "nsIStreamConverter2.h" 
#include "nsIMimeStreamConverter.h"
#include "nsIOutputStream.h"
#include "nsIMimeEmitter.h" 
#include "nsIURI.h"
#include "nsIBuffer.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsIChannel.h"

#define NS_STREAM_CONVERTER_SEGMENT_SIZE   (4*1024)
#define NS_STREAM_CONVERTER_BUFFER_SIZE    (1024*1024)//(32*1024)

class nsStreamConverter : public nsIStreamConverter2, public nsIMimeStreamConverter, public nsIBufferObserver { 
public: 
  nsStreamConverter();
  virtual ~nsStreamConverter();

  NS_DECL_ISUPPORTS 

  // nsIStreamConverter
  NS_IMETHOD Init(nsIURI *aURI, nsIStreamListener *outListener, nsIChannel * aChannel);
  NS_IMETHOD GetContentType(char **aOutputContentType);

  // nsIMimeStreamConverter support
  NS_IMETHOD SetMimeOutputType(nsMimeOutputType aType);
  NS_IMETHOD GetMimeOutputType(nsMimeOutputType *aOutFormat);
  NS_IMETHOD SetStreamURI(nsIURI *aURI);

  ////////////////////////////////////////////////////////////////////////////
  // nsIBufferObserver:
  NS_IMETHOD OnFull(nsIBuffer* buffer);
  NS_IMETHOD OnWrite(nsIBuffer* aBuffer, PRUint32 aCount);
  NS_IMETHOD OnEmpty(nsIBuffer* buffer);

  /////////////////////////////////////////////////////////////////////////////
  // Methods for nsIStreamListener...
  /////////////////////////////////////////////////////////////////////////////

  NS_IMETHOD OnDataAvailable(nsIChannel * aChannel, 
							 nsISupports    *ctxt, 
                             nsIInputStream *inStr, 
                             PRUint32       sourceOffset, 
                             PRUint32       count);

  // Notify the observer that the URL has started to load.  This method is
  // called only once, at the beginning of a URL load.
  //
  NS_IMETHOD OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt);

  //
  // Notify the observer that the URL has finished loading.  This method is 
  // called once when the networking library has finished processing the 
  //
  NS_IMETHOD OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg);


  ////////////////////////////////////////////////////////////////////////////
  // nsStreamConverter specific methods:
  ////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD          InternalCleanup(void);
  NS_IMETHOD          DetermineOutputFormat(const char *url, nsMimeOutputType *newType);

private:
  // the input and output streams form a pipe...they need to be passed around together..
  nsCOMPtr<nsIBufferOutputStream>     mOutputStream;     // output stream
  nsCOMPtr<nsIBufferInputStream>	    mInputStream;

  nsCOMPtr<nsIStreamListener>   mOutListener;   // output stream listener

  nsCOMPtr<nsIMimeEmitter>      mEmitter;       // emitter being used...
  nsCOMPtr<nsIURI>              mURI;           // URI being processed
  nsMimeOutputType              mOutputType;    // the output type we should use for the operation
  PRBool						mAlreadyKnowOutputType;

  void                          *mBridgeStream; // internal libmime data stream
  PRInt32                       mTotalRead;     // Counter variable

  // Type of output, entire message, header only, body only
  char                          *mOutputFormat;
  char                          *mOverrideFormat; // this is a possible override for emitter creation
  PRBool                        mWrapperOutput;   // Should we output the frame split message display 
  PRBool                        mDoneParsing;     // If this is true, we've already been told by libmime to stop sending
                                                  // data so don't feed the parser any more!
}; 

// factory method
extern nsresult NS_NewStreamConverter(const nsIID &aIID, void ** aInstancePtrResult);

#endif /* nsStreamConverter_h_ */

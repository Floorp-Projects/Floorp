/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Brodie Thiesfield <brofield@jellycan.com>
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
#ifndef nsStreamConverter_h_
#define nsStreamConverter_h_

#include "nsIStreamConverter.h" 
#include "nsIMimeStreamConverter.h"
#include "nsIOutputStream.h"
#include "nsIMimeEmitter.h" 
#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#define NS_STREAM_CONVERTER_SEGMENT_SIZE   (4*1024)
#define NS_STREAM_CONVERTER_BUFFER_SIZE    (32*1024)

class nsStreamConverter : public nsIStreamConverter, public nsIMimeStreamConverter { 
public: 
  nsStreamConverter();
  virtual ~nsStreamConverter();

  NS_DECL_ISUPPORTS 

  // nsIMimeStreamConverter support
  NS_DECL_NSIMIMESTREAMCONVERTER
  // nsIStreamConverter methods
  NS_DECL_NSISTREAMCONVERTER
  // nsIStreamListener methods
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver methods
  NS_DECL_NSIREQUESTOBSERVER

  ////////////////////////////////////////////////////////////////////////////
  // nsStreamConverter specific methods:
  ////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD Init(nsIURI *aURI, nsIStreamListener * aOutListener, nsIChannel *aChannel);
  NS_IMETHOD GetContentType(char **aOutputContentType);
  NS_IMETHOD InternalCleanup(void);
  NS_IMETHOD DetermineOutputFormat(const char *url, nsMimeOutputType *newType);
  NS_IMETHOD FirePendingStartRequest(void);

private:
  nsresult Close();

  // the input and output streams form a pipe...they need to be passed around together..
  nsCOMPtr<nsIOutputStream>     mOutputStream;     // output stream
  nsCOMPtr<nsIInputStream>      mInputStream;

  nsCOMPtr<nsIStreamListener>   mOutListener;   // output stream listener
  nsCOMPtr<nsIChannel>          mOutgoingChannel;

  nsCOMPtr<nsIMimeEmitter>      mEmitter;         // emitter being used...
  nsCOMPtr<nsIURI>              mURI;             // URI being processed
  nsMimeOutputType              mOutputType;      // the output type we should use for the operation
  PRBool                        mAlreadyKnowOutputType;

  void                          *mBridgeStream;   // internal libmime data stream

  // Type of output, entire message, header only, body only
  nsCString                     mOutputFormat;
  nsCString                     mRealContentType; // if we know the content type for real, this will be set (used by attachments)

  nsCString                     mOverrideFormat;  // this is a possible override for emitter creation
  PRBool                        mWrapperOutput;   // Should we output the frame split message display 

  nsCOMPtr<nsIMimeStreamConverterListener>  mMimeStreamConverterListener;
  PRBool                        mForwardInline;
  nsCOMPtr<nsIMsgIdentity>      mIdentity;
  nsCString                     mOriginalMsgURI;

#ifdef DEBUG_mscott  
  PRTime mConvertContentTime;
#endif
  nsIRequest *                  mPendingRequest;  // used when we need to delay to fire onStartRequest
  nsISupports *                 mPendingContext;  // used when we need to delay to fire onStartRequest
}; 

#endif /* nsStreamConverter_h_ */

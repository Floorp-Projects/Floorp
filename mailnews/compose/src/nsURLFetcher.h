/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsURLFetcher_h_
#define nsURLFetcher_h_

#include "nsIURLFetcher.h"

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsIInterfaceRequestor.h"
#include "nsCURILoader.h"
#include "nsIURIContentListener.h"
#include "nsIWebProgressListener.h"
#include "nsIHTTPEventSink.h"
#include "nsWeakReference.h"


class nsURLFetcher : public nsIURLFetcher,
                     public nsIStreamListener,
                     public nsIURIContentListener, 
                     public nsIInterfaceRequestor,
                     public nsIWebProgressListener,
                     public nsIHTTPEventSink,
                     public nsSupportsWeakReference
{ 
public: 
  nsURLFetcher();
  virtual ~nsURLFetcher();

  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS

  // Methods for nsIURLFetcher
  NS_DECL_NSIURLFETCHER

  // Methods for nsIStreamListener
  NS_DECL_NSISTREAMLISTENER

  // Methods for nsIRequestObserver
  NS_DECL_NSIREQUESTOBSERVER
  
  // Methods for nsIURICOntentListener
  NS_DECL_NSIURICONTENTLISTENER

  // Methods for nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // Methods for nsIWebProgressListener
  NS_DECL_NSIWEBPROGRESSLISTENER

  // Methods for nsIHTTPEventSink
  NS_DECL_NSIHTTPEVENTSINK

private:
  nsOutputFileStream              *mOutStream;    // the output file stream
  PRBool                          mStillRunning;  // Are we still running?
  PRInt32                         mTotalWritten;  // Size counter variable
  char                            *mContentType;  // The content type retrieved from the server
  char                            *mCharset;      // The charset retrieved from the server
  void                            *mTagData;      // Tag data for callback...
  nsAttachSaveCompletionCallback  mCallback;      // Callback to call once the file is saved...
  nsCOMPtr<nsISupports>           mLoadCookie;    // load cookie used by the uri loader when we fetch the url
  PRBool                          mOnStopRequestProcessed; // used to prevent calling OnStopRequest multiple times
  PRBool                          mRedirection;   // Set when we get a redirection, should ignore stop message.
}; 

#endif /* nsURLFetcher_h_ */

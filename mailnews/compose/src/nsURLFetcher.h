/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef nsURLFetcher_h_
#define nsURLFetcher_h_

#include "nsIURLFetcher.h"

#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCURILoader.h"
#include "nsIURIContentListener.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "nsXPIDLString.h"

class nsURLFetcher : public nsIURLFetcher,
                     public nsIStreamListener,
                     public nsIURIContentListener, 
                     public nsIInterfaceRequestor,
                     public nsIWebProgressListener,
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

protected:
  nsresult InsertConverter(const char * aContentType);

private:
  nsCOMPtr<nsIFileOutputStream>   mOutStream;               // the output file stream
  nsCOMPtr<nsILocalFile>          mLocalFile;               // the output file itself
  nsCOMPtr<nsIStreamListener>     mConverter;               // the stream converter, if needed
  nsXPIDLCString                  mConverterContentType;    // The content type of the converter
  PRBool                          mStillRunning;  // Are we still running?
  PRInt32                         mTotalWritten;  // Size counter variable
  char                            *mBuffer;                 // Buffer used for reading the data
  PRUint32                        mBufferSize;              // Buffer size;
  nsXPIDLCString                  mContentType;             // The content type retrieved from the server
  nsXPIDLCString                  mCharset;                 // The charset retrieved from the server
  void                            *mTagData;      // Tag data for callback...
  nsAttachSaveCompletionCallback  mCallback;      // Callback to call once the file is saved...
  nsCOMPtr<nsISupports>           mLoadCookie;    // load cookie used by the uri loader when we fetch the url
  PRBool                          mOnStopRequestProcessed; // used to prevent calling OnStopRequest multiple times
  PRBool                          mIsFile;        // This is used to check whether the URI is a local file.

  friend class nsURLFetcherStreamConsumer;
}; 


/**
 * Stream consumer used for handling special content type like multipart/x-mixed-replace
 */

class nsURLFetcherStreamConsumer : public nsIStreamListener
{
public:
  nsURLFetcherStreamConsumer(nsURLFetcher* urlFetcher);
  virtual ~nsURLFetcherStreamConsumer();

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsURLFetcher* mURLFetcher;
}; 


#endif /* nsURLFetcher_h_ */

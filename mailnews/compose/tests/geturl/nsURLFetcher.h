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
#ifndef nsURLFetcher_h_
#define nsURLFetcher_h_

#include "nsCOMPtr.h"
#include "nsFileStream.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"

//
// Callback declarations for URL completion
//
// For completion of send/message creation operations...
typedef nsresult (*nsAttachSaveCompletionCallback) (nsIURI* aURL, nsresult aStatus,
                                                    PRInt32 totalSize, const PRUnichar* aMsg, 
                                                    void *tagData);

class nsURLFetcher : public nsIStreamListener { 
public: 
  nsURLFetcher();
  virtual ~nsURLFetcher();

  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS

  // 
  // This is the output stream where the stream converter will write processed data after 
  // conversion. 
  // 
  NS_IMETHOD StillRunning(PRBool *running);

  NS_IMETHOD FireURLRequest(nsIURI *aURL, nsOutputFileStream *fOut, 
                            nsAttachSaveCompletionCallback cb, void *tagData);


  /**
   * Notify the client that data is available in the input stream.  This
   * method is called whenver data is written into the input stream by the
   * networking library...<BR><BR>
   * 
   * @param pIStream  The input stream containing the data.  This stream can
   * be either a blocking or non-blocking stream.
   * @param length    The amount of data that was just pushed into the stream.
   * @return The return value is currently ignored.
   */
  NS_IMETHOD OnDataAvailable(nsIChannel * aChannel, nsISupports * ctxt, nsIInputStream *aIStream, 
                              PRUint32 sourceOffset, PRUint32 aLength);


  // Methods for nsIStreamObserver 
  /**
  * Notify the observer that the URL has started to load.  This method is
  * called only once, at the beginning of a URL load.<BR><BR>
  *
  * @return The return value is currently ignored.  In the future it may be
  * used to cancel the URL load..
  */
  NS_IMETHOD OnStartRequest(nsIChannel * aChannel, nsISupports * aCtxt);
  
  /**
  * Notify the observer that the URL has finished loading.  This method is 
  * called once when the networking library has finished processing the 
  * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
  * 
  * This method is called regardless of whether the URL loaded successfully.<BR><BR>
  * 
  * @param status    Status code for the URL load.
  * @param msg   A text string describing the error.
  * @return The return value is currently ignored.
  */
  NS_IMETHOD OnStopRequest(nsIChannel * /* aChannel */, nsISupports * /* ctxt */, nsresult aStatus, const PRUnichar* aMsg);
;


private:
  nsOutputFileStream              *mOutStream;    // the output file stream
  PRBool                          mStillRunning;  // Are we still running?
  PRInt32                         mTotalWritten;  // Size counter variable
  nsCOMPtr<nsIURI>                mURL;          // URL being processed
  void                            *mTagData;      // Tag data for callback...
  nsAttachSaveCompletionCallback  mCallback;      // Callback to call once the file is saved...
}; 

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewURLFetcher(nsURLFetcher **aInstancePtrResult);

#endif /* nsURLFetcher_h_ */

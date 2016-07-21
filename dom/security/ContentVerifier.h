/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentVerifier_h
#define mozilla_dom_ContentVerifier_h

#include "nsCOMPtr.h"
#include "nsIContentSignatureVerifier.h"
#include "nsIObserver.h"
#include "nsIStreamListener.h"
#include "nsString.h"
#include "nsTArray.h"

/**
 * Mediator intercepting OnStartRequest in nsHttpChannel, blocks until all
 * data is read from the input stream, verifies the content signature and
 * releases the request to the next listener if the verification is successful.
 * If the verification fails or anything else goes wrong, a
 * NS_ERROR_INVALID_SIGNATURE is thrown.
 */
class ContentVerifier : public nsIStreamListener
                      , public nsIContentSignatureReceiverCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICONTENTSIGNATURERECEIVERCALLBACK

  explicit ContentVerifier(nsIStreamListener* aMediatedListener,
                           nsISupports* aMediatedContext)
    : mNextListener(aMediatedListener)
    , mContextCreated(false)
    , mContentRead(false) {}

  nsresult Init(const nsACString& aContentSignatureHeader, nsIRequest* aRequest,
                nsISupports* aContext);

protected:
  virtual ~ContentVerifier() {}

private:
  void FinishSignature();

  // buffered content to verify
  FallibleTArray<nsCString> mContent;
  // content and next listener for nsIStreamListener
  nsCOMPtr<nsIStreamListener> mNextListener;
  // the verifier
  nsCOMPtr<nsIContentSignatureVerifier> mVerifier;
  // holding a pointer to the content request and context to resume/cancel it
  nsCOMPtr<nsIRequest> mContentRequest;
  nsCOMPtr<nsISupports> mContentContext;
  // Semaphors to indicate that the verifying context was created, the entire
  // content was read resp. The context gets created by ContentSignatureVerifier
  // and mContextCreated is set in the ContextCreated callback. The content is
  // read, i.e. mContentRead is set, when the content OnStopRequest is called.
  bool mContextCreated;
  bool mContentRead;
};

#endif /* mozilla_dom_ContentVerifier_h */

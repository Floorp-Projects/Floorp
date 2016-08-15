/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentVerifier.h"

#include "mozilla/fallible.h"
#include "mozilla/Logging.h"
#include "MainThreadUtils.h"
#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"

using namespace mozilla;

static LazyLogModule gContentVerifierPRLog("ContentVerifier");
#define CSV_LOG(args) MOZ_LOG(gContentVerifierPRLog, LogLevel::Debug, args)

NS_IMPL_ISUPPORTS(ContentVerifier,
                  nsIContentSignatureReceiverCallback,
                  nsIStreamListener);

nsresult
ContentVerifier::Init(const nsACString& aContentSignatureHeader,
                      nsIRequest* aRequest, nsISupports* aContext)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aContentSignatureHeader.IsEmpty()) {
    CSV_LOG(("Content-Signature header must not be empty!\n"));
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // initialise the content signature "service"
  nsresult rv;
  mVerifier =
    do_CreateInstance("@mozilla.org/security/contentsignatureverifier;1", &rv);
  if (NS_FAILED(rv) || !mVerifier) {
    return NS_ERROR_INVALID_SIGNATURE;
  }

  // Keep references to the request and context. We need them in FinishSignature
  // and the ContextCreated callback.
  mContentRequest = aRequest;
  mContentContext = aContext;

  rv = mVerifier->CreateContextWithoutCertChain(
    this, aContentSignatureHeader,
    NS_LITERAL_CSTRING("remotenewtab.content-signature.mozilla.org"));
  if (NS_FAILED(rv)){
    mVerifier = nullptr;
  }
  return rv;
}

/**
 * Implement nsIStreamListener
 * We buffer the entire content here and kick off verification
 */
nsresult
AppendNextSegment(nsIInputStream* aInputStream, void* aClosure,
                  const char* aRawSegment, uint32_t aToOffset, uint32_t aCount,
                  uint32_t* outWrittenCount)
{
  FallibleTArray<nsCString>* decodedData =
    static_cast<FallibleTArray<nsCString>*>(aClosure);
  nsDependentCSubstring segment(aRawSegment, aCount);
  if (!decodedData->AppendElement(segment, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *outWrittenCount = aCount;
  return NS_OK;
}

void
ContentVerifier::FinishSignature()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIStreamListener> nextListener;
  nextListener.swap(mNextListener);

  // Verify the content:
  // If this fails, we return an invalid signature error to load a fallback page.
  // If everthing is good, we return a new stream to the next listener and kick
  // that one off.
  bool verified = false;
  nsresult rv = NS_OK;

  // If the content signature check fails, stop the load
  // and return a signature error. NSS resources are freed by the
  // ContentSignatureVerifier on destruction.
  if (NS_FAILED(mVerifier->End(&verified)) || !verified) {
    CSV_LOG(("failed to verify content\n"));
    (void)nextListener->OnStopRequest(mContentRequest, mContentContext,
                                      NS_ERROR_INVALID_SIGNATURE);
    return;
  }
  CSV_LOG(("Successfully verified content signature.\n"));

  // We emptied the input stream so we have to create a new one from mContent
  // to hand it to the consuming listener.
  uint64_t offset = 0;
  for (uint32_t i = 0; i < mContent.Length(); ++i) {
    nsCOMPtr<nsIInputStream> oInStr;
    rv = NS_NewCStringInputStream(getter_AddRefs(oInStr), mContent[i]);
    if (NS_FAILED(rv)) {
      break;
    }
    // let the next listener know that there is data in oInStr
    rv = nextListener->OnDataAvailable(mContentRequest, mContentContext, oInStr,
                                       offset, mContent[i].Length());
    offset += mContent[i].Length();
    if (NS_FAILED(rv)) {
      break;
    }
  }

  // propagate OnStopRequest and return
  nextListener->OnStopRequest(mContentRequest, mContentContext, rv);
}

NS_IMETHODIMP
ContentVerifier::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  MOZ_CRASH("This OnStartRequest should've never been called!");
  return NS_OK;
}

NS_IMETHODIMP
ContentVerifier::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
  // If we don't have a next listener, we handed off this request already.
  // Return, there's nothing to do here.
  if (!mNextListener) {
    return NS_OK;
  }

  if (NS_FAILED(aStatus)) {
    CSV_LOG(("Stream failed\n"));
    nsCOMPtr<nsIStreamListener> nextListener;
    nextListener.swap(mNextListener);
    return nextListener->OnStopRequest(aRequest, aContext, aStatus);
  }

  mContentRead = true;

  // If the ContentSignatureVerifier is initialised, finish the verification.
  if (mContextCreated) {
    FinishSignature();
    return aStatus;
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentVerifier::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                                 nsIInputStream* aInputStream, uint64_t aOffset,
                                 uint32_t aCount)
{
  // buffer the entire stream
  uint32_t read;
  nsresult rv = aInputStream->ReadSegments(AppendNextSegment, &mContent, aCount,
                                           &read);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Update the signature verifier if the context has been created.
  if (mContextCreated) {
    return mVerifier->Update(mContent.LastElement());
  }

  return NS_OK;
}

NS_IMETHODIMP
ContentVerifier::ContextCreated(bool successful)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!successful) {
    // If we don't have a next listener, the request has been handed off already.
    if (!mNextListener) {
      return NS_OK;
    }
    // Get local reference to mNextListener and null it to ensure that we don't
    // call it twice.
    nsCOMPtr<nsIStreamListener> nextListener;
    nextListener.swap(mNextListener);

    // Make sure that OnStartRequest was called and we have a request.
    MOZ_ASSERT(mContentRequest);

    // In this case something went wrong with the cert. Let's stop this load.
    CSV_LOG(("failed to get a valid cert chain\n"));
    if (mContentRequest && nextListener) {
      mContentRequest->Cancel(NS_ERROR_INVALID_SIGNATURE);
      nsresult rv = nextListener->OnStopRequest(mContentRequest, mContentContext,
                                                NS_ERROR_INVALID_SIGNATURE);
      mContentRequest = nullptr;
      mContentContext = nullptr;
      return rv;
    }

    // We should never get here!
    MOZ_ASSERT_UNREACHABLE(
      "ContentVerifier was used without getting OnStartRequest!");
    return NS_OK;
  }

  // In this case the content verifier is initialised and we have to feed it
  // the buffered content.
  mContextCreated = true;
  for (size_t i = 0; i < mContent.Length(); ++i) {
    if (NS_FAILED(mVerifier->Update(mContent[i]))) {
      // Bail out if this fails. We can't return an error here, but if this
      // failed, NS_ERROR_INVALID_SIGNATURE is returned in FinishSignature.
      break;
    }
  }

  // We read all content, let's verify the signature.
  if (mContentRead) {
    FinishSignature();
  }

  return NS_OK;
}

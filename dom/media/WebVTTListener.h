/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebVTTLoadListener_h
#define mozilla_dom_WebVTTLoadListener_h

#include "nsIWebVTTListener.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsCycleCollectionParticipant.h"

class nsIWebVTTParserWrapper;

namespace mozilla {
namespace dom {

class HTMLTrackElement;

class WebVTTListener final : public nsIWebVTTListener,
                             public nsIStreamListener,
                             public nsIChannelEventSink,
                             public nsIInterfaceRequestor
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIWEBVTTLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebVTTListener,
                                           nsIStreamListener)

public:
  explicit WebVTTListener(HTMLTrackElement* aElement);

  /**
   * Loads the WebVTTListener. Must call this in order for the listener to be
   * ready to parse data that is passed to it.
   */
  nsresult LoadResource();

private:
  ~WebVTTListener();

  // List of error codes returned from the WebVTT parser that we care about.
  enum ErrorCodes {
    BadSignature = 0
  };
  static nsresult ParseChunk(nsIInputStream* aInStream, void* aClosure,
                             const char* aFromSegment, uint32_t aToOffset,
                             uint32_t aCount, uint32_t* aWriteCount);

  RefPtr<HTMLTrackElement> mElement;
  nsCOMPtr<nsIWebVTTParserWrapper> mParserWrapper;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebVTTListener_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebVTTLoadListener_h
#define mozilla_dom_WebVTTLoadListener_h

#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsAutoPtr.h"
#include "nsAutoRef.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class HTMLTrackElement;

/**
 * Class that manages the WebVTT parsing library and functions as an
 * interface between Gecko and WebVTT.
 *
 * Currently it's only designed to work with an HTMLTrackElement. The
 * HTMLTrackElement controls the lifetime of the WebVTTLoadListener.
 *
 * The workflow of this class is as follows:
 *  - Gets Loaded via an HTMLTrackElement class.
 *  - As the HTMLTrackElement class gets new data WebVTTLoadListener's
 *    OnDataAvailable() function is called and data is passed to the WebVTT
 *    parser.
 *  - When the WebVTT parser has finished a cue it will call the callbacks
 *    that are exposed by the WebVTTLoadListener.
 *  - When it has returned a cue successfully the WebVTTLoadListener will create
 *    a new TextTrackCue and add it to the HTMLTrackElement's TextTrack.
 */
class WebVTTLoadListener MOZ_FINAL : public nsIStreamListener,
                                     public nsIChannelEventSink,
                                     public nsIInterfaceRequestor
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(WebVTTLoadListener,
                                           nsIStreamListener)

public:
  WebVTTLoadListener(HTMLTrackElement* aElement);
  ~WebVTTLoadListener();

  // Loads the WebVTT parser. Must call this function in order to the
  // WebVTTLoadListener to be ready to accept data.
  nsresult LoadResource();

private:
  static NS_METHOD ParseChunk(nsIInputStream* aInStream, void* aClosure,
                              const char* aFromSegment, uint32_t aToOffset,
                              uint32_t aCount, uint32_t* aWriteCount);

  nsRefPtr<HTMLTrackElement> mElement;
};



} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebVTTLoadListener_h

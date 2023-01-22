/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_

#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"

namespace mozilla::dom {

class WebTransport;
class WebTransportIncomingStreamsAlgorithms
    : public UnderlyingSourceAlgorithmsWrapper {
 public:
  WebTransportIncomingStreamsAlgorithms(Promise* aIncomingStreamPromise,
                                        bool aUnidirectional,
                                        WebTransport* aStream)
      : mIncomingStreamPromise(aIncomingStreamPromise),
        mUnidirectional(aUnidirectional),
        mStream(aStream) {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      WebTransportIncomingStreamsAlgorithms, UnderlyingSourceAlgorithmsWrapper)

  already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

  already_AddRefed<Promise> CancelCallbackImpl(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override;

 protected:
  ~WebTransportIncomingStreamsAlgorithms() override;

 private:
  RefPtr<Promise> mIncomingStreamPromise;
  const bool mUnidirectional;
  RefPtr<WebTransport> mStream;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_

#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/WritableStream.h"

namespace mozilla::dom {

class WebTransport;

class WebTransportIncomingStreamsAlgorithms
    : public UnderlyingSourceAlgorithmsWrapper {
 public:
  enum class StreamType : uint8_t { Unidirectional, Bidirectional };
  WebTransportIncomingStreamsAlgorithms(StreamType aUnidirectional,
                                        WebTransport* aTransport);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      WebTransportIncomingStreamsAlgorithms, UnderlyingSourceAlgorithmsWrapper)

  already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

  // We call EnqueueNative, which is MOZ_CAN_RUN_SCRIPT but won't in this case
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void BuildStream(JSContext* aCx,
                                               ErrorResult& aRv);

  void NotifyIncomingStream();

  void NotifyRejectAll();

 protected:
  ~WebTransportIncomingStreamsAlgorithms() override;

 private:
  const StreamType mUnidirectional;
  RefPtr<WebTransport> mTransport;
  RefPtr<Promise> mCallback;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_API_WEBTRANSPORTSTREAMS__H_

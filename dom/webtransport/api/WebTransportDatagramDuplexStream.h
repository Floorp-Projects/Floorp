/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTDATAGRAMDUPLEXSTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTDATAGRAMDUPLEXSTREAM__H_

#include <utility>
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WebTransportDatagramDuplexStreamBinding.h"

namespace mozilla::dom {

class IncomingDatagramStreamAlgorithms
    : public UnderlyingSourceAlgorithmsWrapper {
 public:
  explicit IncomingDatagramStreamAlgorithms(
      WebTransportDatagramDuplexStream* aDatagrams);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IncomingDatagramStreamAlgorithms,
                                           UnderlyingSourceAlgorithmsWrapper)

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;

  MOZ_CAN_RUN_SCRIPT void ReturnDatagram(JSContext* aCx, ErrorResult& aRv);

  void NotifyDatagramAvailable();

 protected:
  ~IncomingDatagramStreamAlgorithms() override;

 private:
  RefPtr<WebTransportDatagramDuplexStream> mDatagrams;
  RefPtr<Promise> mIncomingDatagramsPullPromise;
};

class EarlyDatagram final {
 public:
  EarlyDatagram(DatagramEntry* aDatagram, Promise* aPromise)
      : mDatagram(aDatagram), mWaitConnectPromise(aPromise) {}

  UniquePtr<DatagramEntry> mDatagram;
  RefPtr<Promise> mWaitConnectPromise;

  ~EarlyDatagram() = default;
};

class OutgoingDatagramStreamAlgorithms final
    : public UnderlyingSinkAlgorithmsWrapper {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(OutgoingDatagramStreamAlgorithms,
                                           UnderlyingSinkAlgorithmsWrapper)

  explicit OutgoingDatagramStreamAlgorithms(
      WebTransportDatagramDuplexStream* aDatagrams)
      : mDatagrams(aDatagrams) {}

  void SetChild(WebTransportChild* aChild);

  // Streams algorithms

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController,
      ErrorResult& aError) override;

 private:
  ~OutgoingDatagramStreamAlgorithms() override = default;

  RefPtr<WebTransportDatagramDuplexStream> mDatagrams;
  RefPtr<WebTransportChild> mChild;
  // only used for datagrams sent before Ready
  UniquePtr<DatagramEntry> mWaitConnect;
  RefPtr<Promise> mWaitConnectPromise;
};

class WebTransportDatagramDuplexStream final : public nsISupports,
                                               public nsWrapperCache {
  friend class IncomingDatagramStreamAlgorithms;
  friend class OutgoingDatagramStreamAlgorithms;

 public:
  WebTransportDatagramDuplexStream(nsIGlobalObject* aGlobal,
                                   WebTransport* aWebTransport);

  void Init(ErrorResult& aError);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransportDatagramDuplexStream)

  void SetChild(WebTransportChild* aChild) {
    mOutgoingAlgorithms->SetChild(aChild);
  }

  void NewDatagramReceived(nsTArray<uint8_t>&& aData,
                           const mozilla::TimeStamp& aTimeStamp);

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<ReadableStream> Readable() const {
    RefPtr<ReadableStream> result(mReadable);
    return result.forget();
  }
  already_AddRefed<WritableStream> Writable() {
    RefPtr<WritableStream> result(mWritable);
    return result.forget();
  }

  uint64_t MaxDatagramSize() const { return mOutgoingMaxDataSize; }
  void SetMaxDatagramSize(const uint64_t& aMaxDatagramSize) {
    mOutgoingMaxDataSize = aMaxDatagramSize;
  }
  double GetIncomingMaxAge(ErrorResult& aRv) const { return mIncomingMaxAge; }
  void SetIncomingMaxAge(double aMaxAge, ErrorResult& aRv);
  double GetOutgoingMaxAge(ErrorResult& aRv) const { return mOutgoingMaxAge; }
  void SetOutgoingMaxAge(double aMaxAge, ErrorResult& aRv);
  double GetIncomingHighWaterMark(ErrorResult& aRv) const {
    return mIncomingHighWaterMark;
  }
  void SetIncomingHighWaterMark(double aWaterMark, ErrorResult& aRv);
  double GetOutgoingHighWaterMark(ErrorResult& aRv) const {
    return mOutgoingHighWaterMark;
  }
  void SetOutgoingHighWaterMark(double aWaterMark, ErrorResult& aRv);

 private:
  ~WebTransportDatagramDuplexStream() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WebTransport> mWebTransport;
  RefPtr<ReadableStream> mReadable;
  RefPtr<WritableStream> mWritable;
  RefPtr<IncomingDatagramStreamAlgorithms> mIncomingAlgorithms;
  RefPtr<OutgoingDatagramStreamAlgorithms> mOutgoingAlgorithms;

  // https://w3c.github.io/webtransport/#webtransportdatagramduplexstream-create
  double mIncomingMaxAge = INFINITY;
  double mOutgoingMaxAge = INFINITY;
  // These are implementation-defined
  double mIncomingHighWaterMark = 1.0;
  double mOutgoingHighWaterMark = 5.0;
  uint64_t mOutgoingMaxDataSize = 1024;  // implementation-defined integer

  mozilla::Queue<UniquePtr<DatagramEntry>, 32> mIncomingDatagramsQueue;
};

}  // namespace mozilla::dom

#endif

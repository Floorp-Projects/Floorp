/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_

#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsISupports.h"
#include "nsTHashMap.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebTransportBinding.h"
#include "mozilla/dom/WebTransportChild.h"
#include "mozilla/dom/WebTransportSendStream.h"
#include "mozilla/dom/WebTransportReceiveStream.h"
#include "mozilla/dom/WebTransportStreams.h"
#include "mozilla/ipc/DataPipe.h"

namespace mozilla::dom {

class WebTransportError;
class WebTransportDatagramDuplexStream;
class WebTransportIncomingStreamsAlgorithms;
class ReadableStream;
class WritableStream;
using BidirectionalPair = std::pair<RefPtr<mozilla::ipc::DataPipeReceiver>,
                                    RefPtr<mozilla::ipc::DataPipeSender>>;

struct DatagramEntry {
  DatagramEntry(nsTArray<uint8_t>&& aData, const mozilla::TimeStamp& aTimeStamp)
      : mBuffer(std::move(aData)), mTimeStamp(aTimeStamp) {}
  DatagramEntry(Span<uint8_t>& aData, const mozilla::TimeStamp& aTimeStamp)
      : mBuffer(aData), mTimeStamp(aTimeStamp) {}

  nsTArray<uint8_t> mBuffer;
  mozilla::TimeStamp mTimeStamp;
};

class WebTransport final : public nsISupports, public nsWrapperCache {
  friend class WebTransportIncomingStreamsAlgorithms;
  // For mSendStreams/mReceiveStreams
  friend class WebTransportSendStream;
  friend class WebTransportReceiveStream;

 public:
  explicit WebTransport(nsIGlobalObject* aGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransport)

  enum class WebTransportState { CONNECTING, CONNECTED, CLOSED, FAILED };

  static void NotifyBFCacheOnMainThread(nsPIDOMWindowInner* aInner,
                                        bool aCreated);
  void NotifyToWindow(bool aCreated) const;

  void Init(const GlobalObject& aGlobal, const nsAString& aUrl,
            const WebTransportOptions& aOptions, ErrorResult& aError);
  void ResolveWaitingConnection(WebTransportReliabilityMode aReliability);
  void RejectWaitingConnection(nsresult aRv);
  bool ParseURL(const nsAString& aURL) const;
  // this calls CloseNative(), which doesn't actually run script.   See bug
  // 1810942
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Cleanup(
      WebTransportError* aError, const WebTransportCloseInfo* aCloseInfo,
      ErrorResult& aRv);

  // From Parent
  void NewBidirectionalStream(
      uint64_t aStreamId,
      const RefPtr<mozilla::ipc::DataPipeReceiver>& aIncoming,
      const RefPtr<mozilla::ipc::DataPipeSender>& aOutgoing);

  void NewUnidirectionalStream(
      uint64_t aStreamId,
      const RefPtr<mozilla::ipc::DataPipeReceiver>& aStream);

  void NewDatagramReceived(nsTArray<uint8_t>&& aData,
                           const mozilla::TimeStamp& aTimeStamp);

  void RemoteClosed(bool aCleanly, const uint32_t& aCode,
                    const nsACString& aReason);

  void OnStreamResetOrStopSending(uint64_t aStreamId,
                                  const StreamResetOrStopSendingError& aError);
  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  static already_AddRefed<WebTransport> Constructor(
      const GlobalObject& aGlobal, const nsAString& aUrl,
      const WebTransportOptions& aOptions, ErrorResult& aError);

  already_AddRefed<Promise> GetStats(ErrorResult& aError);

  already_AddRefed<Promise> Ready() { return do_AddRef(mReady); }
  WebTransportReliabilityMode Reliability();
  WebTransportCongestionControl CongestionControl();
  already_AddRefed<Promise> Closed() { return do_AddRef(mClosed); }
  MOZ_CAN_RUN_SCRIPT void Close(const WebTransportCloseInfo& aOptions,
                                ErrorResult& aRv);
  already_AddRefed<WebTransportDatagramDuplexStream> GetDatagrams(
      ErrorResult& aRv);
  already_AddRefed<Promise> CreateBidirectionalStream(
      const WebTransportSendStreamOptions& aOptions, ErrorResult& aRv);
  already_AddRefed<Promise> CreateUnidirectionalStream(
      const WebTransportSendStreamOptions& aOptions, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<ReadableStream>
  IncomingBidirectionalStreams();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<ReadableStream>
  IncomingUnidirectionalStreams();

  void SendSetSendOrder(uint64_t aStreamId, Maybe<int64_t> aSendOrder);

  void Shutdown() {}

 private:
  ~WebTransport();

  template <typename Stream>
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void PropagateError(Stream* aStream,
                                                  WebTransportError* aError);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  // We are the owner of WebTransportChild.  We must call Shutdown() on it
  // before we're destroyed.
  RefPtr<WebTransportChild> mChild;

  // Spec in 5.8 says it can't be GC'd while CONNECTING or CONNECTED.  We won't
  // hold ref which we drop on CLOSED or FAILED because a reference is also held
  // by IPC.  We drop the IPC connection and by proxy the reference when it goes
  // to FAILED or CLOSED.

  // Spec-defined slots:
  // ordered sets, but we can't have duplicates, and this spec only appends.
  // Order is visible due to
  // https://w3c.github.io/webtransport/#webtransport-procedures step 10: "For
  // each sendStream in sendStreams, error sendStream with error."
  nsTHashMap<uint64_t, RefPtr<WebTransportSendStream>> mSendStreams;
  nsTHashMap<uint64_t, RefPtr<WebTransportReceiveStream>> mReceiveStreams;

  WebTransportState mState;
  RefPtr<Promise> mReady;
  // XXX may not need to be a RefPtr, since we own it through the Streams
  RefPtr<WebTransportIncomingStreamsAlgorithms> mIncomingBidirectionalAlgorithm;
  RefPtr<WebTransportIncomingStreamsAlgorithms>
      mIncomingUnidirectionalAlgorithm;
  WebTransportReliabilityMode mReliability;
  // Incoming streams get queued here.  Use a TArray though it's working as
  // a FIFO - rarely will there be more than one entry in these arrays, so
  // the overhead of mozilla::Queue is unneeded
  nsTArray<std::tuple<uint64_t, RefPtr<mozilla::ipc::DataPipeReceiver>>>
      mUnidirectionalStreams;
  nsTArray<std::tuple<uint64_t, UniquePtr<BidirectionalPair>>>
      mBidirectionalStreams;

  // These are created in the constructor
  RefPtr<ReadableStream> mIncomingUnidirectionalStreams;
  RefPtr<ReadableStream> mIncomingBidirectionalStreams;
  RefPtr<WebTransportDatagramDuplexStream> mDatagrams;
  RefPtr<Promise> mClosed;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_

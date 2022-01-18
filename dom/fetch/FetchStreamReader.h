/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchStreamReader_h
#define mozilla_dom_FetchStreamReader_h

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIAsyncOutputStream.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

class ReadableStream;
class ReadableStreamDefaultReader;
class WeakWorkerRef;

class FetchStreamReader final : public nsIOutputStreamCallback,
                                public PromiseNativeHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(
      FetchStreamReader, nsIOutputStreamCallback)
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  // This creates a nsIInputStream able to retrieve data from the ReadableStream
  // object. The reading starts when StartConsuming() is called.
  static nsresult Create(JSContext* aCx, nsIGlobalObject* aGlobal,
                         FetchStreamReader** aStreamReader,
                         nsIInputStream** aInputStream);

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  // Idempotently close the output stream and null out all state. If aCx is
  // provided, the reader will also be canceled.  aStatus must be a DOM error
  // as understood by DOMException because it will be provided as the
  // cancellation reason.
  //
  // This is a script boundary minimize annotation changes required while
  // we figure out how to handle some more tricky annotation cases (for
  // example, the destructor of this class. Tracking under Bug 1750656)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void CloseAndRelease(JSContext* aCx, nsresult aStatus);

#ifdef MOZ_DOM_STREAMS
  void StartConsuming(JSContext* aCx, ReadableStream* aStream,
                      ReadableStreamDefaultReader** aReader, ErrorResult& aRv);
#else
  void StartConsuming(JSContext* aCx, JS::HandleObject aStream,
                      JS::MutableHandle<JSObject*> aReader, ErrorResult& aRv);
#endif

 private:
  explicit FetchStreamReader(nsIGlobalObject* aGlobal);
  ~FetchStreamReader();

  nsresult WriteBuffer();

  void ReportErrorToConsole(JSContext* aCx, JS::Handle<JS::Value> aValue);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  nsCOMPtr<nsIAsyncOutputStream> mPipeOut;

  RefPtr<WeakWorkerRef> mWorkerRef;

#ifdef MOZ_DOM_STREAMS
  RefPtr<ReadableStreamDefaultReader> mReader;
#else
  JS::Heap<JSObject*> mReader;
#endif

  nsTArray<uint8_t> mBuffer;
  uint32_t mBufferRemaining;
  uint32_t mBufferOffset;

  bool mStreamClosed;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FetchStreamReader_h

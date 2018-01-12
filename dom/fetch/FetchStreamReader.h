/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchStreamReader_h
#define mozilla_dom_FetchStreamReader_h

#include "jsapi.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "nsIAsyncOutputStream.h"

namespace mozilla {
namespace dom {

namespace workers {
class WorkerHolder;
}

class FetchStreamReader final : public nsIOutputStreamCallback
                              , public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(FetchStreamReader, nsIOutputStreamCallback)
  NS_DECL_NSIOUTPUTSTREAMCALLBACK

  // This creates a nsIInputStream able to retrieve data from the ReadableStream
  // object. The reading starts when StartConsuming() is called.
  static nsresult
  Create(JSContext* aCx, nsIGlobalObject* aGlobal,
         FetchStreamReader** aStreamReader,
         nsIInputStream** aInputStream);

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

  // Idempotently close the output stream and null out all state. If aCx is
  // provided, the reader will also be canceled.  aStatus must be a DOM error
  // as understood by DOMException because it will be provided as the
  // cancellation reason.
  void
  CloseAndRelease(JSContext* aCx, nsresult aStatus);

  void
  StartConsuming(JSContext* aCx,
                 JS::HandleObject aStream,
                 JS::MutableHandle<JSObject*> aReader,
                 ErrorResult& aRv);

private:
  explicit FetchStreamReader(nsIGlobalObject* aGlobal);
  ~FetchStreamReader();

  nsresult
  WriteBuffer();

  void
  ReportErrorToConsole(JSContext* aCx, JS::Handle<JS::Value> aValue);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  nsCOMPtr<nsIAsyncOutputStream> mPipeOut;

  UniquePtr<workers::WorkerHolder> mWorkerHolder;

  JS::Heap<JSObject*> mReader;

  UniquePtr<FetchReadableStreamReadDataArray> mBuffer;
  uint32_t mBufferRemaining;
  uint32_t mBufferOffset;

  bool mStreamClosed;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FetchStreamReader_h

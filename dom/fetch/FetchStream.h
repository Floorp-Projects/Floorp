/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FetchStream_h
#define mozilla_dom_FetchStream_h

#include "Fetch.h"
#include "jsapi.h"
#include "nsIAsyncInputStream.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsWeakReference.h"

class nsIGlobalObject;

class nsIInputStream;

namespace mozilla {
namespace dom {

namespace workers {
class WorkerHolder;
}

class FetchStream final : public nsIInputStreamCallback
                        , public nsIObserver
                        , public nsSupportsWeakReference
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOBSERVER

  static JSObject*
  Create(JSContext* aCx, nsIGlobalObject* aGlobal,
         nsIInputStream* aInputStream, ErrorResult& aRv);

  void
  Close();

private:
  FetchStream(nsIGlobalObject* aGlobal, nsIInputStream* aInputStream);
  ~FetchStream();

  static void
  RequestDataCallback(JSContext* aCx, JS::HandleObject aStream,
                      void* aUnderlyingSource, uint8_t aFlags,
                      size_t aDesiredSize);

  static void
  WriteIntoReadRequestCallback(JSContext* aCx, JS::HandleObject aStream,
                               void* aUnderlyingSource, uint8_t aFlags,
                               void* aBuffer, size_t aLength,
                               size_t* aByteWritten);

  static JS::Value
  CancelCallback(JSContext* aCx, JS::HandleObject aStream,
                 void* aUnderlyingSource, uint8_t aFlags,
                 JS::HandleValue aReason);

  static void
  ClosedCallback(JSContext* aCx, JS::HandleObject aStream,
                 void* aUnderlyingSource, uint8_t aFlags);

  static void
  ErroredCallback(JSContext* aCx, JS::HandleObject aStream,
                  void* aUnderlyingSource, uint8_t aFlags,
                  JS::HandleValue reason);

  static void
  FinalizeCallback(void* aUnderlyingSource, uint8_t aFlags);

  void
  ErrorPropagation(JSContext* aCx, JS::HandleObject aStream, nsresult aRv);

  void
  CloseAndReleaseObjects();

  // Common methods

  enum State {
    // RequestDataCallback has not been called yet. We haven't started to read
    // data from the stream yet.
    eWaiting,

    // We are reading data in a separate I/O thread.
    eReading,

    // We are ready to write something in the JS Buffer.
    eWriting,

    // After a writing, we want to check if the stream is closed. After the
    // check, we go back to eWaiting. If a reading request happens in the
    // meantime, we move to eReading state.
    eChecking,

    // Operation completed.
    eClosed,
  };

  // Touched only on the target thread.
  State mState;

  nsCOMPtr<nsIGlobalObject> mGlobal;

  // This is the original inputStream received during the CTOR. It will be
  // converted into an nsIAsyncInputStream and stored into mInputStream at the
  // first use.
  nsCOMPtr<nsIInputStream> mOriginalInputStream;
  nsCOMPtr<nsIAsyncInputStream> mInputStream;

  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  UniquePtr<workers::WorkerHolder> mWorkerHolder;

  JS::Heap<JSObject*> mReadableStream;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_FetchStream_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WritableStreamDefaultWriter_h
#define mozilla_dom_WritableStreamDefaultWriter_h

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class Promise;
class WritableStream;

class WritableStreamDefaultWriter final : public nsISupports,
                                          public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WritableStreamDefaultWriter)

 protected:
  ~WritableStreamDefaultWriter();

 public:
  explicit WritableStreamDefaultWriter(nsIGlobalObject* aGlobal);

  // Slot Getter/Setters:
 public:
  WritableStream* GetStream() const { return mStream; }
  void SetStream(WritableStream* aStream) { mStream = aStream; }

  Promise* ReadyPromise() const { return mReadyPromise; }
  void SetReadyPromise(Promise* aPromise);

  Promise* ClosedPromise() const { return mClosedPromise; }
  void SetClosedPromise(Promise* aPromise);

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // IDL Methods
  static already_AddRefed<WritableStreamDefaultWriter> Constructor(
      const GlobalObject& aGlobal, WritableStream& aStream, ErrorResult& aRv);

  already_AddRefed<Promise> Closed();
  already_AddRefed<Promise> Ready();

  Nullable<double> GetDesiredSize(ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Abort(
      JSContext* aCx, JS::Handle<JS::Value> aReason, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Close(JSContext* aCx,
                                                     ErrorResult& aRv);

  void ReleaseLock(JSContext* aCx);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Write(
      JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

  // Internal Slots:
 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WritableStream> mStream;
  RefPtr<Promise> mReadyPromise;
  RefPtr<Promise> mClosedPromise;
};

namespace streams_abstract {

void SetUpWritableStreamDefaultWriter(WritableStreamDefaultWriter* aWriter,
                                      WritableStream* aStream,
                                      ErrorResult& aRv);

void WritableStreamDefaultWriterEnsureClosedPromiseRejected(
    WritableStreamDefaultWriter* aWriter, JS::Handle<JS::Value> aError);

void WritableStreamDefaultWriterEnsureReadyPromiseRejected(
    WritableStreamDefaultWriter* aWriter, JS::Handle<JS::Value> aError);

Nullable<double> WritableStreamDefaultWriterGetDesiredSize(
    WritableStreamDefaultWriter* aWriter);

void WritableStreamDefaultWriterRelease(JSContext* aCx,
                                        WritableStreamDefaultWriter* aWriter);

MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WritableStreamDefaultWriterWrite(
    JSContext* aCx, WritableStreamDefaultWriter* aWriter,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise>
WritableStreamDefaultWriterCloseWithErrorPropagation(
    JSContext* aCx, WritableStreamDefaultWriter* aWriter, ErrorResult& aRv);

}  // namespace streams_abstract

}  // namespace mozilla::dom

#endif  // mozilla_dom_WritableStreamDefaultWriter_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamDefaultReader_h
#define mozilla_dom_ReadableStreamDefaultReader_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ReadableStreamGenericReader.h"
#include "mozilla/dom/ReadRequest.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/LinkedList.h"

namespace mozilla::dom {

class Promise;
class ReadableStream;

// https://streams.spec.whatwg.org/#default-reader-read
struct Read_ReadRequest : public ReadRequest {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Read_ReadRequest, ReadRequest)

  RefPtr<Promise> mPromise;

  explicit Read_ReadRequest(Promise* aPromise) : mPromise(aPromise) {}

  void ChunkSteps(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                  ErrorResult& aRv) override;

  void CloseSteps(JSContext* aCx, ErrorResult& aRv) override;

  void ErrorSteps(JSContext* aCx, JS::Handle<JS::Value> e,
                  ErrorResult& aRv) override;

 protected:
  ~Read_ReadRequest() override = default;
};

class ReadableStreamDefaultReader final : public ReadableStreamGenericReader,
                                          public nsWrapperCache

{
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_INHERITED(
      ReadableStreamDefaultReader, ReadableStreamGenericReader)

 public:
  explicit ReadableStreamDefaultReader(nsISupports* aGlobal);

 protected:
  ~ReadableStreamDefaultReader() override;

 public:
  bool IsDefault() override { return true; }
  bool IsBYOB() override { return false; }
  ReadableStreamDefaultReader* AsDefault() override { return this; }
  ReadableStreamBYOBReader* AsBYOB() override {
    MOZ_CRASH();
    return nullptr;
  }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<ReadableStreamDefaultReader> Constructor(
      const GlobalObject& aGlobal, ReadableStream& stream, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Read(ErrorResult& aRv);

  void ReleaseLock(ErrorResult& aRv);

  LinkedList<RefPtr<ReadRequest>>& ReadRequests() { return mReadRequests; }

 private:
  LinkedList<RefPtr<ReadRequest>> mReadRequests = {};
};

void SetUpReadableStreamDefaultReader(ReadableStreamDefaultReader* aReader,
                                      ReadableStream* aStream,
                                      ErrorResult& aRv);

void ReadableStreamDefaultReaderErrorReadRequests(
    JSContext* aCx, ReadableStreamDefaultReader* aReader,
    JS::Handle<JS::Value> aError, ErrorResult& aRv);

void ReadableStreamDefaultReaderRelease(JSContext* aCx,
                                        ReadableStreamDefaultReader* aReader,
                                        ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // mozilla_dom_ReadableStreamDefaultReader_h

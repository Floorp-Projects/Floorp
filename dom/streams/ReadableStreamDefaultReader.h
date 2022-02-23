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

namespace mozilla {
namespace dom {

class Promise;
class ReadableStream;

class ReadableStreamDefaultReader final : public ReadableStreamGenericReader,
                                          public nsWrapperCache

{
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      ReadableStreamDefaultReader, ReadableStreamGenericReader)

 public:
  explicit ReadableStreamDefaultReader(nsISupports* aGlobal);

 protected:
  ~ReadableStreamDefaultReader();

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

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Read(JSContext* aCx,
                                                    ErrorResult& aRv);

  void ReleaseLock(ErrorResult& aRv);

  LinkedList<RefPtr<ReadRequest>>& ReadRequests() { return mReadRequests; }

 private:
  LinkedList<RefPtr<ReadRequest>> mReadRequests = {};
};

extern void SetUpReadableStreamDefaultReader(
    JSContext* aCx, ReadableStreamDefaultReader* aReader,
    ReadableStream* aStream, ErrorResult& aRv);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReadableStreamDefaultReader_h

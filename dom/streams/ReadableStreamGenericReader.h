/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReadableStreamGenericReader_h
#define mozilla_dom_ReadableStreamGenericReader_h

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"

namespace mozilla {
namespace dom {

// Base class for internal slots of readable stream readers
class ReadableStreamGenericReader : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ReadableStreamGenericReader)

  explicit ReadableStreamGenericReader(nsCOMPtr<nsIGlobalObject> aGlobal)
      : mGlobal(std::move(aGlobal)) {}

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  Promise* ClosedPromise() { return mClosedPromise; }
  void SetClosedPromise(already_AddRefed<Promise>&& aClosedPromise) {
    mClosedPromise = aClosedPromise;
  }

  ReadableStream* GetStream() { return mStream; }
  void SetStream(already_AddRefed<ReadableStream>&& aStream) {
    mStream = aStream;
  }
  void SetStream(ReadableStream* aStream) {
    RefPtr<ReadableStream> stream(aStream);
    SetStream(stream.forget());
  }

 protected:
  virtual ~ReadableStreamGenericReader() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<Promise> mClosedPromise;
  RefPtr<ReadableStream> mStream;
};

}  // namespace dom
}  // namespace mozilla

#endif

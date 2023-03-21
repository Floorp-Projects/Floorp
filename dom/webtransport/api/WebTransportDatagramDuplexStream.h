/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTDATAGRAMDUPLEXSTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTDATAGRAMDUPLEXSTREAM__H_

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WebTransportDatagramDuplexStreamBinding.h"

namespace mozilla::dom {

class WebTransportDatagramDuplexStream final : public nsISupports,
                                               public nsWrapperCache {
 public:
  explicit WebTransportDatagramDuplexStream(nsIGlobalObject* aGlobal)
      : mGlobal(aGlobal) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransportDatagramDuplexStream)

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
  // XXX not implemented
  uint64_t MaxDatagramSize() const { return 1500; }
  Nullable<double> GetIncomingMaxAge() const { return mIncomingMaxAge; }
  void SetIncomingMaxAge(const Nullable<double>& aMaxAge) {
    mIncomingMaxAge = aMaxAge;
  }
  Nullable<double> GetOutgoingMaxAge() const { return mOutgoingMaxAge; }
  void SetOutgoingMaxAge(const Nullable<double>& aMaxAge) {
    mOutgoingMaxAge = aMaxAge;
  }
  int64_t IncomingHighWaterMark() const { return mIncomingHighWaterMark; }
  void SetIncomingHighWaterMark(int64_t aWaterMark) {
    mIncomingHighWaterMark = aWaterMark;
  }
  int64_t OutgoingHighWaterMark() const { return mOutgoingHighWaterMark; }
  void SetOutgoingHighWaterMark(int64_t aWaterMark) {
    mOutgoingHighWaterMark = aWaterMark;
  }

 private:
  ~WebTransportDatagramDuplexStream() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<ReadableStream> mReadable;
  RefPtr<WritableStream> mWritable;

  Nullable<double> mIncomingMaxAge;
  Nullable<double> mOutgoingMaxAge;
  int64_t mIncomingHighWaterMark = 0;
  int64_t mOutgoingHighWaterMark = 0;
};

}  // namespace mozilla::dom

#endif

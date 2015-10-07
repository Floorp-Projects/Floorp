/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Copyright © 2013 Deutsche Telekom, Inc. */

#ifndef mozilla_dom_MozNDEFRecord_h__
#define mozilla_dom_MozNDEFRecord_h__

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "jsapi.h"

#include "mozilla/dom/MozNDEFRecordBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "jsfriendapi.h"
#include "js/GCAPI.h"
#include "nsISupports.h"

struct JSContext;
struct JSStructuredCloneWriter;

namespace mozilla {
namespace dom {

class MozNDEFRecordOptions;

class MozNDEFRecord final : public nsISupports,
                            public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozNDEFRecord)

public:
  MozNDEFRecord(nsISupports* aParent, TNF aTnf = TNF::Empty);

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<MozNDEFRecord>
  Constructor(const GlobalObject& aGlobal,
              const MozNDEFRecordOptions& aOptions,
              ErrorResult& aRv);

  static already_AddRefed<MozNDEFRecord>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aURI,
              ErrorResult& aRv);

  TNF Tnf() const
  {
    return mTnf;
  }

  void GetType(JSContext* /* unused */, JS::MutableHandle<JSObject*> aRetVal) const
  {
    if (mType) {
      JS::ExposeObjectToActiveJS(mType);
    }
    aRetVal.set(mType);
  }

  void GetId(JSContext* /* unused */, JS::MutableHandle<JSObject*> aRetVal) const
  {
    if (mId) {
      JS::ExposeObjectToActiveJS(mId);
    }
    aRetVal.set(mId);
  }

  void GetPayload(JSContext* /* unused */, JS::MutableHandle<JSObject*> aRetVal) const
  {
    if (mPayload) {
      JS::ExposeObjectToActiveJS(mPayload);
    }
    aRetVal.set(mPayload);
  }

  uint32_t Size() const
  {
    return mSize;
  }

  void GetAsURI(nsAString& aRetVal);

  // Structured clone methods use these to clone MozNDEFRecord.
  bool WriteStructuredClone(JSContext* aCx, JSStructuredCloneWriter* aWriter) const;
  bool ReadStructuredClone(JSContext* aCx, JSStructuredCloneReader* aReader);

protected:
  ~MozNDEFRecord();

private:
  MozNDEFRecord() = delete;
  RefPtr<nsISupports> mParent;
  void HoldData();
  void DropData();
  void InitType(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aType);
  void InitType(JSContext* aCx, const RTD rtd);
  void InitType(JSContext* aCx, JSObject& aType, uint32_t aLen);
  void InitId(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aId);
  void InitId(JSContext* aCx, JSObject& aId, uint32_t aLen);
  void InitPayload(JSContext* aCx, const Optional<Nullable<Uint8Array>>& aPayload);
  void InitPayload(JSContext* aCx, const nsAString& aUri);
  void InitPayload(JSContext* aCx, JSObject& aPayload, uint32_t aLen);
  void IncSize(uint32_t aCount);
  void IncSizeForPayload(uint32_t aLen);
  bool WriteUint8Array(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj, uint32_t aLen) const;

  static bool
  ValidateTNF(const MozNDEFRecordOptions& aOptions, ErrorResult& aRv);
  static uint32_t GetURIIdentifier(const nsCString& aUri);

  TNF mTnf;
  JS::Heap<JSObject*> mType;
  JS::Heap<JSObject*> mId;
  JS::Heap<JSObject*> mPayload;
  uint32_t mSize;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MozNDEFRecord_h__

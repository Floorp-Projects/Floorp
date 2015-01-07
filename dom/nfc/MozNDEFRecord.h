/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
#include "nsPIDOMWindow.h"

struct JSContext;

namespace mozilla {
namespace dom {

class MozNDEFRecordOptions;

class MozNDEFRecord MOZ_FINAL : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozNDEFRecord)

public:
  MozNDEFRecord(nsPIDOMWindow* aWindow, TNF aTnf);

  ~MozNDEFRecord();

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<MozNDEFRecord>
  Constructor(const GlobalObject& aGlobal,
              const MozNDEFRecordOptions& aOptions,
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

private:
  MozNDEFRecord() = delete;
  nsRefPtr<nsPIDOMWindow> mWindow;
  void HoldData();
  void DropData();
  void InitType(JSContext* aCx, const Optional<Uint8Array>& aType);
  void InitId(JSContext* aCx, const Optional<Uint8Array>& aId);
  void InitPayload(JSContext* aCx, const Optional<Uint8Array>& aPayload);
  void IncSize(uint32_t aCount);
  void IncSizeForPayload(uint32_t aLen);

  static bool
  ValidateTNF(const MozNDEFRecordOptions& aOptions, ErrorResult& aRv);

  TNF mTnf;
  JS::Heap<JSObject*> mType;
  JS::Heap<JSObject*> mId;
  JS::Heap<JSObject*> mPayload;
  uint32_t mSize;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MozNDEFRecord_h__

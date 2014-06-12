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

#include "nsIDocument.h"

#include "mozilla/dom/TypedArray.h"
#include "jsfriendapi.h"
#include "js/GCAPI.h"

struct JSContext;

namespace mozilla {
namespace dom {

class MozNDEFRecord MOZ_FINAL : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozNDEFRecord)

public:

  MozNDEFRecord(JSContext* aCx, nsPIDOMWindow* aWindow, uint8_t aTnf,
                const Optional<Uint8Array>& aType,
                const Optional<Uint8Array>& aId,
                const Optional<Uint8Array>& aPlayload);

  ~MozNDEFRecord();

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  static already_AddRefed<MozNDEFRecord>
  Constructor(const GlobalObject& aGlobal, uint8_t aTnf,
              const Optional<Uint8Array>& aType,
              const Optional<Uint8Array>& aId,
              const Optional<Uint8Array>& aPayload, ErrorResult& aRv);

  uint8_t Tnf() const
  {
    return mTnf;
  }

  void GetType(JSContext* cx, JS::MutableHandle<JSObject*> retval) const
  {
    if (mType) {
      JS::ExposeObjectToActiveJS(mType);
    }
    retval.set(mType);
  }

  void GetId(JSContext* cx, JS::MutableHandle<JSObject*> retval) const
  {
    if (mId) {
      JS::ExposeObjectToActiveJS(mId);
    }
    retval.set(mId);
  }

  void GetPayload(JSContext* cx, JS::MutableHandle<JSObject*> retval) const
  {
    if (mPayload) {
      JS::ExposeObjectToActiveJS(mPayload);
    }
    retval.set(mPayload);
  }

private:
  MozNDEFRecord() MOZ_DELETE;
  nsRefPtr<nsPIDOMWindow> mWindow;
  void HoldData();
  void DropData();

  uint8_t mTnf;
  JS::Heap<JSObject*> mType;
  JS::Heap<JSObject*> mId;
  JS::Heap<JSObject*> mPayload;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MozNDEFRecord_h__

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaKeyStatuses_h
#define mozilla_dom_MediaKeyStatuses_h

#include "mozilla/ErrorResult.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"
#include "mozilla/CDMCaps.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class ArrayBufferViewOrArrayBuffer;

// The MediaKeyStatusMap WebIDL interface; maps a keyId to its status.
// Note that the underlying "map" is stored in an array, since we assume
// that a MediaKeySession won't have many key statuses to report.
class MediaKeyStatusMap final : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MediaKeyStatusMap)

public:
  explicit MediaKeyStatusMap(nsPIDOMWindowInner* aParent);

protected:
  ~MediaKeyStatusMap();

public:
  nsPIDOMWindowInner* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void Get(JSContext* aCx,
           const ArrayBufferViewOrArrayBuffer& aKey,
           JS::MutableHandle<JS::Value> aOutValue,
           ErrorResult& aOutRv) const;
  bool Has(const ArrayBufferViewOrArrayBuffer& aKey) const;
  uint32_t Size() const;

  uint32_t GetIterableLength() const;
  TypedArrayCreator<ArrayBuffer> GetKeyAtIndex(uint32_t aIndex) const;
  MediaKeyStatus GetValueAtIndex(uint32_t aIndex) const;

  void Update(const nsTArray<CDMCaps::KeyStatus>& keys);

private:

  nsCOMPtr<nsPIDOMWindowInner> mParent;

  struct KeyStatus {
    KeyStatus(const nsTArray<uint8_t>& aKeyId,
              MediaKeyStatus aStatus)
      : mKeyId(aKeyId)
      , mStatus(aStatus)
    {
    }
    bool operator== (const KeyStatus& aOther) const {
      return aOther.mKeyId == mKeyId;
    }
    bool operator<(const KeyStatus& aOther) const {
      // Copy chromium and compare keys' bytes.
      // Update once https://github.com/w3c/encrypted-media/issues/69
      // is resolved.
      const nsTArray<uint8_t>& other = aOther.mKeyId;
      const nsTArray<uint8_t>& self = mKeyId;
      size_t length = std::min<size_t>(other.Length(), self.Length());
      int cmp = memcmp(self.Elements(), other.Elements(), length);
      if (cmp != 0) {
        return cmp < 0;
      }
      return self.Length() <= other.Length();
    }
    nsTArray<uint8_t> mKeyId;
    MediaKeyStatus mStatus;
  };

  nsTArray<KeyStatus> mStatuses;
};

} // namespace dom
} // namespace mozilla

#endif

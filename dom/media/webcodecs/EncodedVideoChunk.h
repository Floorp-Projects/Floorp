/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EncodedVideoChunk_h
#define mozilla_dom_EncodedVideoChunk_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;

enum class EncodedVideoChunkType : uint8_t;
struct EncodedVideoChunkInit;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class EncodedVideoChunk final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(EncodedVideoChunk)

 public:
  EncodedVideoChunk() = default;

 protected:
  ~EncodedVideoChunk() = default;

 public:
  // This should return something that eventually allows finding a
  // path to the global this object is associated with.  Most simply,
  // returning an actual global works.
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<EncodedVideoChunk> Constructor(
      const GlobalObject& global, const EncodedVideoChunkInit& init,
      ErrorResult& aRv);

  EncodedVideoChunkType Type() const;

  int64_t Timestamp() const;

  Nullable<uint64_t> GetDuration() const;

  uint32_t ByteLength() const;

  void CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& destination,
      ErrorResult& aRv);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_EncodedVideoChunk_h

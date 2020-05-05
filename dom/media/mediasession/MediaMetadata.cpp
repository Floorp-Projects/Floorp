/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaMetadata, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaMetadata)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaMetadata)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaMetadata)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MediaMetadata::MediaMetadata(nsIGlobalObject* aParent, const nsString& aTitle,
                             const nsString& aArtist, const nsString& aAlbum)
    : MediaMetadataBase(aTitle, aArtist, aAlbum), mParent(aParent) {
  MOZ_ASSERT(mParent);
}

nsIGlobalObject* MediaMetadata::GetParentObject() const { return mParent; }

JSObject* MediaMetadata::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return MediaMetadata_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<MediaMetadata> MediaMetadata::Constructor(
    const GlobalObject& aGlobal, const MediaMetadataInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<MediaMetadata> mediaMetadata =
      new MediaMetadata(global, aInit.mTitle, aInit.mArtist, aInit.mAlbum);
  mediaMetadata->SetArtworkInternal(aInit.mArtwork, aRv);
  return aRv.Failed() ? nullptr : mediaMetadata.forget();
}

void MediaMetadata::GetTitle(nsString& aRetVal) const { aRetVal = mTitle; }

void MediaMetadata::SetTitle(const nsAString& aTitle) { mTitle = aTitle; }

void MediaMetadata::GetArtist(nsString& aRetVal) const { aRetVal = mArtist; }

void MediaMetadata::SetArtist(const nsAString& aArtist) { mArtist = aArtist; }

void MediaMetadata::GetAlbum(nsString& aRetVal) const { aRetVal = mAlbum; }

void MediaMetadata::SetAlbum(const nsAString& aAlbum) { mAlbum = aAlbum; }

void MediaMetadata::GetArtwork(JSContext* aCx, nsTArray<JSObject*>& aRetVal,
                               ErrorResult& aRv) const {
  // Convert the MediaImages to JS Objects
  if (!aRetVal.SetCapacity(mArtwork.Length(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (size_t i = 0; i < mArtwork.Length(); ++i) {
    JS::RootedValue value(aCx);
    if (!ToJSValue(aCx, mArtwork[i], &value)) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    JS::Rooted<JSObject*> object(aCx, &value.toObject());
    if (!JS_FreezeObject(aCx, object)) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    aRetVal.AppendElement(object);
  }
}

void MediaMetadata::SetArtwork(JSContext* aCx,
                               const Sequence<JSObject*>& aArtwork,
                               ErrorResult& aRv) {
  // Convert the JS Objects to MediaImages
  Sequence<MediaImage> artwork;
  if (!artwork.SetCapacity(aArtwork.Length(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  for (JSObject* object : aArtwork) {
    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*object));

    MediaImage* image = artwork.AppendElement(fallible);
    MOZ_ASSERT(image, "The capacity is preallocated");
    if (!image->Init(aCx, value)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
  }

  SetArtworkInternal(artwork, aRv);
};

static nsIURI* GetEntryBaseURL() {
  nsCOMPtr<Document> doc = GetEntryDocument();
  return doc ? doc->GetDocBaseURI() : nullptr;
}

// `aURL` is an inout parameter.
static nsresult ResolveURL(nsString& aURL, nsIURI* aBaseURI) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL,
                          /* UTF-8 for charset */ nullptr, aBaseURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return rv;
  }

  CopyUTF8toUTF16(spec, aURL);
  return NS_OK;
}

void MediaMetadata::SetArtworkInternal(const Sequence<MediaImage>& aArtwork,
                                       ErrorResult& aRv) {
  nsTArray<MediaImage> artwork;
  artwork.Assign(aArtwork);

  nsCOMPtr<nsIURI> baseURI = GetEntryBaseURL();
  for (MediaImage& image : artwork) {
    nsresult rv = ResolveURL(image.mSrc, baseURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(NS_ConvertUTF16toUTF8(image.mSrc));
      return;
    }
  }
  mArtwork = std::move(artwork);
}

}  // namespace dom
}  // namespace mozilla

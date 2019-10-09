/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaMetadata.h"
#include "mozilla/dom/MediaSessionBinding.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(MediaMetadata)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaMetadata)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaMetadata)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaMetadata)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsIGlobalObject* MediaMetadata::GetParentObject() const { return nullptr; }

JSObject* MediaMetadata::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return MediaMetadata_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<MediaMetadata> MediaMetadata::Constructor(
    const GlobalObject& aGlobal, const MediaMetadataInit& aInit,
    ErrorResult& aRv) {
  return nullptr;
}

void MediaMetadata::GetTitle(nsString& aRetVal) const {}

void MediaMetadata::SetTitle(const nsAString& aTitle) {}

void MediaMetadata::GetArtist(nsString& aRetVal) const {}

void MediaMetadata::SetArtist(const nsAString& aArtist) {}

void MediaMetadata::GetAlbum(nsString& aRetVal) const {}

void MediaMetadata::SetAlbum(const nsAString& aAlbum) {}

void MediaMetadata::GetArtwork(JSContext* aCx, nsTArray<JSObject*>& aRetVal,
                               ErrorResult& aRv) const {}

void MediaMetadata::SetArtwork(JSContext* aCx,
                               const Sequence<JSObject*>& aArtwork,
                               ErrorResult& aRv){};
}  // namespace dom
}  // namespace mozilla

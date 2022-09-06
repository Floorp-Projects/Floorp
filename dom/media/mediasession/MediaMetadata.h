/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MediaMetadata_h
#define mozilla_dom_MediaMetadata_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MediaSessionBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class MediaMetadataBase {
 public:
  MediaMetadataBase() = default;
  MediaMetadataBase(const nsString& aTitle, const nsString& aArtist,
                    const nsString& aAlbum)
      : mTitle(aTitle), mArtist(aArtist), mAlbum(aAlbum) {}

  static MediaMetadataBase EmptyData() { return MediaMetadataBase(); }

  nsString mTitle;
  nsString mArtist;
  nsString mAlbum;
  CopyableTArray<MediaImage> mArtwork;
};

class MediaMetadata final : public nsISupports,
                            public nsWrapperCache,
                            private MediaMetadataBase {
 public:
  // Ref counting and cycle collection
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MediaMetadata)

  // WebIDL methods
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<MediaMetadata> Constructor(
      const GlobalObject& aGlobal, const MediaMetadataInit& aInit,
      ErrorResult& aRv);

  void GetTitle(nsString& aRetVal) const;

  void SetTitle(const nsAString& aTitle);

  void GetArtist(nsString& aRetVal) const;

  void SetArtist(const nsAString& aArtist);

  void GetAlbum(nsString& aRetVal) const;

  void SetAlbum(const nsAString& aAlbum);

  void GetArtwork(JSContext* aCx, nsTArray<JSObject*>& aRetVal,
                  ErrorResult& aRv) const;

  void SetArtwork(JSContext* aCx, const Sequence<JSObject*>& aArtwork,
                  ErrorResult& aRv);

  // This would expose MediaMetadataBase's members as public, so use this method
  // carefully. Now we only use this when we want to update the metadata to the
  // media session controller in the chrome process.
  MediaMetadataBase* AsMetadataBase() { return this; }

 private:
  MediaMetadata(nsIGlobalObject* aParent, const nsString& aTitle,
                const nsString& aArtist, const nsString& aAlbum);

  ~MediaMetadata() = default;

  // Perform `convert artwork algorithm`. Set `mArtwork` to a converted
  // `aArtwork` if the conversion works, otherwise throw a type error in `aRv`.
  void SetArtworkInternal(const Sequence<MediaImage>& aArtwork,
                          ErrorResult& aRv);

  nsCOMPtr<nsIGlobalObject> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MediaMetadata_h

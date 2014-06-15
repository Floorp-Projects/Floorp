/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrack_h
#define mozilla_dom_TextTrack_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/TextTrackBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class TextTrackList;
class TextTrackCue;
class TextTrackCueList;
class TextTrackRegion;
class HTMLTrackElement;

enum TextTrackSource {
  Track,
  AddTextTrack,
  MediaResourceSpecific
};

// Constants for numeric readyState property values.
enum TextTrackReadyState {
  NotLoaded = 0U,
  Loading = 1U,
  Loaded = 2U,
  FailedToLoad = 3U
};

class TextTrack MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrack, DOMEventTargetHelper)

  TextTrack(nsPIDOMWindow* aOwnerWindow,
            TextTrackKind aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            TextTrackMode aMode,
            TextTrackReadyState aReadyState,
            TextTrackSource aTextTrackSource);
  TextTrack(nsPIDOMWindow* aOwnerWindow,
            TextTrackList* aTextTrackList,
            TextTrackKind aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            TextTrackMode aMode,
            TextTrackReadyState aReadyState,
            TextTrackSource aTextTrackSource);
  ~TextTrack();

  void SetDefaultSettings();

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  TextTrackKind Kind() const
  {
    return mKind;
  }
  void GetLabel(nsAString& aLabel) const
  {
    aLabel = mLabel;
  }
  void GetLanguage(nsAString& aLanguage) const
  {
    aLanguage = mLanguage;
  }
  void GetInBandMetadataTrackDispatchType(nsAString& aType) const
  {
    aType = mType;
  }
  void GetId(nsAString& aId) const;

  TextTrackMode Mode() const
  {
    return mMode;
  }
  void SetMode(TextTrackMode aValue);

  TextTrackCueList* GetCues() const
  {
    if (mMode == TextTrackMode::Disabled) {
      return nullptr;
    }
    return mCueList;
  }

  TextTrackCueList* GetActiveCues();
  void UpdateActiveCueList();
  void GetActiveCueArray(nsTArray<nsRefPtr<TextTrackCue> >& aCues);

  TextTrackReadyState ReadyState() const;
  void SetReadyState(TextTrackReadyState aState);
  void SetReadyState(uint32_t aReadyState);

  void AddCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue, ErrorResult& aRv);
  void SetDirty() { mDirty = true; }
  void SetCuesDirty();

  TextTrackList* GetTextTrackList();
  void SetTextTrackList(TextTrackList* aTextTrackList);

  IMPL_EVENT_HANDLER(cuechange)

  HTMLTrackElement* GetTrackElement();
  void SetTrackElement(HTMLTrackElement* aTrackElement);

  TextTrackSource GetTextTrackSource() {
    return mTextTrackSource;
  }

private:
  nsRefPtr<TextTrackList> mTextTrackList;

  TextTrackKind mKind;
  nsString mLabel;
  nsString mLanguage;
  nsString mType;
  TextTrackMode mMode;

  nsRefPtr<TextTrackCueList> mCueList;
  nsRefPtr<TextTrackCueList> mActiveCueList;
  nsRefPtr<HTMLTrackElement> mTrackElement;

  uint32_t mCuePos;
  TextTrackReadyState mReadyState;
  bool mDirty;

  // An enum that represents where the track was sourced from.
  TextTrackSource mTextTrackSource;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrack_h

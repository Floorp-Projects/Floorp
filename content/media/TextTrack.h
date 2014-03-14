/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrack_h
#define mozilla_dom_TextTrack_h

#include "mozilla/dom/TextTrackBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
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

class TextTrack MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrack, nsDOMEventTargetHelper)

  TextTrack(nsISupports* aParent,
            TextTrackSource aTextTrackSource);
  TextTrack(nsISupports* aParent,
            TextTrackKind aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            TextTrackSource aTextTrackSource);
  TextTrack(nsISupports* aParent,
            TextTrackList* aTextTrackList,
            TextTrackKind aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            TextTrackSource aTextTrackSource);

  void SetDefaultSettings();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mParent;
  }

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
  void GetId(nsAString& aId) const
  {
    aId = mId;
  }

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
  void GetActiveCueArray(nsTArray<nsRefPtr<TextTrackCue> >& aCues);

  uint16_t ReadyState() const;
  void SetReadyState(uint16_t aState);

  void AddCue(TextTrackCue& aCue);
  void RemoveCue(TextTrackCue& aCue, ErrorResult& aRv);
  void CueChanged(TextTrackCue& aCue);
  void SetDirty() { mDirty = true; }

  TextTrackList* GetTextTrackList();
  void SetTextTrackList(TextTrackList* aTextTrackList);

  IMPL_EVENT_HANDLER(cuechange)

  HTMLTrackElement* GetTrackElement();
  void SetTrackElement(HTMLTrackElement* aTrackElement);

  TextTrackSource GetTextTrackSource() {
    return mTextTrackSource;
  }

private:
  void UpdateActiveCueList();

  nsCOMPtr<nsISupports> mParent;
  nsRefPtr<TextTrackList> mTextTrackList;

  TextTrackKind mKind;
  nsString mLabel;
  nsString mLanguage;
  nsString mType;
  nsString mId;
  TextTrackMode mMode;

  nsRefPtr<TextTrackCueList> mCueList;
  nsRefPtr<TextTrackCueList> mActiveCueList;
  nsRefPtr<HTMLTrackElement> mTrackElement;

  uint32_t mCuePos;
  uint16_t mReadyState;
  bool mDirty;

  // An enum that represents where the track was sourced from.
  TextTrackSource mTextTrackSource;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrack_h

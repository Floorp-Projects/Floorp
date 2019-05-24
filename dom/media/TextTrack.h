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
#include "TimeUnits.h"

namespace mozilla {
namespace dom {

class TextTrackList;
class TextTrackCue;
class TextTrackCueList;
class HTMLTrackElement;
class HTMLMediaElement;

enum TextTrackSource { Track, AddTextTrack, MediaResourceSpecific };

// Constants for numeric readyState property values.
enum TextTrackReadyState {
  NotLoaded = 0U,
  Loading = 1U,
  Loaded = 2U,
  FailedToLoad = 3U
};

class TextTrack final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrack, DOMEventTargetHelper)

  TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackKind aKind,
            const nsAString& aLabel, const nsAString& aLanguage,
            TextTrackMode aMode, TextTrackReadyState aReadyState,
            TextTrackSource aTextTrackSource);
  TextTrack(nsPIDOMWindowInner* aOwnerWindow, TextTrackList* aTextTrackList,
            TextTrackKind aKind, const nsAString& aLabel,
            const nsAString& aLanguage, TextTrackMode aMode,
            TextTrackReadyState aReadyState, TextTrackSource aTextTrackSource);

  void SetDefaultSettings();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  TextTrackKind Kind() const { return mKind; }
  void GetLabel(nsAString& aLabel) const;
  void GetLanguage(nsAString& aLanguage) const;
  void GetInBandMetadataTrackDispatchType(nsAString& aType) const {
    aType = mType;
  }
  void GetId(nsAString& aId) const;

  TextTrackMode Mode() const { return mMode; }
  void SetMode(TextTrackMode aValue);

  TextTrackCueList* GetCues() const {
    if (mMode == TextTrackMode::Disabled) {
      return nullptr;
    }
    return mCueList;
  }

  TextTrackCueList* GetActiveCues();
  void GetActiveCueArray(nsTArray<RefPtr<TextTrackCue> >& aCues);

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

  TextTrackSource GetTextTrackSource() { return mTextTrackSource; }

  void SetCuesInactive();

  void NotifyCueUpdated(TextTrackCue* aCue);

  void DispatchAsyncTrustedEvent(const nsString& aEventName);

  bool IsLoaded();

  // Called when associated cue's active flag has been changed, and then we
  // would add or remove the cue to the active cue list.
  void NotifyCueActiveStateChanged(TextTrackCue* aCue);

  // Use this function to request current cues, which start time are less than
  // or equal to the current playback position and whose end times are greater
  // than the current playback position, and other cues, which are not in the
  // current cues. Because there would be LOTS of cues in the other cues, and we
  // don't actually need all of them. Therefore, we use a time interval to get
  // the cues which are overlapping within the time interval.
  void GetCurrentCuesAndOtherCues(RefPtr<TextTrackCueList>& aCurrentCues,
                                  RefPtr<TextTrackCueList>& aOtherCues,
                                  const media::TimeInterval& aInterval) const;

 private:
  ~TextTrack();

  HTMLMediaElement* GetMediaElement() const;

  RefPtr<TextTrackList> mTextTrackList;

  TextTrackKind mKind;
  nsString mLabel;
  nsString mLanguage;
  nsString mType;
  TextTrackMode mMode;

  RefPtr<TextTrackCueList> mCueList;
  RefPtr<TextTrackCueList> mActiveCueList;
  RefPtr<HTMLTrackElement> mTrackElement;

  uint32_t mCuePos;
  TextTrackReadyState mReadyState;
  bool mDirty;

  // An enum that represents where the track was sourced from.
  TextTrackSource mTextTrackSource;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TextTrack_h

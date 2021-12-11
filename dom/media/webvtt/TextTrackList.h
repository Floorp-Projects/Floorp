/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackList_h
#define mozilla_dom_TextTrackList_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/TextTrack.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class Event;
class HTMLMediaElement;
class TextTrackManager;
class CompareTextTracks;
class TrackEvent;
class TrackEventRunner;

class TextTrackList final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackList, DOMEventTargetHelper)

  explicit TextTrackList(nsPIDOMWindowInner* aOwnerWindow);
  TextTrackList(nsPIDOMWindowInner* aOwnerWindow,
                TextTrackManager* aTextTrackManager);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Length() const { return mTextTracks.Length(); }

  // Get all the current active cues.
  void GetShowingCues(nsTArray<RefPtr<TextTrackCue>>& aCues);

  TextTrack* IndexedGetter(uint32_t aIndex, bool& aFound);
  TextTrack* operator[](uint32_t aIndex);

  already_AddRefed<TextTrack> AddTextTrack(
      TextTrackKind aKind, const nsAString& aLabel, const nsAString& aLanguage,
      TextTrackMode aMode, TextTrackReadyState aReadyState,
      TextTrackSource aTextTrackSource, const CompareTextTracks& aCompareTT);
  TextTrack* GetTrackById(const nsAString& aId);

  void AddTextTrack(TextTrack* aTextTrack, const CompareTextTracks& aCompareTT);

  void RemoveTextTrack(TextTrack* aTrack);

  HTMLMediaElement* GetMediaElement();
  void SetTextTrackManager(TextTrackManager* aTextTrackManager);

  nsresult DispatchTrackEvent(Event* aEvent);
  void CreateAndDispatchChangeEvent();
  void SetCuesInactive();

  bool AreTextTracksLoaded();
  nsTArray<RefPtr<TextTrack>>& GetTextTrackArray();

  IMPL_EVENT_HANDLER(change)
  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

 private:
  ~TextTrackList();

  nsTArray<RefPtr<TextTrack>> mTextTracks;
  RefPtr<TextTrackManager> mTextTrackManager;

  void CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                         const nsAString& aEventName);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TextTrackList_h

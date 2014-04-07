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

class HTMLMediaElement;
class TextTrackManager;
class CompareTextTracks;
class TrackEvent;
class TrackEventRunner;

class TextTrackList MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackList, DOMEventTargetHelper)

  TextTrackList(nsPIDOMWindow* aOwnerWindow);
  TextTrackList(nsPIDOMWindow* aOwnerWindow, TextTrackManager* aTextTrackManager);

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  uint32_t Length() const
  {
    return mTextTracks.Length();
  }

  // Get all the current active cues.
  void UpdateAndGetShowingCues(nsTArray<nsRefPtr<TextTrackCue> >& aCues);

  TextTrack* IndexedGetter(uint32_t aIndex, bool& aFound);
  TextTrack* operator[](uint32_t aIndex);

  already_AddRefed<TextTrack> AddTextTrack(TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage,
                                           TextTrackMode aMode,
                                           TextTrackReadyState aReadyState,
                                           TextTrackSource aTextTrackSource,
                                           const CompareTextTracks& aCompareTT);
  TextTrack* GetTrackById(const nsAString& aId);

  void AddTextTrack(TextTrack* aTextTrack, const CompareTextTracks& aCompareTT);

  void RemoveTextTrack(TextTrack* aTrack);
  void DidSeek();

  HTMLMediaElement* GetMediaElement();
  void SetTextTrackManager(TextTrackManager* aTextTrackManager);

  nsresult DispatchTrackEvent(nsIDOMEvent* aEvent);
  void CreateAndDispatchChangeEvent();

  IMPL_EVENT_HANDLER(change)
  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

private:
  nsTArray< nsRefPtr<TextTrack> > mTextTracks;
  nsRefPtr<TextTrackManager> mTextTrackManager;

  void CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                         const nsAString& aEventName);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackList_h

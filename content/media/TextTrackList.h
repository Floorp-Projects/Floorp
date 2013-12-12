/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackList_h
#define mozilla_dom_TextTrackList_h

#include "mozilla/dom/TextTrack.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"

namespace mozilla {
namespace dom {

class TrackEvent;
class TrackEventRunner;

class TextTrackList MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackList, nsDOMEventTargetHelper)

  TextTrackList(nsISupports* aGlobal);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject() const
  {
    return mGlobal;
  }

  uint32_t Length() const
  {
    return mTextTracks.Length();
  }

  // Get all the current active cues.
  void GetAllActiveCues(nsTArray<nsRefPtr<TextTrackCue> >& aCues);

  TextTrack* IndexedGetter(uint32_t aIndex, bool& aFound);

  already_AddRefed<TextTrack> AddTextTrack(HTMLMediaElement* aMediaElement,
                                           TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage);
  TextTrack* GetTrackById(const nsAString& aId);

  void AddTextTrack(TextTrack* aTextTrack) {
    mTextTracks.AppendElement(aTextTrack);
  }

  void RemoveTextTrack(TextTrack* aTrack);
  void DidSeek();

  nsresult DispatchTrackEvent(nsIDOMEvent* aEvent);
  void CreateAndDispatchChangeEvent();

  IMPL_EVENT_HANDLER(change)
  IMPL_EVENT_HANDLER(addtrack)
  IMPL_EVENT_HANDLER(removetrack)

private:
  nsCOMPtr<nsISupports> mGlobal;
  nsTArray< nsRefPtr<TextTrack> > mTextTracks;

  void CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                         const nsAString& aEventName);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackList_h

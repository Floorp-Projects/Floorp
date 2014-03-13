/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackListBinding.h"
#include "mozilla/dom/TrackEvent.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_3(TextTrackList,
                                     nsDOMEventTargetHelper,
                                     mGlobal,
                                     mTextTracks,
                                     mTextTrackManager)

NS_IMPL_ADDREF_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrackList::TextTrackList(nsISupports* aGlobal) : mGlobal(aGlobal)
{
  SetIsDOMBinding();
}

TextTrackList::TextTrackList(nsISupports* aGlobal, TextTrackManager* aTextTrackManager)
 : mGlobal(aGlobal)
 , mTextTrackManager(aTextTrackManager)
{
  SetIsDOMBinding();
}

void
TextTrackList::GetAllActiveCues(nsTArray<nsRefPtr<TextTrackCue> >& aCues)
{
  nsTArray< nsRefPtr<TextTrackCue> > cues;
  for (uint32_t i = 0; i < Length(); i++) {
    if (mTextTracks[i]->Mode() != TextTrackMode::Disabled) {
      mTextTracks[i]->GetActiveCueArray(cues);
      aCues.AppendElements(cues);
    }
  }
}

JSObject*
TextTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return TextTrackListBinding::Wrap(aCx, aScope, this);
}

TextTrack*
TextTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTextTracks.Length();
  return aFound ? mTextTracks[aIndex] : nullptr;
}

already_AddRefed<TextTrack>
TextTrackList::AddTextTrack(TextTrackKind aKind,
                            const nsAString& aLabel,
                            const nsAString& aLanguage,
                            TextTrackMode aMode,
                            TextTrackSource aTextTrackSource,
                            const CompareTextTracks& aCompareTT)
{
  nsRefPtr<TextTrack> track = new TextTrack(mGlobal, this, aKind, aLabel, aLanguage,
                                            aMode, aTextTrackSource);
  AddTextTrack(track, aCompareTT);
  return track.forget();
}

void
TextTrackList::AddTextTrack(TextTrack* aTextTrack,
                            const CompareTextTracks& aCompareTT)
{
  if (mTextTracks.InsertElementSorted(aTextTrack, aCompareTT)) {
    aTextTrack->SetTextTrackList(this);
    CreateAndDispatchTrackEventRunner(aTextTrack, NS_LITERAL_STRING("addtrack"));
  }
}

TextTrack*
TextTrackList::GetTrackById(const nsAString& aId)
{
  nsAutoString id;
  for (uint32_t i = 0; i < Length(); i++) {
    mTextTracks[i]->GetId(id);
    if (aId.Equals(id)) {
      return mTextTracks[i];
    }
  }
  return nullptr;
}

void
TextTrackList::RemoveTextTrack(TextTrack* aTrack)
{
  if (mTextTracks.RemoveElement(aTrack)) {
    CreateAndDispatchTrackEventRunner(aTrack, NS_LITERAL_STRING("removetrack"));
  }
}

void
TextTrackList::DidSeek()
{
  for (uint32_t i = 0; i < mTextTracks.Length(); i++) {
    mTextTracks[i]->SetDirty();
  }
}

class TrackEventRunner MOZ_FINAL: public nsRunnable
{
public:
  TrackEventRunner(TextTrackList* aList, nsIDOMEvent* aEvent)
    : mList(aList)
    , mEvent(aEvent)
  {}

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    return mList->DispatchTrackEvent(mEvent);
  }

private:
  nsRefPtr<TextTrackList> mList;
  nsRefPtr<nsIDOMEvent> mEvent;
};

nsresult
TextTrackList::DispatchTrackEvent(nsIDOMEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

void
TextTrackList::CreateAndDispatchChangeEvent()
{
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create the error event!");
    return;
  }

  rv = event->InitEvent(NS_LITERAL_STRING("change"), false, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to init the change event!");
    return;
  }

  event->SetTrusted(true);

  nsCOMPtr<nsIRunnable> eventRunner = new TrackEventRunner(this, event);
  NS_DispatchToMainThread(eventRunner, NS_DISPATCH_NORMAL);
}

void
TextTrackList::CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                                 const nsAString& aEventName)
{
  TrackEventInit eventInit;
  eventInit.mBubbles = false;
  eventInit.mCancelable = false;
  eventInit.mTrack = aTrack;
  nsRefPtr<TrackEvent> event =
    TrackEvent::Constructor(this, aEventName, eventInit);

  // Dispatch the TrackEvent asynchronously.
  nsCOMPtr<nsIRunnable> eventRunner = new TrackEventRunner(this, event);
  NS_DispatchToMainThread(eventRunner, NS_DISPATCH_NORMAL);
}

HTMLMediaElement*
TextTrackList::GetMediaElement()
{
  if (mTextTrackManager) {
    return mTextTrackManager->mMediaElement;
  }
  return nullptr;
}

void
TextTrackList::SetTextTrackManager(TextTrackManager* aTextTrackManager)
{
  mTextTrackManager = aTextTrackManager;
}

} // namespace dom
} // namespace mozilla

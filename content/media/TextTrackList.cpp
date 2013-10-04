/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackListBinding.h"
#include "mozilla/dom/TrackEvent.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(TextTrackList,
                                     nsDOMEventTargetHelper,
                                     mGlobal,
                                     mTextTracks)

NS_IMPL_ADDREF_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackList, nsDOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackList)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

TextTrackList::TextTrackList(nsISupports* aGlobal) : mGlobal(aGlobal)
{
  SetIsDOMBinding();
}

void
TextTrackList::Update(double aTime)
{
  uint32_t length = Length(), i;
  for (i = 0; i < length; i++) {
    mTextTracks[i]->Update(aTime);
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
TextTrackList::AddTextTrack(HTMLMediaElement* aMediaElement,
                            TextTrackKind aKind,
                            const nsAString& aLabel,
                            const nsAString& aLanguage)
{
  nsRefPtr<TextTrack> track = new TextTrack(mGlobal, aMediaElement, aKind,
                                            aLabel, aLanguage);
  if (mTextTracks.AppendElement(track)) {
    CreateAndDispatchTrackEventRunner(track, NS_LITERAL_STRING("addtrack"));
  }

  return track.forget();
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
TextTrackList::RemoveTextTrack(TextTrack& aTrack)
{
  if (mTextTracks.RemoveElement(&aTrack)) {
    CreateAndDispatchTrackEventRunner(&aTrack, NS_LITERAL_STRING("removetrack"));
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
  TrackEventRunner(TextTrackList* aList, TrackEvent* aEvent)
    : mList(aList)
    , mEvent(aEvent)
  {}

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    return mList->DispatchTrackEvent(mEvent);
  }

private:
  nsRefPtr<TextTrackList> mList;
  nsRefPtr<TrackEvent> mEvent;
};

nsresult
TextTrackList::DispatchTrackEvent(TrackEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

void
TextTrackList::CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                                 const nsAString& aEventName)
{
  TrackEventInitInitializer eventInit;
  eventInit.mBubbles = false;
  eventInit.mCancelable = false;
  eventInit.mTrack = aTrack;
  nsRefPtr<TrackEvent> trackEvent =
    TrackEvent::Constructor(this, aEventName, eventInit);

  // Dispatch the TrackEvent asynchronously.
  nsCOMPtr<nsIRunnable> event = new TrackEventRunner(this, trackEvent);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla

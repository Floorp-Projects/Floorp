/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackList.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/TextTrackListBinding.h"
#include "mozilla/dom/TrackEvent.h"
#include "nsThreadUtils.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/dom/TextTrackManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(TextTrackList,
                                   DOMEventTargetHelper,
                                   mTextTracks,
                                   mTextTrackManager)

NS_IMPL_ADDREF_INHERITED(TextTrackList, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TextTrackList, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(TextTrackList)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TextTrackList::TextTrackList(nsPIDOMWindowInner* aOwnerWindow)
  : DOMEventTargetHelper(aOwnerWindow)
{
}

TextTrackList::TextTrackList(nsPIDOMWindowInner* aOwnerWindow,
                             TextTrackManager* aTextTrackManager)
  : DOMEventTargetHelper(aOwnerWindow)
  , mTextTrackManager(aTextTrackManager)
{
}

TextTrackList::~TextTrackList()
{
}

void
TextTrackList::GetShowingCues(nsTArray<RefPtr<TextTrackCue> >& aCues)
{
  // Only Subtitles and Captions can show on the screen.
  nsTArray< RefPtr<TextTrackCue> > cues;
  for (uint32_t i = 0; i < Length(); i++) {
    if (mTextTracks[i]->Mode() == TextTrackMode::Showing &&
        (mTextTracks[i]->Kind() == TextTrackKind::Subtitles ||
         mTextTracks[i]->Kind() == TextTrackKind::Captions)) {
      mTextTracks[i]->GetActiveCueArray(cues);
      aCues.AppendElements(cues);
    }
  }
}

JSObject*
TextTrackList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TextTrackListBinding::Wrap(aCx, this, aGivenProto);
}

TextTrack*
TextTrackList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mTextTracks.Length();
  if (!aFound) {
    return nullptr;
  }
  return mTextTracks[aIndex];
}

TextTrack*
TextTrackList::operator[](uint32_t aIndex)
{
  return mTextTracks.SafeElementAt(aIndex, nullptr);
}

already_AddRefed<TextTrack>
TextTrackList::AddTextTrack(TextTrackKind aKind,
                            const nsAString& aLabel,
                            const nsAString& aLanguage,
                            TextTrackMode aMode,
                            TextTrackReadyState aReadyState,
                            TextTrackSource aTextTrackSource,
                            const CompareTextTracks& aCompareTT)
{
  RefPtr<TextTrack> track = new TextTrack(GetOwner(), this, aKind, aLabel,
                                            aLanguage, aMode, aReadyState,
                                            aTextTrackSource);
  AddTextTrack(track, aCompareTT);
  return track.forget();
}

void
TextTrackList::AddTextTrack(TextTrack* aTextTrack,
                            const CompareTextTracks& aCompareTT)
{
  if (mTextTracks.Contains(aTextTrack)) {
    return;
  }
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

class TrackEventRunner : public Runnable
{
public:
  TrackEventRunner(TextTrackList* aList, nsIDOMEvent* aEvent)
    : mList(aList)
    , mEvent(aEvent)
  {}

  NS_IMETHOD Run() override
  {
    return mList->DispatchTrackEvent(mEvent);
  }

  RefPtr<TextTrackList> mList;
private:
  RefPtr<nsIDOMEvent> mEvent;
};

class ChangeEventRunner final : public TrackEventRunner
{
public:
  ChangeEventRunner(TextTrackList* aList, nsIDOMEvent* aEvent)
    : TrackEventRunner(aList, aEvent)
  {}

  NS_IMETHOD Run() override
  {
    mList->mPendingTextTrackChange = false;
    return TrackEventRunner::Run();
  }
};

nsresult
TextTrackList::DispatchTrackEvent(nsIDOMEvent* aEvent)
{
  return DispatchTrustedEvent(aEvent);
}

void
TextTrackList::CreateAndDispatchChangeEvent()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mPendingTextTrackChange) {
    nsPIDOMWindowInner* win = GetOwner();
    if (!win) {
      return;
    }

    mPendingTextTrackChange = true;
    RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);

    event->InitEvent(NS_LITERAL_STRING("change"), false, false);
    event->SetTrusted(true);

    nsCOMPtr<nsIRunnable> eventRunner = new ChangeEventRunner(this, event);
    nsGlobalWindow::Cast(win)->Dispatch(
      "TextTrackList::CreateAndDispatchChangeEvent", TaskCategory::Other,
      eventRunner.forget());
  }
}

void
TextTrackList::CreateAndDispatchTrackEventRunner(TextTrack* aTrack,
                                                 const nsAString& aEventName)
{
  DebugOnly<nsresult> rv;
  nsCOMPtr<nsIEventTarget> target = GetMainThreadEventTarget();
  if (!target) {
    // If we are not able to get the main-thread object we are shutting down.
    return;
  }

  TrackEventInit eventInit;
  eventInit.mTrack.SetValue().SetAsTextTrack() = aTrack;
  RefPtr<TrackEvent> event =
    TrackEvent::Constructor(this, aEventName, eventInit);

  // Dispatch the TrackEvent asynchronously.
  rv = target->Dispatch(do_AddRef(new TrackEventRunner(this, event)),
                        NS_DISPATCH_NORMAL);

  // If we are shutting down this can file but it's still ok.
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Dispatch failed");
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

void
TextTrackList::SetCuesInactive()
{
  for (uint32_t i = 0; i < Length(); i++) {
    mTextTracks[i]->SetCuesInactive();
  }
}


bool TextTrackList::AreTextTracksLoaded()
{
  // Return false if any texttrack is not loaded.
  for (uint32_t i = 0; i < Length(); i++) {
    if (!mTextTracks[i]->IsLoaded()) {
      return false;
    }
  }
  return true;
}

nsTArray<RefPtr<TextTrack>>&
TextTrackList::GetTextTrackArray()
{
  return mTextTracks;
}

} // namespace dom
} // namespace mozilla

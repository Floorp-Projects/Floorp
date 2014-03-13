/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TextTrackManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLTrackElement.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackCue.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsComponentManagerUtils.h"
#include "nsVideoFrame.h"
#include "nsIFrame.h"
#include "nsTArrayHelpers.h"
#include "nsIWebVTTParserWrapper.h"

namespace mozilla {
namespace dom {

CompareTextTracks::CompareTextTracks(HTMLMediaElement* aMediaElement)
{
  mMediaElement = aMediaElement;
}

int32_t
CompareTextTracks::TrackChildPosition(TextTrack* aTextTrack) const {
  HTMLTrackElement* trackElement = aTextTrack->GetTrackElement();
  if (!trackElement) {
    return -1;
  }
  return mMediaElement->IndexOf(trackElement);
}

bool
CompareTextTracks::Equals(TextTrack* aOne, TextTrack* aTwo) const {
  // Two tracks can never be equal. If they have corresponding TrackElements
  // they would need to occupy the same tree position (impossible) and in the
  // case of tracks coming from AddTextTrack source we put the newest at the
  // last position, so they won't be equal as well.
  return false;
}

bool
CompareTextTracks::LessThan(TextTrack* aOne, TextTrack* aTwo) const
{
  TextTrackSource sourceOne = aOne->GetTextTrackSource();
  TextTrackSource sourceTwo = aTwo->GetTextTrackSource();
  if (sourceOne != sourceTwo) {
    return sourceOne == Track ||
           (sourceOne == AddTextTrack && sourceTwo == MediaResourceSpecific);
  }
  switch (sourceOne) {
    case Track: {
      int32_t positionOne = TrackChildPosition(aOne);
      int32_t positionTwo = TrackChildPosition(aTwo);
      // If either position one or positiontwo are -1 then something has gone
      // wrong. In this case we should just put them at the back of the list.
      return positionOne != -1 && positionTwo != -1 &&
             positionOne < positionTwo;
    }
    case AddTextTrack:
      // For AddTextTrack sources the tracks will already be in the correct relative
      // order in the source array. Assume we're called in iteration order and can
      // therefore always report aOne < aTwo to maintain the original temporal ordering.
      return true;
    case MediaResourceSpecific:
      // No rules for Media Resource Specific tracks yet.
      break;
  }
  return true;
}

NS_IMPL_CYCLE_COLLECTION_3(TextTrackManager, mTextTracks,
                           mPendingTextTracks, mNewCues)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(TextTrackManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(TextTrackManager, Release)

StaticRefPtr<nsIWebVTTParserWrapper> TextTrackManager::sParserWrapper;

TextTrackManager::TextTrackManager(HTMLMediaElement *aMediaElement)
  : mMediaElement(aMediaElement)
{
  MOZ_COUNT_CTOR(TextTrackManager);
  mNewCues = new TextTrackCueList(mMediaElement->OwnerDoc()->GetParentObject());
  mTextTracks = new TextTrackList(mMediaElement->OwnerDoc()->GetParentObject(),
                                  this);
  mPendingTextTracks =
    new TextTrackList(mMediaElement->OwnerDoc()->GetParentObject(), this);

  if (!sParserWrapper) {
    nsCOMPtr<nsIWebVTTParserWrapper> parserWrapper =
      do_CreateInstance(NS_WEBVTTPARSERWRAPPER_CONTRACTID);
    sParserWrapper = parserWrapper;
    ClearOnShutdown(&sParserWrapper);
  }
}

TextTrackManager::~TextTrackManager()
{
  MOZ_COUNT_DTOR(TextTrackManager);
}

TextTrackList*
TextTrackManager::TextTracks() const
{
  return mTextTracks;
}

already_AddRefed<TextTrack>
TextTrackManager::AddTextTrack(TextTrackKind aKind, const nsAString& aLabel,
                               const nsAString& aLanguage,
                               TextTrackMode aMode,
                               TextTrackReadyState aReadyState,
                               TextTrackSource aTextTrackSource)
{
  if (!mMediaElement) {
    return nullptr;
  }
  nsRefPtr<TextTrack> ttrack =
    mTextTracks->AddTextTrack(aKind, aLabel, aLanguage, aMode, aReadyState,
                              aTextTrackSource, CompareTextTracks(mMediaElement));
  AddCues(ttrack);
  return ttrack.forget();
}

void
TextTrackManager::AddTextTrack(TextTrack* aTextTrack)
{
  if (!mMediaElement) {
    return;
  }
  mTextTracks->AddTextTrack(aTextTrack, CompareTextTracks(mMediaElement));
  AddCues(aTextTrack);
}

void
TextTrackManager::AddCues(TextTrack* aTextTrack)
{
  TextTrackCueList* cueList = aTextTrack->GetCues();
  if (cueList) {
    bool dummy;
    for (uint32_t i = 0; i < cueList->Length(); ++i) {
      mNewCues->AddCue(*cueList->IndexedGetter(i, dummy));
    }
  }
}

void
TextTrackManager::RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly)
{
  mPendingTextTracks->RemoveTextTrack(aTextTrack);
  if (aPendingListOnly) {
    return;
  }

  mTextTracks->RemoveTextTrack(aTextTrack);
}

void
TextTrackManager::DidSeek()
{
  mTextTracks->DidSeek();
}

void
TextTrackManager::UpdateCueDisplay()
{
  nsIFrame* frame = mMediaElement->GetPrimaryFrame();
  if (!frame) {
    return;
  }

  nsVideoFrame* videoFrame = do_QueryFrame(frame);
  if (!videoFrame) {
    return;
  }

  nsCOMPtr<nsIContent> overlay = videoFrame->GetCaptionOverlay();
  if (!overlay) {
    return;
  }

  nsTArray<nsRefPtr<TextTrackCue> > activeCues;
  mTextTracks->GetAllActiveCues(activeCues);

  if (activeCues.Length() > 0) {
    nsCOMPtr<nsIWritableVariant> jsCues =
      do_CreateInstance("@mozilla.org/variant;1");

    jsCues->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                       &NS_GET_IID(nsIDOMEventTarget),
                       activeCues.Length(),
                       static_cast<void*>(activeCues.Elements()));

    nsPIDOMWindow* window = mMediaElement->OwnerDoc()->GetWindow();
    if (window) {
      sParserWrapper->ProcessCues(window, jsCues, overlay);
    }
  } else if (overlay->Length() > 0) {
    nsContentUtils::SetNodeTextContent(overlay, EmptyString(), true);
  }
}

void
TextTrackManager::AddCue(TextTrackCue& aCue)
{
  mNewCues->AddCue(aCue);
}

void
TextTrackManager::PopulatePendingList()
{
  uint32_t len = mTextTracks->Length();
  bool dummy;
  for (uint32_t index = 0; index < len; ++index) {
    TextTrack* ttrack = mTextTracks->IndexedGetter(index, dummy);
    if (ttrack && ttrack->Mode() != TextTrackMode::Disabled &&
        ttrack->ReadyState() == TextTrackReadyState::Loading) {
      mPendingTextTracks->AddTextTrack(ttrack,
                                       CompareTextTracks(mMediaElement));
    }
  }
}

} // namespace dom
} // namespace mozilla

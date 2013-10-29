/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackManager_h
#define mozilla_dom_TextTrackManager_h

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackList.h"

namespace mozilla {
namespace dom {

class HTMLMediaElement;

class TextTrackManager
{
public:
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TextTrackManager)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TextTrackManager);

  TextTrackManager(HTMLMediaElement *aMediaElement);
  ~TextTrackManager();

  TextTrackList* TextTracks() const;
  already_AddRefed<TextTrack> AddTextTrack(TextTrackKind aKind,
                                           const nsAString& aLabel,
                                           const nsAString& aLanguage);
  void AddTextTrack(TextTrack* aTextTrack);
  void RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly);
  void DidSeek();

  // Update the display of cues on the video as per the current play back time
  // of aTime.
  void Update(double aTime);

  void PopulatePendingList();

private:
  // The HTMLMediaElement that this TextTrackManager manages the TextTracks of.
  // This is a weak reference as the life time of TextTrackManager is dependent
  // on the HTMLMediaElement, so it should not be trying to hold the
  // HTMLMediaElement alive.
  HTMLMediaElement* mMediaElement;
  // List of the TextTrackManager's owning HTMLMediaElement's TextTracks.
  nsRefPtr<TextTrackList> mTextTracks;
  // List of text track objects awaiting loading.
  nsRefPtr<TextTrackList> mPendingTextTracks;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackManager_h

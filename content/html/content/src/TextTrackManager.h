/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackManager_h
#define mozilla_dom_TextTrackManager_h

#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackList.h"
#include "mozilla/dom/TextTrackCueList.h"
#include "mozilla/StaticPtr.h"

class nsIWebVTTParserWrapper;

namespace mozilla {
namespace dom {

class HTMLMediaElement;

class CompareTextTracks {
private:
  HTMLMediaElement* mMediaElement;
public:
  CompareTextTracks(HTMLMediaElement* aMediaElement);
  int32_t TrackChildPosition(TextTrack* aTrack) const;
  bool Equals(TextTrack* aOne, TextTrack* aTwo) const;
  bool LessThan(TextTrack* aOne, TextTrack* aTwo) const;
};

class TextTrack;
class TextTrackCue;

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
                                           const nsAString& aLanguage,
                                           TextTrackSource aTextTrackSource);
  void AddTextTrack(TextTrack* aTextTrack);
  void RemoveTextTrack(TextTrack* aTextTrack, bool aPendingListOnly);
  void DidSeek();

  void AddCue(TextTrackCue& aCue);
  void AddCues(TextTrack* aTextTrack);

  /**
   * Overview of WebVTT cuetext and anonymous content setup.
   *
   * WebVTT nodes are the parsed version of WebVTT cuetext. WebVTT cuetext is
   * the portion of a WebVTT cue that specifies what the caption will actually
   * show up as on screen.
   *
   * WebVTT cuetext can contain markup that loosely relates to HTML markup. It
   * can contain tags like <b>, <u>, <i>, <c>, <v>, <ruby>, <rt>, <lang>,
   * including timestamp tags.
   *
   * When the caption is ready to be displayed the WebVTT nodes are converted
   * over to anonymous DOM content. <i>, <u>, <b>, <ruby>, and <rt> all become
   * HTMLElements of their corresponding HTML markup tags. <c> and <v> are
   * converted to <span> tags. Timestamp tags are converted to XML processing
   * instructions. Additionally, all cuetext tags support specifying of classes.
   * This takes the form of <foo.class.subclass>. These classes are then parsed
   * and set as the anonymous content's class attribute.
   *
   * Rules on constructing DOM objects from WebVTT nodes can be found here
   * http://dev.w3.org/html5/webvtt/#webvtt-cue-text-dom-construction-rules.
   * Current rules are taken from revision on April 15, 2013.
   */

  /**
   * Converts the TextTrackCue's cuetext into a tree of DOM objects and attaches
   * it to a div on it's owning TrackElement's MediaElement's caption overlay.
   */
  void UpdateCueDisplay();

  void PopulatePendingList();

  // The HTMLMediaElement that this TextTrackManager manages the TextTracks of.
  // This is a weak reference as the life time of TextTrackManager is dependent
  // on the HTMLMediaElement, so it should not be trying to hold the
  // HTMLMediaElement alive.
  HTMLMediaElement* mMediaElement;
private:
  // List of the TextTrackManager's owning HTMLMediaElement's TextTracks.
  nsRefPtr<TextTrackList> mTextTracks;
  // List of text track objects awaiting loading.
  nsRefPtr<TextTrackList> mPendingTextTracks;
  // List of newly introduced Text Track cues.
  nsRefPtr<TextTrackCueList> mNewCues;

  static StaticRefPtr<nsIWebVTTParserWrapper> sParserWrapper;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackManager_h

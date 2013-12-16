/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCue_h
#define mozilla_dom_TextTrackCue_h

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/VTTCueBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIWebVTTParserWrapper.h"
#include "mozilla/StaticPtr.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

class HTMLTrackElement;
class TextTrack;

class TextTrackCue MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackCue, nsDOMEventTargetHelper)

  // TextTrackCue WebIDL
  // See bug 868509 about splitting out the WebVTT-specific interfaces.
  static already_AddRefed<TextTrackCue>
  Constructor(GlobalObject& aGlobal,
              double aStartTime,
              double aEndTime,
              const nsAString& aText,
              ErrorResult& aRv)
  {
    nsRefPtr<TextTrackCue> ttcue = new TextTrackCue(aGlobal.GetAsSupports(), aStartTime,
                                                    aEndTime, aText, aRv);
    return ttcue.forget();
  }
  TextTrackCue(nsISupports* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, ErrorResult& aRv);

  TextTrackCue(nsISupports* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, HTMLTrackElement* aTrackElement,
               ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsINode* GetParentObject()
  {
    return mDocument;
  }

  TextTrack* GetTrack() const
  {
    return mTrack;
  }

  void GetId(nsAString& aId) const
  {
    aId = mId;
  }

  void SetId(const nsAString& aId)
  {
    if (mId == aId) {
      return;
    }

    mId = aId;
    CueChanged();
  }

  double StartTime() const
  {
    return mStartTime;
  }

  void SetStartTime(double aStartTime)
  {
    if (mStartTime == aStartTime) {
      return;
    }

    mStartTime = aStartTime;
    CueChanged();
  }

  double EndTime() const
  {
    return mEndTime;
  }

  void SetEndTime(double aEndTime)
  {
    if (mEndTime == aEndTime) {
      return;
    }

    mEndTime = aEndTime;
    CueChanged();
  }

  bool PauseOnExit()
  {
    return mPauseOnExit;
  }

  void SetPauseOnExit(bool aPauseOnExit)
  {
    if (mPauseOnExit == aPauseOnExit) {
      return;
    }

    mPauseOnExit = aPauseOnExit;
    CueChanged();
  }

  void GetRegionId(nsAString& aRegionId) const
  {
    aRegionId = mRegionId;
  }

  void SetRegionId(const nsAString& aRegionId)
  {
    if (mRegionId == aRegionId) {
      return;
    }

    mRegionId = aRegionId;
    CueChanged();
  }

  DirectionSetting Vertical() const
  {
    return mVertical;
  }

  void SetVertical(const DirectionSetting& aVertical)
  {
    if (mVertical == aVertical) {
      return;
    }

    mReset = true;
    mVertical = aVertical;
    CueChanged();
  }

  bool SnapToLines()
  {
    return mSnapToLines;
  }

  void SetSnapToLines(bool aSnapToLines)
  {
    if (mSnapToLines == aSnapToLines) {
      return;
    }

    mReset = true;
    mSnapToLines = aSnapToLines;
    CueChanged();
  }

  double Line() const
  {
    return mLine;
  }

  void SetLine(double aLine)
  {
    //XXX: TODO Line position can be a keyword auto. bug882299
    mReset = true;
    mLine = aLine;
  }

  AlignSetting LineAlign() const
  {
    return mLineAlign;
  }

  void SetLineAlign(AlignSetting& aLineAlign, ErrorResult& aRv)
  {
    if (mLineAlign == aLineAlign)
      return;

    if (aLineAlign == AlignSetting::Left ||
        aLineAlign == AlignSetting::Right) {
      return aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    }

    mReset = true;
    mLineAlign = aLineAlign;
    CueChanged();
  }

  int32_t Position() const
  {
    return mPosition;
  }

  void SetPosition(int32_t aPosition, ErrorResult& aRv)
  {
    if (mPosition == aPosition) {
      return;
    }

    if (aPosition > 100 || aPosition < 0){
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    mReset = true;
    mPosition = aPosition;
    CueChanged();
  }

  int32_t Size() const
  {
    return mSize;
  }

  void SetSize(int32_t aSize, ErrorResult& aRv)
  {
    if (mSize == aSize) {
      return;
    }

    if (aSize < 0 || aSize > 100) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    mReset = true;
    mSize = aSize;
    CueChanged();
  }

  AlignSetting Align() const
  {
    return mAlign;
  }

  void SetAlign(AlignSetting& aAlign)
  {
    if (mAlign == aAlign) {
      return;
    }

    mReset = true;
    mAlign = aAlign;
    CueChanged();
  }

  void GetText(nsAString& aText) const
  {
    aText = mText;
  }

  void SetText(const nsAString& aText)
  {
    if (mText == aText) {
      return;
    }

    mReset = true;
    mText = aText;
    CueChanged();
  }

  IMPL_EVENT_HANDLER(enter)
  IMPL_EVENT_HANDLER(exit)

  // Helper functions for implementation.
  bool
  operator==(const TextTrackCue& rhs) const
  {
    return mId.Equals(rhs.mId);
  }

  const nsAString& Id() const
  {
    return mId;
  }

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
  void RenderCue();

  /**
   * Produces a tree of anonymous content based on the tree of the processed
   * cue text.
   *
   * Returns a DocumentFragment that is the head of the tree of anonymous
   * content.
   */
  already_AddRefed<DocumentFragment> GetCueAsHTML();

  void SetTrackElement(HTMLTrackElement* aTrackElement);

private:
  void CueChanged();
  void SetDefaultCueSettings();
  void CreateCueOverlay();
  nsresult StashDocument(nsISupports* aGlobal);

  nsRefPtr<nsIDocument> mDocument;
  nsString mText;
  double mStartTime;
  double mEndTime;

  nsRefPtr<TextTrack> mTrack;
  nsRefPtr<HTMLTrackElement> mTrackElement;
  nsString mId;
  int32_t mPosition;
  int32_t mSize;
  bool mPauseOnExit;
  bool mSnapToLines;
  nsString mRegionId;
  DirectionSetting mVertical;
  int mLine;
  AlignSetting mAlign;
  AlignSetting mLineAlign;

  // Holds the computed DOM elements that represent the parsed cue text.
  // http://www.whatwg.org/specs/web-apps/current-work/#text-track-cue-display-state
  nsCOMPtr<nsIContent> mDisplayState;
  // Tells whether or not we need to recompute mDisplayState. This is set
  // anytime a property that relates to the display of the TextTrackCue is
  // changed.
  bool mReset;

  static StaticRefPtr<nsIWebVTTParserWrapper> sParserWrapper;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCue_h

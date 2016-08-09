/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCue_h
#define mozilla_dom_TextTrackCue_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/VTTCueBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIWebVTTParserWrapper.h"
#include "mozilla/StaticPtr.h"
#include "nsIDocument.h"
#include "mozilla/dom/HTMLDivElement.h"
#include "mozilla/dom/TextTrack.h"

namespace mozilla {
namespace dom {

class HTMLTrackElement;
class TextTrackRegion;

class TextTrackCue final : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextTrackCue, DOMEventTargetHelper)

  // TextTrackCue WebIDL
  // See bug 868509 about splitting out the WebVTT-specific interfaces.
  static already_AddRefed<TextTrackCue>
  Constructor(GlobalObject& aGlobal,
              double aStartTime,
              double aEndTime,
              const nsAString& aText,
              ErrorResult& aRv)
  {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
    RefPtr<TextTrackCue> ttcue = new TextTrackCue(window, aStartTime,
                                                    aEndTime, aText, aRv);
    return ttcue.forget();
  }
  TextTrackCue(nsPIDOMWindowInner* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, ErrorResult& aRv);

  TextTrackCue(nsPIDOMWindowInner* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, HTMLTrackElement* aTrackElement,
               ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

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
    mReset = true;
    NotifyCueUpdated(this);
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
    mReset = true;
    NotifyCueUpdated(this);
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
    NotifyCueUpdated(nullptr);
  }

  TextTrackRegion* GetRegion();
  void SetRegion(TextTrackRegion* aRegion);

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
  }

  void GetLine(OwningDoubleOrAutoKeyword& aLine) const
  {
    if (mLineIsAutoKeyword) {
      aLine.SetAsAutoKeyword() = AutoKeyword::Auto;
      return;
    }
    aLine.SetAsDouble() = mLine;
  }

  void SetLine(const DoubleOrAutoKeyword& aLine)
  {
    if (aLine.IsDouble() &&
        (mLineIsAutoKeyword || (aLine.GetAsDouble() != mLine))) {
      mLineIsAutoKeyword = false;
      mLine = aLine.GetAsDouble();
      mReset = true;
      return;
    }
    if (aLine.IsAutoKeyword() && !mLineIsAutoKeyword) {
      mLineIsAutoKeyword = true;
      mReset = true;
    }
  }

  LineAlignSetting LineAlign() const
  {
    return mLineAlign;
  }

  void SetLineAlign(LineAlignSetting& aLineAlign, ErrorResult& aRv)
  {
    if (mLineAlign == aLineAlign) {
      return;
    }

    mReset = true;
    mLineAlign = aLineAlign;
  }

  void GetPosition(OwningDoubleOrAutoKeyword& aPosition) const
  {
    if (mPositionIsAutoKeyword) {
      aPosition.SetAsAutoKeyword() = AutoKeyword::Auto;
      return;
    }
    aPosition.SetAsDouble() = mPosition;
  }

  void SetPosition(const DoubleOrAutoKeyword& aPosition, ErrorResult& aRv)
  {
    if (!aPosition.IsAutoKeyword() &&
        (aPosition.GetAsDouble() > 100.0 || aPosition.GetAsDouble() < 0.0)){
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    if (aPosition.IsDouble() &&
        (mPositionIsAutoKeyword || (aPosition.GetAsDouble() != mPosition))) {
      mPositionIsAutoKeyword = false;
      mPosition = aPosition.GetAsDouble();
      mReset = true;
      return;
    }

    if (aPosition.IsAutoKeyword() && !mPositionIsAutoKeyword) {
      mPositionIsAutoKeyword = true;
      mReset = true;
    }
  }

  PositionAlignSetting PositionAlign() const
  {
    return mPositionAlign;
  }

  void SetPositionAlign(PositionAlignSetting aPositionAlign, ErrorResult& aRv)
  {
    if (mPositionAlign == aPositionAlign) {
      return;
    }

    mReset = true;
    mPositionAlign = aPositionAlign;
  }

  double Size() const
  {
    return mSize;
  }

  void SetSize(double aSize, ErrorResult& aRv)
  {
    if (mSize == aSize) {
      return;
    }

    if (aSize < 0.0 || aSize > 100.0) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return;
    }

    mReset = true;
    mSize = aSize;
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
  }

  IMPL_EVENT_HANDLER(enter)
  IMPL_EVENT_HANDLER(exit)

  HTMLDivElement* GetDisplayState()
  {
    return static_cast<HTMLDivElement*>(mDisplayState.get());
  }

  void SetDisplayState(HTMLDivElement* aDisplayState)
  {
    mDisplayState = aDisplayState;
    mReset = false;
  }

  void Reset()
  {
    mReset = true;
  }

  bool HasBeenReset()
  {
    return mReset;
  }

  double ComputedLine();
  double ComputedPosition();
  PositionAlignSetting ComputedPositionAlign();

  // Helper functions for implementation.
  const nsAString& Id() const
  {
    return mId;
  }

  void SetTrack(TextTrack* aTextTrack)
  {
    mTrack = aTextTrack;
  }

  /**
   * Produces a tree of anonymous content based on the tree of the processed
   * cue text.
   *
   * Returns a DocumentFragment that is the head of the tree of anonymous
   * content.
   */
  already_AddRefed<DocumentFragment> GetCueAsHTML();

  void SetTrackElement(HTMLTrackElement* aTrackElement);

  void SetActive(bool aActive)
  {
    if (mActive == aActive) {
      return;
    }

    mActive = aActive;
    mDisplayState = mActive ? mDisplayState : nullptr;
  }

  bool GetActive()
  {
    return mActive;
  }

private:
  ~TextTrackCue();

  void NotifyCueUpdated(TextTrackCue* aCue)
  {
    if (mTrack) {
      mTrack->NotifyCueUpdated(aCue);
    }
  }

  void NotifyDisplayStatesChanged();

  void SetDefaultCueSettings();
  nsresult StashDocument();

  RefPtr<nsIDocument> mDocument;
  nsString mText;
  double mStartTime;
  double mEndTime;

  RefPtr<TextTrack> mTrack;
  RefPtr<HTMLTrackElement> mTrackElement;
  nsString mId;
  double mPosition;
  bool mPositionIsAutoKeyword;
  PositionAlignSetting mPositionAlign;
  double mSize;
  bool mPauseOnExit;
  bool mSnapToLines;
  RefPtr<TextTrackRegion> mRegion;
  DirectionSetting mVertical;
  bool mLineIsAutoKeyword;
  double mLine;
  AlignSetting mAlign;
  LineAlignSetting mLineAlign;

  // Holds the computed DOM elements that represent the parsed cue text.
  // http://www.whatwg.org/specs/web-apps/current-work/#text-track-cue-display-state
  RefPtr<nsGenericHTMLElement> mDisplayState;
  // Tells whether or not we need to recompute mDisplayState. This is set
  // anytime a property that relates to the display of the TextTrackCue is
  // changed.
  bool mReset;

  bool mActive;

  static StaticRefPtr<nsIWebVTTParserWrapper> sParserWrapper;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCue_h

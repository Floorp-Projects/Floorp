/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextTrackCue_h
#define mozilla_dom_TextTrackCue_h

#define WEBVTT_NO_CONFIG_H 1
#define WEBVTT_STATIC 1

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/TextTrack.h"
#include "mozilla/dom/TextTrackCueBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMEventTargetHelper.h"
#include "webvtt/node.h"

namespace mozilla {
namespace dom {

class HTMLTrackElement;
class TextTrack;

class TextTrackCue MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(TextTrackCue,
                                                         nsDOMEventTargetHelper)

  // TextTrackCue WebIDL
  // See bug 868509 about splitting out the WebVTT-specific interfaces.
  static already_AddRefed<TextTrackCue>
  Constructor(GlobalObject& aGlobal,
              double aStartTime,
              double aEndTime,
              const nsAString& aText,
              ErrorResult& aRv)
  {
    nsRefPtr<TextTrackCue> ttcue = new TextTrackCue(aGlobal.Get(), aStartTime,
                                                    aEndTime, aText);
    return ttcue.forget();
  }
  TextTrackCue(nsISupports* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText);

  TextTrackCue(nsISupports* aGlobal, double aStartTime, double aEndTime,
               const nsAString& aText, HTMLTrackElement* aTrackElement,
               webvtt_node* head);

  ~TextTrackCue();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  nsISupports* GetParentObject()
  {
    return mGlobal;
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
    //XXXhumph: validate? bug 868519.
    if (mStartTime == aStartTime)
      return;

    mStartTime = aStartTime;
    CueChanged();
  }

  double EndTime() const
  {
    return mEndTime;
  }

  void SetEndTime(double aEndTime)
  {
    //XXXhumph: validate? bug 868519.
    if (mEndTime == aEndTime)
      return;

    mEndTime = aEndTime;
    CueChanged();
  }

  bool PauseOnExit()
  {
    return mPauseOnExit;
  }

  void SetPauseOnExit(bool aPauseOnExit)
  {
    if (mPauseOnExit == aPauseOnExit)
      return;

    mPauseOnExit = aPauseOnExit;
    CueChanged();
  }

  void GetVertical(nsAString& aVertical)
  {
    aVertical = mVertical;
  }

  void SetVertical(const nsAString& aVertical)
  {
    if (mVertical == aVertical)
      return;

    mVertical = aVertical;
    CueChanged();
  }

  bool SnapToLines()
  {
    return mSnapToLines;
  }

  void SetSnapToLines(bool aSnapToLines)
  {
    if (mSnapToLines == aSnapToLines)
      return;

    mSnapToLines = aSnapToLines;
    CueChanged();
  }

  double Line() const
  {
    return mLine;
  }

  void SetLine(double aLine)
  {
    //XXX: validate? bug 868519.
    mLine = aLine;
  }

  int32_t Position() const
  {
    return mPosition;
  }

  void SetPosition(int32_t aPosition)
  {
    // XXXhumph: validate? bug 868519.
    if (mPosition == aPosition)
      return;

    mPosition = aPosition;
    CueChanged();
  }

  int32_t Size() const
  {
    return mSize;
  }

  void SetSize(int32_t aSize)
  {
    if (mSize == aSize) {
      return;
    }

    if (aSize < 0 || aSize > 100) {
      //XXX:throw IndexSizeError; bug 868519.
    }

    mSize = aSize;
    CueChanged();
  }

  TextTrackCueAlign Align() const
  {
    return mAlign;
  }

  void SetAlign(TextTrackCueAlign& aAlign)
  {
    if (mAlign == aAlign)
      return;

    mAlign = aAlign;
    CueChanged();
  }

  void GetText(nsAString& aText) const
  {
    aText = mText;
  }

  void SetText(const nsAString& aText)
  {
    // XXXhumph: validate? bug 868519.
    if (mText == aText)
      return;

    mText = aText;
    CueChanged();
  }

  already_AddRefed<DocumentFragment> GetCueAsHTML() const
  {
    // XXXhumph: todo. Bug 868509.
    return nullptr;
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

  void DisplayCue();

private:
  void CueChanged();
  void SetDefaultCueSettings();

  nsCOMPtr<nsISupports> mGlobal;
  nsString mText;
  double mStartTime;
  double mEndTime;

  nsRefPtr<TextTrack> mTrack;
  nsRefPtr<HTMLTrackElement> mTrackElement;
  webvtt_node *mHead;
  nsString mId;
  int32_t mPosition;
  int32_t mSize;
  bool mPauseOnExit;
  bool mSnapToLines;
  nsString mVertical;
  int mLine;
  TextTrackCueAlign mAlign;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextTrackCue_h

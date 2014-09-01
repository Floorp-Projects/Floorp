/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should not be included by other includes, as it contains code

#ifndef MEDIATRACKCONSTRAINTS_H_
#define MEDIATRACKCONSTRAINTS_H_

#include "mozilla/Attributes.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"

namespace mozilla {

// Normalized internal version of MediaTrackConstraints to simplify downstream
// processing. This implementation-only helper is included as needed by both
// MediaManager (for gUM camera selection) and MediaEngine (for applyConstraints).

template<typename T>
class MediaTrackConstraintsN : public dom::MediaTrackConstraints
{
public:
  typedef T Kind;
  dom::Sequence<Kind> mRequireN;
  bool mUnsupportedRequirement;
  MediaTrackConstraintSet mRequired;
  dom::Sequence<MediaTrackConstraintSet> mNonrequired;

  MediaTrackConstraintsN(const dom::MediaTrackConstraints &aOther,
                         const dom::EnumEntry* aStrings)
  : dom::MediaTrackConstraints(aOther)
  , mUnsupportedRequirement(false)
  , mStrings(aStrings)
  {
    if (mRequire.WasPassed()) {
      auto& array = mRequire.Value();
      for (uint32_t i = 0; i < array.Length(); i++) {
        auto value = ToEnum(array[i]);
        if (value != Kind::Other) {
          mRequireN.AppendElement(value);
        } else {
          mUnsupportedRequirement = true;
        }
      }
    }

    // treat MediaSource special because it's always required
    mRequired.mMediaSource = mMediaSource;

    // we guarantee (int) equivalence from MediaSourceEnum ->MediaSourceType
    // (but not the other way)
    if (mMediaSource != dom::MediaSourceEnum::Camera && mAdvanced.WasPassed()) {
      // iterate through advanced, forcing mediaSource to match "root"
      auto& array = mAdvanced.Value();
      for (uint32_t i = 0; i < array.Length(); i++) {
        if (array[i].mMediaSource == dom::MediaSourceEnum::Camera) {
          array[i].mMediaSource = mMediaSource;
        }
      }
    }
  }
protected:
  MediaTrackConstraintSet& Triage(const Kind kind) {
    if (mRequireN.IndexOf(kind) != mRequireN.NoIndex) {
      return mRequired;
    } else {
      mNonrequired.AppendElement(MediaTrackConstraintSet());
      return mNonrequired[mNonrequired.Length()-1];
    }
  }
private:
  Kind ToEnum(const nsAString& aSrc) {
    for (size_t i = 0; mStrings[i].value; i++) {
      if (aSrc.EqualsASCII(mStrings[i].value)) {
        return Kind(i);
      }
    }
    return Kind::Other;
  }
  const dom::EnumEntry* mStrings;
};

struct AudioTrackConstraintsN :
  public MediaTrackConstraintsN<dom::SupportedAudioConstraints>
{
  MOZ_IMPLICIT AudioTrackConstraintsN(const dom::MediaTrackConstraints &aOther)
  : MediaTrackConstraintsN<dom::SupportedAudioConstraints>(aOther, // B2G ICS compiler bug
                           dom::SupportedAudioConstraintsValues::strings) {}
};

struct VideoTrackConstraintsN :
    public MediaTrackConstraintsN<dom::SupportedVideoConstraints>
{
  MOZ_IMPLICIT VideoTrackConstraintsN(const dom::MediaTrackConstraints &aOther)
  : MediaTrackConstraintsN<dom::SupportedVideoConstraints>(aOther,
                           dom::SupportedVideoConstraintsValues::strings) {
    if (mFacingMode.WasPassed()) {
      Triage(Kind::FacingMode).mFacingMode.Construct(mFacingMode.Value());
    }
    // Reminder: add handling for new constraints both here & SatisfyConstraintSet
    Triage(Kind::Width).mWidth = mWidth;
    Triage(Kind::Height).mHeight = mHeight;
    Triage(Kind::FrameRate).mFrameRate = mFrameRate;
    if (mBrowserWindow.WasPassed()) {
      Triage(Kind::BrowserWindow).mBrowserWindow.Construct(mBrowserWindow.Value());
    }
    if (mScrollWithPage.WasPassed()) {
      Triage(Kind::ScrollWithPage).mScrollWithPage.Construct(mScrollWithPage.Value());
    }
    // treat MediaSource special because it's always required
    mRequired.mMediaSource = mMediaSource;
  }
};

}

#endif /* MEDIATRACKCONSTRAINTS_H_ */

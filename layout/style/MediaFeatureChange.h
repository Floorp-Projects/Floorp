/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A struct defining a media feature change. */

#ifndef mozilla_MediaFeatureChange_h__
#define mozilla_MediaFeatureChange_h__

#include "nsChangeHint.h"
#include "mozilla/Attributes.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/ServoStyleConsts.h"

namespace mozilla {

enum class MediaFeatureChangeReason : uint16_t {
  // The viewport size the document has used has changed.
  //
  // This affects size media queries like min-width.
  ViewportChange = 1 << 0,
  // The effective text zoom has changed. This affects the meaning of em units,
  // and thus affects any media query that uses a Length.
  ZoomChange = 1 << 1,
  // The resolution has changed. This can affect device-pixel-ratio media
  // queries, for example.
  ResolutionChange = 1 << 2,
  // The medium has changed.
  MediumChange = 1 << 3,
  // The size-mode has changed.
  SizeModeChange = 1 << 4,
  // A system metric or multiple have changed. This affects all the media
  // features that expose the presence of a system metric directly.
  SystemMetricsChange = 1 << 5,
  // The fact of whether the device size is the page size has changed, thus
  // resolution media queries can change.
  DeviceSizeIsPageSizeChange = 1 << 6,
  // display-mode changed on the document, thus the display-mode media queries
  // may have changed.
  DisplayModeChange = 1 << 7,
  // A preference that affects media query results has changed. For
  // example, changes to document_color_use will affect
  // prefers-contrast.
  PreferenceChange = 1 << 8,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MediaFeatureChangeReason)

enum class MediaFeatureChangePropagation : uint8_t {
  JustThisDocument = 0,
  SubDocuments = 1 << 0,
  Images = 1 << 1,
  All = Images | SubDocuments,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MediaFeatureChangePropagation)

struct MediaFeatureChange {
  static const auto kAllChanges = static_cast<MediaFeatureChangeReason>(~0);

  RestyleHint mRestyleHint;
  nsChangeHint mChangeHint;
  MediaFeatureChangeReason mReason;

  MOZ_IMPLICIT MediaFeatureChange(MediaFeatureChangeReason aReason)
      : MediaFeatureChange(RestyleHint{0}, nsChangeHint(0), aReason) {}

  MediaFeatureChange(RestyleHint aRestyleHint, nsChangeHint aChangeHint,
                     MediaFeatureChangeReason aReason)
      : mRestyleHint(aRestyleHint),
        mChangeHint(aChangeHint),
        mReason(aReason) {}

  inline MediaFeatureChange& operator|=(const MediaFeatureChange& aOther) {
    mRestyleHint |= aOther.mRestyleHint;
    mChangeHint |= aOther.mChangeHint;
    mReason |= aOther.mReason;
    return *this;
  }

  static MediaFeatureChange ForPreferredColorSchemeChange() {
    // We need to restyle because not only media queries have changed, system
    // colors may as well via the prefers-color-scheme meta tag / effective
    // color-scheme property value.
    return {RestyleHint::RecascadeSubtree(), nsChangeHint(0),
            MediaFeatureChangeReason::SystemMetricsChange};
  }
};

}  // namespace mozilla

#endif

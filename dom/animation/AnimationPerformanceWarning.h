/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationPerformanceWarning_h
#define mozilla_dom_AnimationPerformanceWarning_h

#include "mozilla/InitializerList.h"

class nsXPIDLString;

namespace mozilla {

// Represents the reason why we can't run the CSS property on the compositor.
struct AnimationPerformanceWarning
{
  enum class Type : uint8_t {
    ContentTooSmall,
    ContentTooLarge,
    TransformBackfaceVisibilityHidden,
    TransformPreserve3D,
    TransformSVG,
    TransformWithGeometricProperties,
    TransformFrameInactive,
    OpacityFrameInactive,
    HasRenderingObserver,
  };

  explicit AnimationPerformanceWarning(Type aType)
    : mType(aType) { }

  AnimationPerformanceWarning(Type aType,
                              std::initializer_list<int32_t> aParams)
    : mType(aType)
  {
    // FIXME:  Once std::initializer_list::size() become a constexpr function,
    // we should use static_assert here.
    MOZ_ASSERT(aParams.size() <= kMaxParamsForLocalization,
      "The length of parameters should be less than "
      "kMaxParamsForLocalization");
    mParams.emplace(aParams);
  }

  // Maximum number of parameters passed to
  // nsContentUtils::FormatLocalizedString to localize warning messages.
  //
  // NOTE: This constexpr can't be forward declared, so if you want to use
  // this variable, please include this header file directly.
  // This value is the same as the limit of nsStringBundle::FormatString.
  // See the implementation of nsStringBundle::FormatString.
  static constexpr uint8_t kMaxParamsForLocalization = 10;

  // Indicates why this property could not be animated on the compositor.
  Type mType;

  // Optional parameters that may be used for localization.
  Maybe<nsTArray<int32_t>> mParams;

  bool ToLocalizedString(nsXPIDLString& aLocalizedString) const;
  template<uint32_t N>
  nsresult ToLocalizedStringWithIntParams(
    const char* aKey, nsXPIDLString& aLocalizedString) const;

  bool operator==(const AnimationPerformanceWarning& aOther) const
  {
    return mType == aOther.mType &&
           mParams == aOther.mParams;
  }
  bool operator!=(const AnimationPerformanceWarning& aOther) const
  {
    return !(*this == aOther);
  }
};

} // namespace mozilla

#endif // mozilla_dom_AnimationPerformanceWarning_h

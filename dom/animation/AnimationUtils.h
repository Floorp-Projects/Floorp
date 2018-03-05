/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationUtils_h
#define mozilla_dom_AnimationUtils_h

#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Nullable.h"
#include "nsRFPService.h"
#include "nsStringFwd.h"

class nsIContent;
class nsIDocument;
class nsIFrame;
struct JSContext;

namespace mozilla {

class ComputedTimingFunction;
class EffectSet;

class AnimationUtils
{
public:
  static dom::Nullable<double>
  TimeDurationToDouble(const dom::Nullable<TimeDuration>& aTime)
  {
    dom::Nullable<double> result;

    if (!aTime.IsNull()) {
      // 0 is an inappropriate mixin for this this area; however CSS Animations needs to
      // have it's Time Reduction Logic refactored, so it's currently only clamping for
      // RFP mode. RFP mode gives a much lower time precision, so we accept the security
      // leak here for now
      result.SetValue(
        nsRFPService::ReduceTimePrecisionAsMSecs(aTime.Value().ToMilliseconds(), 0, TimerPrecisionType::RFPOnly)
      );
    }

    return result;
  }

  static dom::Nullable<TimeDuration>
  DoubleToTimeDuration(const dom::Nullable<double>& aTime)
  {
    dom::Nullable<TimeDuration> result;

    if (!aTime.IsNull()) {
      result.SetValue(TimeDuration::FromMilliseconds(aTime.Value()));
    }

    return result;
  }

  static void LogAsyncAnimationFailure(nsCString& aMessage,
                                       const nsIContent* aContent = nullptr);

  /**
   * Get the document from the JS context to use when parsing CSS properties.
   */
  static nsIDocument*
  GetCurrentRealmDocument(JSContext* aCx);

  /**
   * Get the document from the global object, or nullptr if the document has
   * no window, to use when constructing DOM object without entering the
   * target window's compartment (see KeyframeEffect constructor).
   */
  static nsIDocument*
  GetDocumentFromGlobal(JSObject* aGlobalObject);

  /**
   * Checks if offscreen animation throttling is enabled.
   */
  static bool
  IsOffscreenThrottlingEnabled();

  /**
   * Returns true if the given EffectSet contains a current effect that animates
   * scale. |aFrame| is used for calculation of scale values.
   */
  static bool EffectSetContainsAnimatedScale(EffectSet& aEffects,
                                             const nsIFrame* aFrame);
};

} // namespace mozilla

#endif

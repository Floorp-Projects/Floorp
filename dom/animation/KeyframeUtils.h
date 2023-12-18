/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeUtils_h
#define mozilla_KeyframeUtils_h

#include "mozilla/KeyframeEffectParams.h"  // For CompositeOperation
#include "nsCSSPropertyID.h"
#include "nsTArrayForwardDeclare.h"  // For nsTArray
#include "js/RootingAPI.h"           // For JS::Handle

struct JSContext;
class JSObject;

namespace mozilla {
struct AnimatedPropertyID;
struct AnimationProperty;
class ComputedStyle;

enum class PseudoStyleType : uint8_t;
class ErrorResult;
struct Keyframe;
struct PropertyStyleAnimationValuePair;

namespace dom {
class Document;
class Element;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {

// Represents the set of property-value pairs on a Keyframe converted to
// computed values.
using ComputedKeyframeValues = nsTArray<PropertyStyleAnimationValuePair>;

/**
 * Utility methods for processing keyframes.
 */
class KeyframeUtils {
 public:
  /**
   * Converts a JS value representing a property-indexed keyframe or a sequence
   * of keyframes to an array of Keyframe objects.
   *
   * @param aCx The JSContext that corresponds to |aFrames|.
   * @param aDocument The document to use when parsing CSS properties.
   * @param aFrames The JS value, provided as an optional IDL |object?| value,
   *   that is the keyframe list specification.
   * @param aContext Information about who is trying to get keyframes from the
   *   object, for use in error reporting.  This must be be a non-null
   *   pointer representing a null-terminated ASCII string.
   * @param aRv (out) Out-param to hold any error returned by this function.
   *   Must be initially empty.
   * @return The set of processed keyframes. If an error occurs, aRv will be
   *   filled-in with the appropriate error code and an empty array will be
   *   returned.
   */
  static nsTArray<Keyframe> GetKeyframesFromObject(
      JSContext* aCx, dom::Document* aDocument, JS::Handle<JSObject*> aFrames,
      const char* aContext, ErrorResult& aRv);

  /**
   * Calculate the computed offset of keyframes by evenly distributing keyframes
   * with a missing offset.
   *
   * @see
   * https://drafts.csswg.org/web-animations/#calculating-computed-keyframes
   *
   * @param aKeyframes The set of keyframes to adjust.
   */
  static void DistributeKeyframes(nsTArray<Keyframe>& aKeyframes);

  /**
   * Converts an array of Keyframe objects into an array of AnimationProperty
   * objects. This involves creating an array of computed values for each
   * longhand property and determining the offset and timing function to use
   * for each value.
   *
   * @param aKeyframes The input keyframes.
   * @param aElement The context element.
   * @param aStyle The computed style values.
   * @param aEffectComposite The composite operation specified on the effect.
   *   For any keyframes in |aKeyframes| that do not specify a composite
   *   operation, this value will be used.
   * @return The set of animation properties. If an error occurs, the returned
   *   array will be empty.
   */
  static nsTArray<AnimationProperty> GetAnimationPropertiesFromKeyframes(
      const nsTArray<Keyframe>& aKeyframes, dom::Element* aElement,
      PseudoStyleType aPseudoType, const ComputedStyle* aStyle,
      dom::CompositeOperation aEffectComposite);

  /**
   * Check if the property or, for shorthands, one or more of
   * its subproperties, is animatable.
   *
   * @param aProperty The property to check.
   * @param aBackend  The style backend, Servo or Gecko, that should determine
   *                  if the property is animatable or not.
   * @return true if |aProperty| is animatable.
   */
  static bool IsAnimatableProperty(const AnimatedPropertyID& aProperty);
};

}  // namespace mozilla

#endif  // mozilla_KeyframeUtils_h

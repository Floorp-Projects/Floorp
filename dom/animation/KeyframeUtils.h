/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeUtils_h
#define mozilla_KeyframeUtils_h

#include "nsTArrayForwardDeclare.h" // For nsTArray
#include "js/RootingAPI.h" // For JS::Handle
#include "mozilla/KeyframeEffectParams.h" // SpacingMode

struct JSContext;
class JSObject;
class nsIDocument;
class nsStyleContext;
struct ServoComputedValues;
struct RawServoDeclarationBlock;

namespace mozilla {
struct AnimationProperty;
enum class CSSPseudoElementType : uint8_t;
class ErrorResult;
struct Keyframe;
struct PropertyStyleAnimationValuePair;
struct ServoComputedValuesWithParent;

namespace dom {
class Element;
} // namespace dom
} // namespace mozilla


namespace mozilla {

// Represents the set of property-value pairs on a Keyframe converted to
// computed values.
using ComputedKeyframeValues = nsTArray<PropertyStyleAnimationValuePair>;

/**
 * Utility methods for processing keyframes.
 */
class KeyframeUtils
{
public:
  /**
   * Converts a JS value representing a property-indexed keyframe or a sequence
   * of keyframes to an array of Keyframe objects.
   *
   * @param aCx The JSContext that corresponds to |aFrames|.
   * @param aDocument The document to use when parsing CSS properties.
   * @param aFrames The JS value, provided as an optional IDL |object?| value,
   *   that is the keyframe list specification.
   * @param aRv (out) Out-param to hold any error returned by this function.
   *   Must be initially empty.
   * @return The set of processed keyframes. If an error occurs, aRv will be
   *   filled-in with the appropriate error code and an empty array will be
   *   returned.
   */
  static nsTArray<Keyframe>
  GetKeyframesFromObject(JSContext* aCx,
                         nsIDocument* aDocument,
                         JS::Handle<JSObject*> aFrames,
                         ErrorResult& aRv);

  /**
   * Calculate the StyleAnimationValues of properties of each keyframe.
   * This involves expanding shorthand properties into longhand properties,
   * removing the duplicated properties for each keyframe, and creating an
   * array of |property:computed value| pairs for each keyframe.
   *
   * These computed values are used *both* when computing the final set of
   * per-property animation values (see GetAnimationPropertiesFromKeyframes) as
   * well when applying paced spacing. By returning these values here, we allow
   * the result to be re-used in both operations.
   *
   * @param aKeyframes The input keyframes.
   * @param aElement The context element.
   * @param aStyleContext The style context to use when computing values.
   * @return The set of ComputedKeyframeValues. The length will be the same as
   *   aFrames.
   */
  static nsTArray<ComputedKeyframeValues>
  GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                            dom::Element* aElement,
                            nsStyleContext* aStyleContext);

  static nsTArray<ComputedKeyframeValues>
  GetComputedKeyframeValues(const nsTArray<Keyframe>& aKeyframes,
                            dom::Element* aElement,
                            const ServoComputedValuesWithParent& aServoValues);

  /**
   * Fills in the mComputedOffset member of each keyframe in the given array
   * using the specified spacing mode.
   *
   * https://w3c.github.io/web-animations/#spacing-keyframes
   *
   * @param aKeyframes The set of keyframes to adjust.
   * @param aSpacingMode The spacing mode to apply.
   * @param aProperty The paced property. Only used when |aSpacingMode| is
   *   SpacingMode::paced. In all other cases it is ignored and hence may be
   *   any value, e.g. eCSSProperty_UNKNOWN.
   * @param aComputedValues The set of computed keyframe values as returned by
   *   GetComputedKeyframeValues. Only used when |aSpacingMode| is
   *   SpacingMode::paced. In all other cases this parameter is unused and may
   *   be any value including an empty array.
   * @param aStyleContext The style context used for calculating paced spacing
   *                      on transform.
   */
  static void ApplySpacing(nsTArray<Keyframe>& aKeyframes,
                           SpacingMode aSpacingMode,
                           nsCSSPropertyID aProperty,
                           nsTArray<ComputedKeyframeValues>& aComputedValues,
                           nsStyleContext* aStyleContext);
  static void ApplySpacing(nsTArray<Keyframe>& aKeyframes,
                           SpacingMode aSpacingMode,
                           nsCSSPropertyID aProperty,
                           nsTArray<ComputedKeyframeValues>& aComputedValues,
                           const ServoComputedValuesWithParent& aServoValues)
  {
    NS_WARNING("stylo: ApplySpacing not implemented yet");
  }

  /**
   * Wrapper for ApplySpacing to simplify using distribute spacing.
   *
   * @param aKeyframes The set of keyframes to adjust.
   */
  static void ApplyDistributeSpacing(nsTArray<Keyframe>& aKeyframes);

  /**
   * Converts an array of Keyframe objects into an array of AnimationProperty
   * objects. This involves creating an array of computed values for each
   * longhand property and determining the offset and timing function to use
   * for each value.
   *
   * @param aKeyframes The input keyframes.
   * @param aComputedValues The computed keyframe values (as returned by
   *   GetComputedKeyframeValues) used to fill in the individual
   *   AnimationPropertySegment objects. Although these values could be
   *   calculated from |aKeyframes|, passing them in as a separate parameter
   *   allows the result of GetComputedKeyframeValues to be re-used both
   *   here and in ApplySpacing.
   * @param aEffectComposite The composite operation specified on the effect.
   *   For any keyframes in |aKeyframes| that do not specify a composite
   *   operation, this value will be used.
   * @return The set of animation properties. If an error occurs, the returned
   *   array will be empty.
   */
  static nsTArray<AnimationProperty> GetAnimationPropertiesFromKeyframes(
    const nsTArray<Keyframe>& aKeyframes,
    const nsTArray<ComputedKeyframeValues>& aComputedValues,
    dom::CompositeOperation aEffectComposite);

  /**
   * Check if the property or, for shorthands, one or more of
   * its subproperties, is animatable.
   *
   * @param aProperty The property to check.
   * @return true if |aProperty| is animatable.
   */
  static bool IsAnimatableProperty(nsCSSPropertyID aProperty);

  /**
   * Parse a string representing a CSS property value into a
   * RawServoDeclarationBlock.
   *
   * @param aProperty The property to be parsed.
   * @param aValue The specified value.
   * @param aDocument The current document.
   * @return The parsed value as a RawServoDeclarationBlock. We put the value
   *   in a declaration block since that is how we represent specified values
   *   in Servo.
   */
  static already_AddRefed<RawServoDeclarationBlock> ParseProperty(
    nsCSSPropertyID aProperty,
    const nsAString& aValue,
    nsIDocument* aDocument);
};

} // namespace mozilla

#endif // mozilla_KeyframeUtils_h

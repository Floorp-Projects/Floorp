/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_KeyframeUtils_h
#define mozilla_KeyframeUtils_h

#include "nsTArrayForwardDeclare.h" // For nsTArray
#include "js/RootingAPI.h" // For JS::Handle

struct JSContext;
class JSObject;

namespace mozilla {
struct AnimationProperty;
enum class CSSPseudoElementType : uint8_t;
class ErrorResult;
struct Keyframe;

namespace dom {
class Element;
} // namespace dom
} // namespace mozilla


namespace mozilla {

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
                         JS::Handle<JSObject*> aFrames,
                         ErrorResult& aRv);

  /**
   * Fills in the mComputedOffset member of each keyframe in the given array
   * using the "distribute" spacing algorithm.
   *
   * http://w3c.github.io/web-animations/#distribute-keyframe-spacing-mode
   *
   * @param keyframes The set of keyframes to adjust.
   */
  static void ApplyDistributeSpacing(nsTArray<Keyframe>& aKeyframes);

  /**
   * Converts an array of Keyframe objects into an array of AnimationProperty
   * objects. This involves expanding shorthand properties into longhand
   * properties, creating an array of computed values for each longhand
   * property and determining the offset and timing function to use for each
   * value.
   *
   * @param aStyleContext The style context to use when computing values.
   * @param aFrames The input keyframes.
   * @return The set of animation properties. If an error occurs, the returned
   *   array will be empty.
   */
  static nsTArray<AnimationProperty>
  GetAnimationPropertiesFromKeyframes(nsStyleContext* aStyleContext,
                                      dom::Element* aElement,
                                      CSSPseudoElementType aPseudoType,
                                      const nsTArray<Keyframe>& aFrames);
};

} // namespace mozilla

#endif // mozilla_KeyframeUtils_h

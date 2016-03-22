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
class  ErrorResult;

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
  * Converts a JS value to a property-indexed keyframe or a sequence of
  * regular keyframes and builds an array of AnimationProperty objects for the
  * keyframe animation that it specifies.
  *
  * @param aTarget The target of the animation, used to resolve style
  *   for a property's underlying value if needed.
  * @param aFrames The JS value, provided as an optional IDL |object?| value,
  *   that is the keyframe list specification.
  * @param aResult The array into which the resulting AnimationProperty
  *   objects will be appended.
  */
  static void
  BuildAnimationPropertyList(JSContext* aCx, Element* aTarget,
                             CSSPseudoElementType aPseudoType,
                             JS::Handle<JSObject*> aFrames,
                             InfallibleTArray<AnimationProperty>& aResult,
                             ErrorResult& aRv);
};

} // namespace mozilla

#endif // mozilla_KeyframeUtils_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_KeyframeEffect_h
#define mozilla_dom_KeyframeEffect_h

#include "nsWrapperCache.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "mozilla/AnimationTarget.h" // For (Non)OwningAnimationTarget
#include "mozilla/Maybe.h"

struct JSContext;
class JSObject;
class nsIDocument;

namespace mozilla {

class ErrorResult;
struct KeyframeEffectParams;
struct TimingParams;

namespace dom {

class ElementOrCSSPseudoElement;
class GlobalObject;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
class UnrestrictedDoubleOrKeyframeEffectOptions;

class KeyframeEffect : public KeyframeEffectReadOnly
{
public:
  KeyframeEffect(nsIDocument* aDocument,
                 const Maybe<OwningAnimationTarget>& aTarget,
                 const TimingParams& aTiming,
                 const KeyframeEffectParams& aOptions);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<KeyframeEffect>
  Constructor(const GlobalObject& aGlobal,
              const Nullable<ElementOrCSSPseudoElement>& aTarget,
              JS::Handle<JSObject*> aKeyframes,
              const UnrestrictedDoubleOrKeyframeEffectOptions& aOptions,
              ErrorResult& aRv);

  // Variant of Constructor that accepts a KeyframeAnimationOptions object
  // for use with for Animatable.animate.
  // Not exposed to content.
  static already_AddRefed<KeyframeEffect>
  Constructor(const GlobalObject& aGlobal,
              const Nullable<ElementOrCSSPseudoElement>& aTarget,
              JS::Handle<JSObject*> aKeyframes,
              const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
              ErrorResult& aRv);

  void NotifySpecifiedTimingUpdated();

  // This method calls GetTargetStyleContext which is not safe to use when
  // we are in the middle of updating style. If we need to use this when
  // updating style, we should pass the nsStyleContext into this method and use
  // that to update the properties rather than calling
  // GetStyleContextForElement.
  void SetTarget(const Nullable<ElementOrCSSPseudoElement>& aTarget);

  void SetSpacing(JSContext* aCx, const nsAString& aSpacing, ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_KeyframeEffect_h

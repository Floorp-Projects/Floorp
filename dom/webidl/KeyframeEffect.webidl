/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/web-animations/#the-keyframeeffect-interfaces
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum IterationCompositeOperation {
  "replace",
  "accumulate"
};

dictionary KeyframeEffectOptions : AnimationEffectTimingProperties {
  IterationCompositeOperation iterationComposite = "replace";
  CompositeOperation          composite = "replace";
  DOMString                   spacing = "distribute";
};

// Bug 1241783: For the constructor we use (Element or CSSPseudoElement)? for
// the first argument since we cannot convert a mixin into a union type
// automatically.
[Func="nsDocument::IsWebAnimationsEnabled",
 Constructor ((Element or CSSPseudoElement)? target,
              object? keyframes,
              optional (unrestricted double or KeyframeEffectOptions) options),
 Constructor (KeyframeEffectReadOnly source)]
interface KeyframeEffectReadOnly : AnimationEffectReadOnly {
  // Bug 1241783: As with the constructor, we use (Element or CSSPseudoElement)?
  // for the type of |target| instead of Animatable?
  readonly attribute (Element or CSSPseudoElement)?  target;
  readonly attribute IterationCompositeOperation iterationComposite;
  readonly attribute CompositeOperation          composite;
  readonly attribute DOMString                   spacing;

  // We use object instead of ComputedKeyframe so that we can put the
  // property-value pairs on the object.
  [Throws] sequence<object> getKeyframes();
};

// Non-standard extensions
dictionary AnimationPropertyValueDetails {
  required double             offset;
           DOMString          value;
           DOMString          easing;
  required CompositeOperation composite;
};

dictionary AnimationPropertyDetails {
  required DOMString                               property;
  required boolean                                 runningOnCompositor;
           DOMString                               warning;
  required sequence<AnimationPropertyValueDetails> values;
};

partial interface KeyframeEffectReadOnly {
  [ChromeOnly, Throws] sequence<AnimationPropertyDetails> getProperties();
};

[Func="nsDocument::IsWebAnimationsEnabled",
 Constructor ((Element or CSSPseudoElement)? target,
              object? keyframes,
              optional (unrestricted double or KeyframeEffectOptions) options),
 Constructor (KeyframeEffectReadOnly source)]
interface KeyframeEffect : KeyframeEffectReadOnly {
  inherit attribute (Element or CSSPseudoElement)? target;
  [NeedsCallerType]
  inherit attribute IterationCompositeOperation    iterationComposite;
  // Bug 1216844 - implement additive animation
  // inherit attribute CompositeOperation          composite;
  [SetterThrows, NeedsCallerType]
  inherit attribute DOMString                   spacing;
  [Throws]
  void setKeyframes (object? keyframes);
};

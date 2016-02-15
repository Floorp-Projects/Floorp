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
[HeaderFile="mozilla/dom/KeyframeEffect.h",
 Func="nsDocument::IsWebAnimationsEnabled",
 Constructor((Element or CSSPseudoElement)? target,
             object? frames,
             optional (unrestricted double or KeyframeEffectOptions) options)]
interface KeyframeEffectReadOnly : AnimationEffectReadOnly {
  // Bug 1241783: As with the constructor, we use (Element or CSSPseudoElement)?
  // for the type of |target| instead of Animatable?
  readonly attribute (Element or CSSPseudoElement)?  target;
  readonly attribute IterationCompositeOperation iterationComposite;
  readonly attribute CompositeOperation          composite;
  readonly attribute DOMString                   spacing;

  // Not yet implemented:
  // KeyframeEffect             clone();

  // We use object instead of ComputedKeyframe so that we can put the
  // property-value pairs on the object.
  [Throws] sequence<object> getFrames();
};


[Constructor ((Element or CSSPseudoElement)? target,
              object? frames,
              optional (unrestricted double or KeyframeEffectOptions) options)]
interface KeyframeEffect : KeyframeEffectReadOnly {
  // Bug 1067769 - Allow setting KeyframeEffect.target
  // inherit attribute Animatable?                 target;
  // Bug 1216843 - implement animation composition
  // inherit attribute IterationCompositeOperation iterationComposite;
  // Bug 1216844 - implement additive animation
  // inherit attribute CompositeOperation          composite;
  // Bug 1244590 - implement spacing modes
  // inherit attribute DOMString                   spacing;
  // Bug 1244591 - implement setFrames
  // void setFrames (object? frames);
};

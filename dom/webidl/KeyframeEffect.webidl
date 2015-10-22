/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://w3c.github.io/web-animations/#the-keyframeeffect-interfaces
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// For the constructor:
//
// 1. We use Element? for the first argument since we don't support Animatable
//    for pseudo-elements yet.
//
// 2. We use object? instead of
//
//    (PropertyIndexedKeyframes or sequence<Keyframe> or SharedKeyframeList)
//
//    for the second argument so that we can get the property-value pairs from
//    the PropertyIndexedKeyframes or Keyframe objects.  We also don't support
//    SharedKeyframeList yet.
//
// 3. We use unrestricted double instead of
//
//    (unrestricted double or KeyframeEffectOptions)
//
//    since we don't support KeyframeEffectOptions yet.
[HeaderFile="mozilla/dom/KeyframeEffect.h",
 Func="nsDocument::IsWebAnimationsEnabled",
 Constructor(Element? target,
             optional object? frames,
             optional unrestricted double options)]
interface KeyframeEffectReadOnly : AnimationEffectReadOnly {
  readonly attribute Element?  target;
  // Not yet implemented:
  // readonly attribute IterationCompositeOperation iterationComposite;
  // readonly attribute CompositeOperation          composite;
  // readonly attribute DOMString                   spacing;
  // KeyframeEffect             clone();

  // We use object instead of ComputedKeyframe so that we can put the
  // property-value pairs on the object.
  [Throws] sequence<object> getFrames();
};

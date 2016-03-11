/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/web-animations/#animationeffecttiming
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Func="nsDocument::IsWebAnimationsEnabled"]
interface AnimationEffectTiming : AnimationEffectTimingReadOnly {
  //Bug 1244633 - implement AnimationEffectTiming delay
  //inherit attribute double                             delay;
  inherit attribute double                             endDelay;
  //Bug 1244637 - implement AnimationEffectTiming fill
  //inherit attribute FillMode                           fill;
  inherit attribute double                             iterationStart;
  //Bug 1244640 - implement AnimationEffectTiming iterations
  //inherit attribute unrestricted double                iterations;
  [SetterThrows]
  inherit attribute (unrestricted double or DOMString) duration;
  //Bug 1244642 - implement AnimationEffectTiming direction
  //inherit attribute PlaybackDirection                  direction;
  //Bug 1244643 - implement AnimationEffectTiming easing
  //inherit attribute DOMString                          easing;
};

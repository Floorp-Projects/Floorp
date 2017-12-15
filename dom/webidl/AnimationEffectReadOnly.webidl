/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/web-animations/#animationeffectreadonly
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum FillMode {
  "none",
  "forwards",
  "backwards",
  "both",
  "auto"
};

enum PlaybackDirection {
  "normal",
  "reverse",
  "alternate",
  "alternate-reverse"
};

dictionary AnimationEffectTimingProperties {
  double                              delay = 0.0;
  double                              endDelay = 0.0;
  FillMode                            fill = "auto";
  double                              iterationStart = 0.0;
  unrestricted double                 iterations = 1.0;
  (unrestricted double or DOMString)  duration = "auto";
  PlaybackDirection                   direction = "normal";
  DOMString                           easing = "linear";
};

dictionary ComputedTimingProperties : AnimationEffectTimingProperties {
  unrestricted double   endTime = 0.0;
  unrestricted double   activeDuration = 0.0;
  double?               localTime = null;
  double?               progress = null;
  unrestricted double?  currentIteration = null;
};

[Func="nsDocument::IsWebAnimationsEnabled"]
interface AnimationEffectReadOnly {
  [Cached, Constant]
  readonly attribute AnimationEffectTimingReadOnly timing;
  [BinaryName="getComputedTimingAsDict"]
  ComputedTimingProperties getComputedTiming();
};

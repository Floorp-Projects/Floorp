/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/web-animations/#the-compositeoperation-enumeration
 * https://w3c.github.io/web-animations/#dictdef-basepropertyindexedkeyframe
 * https://w3c.github.io/web-animations/#dictdef-basekeyframe
 * https://w3c.github.io/web-animations/#dictdef-basecomputedkeyframe
 *
 * Copyright © 2016 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum CompositeOperation { "replace", "add", "accumulate" };

// The following dictionary types are not referred to by other .webidl files,
// but we use it for manual JS->IDL and IDL->JS conversions in
// KeyframeEffectReadOnly's implementation.

dictionary BasePropertyIndexedKeyframe {
  DOMString easing = "linear";
  CompositeOperation composite;
};

dictionary BaseKeyframe {
  double? offset = null;
  DOMString easing = "linear";
  CompositeOperation composite;
};

dictionary BaseComputedKeyframe : BaseKeyframe {
  double computedOffset;
};

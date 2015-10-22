/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/web-animations/#the-propertyindexedkeyframe-dictionary
 *
 * Copyright © 2015 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

// No other .webidl files references this dictionary, but we use it for
// manual JS->IDL conversions in KeyframeEffectReadOnly's implementation.
dictionary PropertyIndexedKeyframes {
  DOMString           easing = "linear";
  CompositeOperation? composite = null;
};

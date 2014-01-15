/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/2012/WD-dom-20120105/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Constructor(DOMString type, optional EventInit eventInitDict)]
interface Event {
  [Pure]
  readonly attribute DOMString type;
  [Pure]
  readonly attribute EventTarget? target;
  [Pure]
  readonly attribute EventTarget? currentTarget;

  const unsigned short NONE = 0;
  const unsigned short CAPTURING_PHASE = 1;
  const unsigned short AT_TARGET = 2;
  const unsigned short BUBBLING_PHASE = 3;
  [Pure]
  readonly attribute unsigned short eventPhase;

  void stopPropagation();
  void stopImmediatePropagation();

  [Pure]
  readonly attribute boolean bubbles;
  [Pure]
  readonly attribute boolean cancelable;
  void preventDefault();
  [Pure]
  readonly attribute boolean defaultPrevented;

  [Unforgeable, Pure]
  readonly attribute boolean isTrusted;
  [Pure]
  readonly attribute DOMTimeStamp timeStamp;

  [Throws]
  void initEvent(DOMString type, boolean bubbles, boolean cancelable);
};

// Mozilla specific legacy stuff.
partial interface Event {
  const long ALT_MASK     = 0x00000001;
  const long CONTROL_MASK = 0x00000002;
  const long SHIFT_MASK   = 0x00000004;
  const long META_MASK    = 0x00000008;

  readonly attribute EventTarget? originalTarget;
  readonly attribute EventTarget? explicitOriginalTarget;
  [ChromeOnly] readonly attribute boolean multipleActionsPrevented;
  [ChromeOnly] readonly attribute boolean isSynthesized;

  boolean getPreventDefault();
};

dictionary EventInit {
  boolean bubbles = false;
  boolean cancelable = false;
};


/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface Screen : EventTarget {
  // CSSOM-View
  // http://dev.w3.org/csswg/cssom-view/#the-screen-interface
  [Throws]
  readonly attribute long availWidth;
  [Throws]
  readonly attribute long availHeight;
  [Throws]
  readonly attribute long width;
  [Throws]
  readonly attribute long height;
  [Throws]
  readonly attribute long colorDepth;
  [Throws]
  readonly attribute long pixelDepth;

  [Throws]
  readonly attribute long top;
  [Throws]
  readonly attribute long left;
  [Throws]
  readonly attribute long availTop;
  [Throws]
  readonly attribute long availLeft;

  /**
   * Returns the current screen orientation.
   * Can be: landscape-primary, landscape-secondary,
   *         portrait-primary or portrait-secondary.
   */
  readonly attribute DOMString mozOrientation;

  attribute EventHandler onmozorientationchange;

  /**
   * Lock screen orientation to the specified type.
   */
  [Throws]
  boolean mozLockOrientation(DOMString orientation);
  [Throws]
  boolean mozLockOrientation(sequence<DOMString> orientation);

  /**
   * Unlock the screen orientation.
   */
  void mozUnlockOrientation();
};

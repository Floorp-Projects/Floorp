/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window]
interface Screen : EventTarget {
  // CSSOM-View
  // http://dev.w3.org/csswg/cssom-view/#the-screen-interface
  readonly attribute long availWidth;
  readonly attribute long availHeight;
  readonly attribute long width;
  readonly attribute long height;
  readonly attribute long colorDepth;
  readonly attribute long pixelDepth;

  readonly attribute long top;
  readonly attribute long left;
  readonly attribute long availTop;
  readonly attribute long availLeft;

  /**
   * DEPRECATED, use ScreenOrientation API instead.
   * Returns the current screen orientation.
   * Can be: landscape-primary, landscape-secondary,
   *         portrait-primary or portrait-secondary.
   */
  [NeedsCallerType]
  readonly attribute DOMString mozOrientation;

  attribute EventHandler onmozorientationchange;

  /**
   * DEPRECATED, use ScreenOrientation API instead.
   * Lock/unlock screen orientation to the specified type.
   *
   * FIXME(emilio): These do literally nothing, we should
   * try to remove these.
   */
  boolean mozLockOrientation(DOMString orientation);
  boolean mozLockOrientation(sequence<DOMString> orientation);
  undefined mozUnlockOrientation();
};

// https://w3c.github.io/screen-orientation
partial interface Screen {
  readonly attribute ScreenOrientation orientation;
};

// https://wicg.github.io/media-capabilities/#idl-index
enum ScreenColorGamut {
  "srgb",
  "p3",
  "rec2020",
};

[Func="nsScreen::MediaCapabilitiesEnabled",
 Exposed=Window]
interface ScreenLuminance {
  readonly attribute double min;
  readonly attribute double max;
  readonly attribute double maxAverage;
};

partial interface Screen {
  [Func="nsScreen::MediaCapabilitiesEnabled"]
  readonly attribute ScreenColorGamut colorGamut;
  [Func="nsScreen::MediaCapabilitiesEnabled"]
  readonly attribute ScreenLuminance? luminance;

  [Func="nsScreen::MediaCapabilitiesEnabled"]
  attribute EventHandler onchange;
};

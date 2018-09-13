/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// And the things from nsIFrameLoaderOwner
[NoInterfaceObject]
interface MozFrameLoaderOwner {
  [ChromeOnly]
  readonly attribute FrameLoader? frameLoader;

  [ChromeOnly, Throws]
  void presetOpenerWindow(WindowProxy? window);

  [ChromeOnly, Throws]
  void swapFrameLoaders(XULFrameElement aOtherLoaderOwner);

  [ChromeOnly, Throws]
  void swapFrameLoaders(HTMLIFrameElement aOtherLoaderOwner);
};

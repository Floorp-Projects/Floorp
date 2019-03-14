/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary RemotenessOptions {
  DOMString? remoteType;
  FrameLoader? sameProcessAsFrameLoader;
  WindowProxy? opener;
};

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

  [ChromeOnly, Throws]
  void changeRemoteness(optional RemotenessOptions aOptions);
};

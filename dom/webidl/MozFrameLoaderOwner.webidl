/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary RemotenessOptions {
  DOMString? remoteType;
  FrameLoader? sameProcessAsFrameLoader;
  WindowProxy? opener;

  // Used to resume a given channel load within the target process. If present,
  // it will be used rather than the `src` & `srcdoc` attributes on the
  // frameloader to control the load behaviour.
  unsigned long long pendingSwitchID;
  boolean replaceBrowsingContext = false;
};

[NoInterfaceObject]
interface MozFrameLoaderOwner {
  [ChromeOnly]
  readonly attribute FrameLoader? frameLoader;

  [ChromeOnly]
  readonly attribute BrowsingContext? browsingContext;

  [ChromeOnly, Throws]
  void presetOpenerWindow(WindowProxy? window);

  [ChromeOnly, Throws]
  void swapFrameLoaders(XULFrameElement aOtherLoaderOwner);

  [ChromeOnly, Throws]
  void swapFrameLoaders(HTMLIFrameElement aOtherLoaderOwner);

  [ChromeOnly, Throws]
  void changeRemoteness(optional RemotenessOptions aOptions);
};

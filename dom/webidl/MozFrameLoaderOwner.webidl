/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary RemotenessOptions {
  required DOMString? remoteType;

  // Used to indicate that there is an error condition that needs to
  // be handled.
  unsigned long error;

  // Used to resume a given channel load within the target process. If present,
  // it will be used rather than the `src` & `srcdoc` attributes on the
  // frameloader to control the load behaviour.
  unsigned long long pendingSwitchID;
  boolean replaceBrowsingContext = false;
};

/**
 * An interface implemented by elements that are 'browsing context containers'
 * in HTML5 terms (that is, elements such as iframe that creates a new
 * browsing context):
 *
 * https://html.spec.whatwg.org/#browsing-context-container
 *
 * Object implementing this interface must implement nsFrameLoaderOwner in
 * native C++ code.
 */
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
  void changeRemoteness(RemotenessOptions aOptions);
};

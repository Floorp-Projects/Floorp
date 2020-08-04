/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;
interface nsIWebNavigation;
interface nsIOpenWindowInfo;

[ChromeOnly,
 Exposed=Window]
interface XULFrameElement : XULElement
{
  [HTMLConstructor] constructor();

  readonly attribute nsIDocShell? docShell;
  readonly attribute nsIWebNavigation? webNavigation;

  readonly attribute WindowProxy? contentWindow;
  readonly attribute Document? contentDocument;

  readonly attribute unsigned long long browserId;

  /**
   * The optional open window information provided by the window creation code
   * and used to initialize a new browser.
   */
  attribute nsIOpenWindowInfo? openWindowInfo;
};

XULFrameElement includes MozFrameLoaderOwner;

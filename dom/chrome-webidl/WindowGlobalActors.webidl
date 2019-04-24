/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface Principal;
interface URI;
interface nsIDocShell;
interface RemoteTab;

[Exposed=Window, ChromeOnly]
interface WindowGlobalParent {
  readonly attribute boolean isClosed;
  readonly attribute boolean isInProcess;
  readonly attribute CanonicalBrowsingContext browsingContext;

  readonly attribute boolean isCurrentGlobal;

  readonly attribute unsigned long long innerWindowId;
  readonly attribute unsigned long long outerWindowId;

  readonly attribute FrameLoader? rootFrameLoader; // Embedded (browser) only

  readonly attribute WindowGlobalChild? childActor; // in-process only

  readonly attribute RemoteTab? remoteTab; // out-of-process only

  // Information about the currently loaded document.
  readonly attribute Principal documentPrincipal;
  readonly attribute URI? documentURI;

  static WindowGlobalParent? getByInnerWindowId(unsigned long long innerWindowId);

  [Throws]
  JSWindowActorParent getActor(DOMString name);

  [Throws]
  Promise<RemoteTab> changeFrameRemoteness(
    BrowsingContext? bc, DOMString remoteType,
    unsigned long long pendingSwitchId);
};

[Exposed=Window, ChromeOnly]
interface WindowGlobalChild {
  readonly attribute boolean isClosed;
  readonly attribute boolean isInProcess;
  readonly attribute BrowsingContext browsingContext;

  readonly attribute boolean isCurrentGlobal;

  readonly attribute unsigned long long innerWindowId;
  readonly attribute unsigned long long outerWindowId;

  readonly attribute WindowGlobalParent? parentActor; // in-process only

  static WindowGlobalChild? getByInnerWindowId(unsigned long long innerWIndowId);

  [Throws]
  JSWindowActorChild getActor(DOMString name);
};

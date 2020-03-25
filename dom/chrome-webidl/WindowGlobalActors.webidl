/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface Principal;
interface URI;
interface nsIDocShell;
interface RemoteTab;
interface nsITransportSecurityInfo;

[Exposed=Window, ChromeOnly]
interface WindowContext {
  readonly attribute unsigned long long innerWindowId;
};

[Exposed=Window, ChromeOnly]
interface WindowGlobalParent : WindowContext {
  readonly attribute boolean isClosed;
  readonly attribute boolean isInProcess;
  readonly attribute CanonicalBrowsingContext browsingContext;

  readonly attribute boolean isCurrentGlobal;

  readonly attribute unsigned long long outerWindowId;
  readonly attribute unsigned long long contentParentId;

  readonly attribute long osPid;

  // A WindowGlobalParent is the root in its process if it has no parent, or its
  // embedder is in a different process.
  readonly attribute boolean isProcessRoot;

  // True if this window has registered a "beforeunload" event handler.
  readonly attribute boolean hasBeforeUnload;

  // Is the document loaded in this WindowGlobalParent the initial document
  // implicitly created while "creating a new browsing context".
  // https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context
  readonly attribute boolean isInitialDocument;

  readonly attribute FrameLoader? rootFrameLoader; // Embedded (browser) only

  readonly attribute WindowGlobalChild? childActor; // in-process only

  // Information about the currently loaded document.
  readonly attribute Principal documentPrincipal;
  readonly attribute Principal? contentBlockingAllowListPrincipal;
  readonly attribute URI? documentURI;

  // Bit mask containing content blocking events that are recorded in
  // the document's content blocking log.
  readonly attribute unsigned long contentBlockingEvents;

  // String containing serialized content blocking log.
  readonly attribute DOMString contentBlockingLog;

  // ContentParent of the process this window is loaded in.
  // Will be `null` for windows loaded in the parent process.
  readonly attribute nsIContentParent? contentParent;

  static WindowGlobalParent? getByInnerWindowId(unsigned long long innerWindowId);

  /**
   * Get or create the JSWindowActor with the given name.
   *
   * See WindowActorOptions from JSWindowActor.webidl for details on how to
   * customize actor creation.
   */
  [Throws]
  JSWindowActorParent getActor(DOMString name);

  /**
   * Renders a region of the frame into an image bitmap.
   *
   * @param rect Specify the area of the window to render, in CSS pixels. This
   * is relative to the current scroll position. If null, the entire viewport
   * is rendered.
   * @param scale The scale to render the window at. Use devicePixelRatio
   * to have comparable rendering to the OS.
   * @param backgroundColor The background color to use.
   *
   * This API can only be used in the parent process, as content processes
   * cannot access the rendering of out of process iframes. This API works
   * with remote and local frames.
   */
  [Throws]
  Promise<ImageBitmap> drawSnapshot(DOMRect? rect,
                                    double scale,
                                    UTF8String backgroundColor);

  /**
   * Fetches the securityInfo object for this window. This function will
   * look for failed and successful channels to find the security info,
   * thus it will work on regular HTTPS pages as well as certificate
   * error pages.
   *
   * This returns a Promise which resolves to an nsITransportSecurity
   * object with certificate data or undefined if no security info is available.
   */
  [Throws]
  Promise<nsITransportSecurityInfo> getSecurityInfo();
};

[Exposed=Window, ChromeOnly]
interface WindowGlobalChild {
  readonly attribute boolean isClosed;
  readonly attribute boolean isInProcess;
  readonly attribute BrowsingContext browsingContext;
  readonly attribute WindowContext windowContext;

  readonly attribute boolean isCurrentGlobal;

  readonly attribute unsigned long long innerWindowId;
  readonly attribute unsigned long long outerWindowId;
  readonly attribute unsigned long long contentParentId;

  // A WindowGlobalChild is the root in its process if it has no parent, or its
  // embedder is in a different process.
  readonly attribute boolean isProcessRoot;

  readonly attribute WindowGlobalParent? parentActor; // in-process only

  static WindowGlobalChild? getByInnerWindowId(unsigned long long innerWIndowId);

  /**
   * Get or create the JSWindowActor with the given name.
   *
   * See WindowActorOptions from JSWindowActor.webidl for details on how to
   * customize actor creation.
   */
  [Throws]
  JSWindowActorChild getActor(DOMString name);
};

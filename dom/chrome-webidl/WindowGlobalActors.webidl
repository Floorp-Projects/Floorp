/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface Principal;
interface URI;
interface nsIDocShell;
interface RemoteTab;
interface nsITransportSecurityInfo;
interface nsIDOMProcessParent;

[Exposed=Window, ChromeOnly]
interface WindowContext {
  readonly attribute BrowsingContext? browsingContext;

  readonly attribute WindowGlobalChild? windowGlobalChild; // in-process only

  readonly attribute unsigned long long innerWindowId;

  readonly attribute WindowContext? parentWindowContext;

  readonly attribute WindowContext topWindowContext;

  readonly attribute boolean isInProcess;

  // True if this WindowContext is currently frozen in the BFCache.
  readonly attribute boolean isInBFCache;

  // True if this window has registered a "beforeunload" event handler.
  readonly attribute boolean hasBeforeUnload;

  // True if the principal of this window is for a local ip address.
  readonly attribute boolean isLocalIP;

  /**
   * Partially determines whether script execution is allowed in this
   * BrowsingContext. Script execution will be permitted only if this
   * attribute is true and script execution is allowed in the owner
   * BrowsingContext.
   *
   * May only be set in the context's owning process.
   */
  [SetterThrows] attribute boolean allowJavascript;
};

// Keep this in sync with nsIContentViewer::PermitUnloadAction.
enum PermitUnloadAction {
  "prompt",
  "dontUnload",
  "unload",
};

[Exposed=Window, ChromeOnly]
interface WindowGlobalParent : WindowContext {
  readonly attribute boolean isClosed;

  readonly attribute boolean isCurrentGlobal;

  readonly attribute unsigned long long outerWindowId;
  readonly attribute unsigned long long contentParentId;

  readonly attribute long osPid;

  // A WindowGlobalParent is the root in its process if it has no parent, or its
  // embedder is in a different process.
  readonly attribute boolean isProcessRoot;

  // Is the document loaded in this WindowGlobalParent the initial document
  // implicitly created while "creating a new browsing context".
  // https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context
  readonly attribute boolean isInitialDocument;

  readonly attribute FrameLoader? rootFrameLoader; // Embedded (browser) only

  readonly attribute WindowGlobalChild? childActor; // in-process only

  // Checks for any WindowContexts with "beforeunload" listeners in this
  // WindowGlobal's subtree. If any exist, a "beforeunload" event is
  // dispatched to them. If any of those request to block the navigation,
  // displays a prompt to the user. Returns a boolean which resolves to true
  // if the navigation should be allowed.
  //
  // If `timeout` is greater than 0, it is the maximum time (in milliseconds)
  // we will wait for a child process to respond with a request to block
  // navigation before proceeding. If the user needs to be prompted, however,
  // the promise will not resolve until the user has responded, regardless of
  // the timeout.
  [NewObject]
  Promise<boolean> permitUnload(optional PermitUnloadAction action = "prompt",
                                optional unsigned long timeout = 0);

  // Information about the currently loaded document.
  readonly attribute Principal documentPrincipal;
  readonly attribute Principal documentStoragePrincipal;
  readonly attribute Principal? contentBlockingAllowListPrincipal;
  readonly attribute URI? documentURI;
  readonly attribute DOMString documentTitle;
  readonly attribute nsICookieJarSettings? cookieJarSettings;

  // Bit mask containing content blocking events that are recorded in
  // the document's content blocking log.
  readonly attribute unsigned long contentBlockingEvents;

  // String containing serialized content blocking log.
  readonly attribute DOMString contentBlockingLog;

  // DOM Process which this window was loaded in. Will be either InProcessParent
  // for windows loaded in the parent process, or ContentParent for windows
  // loaded in the content process.
  readonly attribute nsIDOMProcessParent? domProcess;

  static WindowGlobalParent? getByInnerWindowId(unsigned long long innerWindowId);

  /**
   * Get or create the JSWindowActor with the given name.
   *
   * See WindowActorOptions from JSWindowActor.webidl for details on how to
   * customize actor creation.
   */
  [Throws]
  JSWindowActorParent getActor(UTF8String name);
  JSWindowActorParent? getExistingActor(UTF8String name);

  /**
   * Renders a region of the frame into an image bitmap.
   *
   * @param rect Specify the area of the document to render, in CSS pixels,
   * relative to the page. If null, the currently visible viewport is rendered.
   * @param scale The scale to render the window at. Use devicePixelRatio
   * to have comparable rendering to the OS.
   * @param backgroundColor The background color to use.
   * @param resetScrollPosition If true, temporarily resets the scroll position
   * of the root scroll frame to 0, such that position:fixed elements are drawn
   * at their initial position. This parameter only takes effect when passing a
   * non-null rect.
   *
   * This API can only be used in the parent process, as content processes
   * cannot access the rendering of out of process iframes. This API works
   * with remote and local frames.
   */
  [NewObject]
  Promise<ImageBitmap> drawSnapshot(DOMRect? rect,
                                    double scale,
                                    UTF8String backgroundColor,
                                    optional boolean resetScrollPosition = false);

  /**
   * Fetches the securityInfo object for this window. This function will
   * look for failed and successful channels to find the security info,
   * thus it will work on regular HTTPS pages as well as certificate
   * error pages.
   *
   * This returns a Promise which resolves to an nsITransportSecurity
   * object with certificate data or undefined if no security info is available.
   */
  [NewObject]
  Promise<nsITransportSecurityInfo> getSecurityInfo();

  // True if any of the windows in the subtree rooted at this window
  // has active peer connections.  If this is called for a non-top-level
  // context, it always returns false.
  boolean hasActivePeerConnections();
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

  // Is this WindowGlobalChild same-origin with `window.top`?
  readonly attribute boolean sameOriginWithTop;

  readonly attribute WindowGlobalParent? parentActor; // in-process only

  static WindowGlobalChild? getByInnerWindowId(unsigned long long innerWIndowId);

  /**
   * Get or create the JSWindowActor with the given name.
   *
   * See WindowActorOptions from JSWindowActor.webidl for details on how to
   * customize actor creation.
   */
  [Throws]
  JSWindowActorChild getActor(UTF8String name);
  JSWindowActorChild? getExistingActor(UTF8String name);
};

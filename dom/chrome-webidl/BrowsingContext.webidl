/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;
interface nsISecureBrowserUI;
interface nsIWebProgress;

interface mixin LoadContextMixin {
  readonly attribute WindowProxy? associatedWindow;

  readonly attribute WindowProxy? topWindow;

  readonly attribute Element? topFrameElement;

  readonly attribute boolean isContent;

  [SetterThrows]
  attribute boolean usePrivateBrowsing;

  readonly attribute boolean useRemoteTabs;

  readonly attribute boolean useRemoteSubframes;

  [BinaryName="useTrackingProtectionWebIDL", SetterThrows]
  attribute boolean useTrackingProtection;

  [NewObject, Throws]
  readonly attribute any originAttributes;
};

[Exposed=Window, ChromeOnly]
interface BrowsingContext {
  static BrowsingContext? get(unsigned long long aId);

  static BrowsingContext? getFromWindow(WindowProxy window);

  sequence<BrowsingContext> getAllBrowsingContextsInSubtree();

  BrowsingContext? findChildWithName(DOMString name, BrowsingContext accessor);
  BrowsingContext? findWithName(DOMString name);

  readonly attribute DOMString name;

  readonly attribute BrowsingContext? parent;

  readonly attribute BrowsingContext top;

  [Cached, Frozen, Pure]
  readonly attribute sequence<BrowsingContext> children;

  readonly attribute nsIDocShell? docShell;

  readonly attribute Element? embedderElement;

  readonly attribute unsigned long long id;

  readonly attribute BrowsingContext? opener;

  readonly attribute BrowsingContextGroup group;

  readonly attribute WindowProxy? window;

  readonly attribute WindowContext? currentWindowContext;

  readonly attribute WindowContext? parentWindowContext;

  readonly attribute WindowContext? topWindowContext;

  [SetterThrows] attribute [TreatNullAs=EmptyString] DOMString customPlatform;

  [SetterThrows] attribute [TreatNullAs=EmptyString] DOMString customUserAgent;

  readonly attribute DOMString embedderElementType;

  /**
   * The sandbox flags on the browsing context. These reflect the value of the
   * sandbox attribute of the associated IFRAME or CSP-protectable content, if
   * existent. See the HTML5 spec for more details.
   * These flags on the browsing context reflect the current state of the
   * sandbox attribute, which is modifiable. They are only used when loading new
   * content, sandbox flags are also immutably set on the document when it is
   * loaded.
   * The sandbox flags of a document depend on the sandbox flags on its
   * browsing context and of its parent document, if any.
   * See nsSandboxFlags.h for the possible flags.
   */
  [SetterThrows] attribute unsigned long sandboxFlags;

  // The inRDMPane flag indicates whether or not Responsive Design Mode is
  // active for the browsing context.
  [SetterThrows] attribute boolean inRDMPane;

  [SetterThrows] attribute float fullZoom;

  [SetterThrows] attribute float textZoom;

  [SetterThrows] attribute boolean isAwaitingPrint;

  /**
   * Whether this docshell should save entries in global history.
   */
  [SetterThrows] attribute boolean useGlobalHistory;

  // Extension to give chrome JS the ability to set the window screen
  // orientation while in RDM.
  [Throws] void setRDMPaneOrientation(OrientationType type, float rotationAngle);

  // Extension to give chrome JS the ability to set a maxTouchPoints override
  // while in RDM.
  [Throws] void setRDMPaneMaxTouchPoints(octet maxTouchPoints);

  // The watchedByDevTools flag indicates whether or not DevTools are currently
  // debugging this browsing context.
  [SetterThrows] attribute boolean watchedByDevTools;

  /**
   * A unique identifier for the browser element that is hosting this
   * BrowsingContext tree. Every BrowsingContext in the element's tree will
   * return the same ID in all processes and it will remain stable regardless of
   * process changes. When a browser element's frameloader is switched to
   * another browser element this ID will remain the same but hosted under the
   * under the new browser element.
   */
  [SetterThrows] attribute unsigned long long browserId;
};

BrowsingContext includes LoadContextMixin;

[Exposed=Window, ChromeOnly]
interface CanonicalBrowsingContext : BrowsingContext {
  sequence<WindowGlobalParent> getWindowGlobals();

  readonly attribute WindowGlobalParent? currentWindowGlobal;

  readonly attribute WindowProxy? topChromeWindow;

  // XXX(nika): This feels kinda hacky, but will do for now while we don't
  // synchronously create WindowGlobalParent. It can throw if somehow the
  // content process has died.
  [Throws]
  readonly attribute UTF8String? currentRemoteType;

  readonly attribute WindowGlobalParent? embedderWindowGlobal;

  void notifyStartDelayedAutoplayMedia();
  [Throws] void notifyMediaMutedChanged(boolean muted);

  readonly attribute nsISecureBrowserUI? secureBrowserUI;

  /**
   * Returns an nsIWebProgress object for this BrowsingContext, if this
   * is a top-level content BC.
   *
   * Progress listeners attached to this will get notifications filtered by
   * nsBrowserStatusFilter, and don't get any notifications from sub frames.
   */
  readonly attribute nsIWebProgress? webProgress;

  static unsigned long countSiteOrigins(sequence<BrowsingContext> roots);

  /**
   * Loads a given URI.  This will give priority to loading the requested URI
   * in the object implementing this interface.  If it can't be loaded here
   * however, the URI dispatcher will go through its normal process of content
   * loading.
   *
   * @param aURI
   *        The URI string to load.  For HTTP and FTP URLs and possibly others,
   *        characters above U+007F will be converted to UTF-8 and then URL-
   *        escaped per the rules of RFC 2396.
   * @param aLoadURIOptions
   *        A JSObject defined in LoadURIOptions.webidl holding info like e.g.
   *        the triggeringPrincipal, the referrer info.
   */
  [Throws]
  void loadURI(DOMString aURI, optional LoadURIOptions aOptions = {});

  /**
   * These methods implement the nsIWebNavigation methods of the same names
   */
  void goBack(optional long aCancelContentJSEpoch, optional boolean aRequireUserInteraction = false);
  void goForward(optional long aCancelContentJSEpoch, optional boolean aRequireUserInteraction  = false);
  void goToIndex(long aIndex, optional long aCancelContentJSEpoch);
  void reload(unsigned long aReloadFlags);
  void stop(unsigned long aStopFlags);

  readonly attribute nsISHistory? sessionHistory;

  readonly attribute MediaController? mediaController;
};

[Exposed=Window, ChromeOnly]
interface BrowsingContextGroup {
  sequence<BrowsingContext> getToplevels();

  readonly attribute unsigned long long id;
};

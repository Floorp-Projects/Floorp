/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;

[Exposed=Window, ChromeOnly]
interface BrowsingContext {
  static BrowsingContext? get(unsigned long long aId);

  static BrowsingContext? getFromWindow(WindowProxy window);

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

  attribute [TreatNullAs=EmptyString] DOMString customUserAgent;

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
  attribute unsigned long sandboxFlags;

  // The inRDMPane flag indicates whether or not Responsive Design Mode is
  // active for the browsing context.
  attribute boolean inRDMPane;

  // Extension to give chrome JS the ability to set the window screen
  // orientation while in RDM.
  void setRDMPaneOrientation(OrientationType type, float rotationAngle);
};

[Exposed=Window, ChromeOnly]
interface CanonicalBrowsingContext : BrowsingContext {
  sequence<WindowGlobalParent> getWindowGlobals();

  readonly attribute WindowGlobalParent? currentWindowGlobal;

  // XXX(nika): This feels kinda hacky, but will do for now while we don't
  // synchronously create WindowGlobalParent. It can throw if somehow the
  // content process has died.
  [Throws]
  readonly attribute DOMString? currentRemoteType;

  readonly attribute WindowGlobalParent? embedderWindowGlobal;

  void notifyStartDelayedAutoplayMedia();
  void notifyMediaMutedChanged(boolean muted);

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

  [Throws]
  Promise<unsigned long long> changeFrameRemoteness(
      DOMString remoteType, unsigned long long pendingSwitchId);

  readonly attribute nsISHistory? sessionHistory;
};

[Exposed=Window, ChromeOnly]
interface BrowsingContextGroup {
  sequence<BrowsingContext> getToplevels();
};

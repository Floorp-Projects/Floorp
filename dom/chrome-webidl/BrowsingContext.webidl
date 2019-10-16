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
  BrowsingContext? findWithName(DOMString name, BrowsingContext accessor);

  readonly attribute DOMString name;

  readonly attribute BrowsingContext? parent;

  readonly attribute BrowsingContext top;

  sequence<BrowsingContext> getChildren();

  readonly attribute nsIDocShell? docShell;

  readonly attribute Element? embedderElement;

  readonly attribute unsigned long long id;

  readonly attribute BrowsingContext? opener;

  readonly attribute BrowsingContextGroup group;

  readonly attribute WindowProxy? window;

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

  [Throws]
  Promise<unsigned long long> changeFrameRemoteness(
      DOMString remoteType, unsigned long long pendingSwitchId);
};

[Exposed=Window, ChromeOnly]
interface BrowsingContextGroup {
  sequence<BrowsingContext> getToplevels();
};

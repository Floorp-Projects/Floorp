/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface URI;
interface WindowProxy;

callback WebExtensionLocalizeCallback = DOMString (DOMString unlocalizedText);

/**
 * Defines the platform-level policies for a WebExtension, including its
 * permissions and the characteristics of its moz-extension: URLs.
 */
[Constructor(WebExtensionInit options), ChromeOnly, Exposed=System]
interface WebExtensionPolicy {
  /**
   * The add-on's internal ID, as specified in its manifest.json file or its
   * XPI signature.
   */
  [Constant, StoreInSlot]
  readonly attribute DOMString id;

  /**
   * The hostname part of the extension's moz-extension: URLs. This value is
   * generated randomly at install time.
   */
  [Constant, StoreInSlot]
  readonly attribute ByteString mozExtensionHostname;

  /**
   * The file: or jar: URL to use for the base of the extension's
   * moz-extension: URL root.
   */
  [Constant]
  readonly attribute ByteString baseURL;

  /**
   * The extension's user-visible name.
   */
  [Constant]
  readonly attribute DOMString name;

  /**
   * The content security policy string to apply to all pages loaded from the
   * extension's moz-extension: protocol.
   */
  [Constant]
  readonly attribute DOMString contentSecurityPolicy;


  /**
   * The list of currently-active permissions for the extension, as specified
   * in its manifest.json file. May be updated to reflect changes in the
   * extension's optional permissions.
   */
  [Cached, Frozen, Pure]
  attribute sequence<DOMString> permissions;

  /**
   * Match patterns for the set of web origins to which the extension is
   * currently allowed access. May be updated to reflect changes in the
   * extension's optional permissions.
   */
  [Pure]
  attribute MatchPatternSet allowedOrigins;

  /**
   * The set of content scripts active for this extension.
   */
  [Cached, Constant, Frozen]
  readonly attribute sequence<WebExtensionContentScript> contentScripts;

  /**
   * True if the extension is currently active, false otherwise. When active,
   * the extension's moz-extension: protocol will point to the given baseURI,
   * and the set of policies for this object will be active for its ID.
   *
   * Only one extension policy with a given ID or hostname may be active at a
   * time. Attempting to activate a policy while a conflicting policy is
   * active will raise an error.
   */
  [Affects=Everything, SetterThrows]
  attribute boolean active;


  static readonly attribute boolean isExtensionProcess;


  /**
   * Returns true if the extension has cross-origin access to the given URI.
   */
  boolean canAccessURI(URI uri, optional boolean explicit = false);

  /**
   * Returns true if the extension currently has the given permission.
   */
  boolean hasPermission(DOMString permission);

  /**
   * Returns true if the given path relative to the extension's moz-extension:
   * URL root may be accessed by web content.
   */
  boolean isPathWebAccessible(DOMString pathname);

  /**
   * Replaces localization placeholders in the given string with localized
   * text from the extension's currently active locale.
   */
  DOMString localize(DOMString unlocalizedText);

  /**
   * Returns the moz-extension: URL for the given path.
   */
  [Throws]
  DOMString getURL(optional DOMString path = "");


  /**
   * Returns the list of currently active extension policies.
   */
  static sequence<WebExtensionPolicy> getActiveExtensions();

  /**
   * Returns the currently-active policy for the extension with the given ID,
   * or null if no policy is active for that ID.
   */
  static WebExtensionPolicy? getByID(DOMString id);

  /**
   * Returns the currently-active policy for the extension with the given
   * moz-extension: hostname, or null if no policy is active for that
   * hostname.
   */
  static WebExtensionPolicy? getByHostname(ByteString hostname);

  /**
   * Returns the currently-active policy for the extension extension URI, or
   * null if the URI is not an extension URI, or no policy is currently active
   * for it.
   */
  static WebExtensionPolicy? getByURI(URI uri);
};

dictionary WebExtensionInit {
  required DOMString id;

  required ByteString mozExtensionHostname;

  required DOMString baseURL;

  DOMString name = "";

  required WebExtensionLocalizeCallback localizeCallback;

  required MatchPatternSet allowedOrigins;

  sequence<DOMString> permissions = [];

  sequence<MatchGlob> webAccessibleResources = [];

  sequence<WebExtensionContentScriptInit> contentScripts = [];

  DOMString? contentSecurityPolicy = null;

  sequence<DOMString>? backgroundScripts = null;
};

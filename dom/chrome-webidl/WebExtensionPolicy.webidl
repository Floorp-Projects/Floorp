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
[ChromeOnly, Exposed=Window]
interface WebExtensionPolicy {
  [Throws]
  constructor(WebExtensionInit options);
  
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
   * The add-on's internal type as determined by parsing the manifest.json file.
   */
  [Constant]
  readonly attribute DOMString type;

  /**
   * Whether the extension has access to privileged features
   */
  [Constant]
  readonly attribute boolean isPrivileged;

  /**
   * Whether the extension is installed temporarily
   */
  [Constant]
  readonly attribute boolean temporarilyInstalled;

  /**
   * The manifest version in use by the extension.
   */
  [Constant]
  readonly attribute unsigned long manifestVersion;

  /**
   * The base content security policy string to apply on extension
   * pages for this extension.  The baseCSP is specific to the
   * manifest version.  If the manifest version is 3 or higher it
   * is also applied to content scripts.
   */
  [Constant]
  readonly attribute DOMString baseCSP;

  /**
   * The content security policy string to apply to all pages loaded from the
   * extension's moz-extension: protocol.  If one is not provided by the
   * extension the default value from preferences is used.
   * See extensions.webextensions.default-content-security-policy.
   */
  [Constant]
  readonly attribute DOMString extensionPageCSP;

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
  [Cached, Frozen, Pure]
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

  /**
   * True if both e10s and webextensions.remote are enabled.  This must be
   * used instead of checking the remote pref directly since remote extensions
   * require both to be enabled.
   */
  static readonly attribute boolean useRemoteWebExtensions;

  /**
   * True if the calling process is an extension process.
   */
  static readonly attribute boolean isExtensionProcess;

  /**
   * Whether the background.service_worker in the extension manifest.json file
   * is enabled.
   *
   * NOTE: **do not use Services.prefs to retrieve the value of the undelying pref**
   *
   * It is defined in StaticPrefs.yaml as `mirror: once` and so checking
   * its current value using Services.prefs doesn't guarantee that it does
   * match the value as accessible from the C++ layers, and unexpected issue
   * may be possible if different code has a different idea of its value.
   */
  static readonly attribute boolean backgroundServiceWorkerEnabled;

  /**
   * Set based on the manifest.incognito value:
   * If "spanning" or "split" will be true.
   * If "not_allowed" will be false.
   */
  [Pure]
  readonly attribute boolean privateBrowsingAllowed;

  /**
   * Returns true if the extension can access a window.  Access is
   * determined by matching the windows private browsing context
   * with privateBrowsingMode.  This does not, and is not meant to
   * handle specific differences between spanning and split mode.
   */
  [Affects=Nothing]
  boolean canAccessWindow(WindowProxy window);

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
   * URL root is listed as a web accessible path. Access checks on a path, such
   * as performed in nsScriptSecurityManager, use sourceMayAccessPath below.
   */
  boolean isWebAccessiblePath(DOMString pathname);

  /**
   * Returns true if the given path relative to the extension's moz-extension:
   * URL root may be accessed by web content at sourceURI.  For Manifest V2,
   * sourceURI is ignored and the path must merely be listed as web accessible.
   */
  boolean sourceMayAccessPath(URI sourceURI, DOMString pathname);

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
   * Register a new content script programmatically.
   */
  [Throws]
  void registerContentScript(WebExtensionContentScript script);

  /**
   * Unregister a content script.
   */
  [Throws]
  void unregisterContentScript(WebExtensionContentScript script);

  /**
   * Injects the extension's content script into all existing matching windows.
   */
  [Throws]
  void injectContentScripts();

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

  /**
   * Returns true if the URI is restricted for any extension.
   */
  static boolean isRestrictedURI(URI uri);

  /**
   * When present, the extension is not yet ready to load URLs. In that case,
   * this policy object is a stub, and the attribute contains a promise which
   * resolves to a new, non-stub policy object when the extension is ready.
   *
   * This may be used to delay operations, such as loading extension pages,
   * which depend on extensions being fully initialized.
   *
   * Note: This will always be either a Promise<WebExtensionPolicy?> or null,
   * but the WebIDL grammar does not allow us to specify a nullable Promise
   * type.
   *
   * Note: This could resolve to null when the startup was interrupted.
   */
  readonly attribute object? readyPromise;

  /**
   * Returns true if the given worker script URL matches the background
   * service worker url declared in the extension manifest.json file.
   */
  boolean isManifestBackgroundWorker(DOMString workerURL);

  /**
   * Get the unique BrowsingContextGroup ID which will be used for toplevel
   * page loads from this extension.
   *
   * This method will raise an exception if called from outside of the parent
   * process, or if the extension is inactive.
   */
  [Throws]
  readonly attribute unsigned long long browsingContextGroupId;
};

dictionary WebAccessibleResourceInit {
  required sequence<MatchGlobOrString> resources;
  MatchPatternSetOrStringSequence? matches = null;
  sequence<DOMString>? extension_ids = null;
};

dictionary WebExtensionInit {
  required DOMString id;

  required ByteString mozExtensionHostname;

  required DOMString baseURL;

  DOMString name = "";

  DOMString type = "";

  boolean isPrivileged = false;

  boolean temporarilyInstalled = false;

  required WebExtensionLocalizeCallback localizeCallback;

  required MatchPatternSetOrStringSequence allowedOrigins;

  sequence<DOMString> permissions = [];

  sequence<WebAccessibleResourceInit> webAccessibleResources = [];

  sequence<WebExtensionContentScriptInit> contentScripts = [];

  // The use of a content script csp is determined by the manifest version.
  unsigned long manifestVersion = 2;
  DOMString? extensionPageCSP = null;

  sequence<DOMString>? backgroundScripts = null;
  DOMString? backgroundWorkerScript = null;

  Promise<WebExtensionPolicy?> readyPromise;
};

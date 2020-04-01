/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface LoadInfo;
interface MozChannel;
interface RemoteTab;
interface URI;
interface nsISupports;

/**
 * Load types that correspond to the external types in nsIContentPolicy.idl.
 * Please also update that IDL when updating this list.
 */
enum MozContentPolicyType {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "object_subrequest",
  "xmlhttprequest",
  "fetch",
  "xbl",
  "xslt",
  "ping",
  "beacon",
  "xml_dtd",
  "font",
  "media",
  "websocket",
  "csp_report",
  "imageset",
  "web_manifest",
  "speculative",
  "other"
};

/**
 * String versions of CLASSIFIED_* tracking flags from nsIClassifiedChannel.idl
 */
enum MozUrlClassificationFlags {
  "fingerprinting",
  "fingerprinting_content",
  "cryptomining",
  "cryptomining_content",
  "tracking",
  "tracking_ad",
  "tracking_analytics",
  "tracking_social",
  "tracking_content",
  "socialtracking",
  "socialtracking_facebook",
  "socialtracking_linkedin",
  "socialtracking_twitter",
  "any_basic_tracking",
  "any_strict_tracking",
  "any_social_tracking"
};

/**
 * A thin wrapper around nsIChannel and nsIHttpChannel that allows JS
 * callers to access them without XPConnect overhead.
 */
[ChromeOnly, Exposed=Window]
interface ChannelWrapper : EventTarget {
  /**
   * Returns the wrapper instance for the given channel. The same wrapper is
   * always returned for a given channel.
   */
  static ChannelWrapper get(MozChannel channel);

  /**
   * Returns the wrapper instance for the given channel. The same wrapper is
   * always returned for a given channel.
   */
  static ChannelWrapper? getRegisteredChannel(unsigned long long aChannelId,
                                             WebExtensionPolicy extension,
                                             RemoteTab? remoteTab);

  /**
   * A unique ID for for the requests which remains constant throughout the
   * redirect chain.
   */
  [Constant, StoreInSlot]
  readonly attribute unsigned long long id;

  // Not technically pure, since it's backed by a weak reference, but if JS
  // has a reference to the previous value, we can depend on it not being
  // collected.
  [Pure]
  attribute MozChannel? channel;


  /**
   * Cancels the request with the given nsresult status code.
   *
   * The optional reason parameter should be one of the BLOCKING_REASON
   * constants from nsILoadInfo.idl
   */
  [Throws]
  void cancel(unsigned long result, optional unsigned long reason = 0);

  /**
   * Redirects the wrapped HTTP channel to the given URI. For other channel
   * types, this method will throw. The redirect is an internal redirect, and
   * the behavior is the same as nsIHttpChannel.redirectTo.
   */
  [Throws]
  void redirectTo(URI url);

  /**
   * Requests an upgrade of the HTTP channel to a secure request. For other channel
   * types, this method will throw. The redirect is an internal redirect, and
   * the behavior is the same as nsIHttpChannel.upgradeToSecure. Setting this
   * flag is only effective during the WebRequest.onBeforeRequest in
   * Web Extensions, calling this at any other point during the request will
   * have no effect. Setting this flag in addition to calling redirectTo
   * results in the redirect happening rather than the upgrade request.
   */
  [Throws]
  void upgradeToSecure();

  /**
   * Suspends the underlying channel.
   */
  [Throws]
  void suspend();

  /**
   * Resumes (un-suspends) the underlying channel.  The profilerText parameter
   * is only used to annotate profiles.
   */
  [Throws]
  void resume(ByteString profileText);

  /**
   * The content type of the request, usually as read from the Content-Type
   * header. This should be used in preference to the header to determine the
   * content type of the channel.
   */
  [Pure]
  attribute ByteString contentType;


  /**
   * For HTTP requests, the request method (e.g., GET, POST, HEAD). For other
   * request types, returns an empty string.
   */
  [Cached, Pure]
  readonly attribute ByteString method;

  /**
   * For requests with LoadInfo, the content policy type that corresponds to
   * the request. For requests without LoadInfo, returns "other".
   */
  [Cached, Pure]
  readonly attribute MozContentPolicyType type;


  /**
   * When true, the request is currently suspended by the wrapper. When false,
   * the request is not suspended by the wrapper, but may still be suspended
   * by another caller.
   */
  [Pure]
  readonly attribute boolean suspended;


  /**
   * The final URI of the channel (as returned by NS_GetFinalChannelURI) after
   * any redirects have been processed.
   */
  [Cached, Pure]
  readonly attribute URI finalURI;

  /**
   * The string version of finalURI (but cheaper to access than
   * finalURI.spec).
   */
  [Cached, Pure]
  readonly attribute DOMString finalURL;


  /**
   * Returns true if the request matches the given request filter, and the
   * given extension has permission to access it.
   */
  boolean matches(optional MozRequestFilter filter = {},
                  optional WebExtensionPolicy? extension = null,
                  optional MozRequestMatchOptions options = {});


  /**
   * Register's this channel as traceable by the given add-on when accessed
   * via the process of the given RemoteTab.
   */
  void registerTraceableChannel(WebExtensionPolicy extension, RemoteTab? remoteTab);

  /**
   * The current HTTP status code of the request. This will be 0 if a response
   * has not yet been received, or if the request is not an HTTP request.
   */
  [Cached, Pure]
  readonly attribute unsigned long statusCode;

  /**
   * The HTTP status line for the request (e.g., "HTTP/1.0 200 Success"). This
   * will be an empty string if a response has not yet been received, or if
   * the request is not an HTTP request.
   */
  [Cached, Pure]
  readonly attribute ByteString statusLine;


  /**
   * If the request has failed or been canceled, an opaque string representing
   * the error. For requests that failed at the NSS layer, this is an NSS
   * error message. For requests that failed for any other reason, it is the
   * name of an nsresult error code. For requests which haven't failed, this
   * is null.
   *
   * This string is used in the error message when notifying extension
   * webRequest listeners of failure. The documentation specifically states
   * that this value MUST NOT be parsed, and is only meant to be displayed to
   * humans, but we all know how that works in real life.
   */
  [Cached, Pure]
  readonly attribute DOMString? errorString;

  /**
   * Dispatched when the channel is closed with an error status. Check
   * errorString for the error details.
   */
  attribute EventHandler onerror;

  /**
   * Checks the request's current status and dispatches an error event if the
   * request has failed and one has not already been dispatched.
   */
  void errorCheck();


  /**
   * Dispatched when the channel begins receiving data.
   */
  attribute EventHandler onstart;

  /**
   * Dispatched when the channel has finished receiving data.
   */
  attribute EventHandler onstop;


  /**
   * Information about the proxy server which is handling this request, or
   * null if the request is not proxied.
   */
  [Cached, Frozen, GetterThrows, Pure]
  readonly attribute MozProxyInfo? proxyInfo;

  /**
   * For HTTP requests, the IP address of the remote server handling the
   * request. For other request types, returns null.
   */
  [Cached, Pure]
  readonly attribute ByteString? remoteAddress;


  /**
   * The LoadInfo object for this channel, if available. Null for channels
   * without load info, until support for those is removed.
   */
  [Cached, Pure]
  readonly attribute LoadInfo? loadInfo;

  /**
   * True if this load was triggered by a system caller. This currently always
   * false if the request has no LoadInfo or is a top-level document load.
   */
  [Cached, Pure]
  readonly attribute boolean isSystemLoad;

  /**
   * The URL of the principal that triggered this load. This is equivalent to
   * the LoadInfo's triggeringPrincipal, and will only ever be null for
   * requests without LoadInfo.
   */
  [Cached, Pure]
  readonly attribute ByteString? originURL;

  /**
   * The URL of the document loading the content for this request. This is
   * equivalent to the LoadInfo's loadingPrincipal. This may only ever be null
   * for top-level requests and requests without LoadInfo.
   */
  [Cached, Pure]
  readonly attribute ByteString? documentURL;

  /**
   * The URI version of originURL. Will be null only when originURL is null.
   */
  [Pure]
  readonly attribute URI? originURI;

  /**
   * The URI version of documentURL. Will be null only when documentURL is
   * null.
   */
  [Pure]
  readonly attribute URI? documentURI;


  /**
   * True if extensions may modify this request. This is currently false only
   * if the request belongs to a document which has access to the
   * mozAddonManager API.
   */
  [Cached, GetterThrows, Pure]
  readonly attribute boolean canModify;


  /**
   * The outer window ID of the frame that the request belongs to, or 0 if it
   * is a top-level load or does not belong to a document.
   */
  [Cached, Constant]
  readonly attribute long long windowId;

  /**
   * The outer window ID of the parent frame of the window that the request
   * belongs to, 0 if that parent frame is the top-level frame, and -1 if the
   * request belongs to a top-level frame.
   */
  [Cached, Constant]
  readonly attribute long long parentWindowId;

  /**
   * For cross-process requests, the <browser> or <iframe> element to which the
   * content loading this request belongs. For requests that don't originate
   * from a remote browser, this is null.
   *
   * This is not an Element because those are by default only exposed in
   * Window, but we're exposed in System.
   */
  [Cached, Pure]
  readonly attribute nsISupports? browserElement;

  /**
   * Returns an array of objects that combine the url and frameId from the
   * ancestorPrincipals and ancestorOuterWindowIDs on loadInfo.
   * The immediate parent is the first entry, the last entry is always the top
   * level frame.  It will be an empty list for toplevel window loads and
   * non-subdocument resource loads within a toplevel window.  For the latter,
   * originURL will provide information on what window is doing the load.  It
   * will be null if the request is not associated with a window (e.g. XHR with
   * mozBackgroundRequest = true).
   */
  [Cached, Frozen, GetterThrows, Pure]
  readonly attribute sequence<MozFrameAncestorInfo>? frameAncestors;

  /**
   * For HTTP requests, returns an array of request headers which will be, or
   * have been, sent with this request.
   *
   * For non-HTTP requests, throws NS_ERROR_UNEXPECTED.
   */
  [Throws]
  sequence<MozHTTPHeader> getRequestHeaders();

  /**
   * For HTTP requests, returns an array of response headers which were
   * received for this request, in the same format as returned by
   * getRequestHeaders.

   * Throws NS_ERROR_NOT_AVAILABLE if a response has not yet been received, or
   * NS_ERROR_UNEXPECTED if the channel is not an HTTP channel.
   *
   * Note: The Content-Type header is handled specially. That header is
   * usually not mutable after the request has been received, and the content
   * type must instead be changed via the contentType attribute. If a caller
   * attempts to set the Content-Type header via setRequestHeader, however,
   * that value is assigned to the contentType attribute and its original
   * string value is cached. That original value is returned in place of the
   * actual Content-Type header.
   */
  [Throws]
  sequence<MozHTTPHeader> getResponseHeaders();

  /**
   * Sets the given request header to the given value, overwriting any
   * previous value. Setting a header to a null string has the effect of
   * removing it.  If merge is true, then the passed value will be merged
   * to any existing value that exists for the header. Otherwise, any prior
   * value for the header will be overwritten. Merge is ignored for headers
   * that cannot be merged.
   *
   * For non-HTTP requests, throws NS_ERROR_UNEXPECTED.
   */
  [Throws]
  void setRequestHeader(ByteString header,
                        ByteString value,
                        optional boolean merge = false);

  /**
   * Sets the given response header to the given value, overwriting any
   * previous value. Setting a header to a null string has the effect of
   * removing it.  If merge is true, then the passed value will be merged
   * to any existing value that exists for the header (e.g. handling multiple
   * Set-Cookie headers).  Otherwise, any prior value for the header will be
   * overwritten. Merge is ignored for headers that cannot be merged.
   *
   * For non-HTTP requests, throws NS_ERROR_UNEXPECTED.
   *
   * Note: The content type header is handled specially by this function. See
   * getResponseHeaders() for details.
   */
  [Throws]
  void setResponseHeader(ByteString header,
                         ByteString value,
                         optional boolean merge = false);

  /**
   * Provides the tracking classification data when it is available.
   */
  [Cached, Frozen, GetterThrows, Pure]
  readonly attribute MozUrlClassification? urlClassification;

  /**
   * Indicates if this response and its content window hierarchy is third
   * party.
   */
  [Cached, Constant]
  readonly attribute boolean thirdParty;

  /**
   * The current bytes sent of the request. This will be 0 if a request has not
   * sent yet, or if the request is not an HTTP request.
   */
  [Cached, Pure]
  readonly attribute unsigned long long requestSize;

  /**
   * The current bytes received of the response. This will be 0 if a response
   * has not recieved yet, or if the request is not an HTTP response.
   */
  [Cached, Pure]
  readonly attribute unsigned long long responseSize;
};

/**
 * Wrapper for first and third party tracking classification data.
 */
dictionary MozUrlClassification {
  required sequence<MozUrlClassificationFlags> firstParty;
  required sequence<MozUrlClassificationFlags> thirdParty;
};

/**
 * Information about the proxy server handing a request. This is approximately
 * equivalent to nsIProxyInfo.
 */
dictionary MozProxyInfo {
  /**
   * The hostname of the server.
   */
  required ByteString host;
  /**
   * The TCP port of the server.
   */
  required long port;
  /**
   * The type of proxy (e.g., HTTP, SOCKS).
   */
  required ByteString type;

  /**
   * True if the proxy is responsible for DNS lookups.
   */
  required boolean proxyDNS;

  /**
   * The authentication username for the proxy, if any.
   */
  ByteString? username = null;

  /**
   * The timeout, in seconds, before the network stack will failover to the
   * next candidate proxy server if it has not received a response.
   */
  unsigned long failoverTimeout;

  /**
   * Any non-empty value will be passed directly as Proxy-Authorization header
   * value for the CONNECT request attempt.  However, this header set on the
   * resource request itself takes precedence.
   */
  ByteString? proxyAuthorizationHeader = null;

  /**
   * An optional key used for additional isolation of this proxy connection.
   */
  ByteString? connectionIsolationKey = null;
};

/**
 * MozFrameAncestorInfo combines loadInfo::AncestorPrincipals with
 * loadInfo::AncestorOuterWindowIDs for easier access in the WebRequest API.
 *
 * url represents the parent of the loading window.
 * frameId is the outerWindowID for the parent of the loading window.
 *
 * For further details see nsILoadInfo.idl and Document::AncestorPrincipals.
 */
dictionary MozFrameAncestorInfo {
  required ByteString url;
  required unsigned long long frameId;
};

/**
 * Represents an HTTP request or response header.
 */
dictionary MozHTTPHeader {
  /**
   * The case-insensitive, non-case-normalized header name.
   */
  required ByteString name;
  /**
   * The header value.
   */
  required ByteString value;
};

/**
 * An object used for filtering requests.
 */
dictionary MozRequestFilter {
  /**
   * If present, the request only matches if its `type` attribute matches one
   * of the given types.
   */
  sequence<MozContentPolicyType>? types = null;

  /**
   * If present, the request only matches if its finalURI matches the given
   * match pattern set.
   */
  MatchPatternSet? urls = null;

  /**
   * If present, the request only matches if the loadInfo privateBrowsingId matches
   * against the given incognito value.
   */
  boolean? incognito = null;
};

dictionary MozRequestMatchOptions {
  /**
   * True if we're matching for the proxy portion of a proxied request.
   */
  boolean isProxy = false;
};

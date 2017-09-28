/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface LoadInfo;
interface MozChannel;
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
  "other"
};

/**
 * A thin wrapper around nsIChannel and nsIHttpChannel that allows JS
 * callers to access them without XPConnect overhead.
 */
[ChromeOnly, Exposed=System]
interface ChannelWrapper {
  /**
   * Returns the wrapper instance for the given channel. The same wrapper is
   * always returned for a given channel.
   */
  static ChannelWrapper get(MozChannel channel);

  [Constant, StoreInSlot]
  readonly attribute unsigned long long id;

  // Not technically pure, since it's backed by a weak reference, but if JS
  // has a reference to the previous value, we can depend on it not being
  // collected.
  [Pure]
  attribute MozChannel? channel;


  [Throws]
  void cancel(unsigned long result);

  [Throws]
  void redirectTo(URI url);


  [Pure]
  attribute ByteString contentType;


  [Cached, Pure]
  readonly attribute ByteString method;

  [Cached, Pure]
  readonly attribute MozContentPolicyType type;


  [Pure, SetterThrows]
  attribute boolean suspended;


  [Cached, GetterThrows, Pure]
  readonly attribute URI finalURI;

  [Cached, GetterThrows, Pure]
  readonly attribute ByteString finalURL;


  [Cached, Pure]
  readonly attribute unsigned long statusCode;

  [Cached, Pure]
  readonly attribute ByteString statusLine;


  [Cached, Frozen, GetterThrows, Pure]
  readonly attribute MozProxyInfo? proxyInfo;

  [Cached, Pure]
  readonly attribute ByteString? remoteAddress;


  [Cached, Pure]
  readonly attribute LoadInfo? loadInfo;

  [Cached, Pure]
  readonly attribute boolean isSystemLoad;

  [Cached, Pure]
  readonly attribute ByteString? originURL;

  [Cached, Pure]
  readonly attribute ByteString? documentURL;

  [Pure]
  readonly attribute URI? originURI;

  [Pure]
  readonly attribute URI? documentURI;


  [Cached, GetterThrows, Pure]
  readonly attribute boolean canModify;


  [Cached, Constant]
  readonly attribute long long windowId;

  [Cached, Constant]
  readonly attribute long long parentWindowId;

  [Cached, Pure]
  readonly attribute nsISupports? browserElement;


  [Throws]
  object getRequestHeaders();

  [Throws]
  object getResponseHeaders();

  [Throws]
  void setRequestHeader(ByteString header, ByteString value);

  [Throws]
  void setResponseHeader(ByteString header, ByteString value);
};

dictionary MozProxyInfo {
  required ByteString host;
  required long port;
  required ByteString type;

  required boolean proxyDNS;

  ByteString? username = null;

  unsigned long failoverTimeout;
};

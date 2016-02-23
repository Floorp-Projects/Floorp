/* -*- Mode: IDL; tab-width: 1; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://fetch.spec.whatwg.org/#request-class
 */

typedef (Request or USVString) RequestInfo;
typedef unsigned long nsContentPolicyType;

[Constructor(RequestInfo input, optional RequestInit init),
 Exposed=(Window,Worker)]
interface Request {
  readonly attribute ByteString method;
  readonly attribute USVString url;
  [SameObject] readonly attribute Headers headers;

  [Func="mozilla::dom::Request::RequestContextEnabled"]
  readonly attribute RequestContext context;
  readonly attribute USVString referrer;
  readonly attribute RequestMode mode;
  readonly attribute RequestCredentials credentials;
  [Func="mozilla::dom::Request::RequestCacheEnabled"]
  readonly attribute RequestCache cache;
  readonly attribute RequestRedirect redirect;

  [Throws,
   NewObject] Request clone();

  // Bug 1124638 - Allow chrome callers to set the context.
  [ChromeOnly]
  void setContentPolicyType(nsContentPolicyType context);
};
Request implements Body;

dictionary RequestInit {
  ByteString method;
  HeadersInit headers;
  BodyInit body;
  RequestMode mode;
  RequestCredentials credentials;
  RequestCache cache;
  RequestRedirect redirect;
};

// Gecko currently does not ship RequestContext, so please don't use it in IDL
// that is exposed to script.
enum RequestContext {
  "audio", "beacon", "cspreport", "download", "embed", "eventsource", "favicon", "fetch",
  "font", "form", "frame", "hyperlink", "iframe", "image", "imageset", "import",
  "internal", "location", "manifest", "object", "ping", "plugin", "prefetch", "script",
  "sharedworker", "subresource", "style", "track", "video", "worker", "xmlhttprequest",
  "xslt"
};

enum RequestMode { "same-origin", "no-cors", "cors", "navigate" };
enum RequestCredentials { "omit", "same-origin", "include" };
enum RequestCache { "default", "no-store", "reload", "no-cache", "force-cache" };
enum RequestRedirect { "follow", "error", "manual" };

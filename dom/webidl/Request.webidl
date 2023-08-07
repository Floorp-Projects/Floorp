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

[Exposed=(Window,Worker)]
interface Request {
  [Throws]
  constructor(RequestInfo input, optional RequestInit init = {});

  readonly attribute ByteString method;
  readonly attribute USVString url;
  [SameObject, BinaryName="headers_"] readonly attribute Headers headers;

  readonly attribute RequestDestination destination;
  readonly attribute USVString referrer;
  [BinaryName="referrerPolicy_"]
  readonly attribute ReferrerPolicy referrerPolicy;
  readonly attribute RequestMode mode;
  readonly attribute RequestCredentials credentials;
  readonly attribute RequestCache cache;
  readonly attribute RequestRedirect redirect;
  readonly attribute DOMString integrity;

  // If a main-thread fetch() promise rejects, the error passed will be a
  // nsresult code.
  [ChromeOnly]
  readonly attribute boolean mozErrors;

  [BinaryName="getOrCreateSignal"]
  readonly attribute AbortSignal signal;

  [Throws,
   NewObject] Request clone();

  // Bug 1124638 - Allow chrome callers to set the context.
  [ChromeOnly]
  undefined overrideContentPolicyType(nsContentPolicyType context);
};
Request includes Body;

// <https://fetch.spec.whatwg.org/#requestinit>.
dictionary RequestInit {
  ByteString method;
  HeadersInit headers;
  BodyInit? body;
  USVString referrer;
  ReferrerPolicy referrerPolicy;
  RequestMode mode;
  RequestCredentials credentials;
  RequestCache cache;
  RequestRedirect redirect;
  DOMString integrity;

  [ChromeOnly]
  boolean mozErrors;

  AbortSignal? signal;

  [Pref="dom.fetchObserver.enabled"]
  ObserverCallback observe;
};

enum RequestDestination {
  "",
  "audio", "audioworklet", "document", "embed", "font", "frame", "iframe",
  "image", "manifest", "object", "paintworklet", "report", "script",
  "sharedworker", "style",  "track", "video", "worker", "xslt"
};

enum RequestMode { "same-origin", "no-cors", "cors", "navigate" };
enum RequestCredentials { "omit", "same-origin", "include" };
enum RequestCache { "default", "no-store", "reload", "no-cache", "force-cache", "only-if-cached" };
enum RequestRedirect { "follow", "error", "manual" };
enum RequestPriority { "high" , "low" , "auto" };

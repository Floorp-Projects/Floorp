/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://fetch.spec.whatwg.org/#response-class
 */

[Exposed=(Window,Worker)]
interface Response {
  // This should be constructor(optional BodyInit... but BodyInit doesn't
  // include ReadableStream yet because we don't want to expose Streams API to
  // Request.
  [Throws]
  constructor(optional (Blob or BufferSource or FormData or URLSearchParams or ReadableStream or USVString)? body = null,
              optional ResponseInit init = {});

  [NewObject] static Response error();
  [Throws,
   NewObject] static Response redirect(USVString url, optional unsigned short status = 302);

  readonly attribute ResponseType type;

  readonly attribute USVString url;
  readonly attribute boolean redirected;
  readonly attribute unsigned short status;
  readonly attribute boolean ok;
  readonly attribute ByteString statusText;
  [SameObject, BinaryName="headers_"] readonly attribute Headers headers;

  [Throws,
   NewObject] Response clone();

  [ChromeOnly, NewObject, Throws] Response cloneUnfiltered();

  // For testing only.
  [ChromeOnly] readonly attribute boolean hasCacheInfoChannel;
};
Response includes Body;

// This should be part of Body but we don't want to expose body to request yet.
// See bug 1387483.
partial interface Response {
  [GetterThrows, Pref="javascript.options.streams"]
  readonly attribute ReadableStream? body;
};

dictionary ResponseInit {
  unsigned short status = 200;
  ByteString statusText = "";
  HeadersInit headers;
};

enum ResponseType { "basic", "cors", "default", "error", "opaque", "opaqueredirect" };

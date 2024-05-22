/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://fetch.spec.whatwg.org/
 */

typedef object JSON;
typedef (Blob or BufferSource or FormData or URLSearchParams or USVString) XMLHttpRequestBodyInit;
/* no support for request body streams yet */
typedef XMLHttpRequestBodyInit BodyInit;

interface mixin Body {
  readonly attribute boolean bodyUsed;
  [NewObject]
  Promise<ArrayBuffer> arrayBuffer();
  [NewObject]
  Promise<Blob> blob();
  [NewObject]
  Promise<Uint8Array> bytes();
  [NewObject]
  Promise<FormData> formData();
  [NewObject]
  Promise<JSON> json();
  [NewObject]
  Promise<USVString> text();
};

// These are helper dictionaries for the parsing of a
// getReader().read().then(data) parsing.
// See more about how these 2 helpers are used in
// dom/fetch/FetchStreamReader.cpp
[GenerateInit]
dictionary FetchReadableStreamReadDataDone {
  boolean done = false;
};

[GenerateInit]
dictionary FetchReadableStreamReadDataArray {
  Uint8Array value;
};

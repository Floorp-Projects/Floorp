/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#rs-byob-request-class-definition
 */

[Exposed=*, Pref="dom.streams.byte_streams.enabled"]
interface ReadableStreamBYOBRequest {
  readonly attribute ArrayBufferView? view;

  [Throws]
  undefined respond([EnforceRange] unsigned long long bytesWritten);

  [Throws]
  undefined respondWithNewView(ArrayBufferView view);
};

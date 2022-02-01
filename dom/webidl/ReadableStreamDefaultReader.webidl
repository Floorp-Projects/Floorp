/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#generic-reader-mixin-definition
 * https://streams.spec.whatwg.org/#default-reader-class-definition
 */

typedef (ReadableStreamDefaultReader or ReadableStreamBYOBReader) ReadableStreamReader;

enum ReadableStreamType { "bytes" };

interface mixin ReadableStreamGenericReader {
  readonly attribute Promise<void> closed;

  [Throws]
  Promise<void> cancel(optional any reason);
};

[Exposed=(Window,Worker,Worklet),
Pref="dom.streams.expose.ReadableStreamDefaultReader"]
interface ReadableStreamDefaultReader {
  [Throws]
  constructor(ReadableStream stream);

  [Throws]
  Promise<ReadableStreamDefaultReadResult> read();

  [Throws]
  void releaseLock();
};
ReadableStreamDefaultReader includes ReadableStreamGenericReader;


dictionary ReadableStreamDefaultReadResult {
 any value;
 boolean done;
};

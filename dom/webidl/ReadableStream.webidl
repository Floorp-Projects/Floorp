/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#rs-class-definition
 */

[Exposed=*] // [Transferable] - See Bug 1562065
interface ReadableStream {
  [Throws]
  constructor(optional object underlyingSource, optional QueuingStrategy strategy = {});

  [Pref="dom.streams.from.enabled", Throws]
  static ReadableStream from(any asyncIterable);

  readonly attribute boolean locked;

  [NewObject]
  Promise<undefined> cancel(optional any reason);

  [Throws]
  ReadableStreamReader getReader(optional ReadableStreamGetReaderOptions options = {});

  [Throws]
  ReadableStream pipeThrough(ReadableWritablePair transform, optional StreamPipeOptions options = {});

  [NewObject]
  Promise<undefined> pipeTo(WritableStream destination, optional StreamPipeOptions options = {});

  [Throws]
  sequence<ReadableStream> tee();

  [GenerateReturnMethod]
  async iterable<any>(optional ReadableStreamIteratorOptions options = {});
};

enum ReadableStreamReaderMode { "byob" };

dictionary ReadableStreamGetReaderOptions {
  ReadableStreamReaderMode mode;
};

dictionary ReadableStreamIteratorOptions {
  boolean preventCancel = false;
};

dictionary ReadableWritablePair {
  required ReadableStream readable;
  required WritableStream writable;
};

dictionary StreamPipeOptions {
  boolean preventClose = false;
  boolean preventAbort = false;
  boolean preventCancel = false;
  AbortSignal signal;
};

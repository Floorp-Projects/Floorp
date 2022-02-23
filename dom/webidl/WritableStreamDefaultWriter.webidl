/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#default-writer-class-definition
 */

[Exposed=(Window,Worker,Worklet), Pref="dom.streams.writable_streams.enabled"]
interface WritableStreamDefaultWriter {
  [Throws]
  constructor(WritableStream stream);

  readonly attribute Promise<void> closed;
  [Throws] readonly attribute unrestricted double? desiredSize;
  readonly attribute Promise<void> ready;

  [Throws]
  Promise<void> abort(optional any reason);

  [Throws]
  Promise<void> close();

  [Throws]
  void releaseLock();

  [Throws]
  Promise<void> write(optional any chunk);
};

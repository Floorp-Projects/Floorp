/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#default-writer-class-definition
 */

[Exposed=*]
interface WritableStreamDefaultWriter {
  [Throws]
  constructor(WritableStream stream);

  readonly attribute Promise<undefined> closed;
  [Throws] readonly attribute unrestricted double? desiredSize;
  readonly attribute Promise<undefined> ready;

  [Throws]
  Promise<undefined> abort(optional any reason);

  [NewObject]
  Promise<undefined> close();

  undefined releaseLock();

  [NewObject]
  Promise<undefined> write(optional any chunk);
};

/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#ts-class-definition
 */

[Exposed=*,
 //Transferable See Bug 1734240
 Pref="dom.streams.transform_streams.enabled"]
interface TransformStream {
  [Throws]
  constructor(optional object transformer,
              optional QueuingStrategy writableStrategy = {},
              optional QueuingStrategy readableStrategy = {});

  [GetterThrows /* skeleton only */] readonly attribute ReadableStream readable;
  [GetterThrows /* skeleton only */] readonly attribute WritableStream writable;
};

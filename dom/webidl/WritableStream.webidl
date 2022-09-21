/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://streams.spec.whatwg.org/#ws-class-definition
 */

[Exposed=*,
//Transferable See Bug 1734240
Pref="dom.streams.writable_streams.enabled"
]
interface WritableStream {
  [Throws]
  constructor(optional object underlyingSink, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  [Throws]
  Promise<undefined> abort(optional any reason);

  [NewObject]
  Promise<undefined> close();

  [Throws]
  WritableStreamDefaultWriter getWriter();
};

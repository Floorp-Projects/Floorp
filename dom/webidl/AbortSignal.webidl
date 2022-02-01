/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dom.spec.whatwg.org/#abortsignal
 */

[Exposed=(Window,Worker)]
interface AbortSignal : EventTarget {
  [NewObject, Throws] static AbortSignal abort(optional any reason);

  readonly attribute boolean aborted;
  readonly attribute any reason;
  [Throws] void throwIfAborted();

  attribute EventHandler onabort;
};

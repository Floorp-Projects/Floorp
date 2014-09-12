/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback AbortCallback = void ();

[Constructor(PromiseInit init, AbortCallback abortCallback),
 Pref="dom.abortablepromise.enabled"]
interface MozAbortablePromise : _Promise {
  /*
   * Aborts the promise.
   * If the promise has not been resolved or rejected, it should be rejected
   * with an Exception of type abort and then AbortCallback is called
   * asynchronously. Otherwise, nothing should be done.
   */
  void abort();
};

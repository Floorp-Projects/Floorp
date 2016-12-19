/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// A dumping ground for random testing functions

callback PromiseReturner = Promise<any>();

[Pref="dom.expose_test_interfaces"]
interface TestFunctions {
  [Throws]
  static void throwUncatchableException();

  // Simply returns its argument.  Can be used to test Promise
  // argument processing behavior.
  static Promise<any> passThroughPromise(Promise<any> arg);

  // Returns whatever Promise the given PromiseReturner returned.
  [Throws]
  static Promise<any> passThroughCallbackPromise(PromiseReturner callback);
};

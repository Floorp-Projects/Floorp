/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org/#promises
 */

// TODO We use object instead Function.  There is an open issue on WebIDL to
// have different types for "platform-provided function" and "user-provided
// function"; for now, we just use "object".
callback PromiseInit = void (object resolve, object reject);

[TreatNonCallableAsNull]
callback AnyCallback = any (any value);

// REMOVE THE RELEVANT ENTRY FROM test_interfaces.html WHEN THIS IS IMPLEMENTED IN JS.
[Constructor(PromiseInit init)]
interface Promise {
  // TODO bug 875289 - static Promise fulfill(any value);

  // Disable the static methods when the interface object is supposed to be
  // disabled, just in case some code decides to walk over to .constructor from
  // the proto of a promise object or someone screws up and manages to create a
  // Promise object in this scope without having resolved the interface object
  // first.
  [NewObject, Throws]
  static Promise resolve(optional any value);
  [NewObject, Throws]
  static Promise reject(optional any value);

  // The [TreatNonCallableAsNull] annotation is required since then() should do
  // nothing instead of throwing errors when non-callable arguments are passed.
  [NewObject]
  Promise then([TreatNonCallableAsNull] optional AnyCallback? fulfillCallback = null,
               [TreatNonCallableAsNull] optional AnyCallback? rejectCallback = null);

  [NewObject]
  Promise catch([TreatNonCallableAsNull] optional AnyCallback? rejectCallback = null);

  [NewObject, Throws]
  static Promise all(sequence<any> iterable);

  [NewObject, Throws]
  static Promise race(sequence<any> iterable);
};

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
[Constructor(PromiseInit init),
 Exposed=(Window,Worker,System)]
// Need to escape "Promise" so it's treated as an identifier.
interface _Promise {
  // Have to use "any" (or "object", but "any" is simpler) as the type to
  // support the subclassing behavior, since nothing actually requires the
  // return value of PromiseSubclass.resolve/reject to be a Promise object.
  [NewObject, Throws]
  static any resolve(optional any value);
  [NewObject, Throws]
  static any reject(optional any value);

  // The [TreatNonCallableAsNull] annotation is required since then() should do
  // nothing instead of throwing errors when non-callable arguments are passed.
  // Have to use "any" (or "object", but "any" is simpler) as the type to
  // support the subclassing behavior, since nothing actually requires the
  // return value of PromiseSubclass.then/catch to be a Promise object.
  [NewObject, Throws]
  any then([TreatNonCallableAsNull] optional AnyCallback? fulfillCallback = null,
           [TreatNonCallableAsNull] optional AnyCallback? rejectCallback = null);

  [NewObject, Throws]
  any catch([TreatNonCallableAsNull] optional AnyCallback? rejectCallback = null);

  // Have to use "any" (or "object", but "any" is simpler) as the type to
  // support the subclassing behavior, since nothing actually requires the
  // return value of PromiseSubclass.all to be a Promise object.  As a result,
  // we also have to do our argument conversion manually, because we want to
  // convert its exceptions into rejections.
  [NewObject, Throws]
  static any all(optional any iterable);

  // Have to use "any" (or "object", but "any" is simpler) as the type to
  // support the subclassing behavior, since nothing actually requires the
  // return value of PromiseSubclass.race to be a Promise object.  As a result,
  // we also have to do our argument conversion manually, because we want to
  // convert its exceptions into rejections.
  [NewObject, Throws]
  static any race(optional any iterable);
};

/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// A dumping ground for random testing functions

callback PromiseReturner = Promise<any>();

[Pref="dom.expose_test_interfaces"]
interface WrapperCachedNonISupportsTestInterface {
};

[Pref="dom.expose_test_interfaces"]
interface TestFunctions {
  constructor();

  [Throws]
  static void throwUncatchableException();

  // Simply returns its argument.  Can be used to test Promise
  // argument processing behavior.
  static Promise<any> passThroughPromise(Promise<any> arg);

  // Returns whatever Promise the given PromiseReturner returned.
  [Throws]
  static Promise<any> passThroughCallbackPromise(PromiseReturner callback);

  // Some basic tests for string binding round-tripping behavior.
  void setStringData(DOMString arg);

  // Get the string data, using an nsAString argument on the C++ side.
  // This will just use Assign/operator=, whatever that does.
  DOMString getStringDataAsAString();

  // Get the string data, but only "length" chars of it, using an
  // nsAString argument on the C++ side.  This will always copy on the
  // C++ side.
  DOMString getStringDataAsAString(unsigned long length);

  // Get the string data, but only "length" chars of it, using a
  // DOMString argument on the C++ side and trying to hand it
  // stringbuffers.  If length not passed, use our full length.
  DOMString getStringDataAsDOMString(optional unsigned long length);

  // Functions that just punch through to mozITestInterfaceJS.idl
  [Throws]
  void testThrowNsresult();
  [Throws]
  void testThrowNsresultFromNative();

  // Throws an InvalidStateError to auto-create a rejected promise.
  [Throws]
  static Promise<any> throwToRejectPromise();

  // Some attributes for the toJSON to work with.
  readonly attribute long one;
  [Func="mozilla::dom::TestFunctions::ObjectFromAboutBlank"]
  readonly attribute long two;

  // Testing for how default toJSON behaves.
  [Default] object toJSON();

  // This returns a wrappercached non-ISupports object. While this will always
  // return the same object, no optimization attributes like [Pure] should be
  // used here because the object should not be held alive from JS by the
  // bindings. This is needed to test wrapper preservation for weak map keys.
  // See bug 1351501.
  readonly attribute WrapperCachedNonISupportsTestInterface wrapperCachedNonISupportsObject;
};

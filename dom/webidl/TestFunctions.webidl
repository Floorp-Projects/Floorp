/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// A dumping ground for random testing functions

callback PromiseReturner = Promise<any>();
callback PromiseReturner2 = Promise<any>(any arg, DOMString arg2);

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface WrapperCachedNonISupportsTestInterface {
  [Pref="dom.webidl.test1"] constructor();
};

[Trial="TestTrial", Exposed=*]
interface TestTrialInterface {
  constructor();
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceLength {
  [ChromeOnly]
  constructor(boolean arg);
};

// The type of string C++ sees.
enum StringType {
  "literal",      // A string with the LITERAL flag.
  "stringbuffer", // A string with the REFCOUNTED flag.
  "inline",       // A string with the INLINE flag.
  "other",        // Anything else.
};

[Pref="dom.expose_test_interfaces",
 Exposed=Window]
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

  // Get a short (short enough to fit in a JS inline string) literal string.
  DOMString getShortLiteralString();

  // Get a medium (long enough to not be a JS inline, but short enough
  // to fit in a FakeString inline buffer) literal string.
  DOMString getMediumLiteralString();

  // Get a long (long enough to not fit in any inline buffers) literal string.
  DOMString getLongLiteralString();

  // Get a stringbuffer string for whatever string is passed in.
  DOMString getStringbufferString(DOMString input);

  // Get the type of string that the C++ sees after going through bindings.
  StringType getStringType(DOMString str);

  // Returns true if both the incoming string and the stored (via setStringData())
  // string have stringbuffers and they're the same stringbuffer.
  boolean stringbufferMatchesStored(DOMString str);

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

  attribute [Clamp] octet? clampedNullableOctet;
  attribute [EnforceRange] octet? enforcedNullableOctet;

  // Testing for [AllowShared]
  [GetterThrows]
  attribute ArrayBufferView arrayBufferView;
  [GetterThrows]
  attribute [AllowShared] ArrayBufferView allowSharedArrayBufferView;
  [Cached, Pure, GetterThrows]
  attribute sequence<ArrayBufferView> sequenceOfArrayBufferView;
  [Cached, Pure, GetterThrows]
  attribute sequence<[AllowShared] ArrayBufferView> sequenceOfAllowSharedArrayBufferView;
  [GetterThrows]
  attribute ArrayBuffer arrayBuffer;
  [GetterThrows]
  attribute [AllowShared] ArrayBuffer allowSharedArrayBuffer;
  [Cached, Pure, GetterThrows]
  attribute sequence<ArrayBuffer> sequenceOfArrayBuffer;
  [Cached, Pure, GetterThrows]
  attribute sequence<[AllowShared] ArrayBuffer> sequenceOfAllowSharedArrayBuffer;
  void testNotAllowShared(ArrayBufferView buffer);
  void testNotAllowShared(ArrayBuffer buffer);
  void testNotAllowShared(DOMString buffer);
  void testAllowShared([AllowShared] ArrayBufferView buffer);
  void testAllowShared([AllowShared] ArrayBuffer buffer);
  void testDictWithAllowShared(optional DictWithAllowSharedBufferSource buffer = {});
  void testUnionOfBuffferSource((ArrayBuffer or ArrayBufferView or DOMString) foo);
  void testUnionOfAllowSharedBuffferSource(([AllowShared] ArrayBuffer or [AllowShared] ArrayBufferView) foo);
};

dictionary DictWithAllowSharedBufferSource {
  ArrayBuffer arrayBuffer;
  ArrayBufferView arrayBufferView;
  [AllowShared] ArrayBuffer allowSharedArrayBuffer;
  [AllowShared] ArrayBufferView allowSharedArrayBufferView;
};

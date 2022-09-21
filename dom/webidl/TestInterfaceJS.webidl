/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary TestInterfaceJSUnionableDictionary {
  object objectMember;
  any anyMember;
};

[JSImplementation="@mozilla.org/dom/test-interface-js;1",
 Pref="dom.expose_test_interfaces",
 Exposed=Window]
interface TestInterfaceJS : EventTarget {
  [Throws]
  constructor(optional any anyArg, optional object objectArg,
              optional TestInterfaceJSDictionary dictionaryArg = {});

  readonly attribute any anyArg;
  readonly attribute object objectArg;
  TestInterfaceJSDictionary getDictionaryArg();
  attribute any anyAttr;
  attribute object objectAttr;
  TestInterfaceJSDictionary getDictionaryAttr();
  undefined setDictionaryAttr(optional TestInterfaceJSDictionary dict = {});
  any pingPongAny(any arg);
  object pingPongObject(object obj);
  any pingPongObjectOrString((object or DOMString) objOrString);
  TestInterfaceJSDictionary pingPongDictionary(optional TestInterfaceJSDictionary dict = {});
  long pingPongDictionaryOrLong(optional (TestInterfaceJSUnionableDictionary or long) dictOrLong = {});
  DOMString pingPongRecord(record<DOMString, any> rec);
  long objectSequenceLength(sequence<object> seq);
  long anySequenceLength(sequence<any> seq);

  // For testing bug 968335.
  DOMString getCallerPrincipal();

  DOMString convertSVS(USVString svs);

  (TestInterfaceJS or long) pingPongUnion((TestInterfaceJS or long) something);
  (DOMString or TestInterfaceJS?) pingPongUnionContainingNull((TestInterfaceJS? or DOMString) something);
  (TestInterfaceJS or long)? pingPongNullableUnion((TestInterfaceJS or long)? something);
  (Location or TestInterfaceJS) returnBadUnion();

  // Test for sequence overloading and union behavior
  undefined testSequenceOverload(sequence<DOMString> arg);
  undefined testSequenceOverload(DOMString arg);

  undefined testSequenceUnion((sequence<DOMString> or DOMString) arg);

  // Tests for exception-throwing behavior
  [Throws]
  undefined testThrowError();

  [Throws]
  undefined testThrowDOMException();

  [Throws]
  undefined testThrowTypeError();

  [Throws]
  undefined testThrowCallbackError(Function callback);

  [Throws]
  undefined testThrowXraySelfHosted();

  [Throws]
  undefined testThrowSelfHosted();

  // Tests for promise-rejection behavior
  Promise<undefined> testPromiseWithThrowingChromePromiseInit();
  Promise<undefined> testPromiseWithThrowingContentPromiseInit(Function func);
  Promise<undefined> testPromiseWithDOMExceptionThrowingPromiseInit();
  Promise<undefined> testPromiseWithThrowingChromeThenFunction();
  Promise<undefined> testPromiseWithThrowingContentThenFunction(AnyCallback func);
  Promise<undefined> testPromiseWithDOMExceptionThrowingThenFunction();
  Promise<undefined> testPromiseWithThrowingChromeThenable();
  Promise<undefined> testPromiseWithThrowingContentThenable(object thenable);
  Promise<undefined> testPromiseWithDOMExceptionThrowingThenable();

  // Event handler tests
  attribute EventHandler onsomething;
};

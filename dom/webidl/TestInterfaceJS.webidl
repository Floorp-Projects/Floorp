/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[JSImplementation="@mozilla.org/dom/test-interface-js;1",
 Pref="dom.expose_test_interfaces",
 Constructor(optional any anyArg, optional object objectArg)]
interface TestInterfaceJS {
  readonly attribute any anyArg;
  readonly attribute object objectArg;
  attribute any anyAttr;
  attribute object objectAttr;
  any pingPongAny(any arg);
  object pingPongObject(any obj);

  // For testing bug 968335.
  DOMString getCallerPrincipal();

  DOMString convertSVS(ScalarValueString svs);

  (TestInterfaceJS or long) pingPongUnion((TestInterfaceJS or long) something);
  (DOMString or TestInterfaceJS?) pingPongUnionContainingNull((TestInterfaceJS? or DOMString) something);
  (TestInterfaceJS or long)? pingPongNullableUnion((TestInterfaceJS or long)? something);
  (Location or TestInterfaceJS) returnBadUnion();
};

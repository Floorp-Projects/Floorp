/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor, JSImplementation="@mozilla.org/test-js-impl-interface2;1"]
interface TestJSImplInterface2 :  TestCImplementedInterface {
};

[Constructor, JSImplementation="@mozilla.org/test-js-impl-interface3;1"]
interface TestJSImplInterface3 : TestCImplementedInterface2 {
};

// Important: TestJSImplInterface5 needs to come before TestJSImplInterface6 in
// this file to test what it's trying to test.
[Constructor, JSImplementation="@mozilla.org/test-js-impl-interface5;1"]
interface TestJSImplInterface5 : TestJSImplInterface6 {
};

// Important: TestJSImplInterface6 needs to come after TestJSImplInterface3 in
// this file to test what it's trying to test.
[Constructor, JSImplementation="@mozilla.org/test-js-impl-interface6;1"]
interface TestJSImplInterface6 : TestJSImplInterface3 {
};

[Constructor, JSImplementation="@mozilla.org/test-js-impl-interface4;1"]
interface TestJSImplInterface4 : EventTarget {
};

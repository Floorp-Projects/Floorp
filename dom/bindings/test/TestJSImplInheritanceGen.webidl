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

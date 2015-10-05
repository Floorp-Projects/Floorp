/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function assertThrowsSE(code) {
  assertThrows(() => Reflect.parse(code), SyntaxError);
}

if (asyncFunctionsEnabled()) {
    assertThrowsSE("'use strict'; async function eval() {}");
    assertThrowsSE("'use strict'; async function arguments() {}");
    assertThrowsSE("async function a(k = super.prop) { }");
    assertThrowsSE("async function a() { super.prop(); }");
    assertThrowsSE("async function a() { super(); }");

    assertThrowsSE("async function a(k = await 3) {}");
}

if (typeof reportCompare === "function")
    reportCompare(true, true);

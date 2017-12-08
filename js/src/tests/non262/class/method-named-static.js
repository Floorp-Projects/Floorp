/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Instance method named "static", with and without escape sequence.
assertEq((new class {
    static() { return "method-static-no-escape"; }
}).static(), "method-static-no-escape");

assertEq((new class {
    st\u0061tic() { return "method-static-escape"; }
}).static(), "method-static-escape");

// Instance getter named "static", with and without escape sequence.
assertEq((new class {
    get static() { return "getter-static-no-escape"; }
}).static, "getter-static-no-escape");

assertEq((new class {
    get static() { return "getter-static-escape"; }
}).static, "getter-static-escape");

// Static method named "static", with and without escape sequence.
assertEq(class {
    static static() { return "static-method-static-no-escape"; }
}.static(), "static-method-static-no-escape");

assertEq(class {
    static st\u0061tic() { return "static-method-static-escape"; }
}.static(), "static-method-static-escape");

// Static getter named "static", with and without escape sequence.
assertEq(class {
    static get static() { return "static-getter-static-no-escape"; }
}.static, "static-getter-static-no-escape");

assertEq(class {
    static get st\u0061tic() { return "static-getter-static-escape"; }
}.static, "static-getter-static-escape");


// The static modifier itself must not contain any escape sequences.
assertThrowsInstanceOf(() => eval(String.raw`
    class C {
        st\u0061tic m() {}
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    class C {
        st\u0061tic get m() {}
    }
`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");

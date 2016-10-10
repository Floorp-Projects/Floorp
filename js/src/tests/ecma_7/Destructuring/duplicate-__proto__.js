/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring assignment.
var a, b;
({__proto__: a, __proto__: b} = {});
assertEq(a, Object.prototype);
assertEq(b, Object.prototype);

// Destructuring binding with "var".
var {__proto__: a, __proto__: b} = {};
assertEq(a, Object.prototype);
assertEq(b, Object.prototype);

// Destructuring binding with "let".
{
    let {__proto__: a, __proto__: b} = {};
    assertEq(a, Object.prototype);
    assertEq(b, Object.prototype);
}

// Destructuring binding with "const".
{
    const {__proto__: a, __proto__: b} = {};
    assertEq(a, Object.prototype);
    assertEq(b, Object.prototype);
}

// Function parameters.
function f1({__proto__: a, __proto__: b}) {
    assertEq(a, Object.prototype);
    assertEq(b, Object.prototype);
}
f1({});

// Arrow function parameters.
var f2 = ({__proto__: a, __proto__: b}) => {
    assertEq(a, Object.prototype);
    assertEq(b, Object.prototype);
};
f2({});

// Arrow function parameters with defaults (initially parsed as destructuring assignment).
var f3 = ({__proto__: a, __proto__: b} = {}) => {
    assertEq(a, Object.prototype);
    assertEq(b, Object.prototype);
};
f3({});


if (typeof reportCompare === "function")
    reportCompare(0, 0);

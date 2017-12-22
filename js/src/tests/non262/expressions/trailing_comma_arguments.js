/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Trailing comma in Arguments production.

// 12.3 Left-Hand-Side Expressions
// Arguments[Yield]:
//   ()
//   ( ArgumentList[?Yield] )
//   ( ArgumentList[?Yield] , )


function argsLength() {
    return {value: arguments.length};
}
function sum(...rest) {
    return {value: rest.reduce((a, c) => a + c, 0)};
}

function call(f, argList) {
    return eval(`(${f}(${argList})).value`);
}

function newCall(F, argList) {
    return eval(`(new ${F}(${argList})).value`);
}

function superCall(superClass, argList) {
    return eval(`(new class extends ${superClass} {
        constructor() {
            super(${argList});
        }
    }).value`);
}

// Ensure the correct number of arguments is passed.
for (let type of [call, newCall, superCall]) {
    let test = type.bind(null, "argsLength");

    assertEq(test("10, "), 1);
    assertEq(test("10, 20, "), 2);
    assertEq(test("10, 20, 30, "), 3);
    assertEq(test("10, 20, 30, 40, "), 4);

    assertEq(test("...[10, 20], "), 2);
    assertEq(test("...[10, 20], 30, "), 3);
    assertEq(test("...[10, 20], ...[30], "), 3);
}

// Ensure the arguments themselves are passed correctly.
for (let type of [call, newCall, superCall]) {
    let test = type.bind(null, "sum");

    assertEq(test("10, "), 10);
    assertEq(test("10, 20, "), 30);
    assertEq(test("10, 20, 30, "), 60);
    assertEq(test("10, 20, 30, 40, "), 100);

    assertEq(test("...[10, 20], "), 30);
    assertEq(test("...[10, 20], 30, "), 60);
    assertEq(test("...[10, 20], ...[30], "), 60);
}

// Error cases.
for (let type of [call, newCall, superCall]) {
    let test = type.bind(null, "f");

    // Trailing comma in empty arguments list.
    assertThrowsInstanceOf(() => test(","), SyntaxError);

    // Leading comma.
    assertThrowsInstanceOf(() => test(", a"), SyntaxError);
    assertThrowsInstanceOf(() => test(", ...a"), SyntaxError);

    // Multiple trailing comma.
    assertThrowsInstanceOf(() => test("a, , "), SyntaxError);
    assertThrowsInstanceOf(() => test("...a, , "), SyntaxError);

    // Elision.
    assertThrowsInstanceOf(() => test("a, , b"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);

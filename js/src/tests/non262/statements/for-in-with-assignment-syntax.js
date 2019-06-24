/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const validSyntax = [
    "var x",
];

const destructuring = [
    "[]",
    "[,]",
    "[a]",
    "[a = 0]",
    "[...a]",
    "[...[]]",
    "[...[a]]",
    "{}",
    "{p: x}",
    "{p: x = 0}",
    "{x}",
    "{x = 0}",
];

const invalidSyntax = [
    ...destructuring.map(binding => `var ${binding}`),
    "let x",
    ...destructuring.map(binding => `let ${binding}`),
    "const x",
    ...destructuring.map(binding => `const ${binding}`),
    "x",
    ...destructuring.map(binding => `${binding}`),
    "o.p",
    "o[0]",
    "f()",
];

for (let valid of validSyntax) {
    eval(`for (${valid} = 0 in {});`);
    assertThrowsInstanceOf(() => eval(`"use strict"; for (${valid} = 0 in {});`),
                           SyntaxError);
}

for (let invalid of invalidSyntax) {
    assertThrowsInstanceOf(() => eval(`for (${invalid} = 0 in {});`), SyntaxError);
}

// Invalid syntax, needs method context to parse.
assertThrowsInstanceOf(() => eval(`({ m() { for (super.p = 0 in {}); } })`), SyntaxError);
assertThrowsInstanceOf(() => eval(`({ m() { for (super[0] = 0 in {}); } })`), SyntaxError);

// Throws ReferenceError instead of SyntaxError, because we intermingle parsing
// and early error checking.
assertThrowsInstanceOf(() => eval(`for (0 = 0 in {});`), ReferenceError);
assertThrowsInstanceOf(() => eval(`for (i++ = 0 in {});`), ReferenceError);
assertThrowsInstanceOf(() => eval(`for (new F() = 0 in {});`), ReferenceError);
assertThrowsInstanceOf(() => eval(`function f() { for (new.target = 0 in {}); }`), ReferenceError);
assertThrowsInstanceOf(() => eval(`class C extends D { constructor() { for (super() = 0 in {}); } }`), ReferenceError);

// Same as above, only this time don't make it look like we actually parse a for-in statement.
assertThrowsInstanceOf(() => eval(`for (0 = 0 #####`), ReferenceError);
assertThrowsInstanceOf(() => eval(`for (i++ = 0 #####`), ReferenceError);
assertThrowsInstanceOf(() => eval(`for (new F() = 0 #####`), ReferenceError);
assertThrowsInstanceOf(() => eval(`function f() { for (new.target = 0 #####`), ReferenceError);
assertThrowsInstanceOf(() => eval(`class C extends D { constructor() { for (super() = 0 #####`), ReferenceError);


if (typeof reportCompare === "function")
    reportCompare(true, true);

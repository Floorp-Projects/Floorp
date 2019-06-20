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
    Function(`for (${valid} = 0 in {});`);
    assertThrowsInstanceOf(() => Function(`"use strict"; for (${valid} = 0 in {});`),
                           SyntaxError);
}

for (let invalid of invalidSyntax) {
    assertThrowsInstanceOf(() => Function(`for (${invalid} = 0 in {});`), SyntaxError);
}

// Invalid syntax, needs method context to parse.
assertThrowsInstanceOf(() => Function(`({ m() { for (super.p = 0 in {}); } })`), SyntaxError);
assertThrowsInstanceOf(() => Function(`({ m() { for (super[0] = 0 in {}); } })`), SyntaxError);

assertThrowsInstanceOf(() => Function(`for (0 = 0 in {});`), SyntaxError);
assertThrowsInstanceOf(() => Function(`for (i++ = 0 in {});`), SyntaxError);
assertThrowsInstanceOf(() => Function(`for (new F() = 0 in {});`), SyntaxError);
assertThrowsInstanceOf(() => Function(`function f() { for (new.target = 0 in {}); }`), SyntaxError);
assertThrowsInstanceOf(() => Function(`class C extends D { constructor() { for (super() = 0 in {}); } }`), SyntaxError);

// Same as above, only this time don't make it look like we actually parse a for-in statement.
assertThrowsInstanceOf(() => Function(`for (0 = 0 #####`), SyntaxError);
assertThrowsInstanceOf(() => Function(`for (i++ = 0 #####`), SyntaxError);
assertThrowsInstanceOf(() => Function(`for (new F() = 0 #####`), SyntaxError);
assertThrowsInstanceOf(() => Function(`function f() { for (new.target = 0 #####`), SyntaxError);
assertThrowsInstanceOf(() => Function(`class C extends D { constructor() { for (super() = 0 #####`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(true, true);

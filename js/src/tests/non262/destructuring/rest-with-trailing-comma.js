/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const invalidSyntax = [
    "[...r, ]",
    "[a, ...r, ]",
    "[a = 0, ...r, ]",
    "[[], ...r, ]",
    "[[...r,]]",
    "[[...r,], ]",
    "[[...r,], a]",
];

const validSyntax = [
    "[, ]",
    "[a, ]",
    "[[], ]",
];

const destructuringForms = [
    a => `${a} = [];`,
    a => `var ${a} = [];`,
    a => `let ${a} = [];`,
    a => `const ${a} = [];`,
    a => `(${a}) => {};`,
    a => `(${a} = []) => {};`,
    a => `function f(${a}) {}`,
];

for (let invalid of invalidSyntax) {
    for (let fn of destructuringForms) {
        assertThrowsInstanceOf(() => Function(fn(invalid)), SyntaxError);
    }
}

for (let invalid of validSyntax) {
    for (let fn of destructuringForms) {
        Function(fn(invalid));
    }
}


if (typeof reportCompare === "function")
    reportCompare(0, 0);

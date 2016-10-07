/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const yieldInParameters = [
    `(a = yield) => {}`,
    `(a = yield /a/g) => {}`, // Should parse as division, not yield expression with regexp.
    `yield => {};`,
    `(yield) => {};`,
    `(yield = 0) => {};`,
    `([yield]) => {};`,
    `([yield = 0]) => {};`,
    `([...yield]) => {};`,
    `({a: yield}) => {};`,
    `({yield}) => {};`,
    `({yield = 0}) => {};`,
];

const yieldInBody = [
    `() => yield;`,
    `() => yield /a/g;`,
    `() => { var x = yield; }`,
    `() => { var x = yield /a/g; }`,

    `() => { var yield; };`,
    `() => { var yield = 0; };`,
    `() => { var [yield] = []; };`,
    `() => { var [yield = 0] = []; };`,
    `() => { var [...yield] = []; };`,
    `() => { var {a: yield} = {}; };`,
    `() => { var {yield} = {}; };`,
    `() => { var {yield = 0} = {}; };`,

    `() => { let yield; };`,
    `() => { let yield = 0; };`,
    `() => { let [yield] = []; };`,
    `() => { let [yield = 0] = []; };`,
    `() => { let [...yield] = []; };`,
    `() => { let {a: yield} = {}; };`,
    `() => { let {yield} = {}; };`,
    `() => { let {yield = 0} = {}; };`,

    `() => { const yield = 0; };`,
    `() => { const [yield] = []; };`,
    `() => { const [yield = 0] = []; };`,
    `() => { const [...yield] = []; };`,
    `() => { const {a: yield} = {}; };`,
    `() => { const {yield} = {}; };`,
    `() => { const {yield = 0} = {}; };`,
];


// Script context.
for (let test of [...yieldInParameters, ...yieldInBody]) {
    eval(test);
    assertThrowsInstanceOf(() => eval(`"use strict"; ${test}`), SyntaxError);
}

// Function context.
for (let test of [...yieldInParameters, ...yieldInBody]) {
    eval(`function f() { ${test} }`);
    assertThrowsInstanceOf(() => eval(`"use strict"; function f() { ${test} }`), SyntaxError);
}

// Generator context.
for (let test of yieldInParameters) {
    assertThrowsInstanceOf(() => eval(`function* g() { ${test} }`), SyntaxError);
}
for (let test of yieldInBody) {
    eval(`function* g() { ${test} }`);
}
for (let test of [...yieldInParameters, ...yieldInBody]) {
    assertThrowsInstanceOf(() => eval(`"use strict"; function* g() { ${test} }`), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// CoverInitName in arrow parameters.
({a = 1}, {b = 2}, {c = 3}) => {};
({a = 1} = {}, {b = 2}, {c = 3}) => {};
({a = 1} = {}, {b = 2} = {}, {c = 3}) => {};
({a = 1} = {}, {b = 2} = {}, {c = 3} = {}) => {};


// CoverInitName in CoverParenthesizedExpressionAndArrowParameterList,
// but not ArrowParameters.
assertThrowsInstanceOf(() => eval(`
    ({a = 1}, {b = 2}, {c = 3});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    ({a = 1}, {b = 2}, {c = 3} = {});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    ({a = 1}, {b = 2} = {}, {c = 3} = {});
`), SyntaxError);


// CoverInitName nested in array destructuring.
[{a = 0}] = [{}];
var [{a = 0}] = [{}];
{ let [{a = 0}] = [{}]; }
{ const [{a = 0}] = [{}]; }

for ([{a = 0}] of []);
for (var [{a = 0}] of []);
for (let [{a = 0}] of []);
for (const [{a = 0}] of []);

function f([{a = 0}]) {}
var h = ([{a = 0}]) => {};


// CoverInitName nested in rest pattern.
[...[{a = 0}]] = [{}];
var [...[{a = 0}]] = [{}];
{ let [...[{a = 0}]] = [{}]; }
{ const [...[{a = 0}]] = [{}]; }

for ([...[{a = 0}]] of []);
for (var [...[{a = 0}]] of []);
for (let [...[{a = 0}]] of []);
for (const [...[{a = 0}]] of []);

function f([...[{a = 0}]]) {}
var h = ([...[{a = 0}]]) => {};


// CoverInitName nested in object destructuring.
({p: {a = 0}} = {p: {}});
var {p: {a = 0}} = {p: {}};
{ let {p: {a = 0}} = {p: {}}; }
{ const {p: {a = 0}} = {p: {}}; }

for ({p: {a = 0}} of []);
for (var {p: {a = 0}} of []);
for (let {p: {a = 0}} of []);
for (const {p: {a = 0}} of []);

function f({p: {a = 0}}) {}
var h = ({p: {a = 0}}) => {};


if (typeof reportCompare === "function")
    reportCompare(0, 0);

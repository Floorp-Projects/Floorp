/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// CoverInitName in async arrow parameters.
async ({a = 1}) => {};
async ({a = 1}, {b = 2}) => {};
async ({a = 1}, {b = 2}, {c = 3}) => {};
async ({a = 1} = {}, {b = 2}, {c = 3}) => {};
async ({a = 1} = {}, {b = 2} = {}, {c = 3}) => {};
async ({a = 1} = {}, {b = 2} = {}, {c = 3} = {}) => {};

// CoverInitName nested in array destructuring.
async ([{a = 0}]) => {};

// CoverInitName nested in rest pattern.
async ([...[{a = 0}]]) => {};

// CoverInitName nested in object destructuring.
async ({p: {a = 0}}) => {};

// CoverInitName in CoverCallExpressionAndAsyncArrowHead, but not AsyncArrowHead.
assertThrowsInstanceOf(() => eval(`
    NotAsync({a = 1});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    NotAsync({a = 1}, {b = 2}, {c = 3});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    NotAsync({a = 1}, {b = 2}, {c = 3} = {});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    NotAsync({a = 1}, {b = 2} = {}, {c = 3} = {});
`), SyntaxError);

// Starts with "async", but not called from AssignmentExpression.
assertThrowsInstanceOf(() => eval(`
    typeof async({a = 1});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    typeof async({a = 1}, {b = 2}, {c = 3});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    typeof async({a = 1}, {b = 2}, {c = 3} = {});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    typeof async({a = 1}, {b = 2} = {}, {c = 3} = {});
`), SyntaxError);

// CoverInitName in CoverCallExpressionAndAsyncArrowHead, but not AsyncArrowHead.
assertThrowsInstanceOf(() => eval(`
    obj.async({a = 1});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    obj.async({a = 1}, {b = 2}, {c = 3});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    obj.async({a = 1}, {b = 2}, {c = 3} = {});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
    obj.async({a = 1}, {b = 2} = {}, {c = 3} = {});
`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(0, 0);

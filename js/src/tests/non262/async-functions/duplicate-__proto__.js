/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Async arrow function parameters.
async ({__proto__: a, __proto__: b}) => {};

// Async arrow function parameters with defaults (initially parsed as destructuring assignment).
async ({__proto__: a, __proto__: b} = {}) => {};

// Duplicate __proto__ in CoverCallExpressionAndAsyncArrowHead, but not AsyncArrowHead.
assertThrowsInstanceOf(() => eval(`
    NotAsync({__proto__: a, __proto__: b});
`), SyntaxError);

// Starts with "async", but not called from AssignmentExpression.
assertThrowsInstanceOf(() => eval(`
    typeof async({__proto__: a, __proto__: b});
`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(0, 0);

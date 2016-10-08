/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const defaultValue = "default-value";
const unreachable = () => { throw "unreachable"; };

// for-in statement, object destructuring.
var forIn;
for ({forIn = defaultValue} in {"": null});
assertEq(forIn, defaultValue);

forIn = undefined;
String.prototype.forIn = defaultValue;
for ({forIn = unreachable()} in {"": null});
delete String.prototype.forIn;
assertEq(forIn, defaultValue);

// for-in statement, array destructuring.
forIn = undefined;
for ([forIn = defaultValue] in {"": null});
assertEq(forIn, defaultValue);

forIn = undefined;
for ([forIn = unreachable()] in {"ABC": null});
assertEq(forIn, "A");


// for-of statement, object destructuring.
var forOf;
for ({forOf = defaultValue} of [{}]);
assertEq(forOf, defaultValue);

forOf = undefined;
for ({forOf = unreachable()} of [{forOf: defaultValue}]);
assertEq(forOf, defaultValue);

// for-of statement, array destructuring.
forOf = undefined;
for ([forOf = defaultValue] of [[]]);
assertEq(forOf, defaultValue);

forOf = undefined;
for ([forOf = unreachable()] of [[defaultValue]]);
assertEq(forOf, defaultValue);


// for-statement, object destructuring.
assertThrowsInstanceOf(() => eval(`
  for ({invalid = 0};;);
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(true, true);

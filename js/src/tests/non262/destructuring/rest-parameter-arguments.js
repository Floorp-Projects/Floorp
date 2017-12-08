/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure the |arguments| object works as expected when a destructuring rest
// parameter is present.

// |arguments.length| with destructuring rest array.
function argsLengthEmptyRestArray(...[]) {
    return arguments.length;
}
assertEq(argsLengthEmptyRestArray(), 0);
assertEq(argsLengthEmptyRestArray(10), 1);
assertEq(argsLengthEmptyRestArray(10, 20), 2);

function argsLengthRestArray(...[a]) {
    return arguments.length;
}
assertEq(argsLengthRestArray(), 0);
assertEq(argsLengthRestArray(10), 1);
assertEq(argsLengthRestArray(10, 20), 2);

function argsLengthRestArrayWithDefault(...[a = 0]) {
    return arguments.length;
}
assertEq(argsLengthRestArrayWithDefault(), 0);
assertEq(argsLengthRestArrayWithDefault(10), 1);
assertEq(argsLengthRestArrayWithDefault(10, 20), 2);


// |arguments.length| with destructuring rest object.
function argsLengthEmptyRestObject(...{}) {
    return arguments.length;
}
assertEq(argsLengthEmptyRestObject(), 0);
assertEq(argsLengthEmptyRestObject(10), 1);
assertEq(argsLengthEmptyRestObject(10, 20), 2);

function argsLengthRestObject(...{a}) {
    return arguments.length;
}
assertEq(argsLengthRestObject(), 0);
assertEq(argsLengthRestObject(10), 1);
assertEq(argsLengthRestObject(10, 20), 2);

function argsLengthRestObjectWithDefault(...{a = 0}) {
    return arguments.length;
}
assertEq(argsLengthRestObjectWithDefault(), 0);
assertEq(argsLengthRestObjectWithDefault(10), 1);
assertEq(argsLengthRestObjectWithDefault(10, 20), 2);


// |arguments| access with destructuring rest array.
function argsAccessEmptyRestArray(...[]) {
    return arguments[0];
}
assertEq(argsAccessEmptyRestArray(), undefined);
assertEq(argsAccessEmptyRestArray(10), 10);
assertEq(argsAccessEmptyRestArray(10, 20), 10);

function argsAccessRestArray(...[a]) {
    return arguments[0];
}
assertEq(argsAccessRestArray(), undefined);
assertEq(argsAccessRestArray(10), 10);
assertEq(argsAccessRestArray(10, 20), 10);

function argsAccessRestArrayWithDefault(...[a = 0]) {
    return arguments[0];
}
assertEq(argsAccessRestArrayWithDefault(), undefined);
assertEq(argsAccessRestArrayWithDefault(10), 10);
assertEq(argsAccessRestArrayWithDefault(10, 20), 10);


// |arguments| access with destructuring rest object.
function argsAccessEmptyRestObject(...{}) {
    return arguments[0];
}
assertEq(argsAccessEmptyRestObject(), undefined);
assertEq(argsAccessEmptyRestObject(10), 10);
assertEq(argsAccessEmptyRestObject(10, 20), 10);

function argsAccessRestObject(...{a}) {
    return arguments[0];
}
assertEq(argsAccessRestObject(), undefined);
assertEq(argsAccessRestObject(10), 10);
assertEq(argsAccessRestObject(10, 20), 10);

function argsAccessRestObjectWithDefault(...{a = 0}) {
    return arguments[0];
}
assertEq(argsAccessRestObjectWithDefault(), undefined);
assertEq(argsAccessRestObjectWithDefault(10), 10);
assertEq(argsAccessRestObjectWithDefault(10, 20), 10);


if (typeof reportCompare === "function")
    reportCompare(0, 0);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const unreachable = () => { throw "unreachable"; };

// var-initializer expression is executed before for-in expression.
var log = "";
for (var x = (log += "head") in (log += "|expr", null)) unreachable();
assertEq(log, "head|expr");

log = "";
for (var x = (log += "head") in (log += "|expr", {})) unreachable();
assertEq(log, "head|expr");


// for-in expression isn't executed when var-initializer throws exception.
function ExpectedError() {}
assertThrowsInstanceOf(() => {
    var throwErr = () => { throw new ExpectedError(); };
    for (var x = throwErr() in unreachable()) unreachable();
}, ExpectedError);


// Ensure environment operations are performed correctly.
var scope = new Proxy({x: 0}, new Proxy({}, {
    get(t, pk, r) {
        log += pk + "|";
    }
}));

log = "";
with (scope) {
    for (var x = 0 in {}) ;
}
assertEq(log, "has|get|set|getOwnPropertyDescriptor|defineProperty|");

log = "";
with (scope) {
    for (var x = 0 in {p: 0}) ;
}
assertEq(log, "has|get|set|getOwnPropertyDescriptor|defineProperty|".repeat(2));


if (typeof reportCompare === "function")
    reportCompare(true, true);

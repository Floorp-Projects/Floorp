/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 549617;
var summary = 'flat closure debugged via trap while still active';

var expect = "abc";
var actual = expect;

function a(x, y) {
    return function () { return x; };
}

var f = a("abc", 123);
if (this.trap && this.setDebug) {
    setDebug(true);
    trap(f, "try {actual = x} catch (e) {actual = e}");
}
f();

reportCompare(expect, actual, summary);

printStatus("All tests passed!");

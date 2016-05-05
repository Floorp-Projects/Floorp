/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var expect = "pass";
var actual;

/*
 * We hardcode here that a program is limited to 2^20 blocks. Start
 * with 2^19 blocks, then test 2^20 - 1 blocks, finally test the limit.
 */
var s = "{}";
for (var i = 0; i < 21; i++)
    s += s;

try {
    eval(s);
    actual = "pass";
} catch (e) {
    actual = "fail: " + e;
}

assertEq(actual, expect);

s += s.slice(0, -4);

try {
    eval(s);
    actual = "pass";
} catch (e) {
    actual = "fail: " + e;
}

assertEq(actual, expect);

s += "{}";

try {
    eval(s);
    actual = "fail: expected InternalError: program too large";
} catch (e) {
    actual = (e.message == "program too large") ? "pass" : "fail: " + e;
}

assertEq(actual, expect);

reportCompare(0, 0, "ok");

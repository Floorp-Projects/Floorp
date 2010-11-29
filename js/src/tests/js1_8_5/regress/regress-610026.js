/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var expect = "pass";
var actual;

/*
 * We hardcode here that GenerateBlockId limits a program to 2^20 blocks. Start
 * with 2^19 blocks, then test 2^20 - 1 blocks, finally test the limit.
 */
var s = "{}";
for (var i = 0; i < 19; i++)
    s += s;

try {
    eval(s);
    actual = "pass";
} catch (e) {
    actual = "fail: " + e;
}

assertEq(actual, expect);

s += s.slice(0, -2);

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

/* Make 64K blocks in a row, each with two vars, the second one named x. */
s = "{let y, x;}";
for (i = 0; i < 16; i++)
    s += s;

/* Now append code to alias block 0 and botch a JS_NOT_REACHED or get the wrong x. */
s += "var g; { let x = 42; g = function() { return x; }; x = x; }";

try {
    eval(s);
    actual = g();
} catch (e) {
    actual = e;
}
assertEq(actual, 42);

reportCompare(0, 0, "ok");

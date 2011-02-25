/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = true;
var actual = expect;

function f() {
    'use strict';
    for (; false; (0 for each (t in eval("")))) { }
}
fs = "" + f;
try {
    eval("(" + fs + ")");
} catch (e) {
    actual = false;
}

reportCompare(expect, actual, "ok");

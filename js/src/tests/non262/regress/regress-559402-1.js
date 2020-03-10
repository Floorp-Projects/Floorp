/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = "No error";
var actual = expect;

try {
    eval('function foo() { "use strict"; }');
} catch (e) {
    actual = '' + e;
}

reportCompare(expect, actual, "ok");

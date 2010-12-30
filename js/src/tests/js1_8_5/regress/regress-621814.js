/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
var expect = 'pass';
var actual = expect;
function f({"\xF51F": x}) {}
try {
    eval(uneval(f));
} catch (e) {
    actual = '' + e;
}
reportCompare(expect, actual, "");

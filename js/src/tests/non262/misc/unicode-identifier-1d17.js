/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var o = {}
try {
    eval('o.\\u1d17 = 42;');
}
catch (e) {
    assertEq('should not fail', true);
}
assertEq(o['\u1d17'], 42);

if (typeof reportCompare == 'function')
    reportCompare(true, true);

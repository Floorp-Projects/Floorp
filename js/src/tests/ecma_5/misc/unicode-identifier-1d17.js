/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


var BUGNUMBER = 497692;
var summary = 'Javascript does not treat Unicode 1D17 as valid identifier character';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = {}
try {
    eval('o.\\u1D17 = 42');
}
catch (e) {
    assertEq('should not fail', true);
}

assertEq(o['\u1d17'], 42);

if (typeof reportCompare == 'function')
    reportCompare(true, true);

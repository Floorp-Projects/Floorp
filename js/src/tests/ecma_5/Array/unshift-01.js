/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 614070;
var summary = 'Array.prototype.unshift without args';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a = {};
a.length = 4294967296;
assertEq([].unshift.call(a), 0);
assertEq(a.length, 0);

function testGetSet(len, expected) {
    var newlen;
    var a = { get length() { return len; }, set length(v) { newlen = v; } };
    var res = [].unshift.call(a);
    assertEq(res, expected);
    assertEq(newlen, expected);
}

testGetSet(0, 0);
testGetSet(10, 10);
testGetSet("1", 1);
testGetSet(null, 0);
testGetSet(4294967297, 1);
testGetSet(-5, 4294967291);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

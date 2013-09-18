/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 501739;
var summary =
  "String.prototype.relace should zero the .lastIndex when called with a " +
  "global RegExp";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var s = '0x2x4x6x8';

var p1 = /x/g;
p1.lastIndex = 3;
s.replace(p1, '');
assertEq(p1.lastIndex, 0);

var p2 = /x/g;
p2.lastIndex = 3;
var c = 0;
s.replace(p2, function(s) {
    assertEq(p2.lastIndex++, c++);
    return 'y';
});
assertEq(p2.lastIndex, 4);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

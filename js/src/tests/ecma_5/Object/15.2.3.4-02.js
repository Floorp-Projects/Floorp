/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = '15.2.3.4-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 518663;
var summary = 'Object.getOwnPropertyNames: array objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var a, names, expected;

function arraysEqual(a1, a2)
{
  return a1.length === a2.length &&
         a1.every(function(v, i) { return v === a2[i]; });
}


a = [0, 1, 2];

names = Object.getOwnPropertyNames(a).sort();
expected = ["0", "1", "2", "length"].sort();

a = [1, , , 7];
a.p = 2;
Object.defineProperty(a, "q", { value: 42, enumerable: false });

names = Object.getOwnPropertyNames(a).sort();
expected = ["0", "3", "p", "q", "length"].sort();
assertEq(arraysEqual(names, expected), true);


a = [];

names = Object.getOwnPropertyNames(a).sort();
expected = ["length"];
assertEq(arraysEqual(names, expected), true);


/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");

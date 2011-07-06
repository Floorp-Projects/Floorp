// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var gTestfile = 'parse-mega-huge-array.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 667527;
var summary = "JSON.parse should parse arrays of essentially unlimited size";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var str = '[';
for (var i = 0, sz = Math.pow(2, 21); i < sz; i++)
  str += '0,';
str += '0]';

var arr = JSON.parse(str);
assertEq(arr.length, Math.pow(2, 21) + 1);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

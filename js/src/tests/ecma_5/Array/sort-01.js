/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 604971;
var summary = 'array.sort compare-function gets incorrect this';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

[1, 2, 3].sort(function() { "use strict"; assertEq(this, undefined); });

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

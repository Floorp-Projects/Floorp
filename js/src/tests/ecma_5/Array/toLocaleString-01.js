/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 562446;
var summary = 'ES5: Array.prototype.toLocaleString';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o;

o = { length: 2, 0: 7, 1: { toLocaleString: function() { return "baz" } } };
assertEq(Array.prototype.toLocaleString.call(o), "7,baz");

o = {};
assertEq(Array.prototype.toLocaleString.call(o), "");

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");

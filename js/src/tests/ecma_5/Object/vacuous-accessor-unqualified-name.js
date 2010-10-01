/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'vacuous-accessor-unqualified-name.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 560216;
var summary =
  "Using a name referring to a { get: undefined, set: undefined } descriptor " +
  "shouldn't assert";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

Object.defineProperty(this, "x", { set: undefined });
x;

/******************************************************************************/

reportCompare(true, true);

print("All tests passed!");

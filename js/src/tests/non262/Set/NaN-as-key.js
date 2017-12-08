/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 722260;
var summary = 'All NaNs must be treated as identical keys for Set';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

/* Avoid constant-folding that would happen were |undefined| to be used. */
var key = -/a/g.missingProperty;

var s = new Set();
s.add(key, 17);
assertEq(s.has(key), true);
assertEq(s.has(-key), true);
assertEq(s.has(NaN), true);

s.delete(-key);
assertEq(s.has(key), false);
assertEq(s.has(-key), false);
assertEq(s.has(NaN), false);

s.add(-key, 17);
assertEq(s.has(key), true);
assertEq(s.has(-key), true);
assertEq(s.has(NaN), true);

s.delete(NaN);
assertEq(s.has(key), false);
assertEq(s.has(-key), false);
assertEq(s.has(NaN), false);

s.add(NaN, 17);
assertEq(s.has(key), true);
assertEq(s.has(-key), true);
assertEq(s.has(NaN), true);

s.delete(key);
assertEq(s.has(key), false);
assertEq(s.has(-key), false);
assertEq(s.has(NaN), false);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

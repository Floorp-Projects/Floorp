/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

var gTestfile = 'preventExtensions-idempotent.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 599459;
var summary = 'Object.preventExtensions should be idempotent';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj = {};
assertEq(Object.preventExtensions(obj), obj);
assertEq(Object.isExtensible(obj), false);
assertEq(Object.preventExtensions(obj), obj);
assertEq(Object.isExtensible(obj), false);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

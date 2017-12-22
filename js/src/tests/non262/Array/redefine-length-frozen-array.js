/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 866580;
var summary = "Assertion redefining length property of a frozen array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr = Object.freeze([]);
Object.defineProperty(arr, "length", {});

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

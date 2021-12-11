/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1288459;
var summary =
  "Properly implement the spec's distinctions between StatementListItem and " +
  "Statement grammar productions and their uses";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertThrowsInstanceOf(() => Function("a: let x;"), SyntaxError);
assertThrowsInstanceOf(() => Function("b: const y = 3;"), SyntaxError);
assertThrowsInstanceOf(() => Function("c: class z {};"), SyntaxError);

assertThrowsInstanceOf(() => Function("'use strict'; d: function w() {};"), SyntaxError);

// Annex B.3.2 allows this in non-strict mode code.
Function("e: function x() {};");

assertThrowsInstanceOf(() => Function("f: function* y() {}"), SyntaxError);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

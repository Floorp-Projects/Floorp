// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 920479;
var summary =
  "Convert all arguments passed to Function() to string before doing any " +
  "parsing of the to-be-created Function's parameters or body text";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertThrowsValue(() => Function("@", { toString() { throw 42; } }), 42);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

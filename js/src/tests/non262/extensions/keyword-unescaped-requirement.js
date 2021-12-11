/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1204027;
var summary =
  "Escape sequences aren't allowed in bolded grammar tokens (that is, in " +
  "keywords, possibly contextual keywords)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var randomExtensions =
  [
   "for \\u0065ach (var x in []);",
   "for e\\u0061ch (var x in []);",
   "[0 for \\u0065ach (var x in [])]",
   "[0 for e\\u0061ch (var x in [])]",
   "(0 for \\u0065ach (var x in []))",
   "(0 for e\\u0061ch (var x in []))",

   // Soon to be not an extension, maybe...
   "(for (x \\u006ff [1]) x)",
   "(for (x o\\u0066 [1]) x)",
  ];

for (var extension of randomExtensions)
{
  assertThrowsInstanceOf(() => Function(extension), SyntaxError,
                         "bad behavior for: " + extension);
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

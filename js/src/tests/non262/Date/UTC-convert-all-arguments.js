/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 1160356;
var summary =
  "Date.UTC must convert *all* arguments to number, not return NaN early if " +
  "a non-finite argument is encountered";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function expectThrowTypeError(f, i)
{
  try
  {
    f();
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e, 42, "index " + i + ": expected 42, got " + e);
  }
}

var bad =
  { toString: function() { throw 17; }, valueOf: function() { throw 42; } };

var args =
 [
  [bad],

  [NaN, bad],
  [Infinity, bad],
  [1970, bad],

  [1970, NaN, bad],
  [1970, Infinity, bad],
  [1970, 4, bad],

  [1970, 4, NaN, bad],
  [1970, 4, Infinity, bad],
  [1970, 4, 17, bad],

  [1970, 4, 17, NaN, bad],
  [1970, 4, 17, Infinity, bad],
  [1970, 4, 17, 13, bad],

  [1970, 4, 17, 13, NaN, bad],
  [1970, 4, 17, 13, Infinity, bad],
  [1970, 4, 17, 13, 37, bad],

  [1970, 4, 17, 13, 37, NaN, bad],
  [1970, 4, 17, 13, 37, Infinity, bad],
  [1970, 4, 17, 13, 37, 23, bad],
 ];

for (var i = 0, len = args.length; i < len; i++)
  expectThrowTypeError(function() { Date.UTC.apply(null, args[i]); }, i);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

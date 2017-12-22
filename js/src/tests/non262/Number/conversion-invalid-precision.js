/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommonn.org/licenses/publicdomain/
 */

var BUGNUMBER = 795745;
var summary =
  "Number.prototype.to* should throw a RangeError when passed a bad precision";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function test(method, prec)
{
  try
  {
    Number.prototype[method].call(0, prec);
    throw "should have thrown";
  }
  catch (e)
  {
    assertEq(e instanceof RangeError, true,
             "expected RangeError for " + method + " with precision " + prec +
             ", got " + e);
  }
}

test("toExponential", -32);
test("toFixed", -32);
test("toPrecision", -32);

test("toExponential", 9999999);
test("toFixed", 9999999);
test("toPrecision", 9999999);

test("toPrecision", 0);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1299519;
var summary =
  "Generator comprehension lambdas in derived constructors shouldn't assert";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

class Base {};

class Derived extends Base
{
  constructor() {
    var a = (for (_ of []) _);
    var b = [for (_ of []) _];
  }
};

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

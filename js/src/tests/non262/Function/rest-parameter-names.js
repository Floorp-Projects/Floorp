// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1288460;
var summary =
  "Rest parameters to functions can be named |yield| or |eval| or |let| in "
  "non-strict code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var f1 = (...yield) => yield + 42;
assertEq(f1(), "42");
assertEq(f1(1), "142");

var f2 = (...eval) => eval + 42;
assertEq(f2(), "42");
assertEq(f2(1), "142");

var f3 = (...let) => let + 42;
assertEq(f3(), "42");
assertEq(f3(1), "142");

function g1(x, ...yield)
{
  return yield + x;
}
assertEq(g1(0, 42), "420");

function g2(x, ...eval)
{
  return eval + x;
}
assertEq(g2(0, 42), "420");

function g3(x, ...let)
{
  return let + x;
}
assertEq(g3(0, 42), "420");

function h()
{
  "use strict";

  var badNames = ["yield", "eval", "let"];

  for (var badName of ["yield", "eval", "let"])
  {
    assertThrowsInstanceOf(() => eval(`var q = (...${badName}) => ${badName} + 42;`),
                           SyntaxError);

    assertThrowsInstanceOf(() => eval(`function r(x, ...${badName}) { return x + ${badName}; }`),
                           SyntaxError);
  }
}
h();

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

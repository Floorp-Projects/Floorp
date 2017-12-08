// |reftest| skip-if(Android)
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Christian Holler <decoder@own-hero.net>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 623301;
var summary = "Properly root argument names during Function()";
print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

if (typeof gczeal === "function")
  gczeal(2);

function crashMe(n)
{
  var nasty = [];
  while (n--)
    nasty.push("a" + n);
  return Function.apply(null, nasty);
}

var count = 64; // exact value not important
assertEq(crashMe(count + 1).length, count);

if (typeof gczeal === "function")
    gczeal(0); // reset

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

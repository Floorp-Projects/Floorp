// |reftest| skip
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 600128;
var summary =
  "Properly handle attempted addition of properties to non-extensible objects";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = Object.freeze({});
for (var i = 0; i < 10; i++)
  print(o.u = "");

Object.freeze(this);
for (let j = 0; j < 10; j++)
  print(u = "");


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

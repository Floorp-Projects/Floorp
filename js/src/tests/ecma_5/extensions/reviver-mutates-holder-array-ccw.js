/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 901351;
var summary = "Behavior when the JSON.parse reviver mutates the holder array";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

if (typeof newGlobal !== "function")
{
  var newGlobal = function()
  {
    return { evaluate: eval };
  };
}

var proxyObj = null;

var arr = JSON.parse('[0, 1]', function(prop, v) {
  if (prop === "0")
  {
    proxyObj = newGlobal().evaluate("({ c: 17, d: 42 })");
    this[1] = proxyObj;
  }
  return v;
});

assertEq(arr[0], 0);
assertEq(arr[1], proxyObj);
assertEq(arr[1].c, 17);
assertEq(arr[1].d, 42);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

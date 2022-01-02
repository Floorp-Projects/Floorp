/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Jeff Walden <jwalden+code@mit.edu>
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 901380;
var summary = "Behavior when JSON.parse walks over a non-native object";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// A little trickiness to account for the undefined-ness of property
// enumeration order.
var first = "unset";

var observedTypedArrayElementCount = 0;

var typedArray = null;

var obj = JSON.parse('{ "a": 0, "b": 1 }', function(prop, v) {
  if (first === "unset")
  {
    first = prop;
    var second = (prop === "a") ? "b" : "a";
    typedArray = new Int8Array(1);
    Object.defineProperty(this, second, { value: typedArray });
  }
  if (this instanceof Int8Array)
  {
    assertEq(prop, "0");
    observedTypedArrayElementCount++;
  }
  return v;
});

if (first === "a")
{
  assertEq(obj.a, 0);
  assertEq(obj.b, typedArray);
}
else
{
  assertEq(obj.a, typedArray);
  assertEq(obj.b, 1);
}

assertEq(observedTypedArrayElementCount, 1);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");

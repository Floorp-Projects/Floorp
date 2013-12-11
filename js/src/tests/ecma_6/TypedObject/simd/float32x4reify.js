// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 938728;
var summary = 'float32x4 reify';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var float32x4 = TypedObject.float32x4;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = new ArrayType(float32x4, 3);
  var array = new Array([float32x4(1, 2, 3, 4),
                         float32x4(5, 6, 7, 8),
                         float32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of float32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.w, 8);
  assertEq(array[1].w, 8);
  array[1] = float32x4(15, 16, 17, 18);
  assertEq(f.w, 8);
  assertEq(array[1].w, 18);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

test();

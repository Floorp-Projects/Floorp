// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 938728;
var summary = 'int32x4 reify';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var int32x4 = TypedObject.int32x4;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = int32x4.array(3);
  var array = new Array([int32x4(1, 2, 3, 4),
                         int32x4(5, 6, 7, 8),
                         int32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of int32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.w, 8);
  assertEq(array[1].w, 8);
  array[1] = int32x4(15, 16, 17, 18);
  assertEq(f.w, 8);
  assertEq(array[1].w, 18);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

test();

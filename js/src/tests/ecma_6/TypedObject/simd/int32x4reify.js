// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 938728;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var summary = 'int32x4 reify';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;

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
}

test();

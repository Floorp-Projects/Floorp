// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 938728;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var summary = 'int32x4 setting';

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
  assertEq(array[1].w, 8);

  // Test that we are allowed to write int32x4 values into array,
  // but not other things.

  array[1] = int32x4(15, 16, 17, 18);
  assertEq(array[1].w, 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting int32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting int32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting int32x4 from a number");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

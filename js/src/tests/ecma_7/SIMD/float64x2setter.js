// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;

var summary = 'float64x2 setter';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var ArrayType = TypedObject.ArrayType;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = float64x2.array(3);
  var array = new Array([float64x2(1, 2),
                         float64x2(3, 4),
                         float64x2(5, 6)]);
  assertEq(array[1].y, 4);

  // Test that we are allowed to write float64x2 values into array,
  // but not other things.

  array[1] = float64x2(7, 8);
  assertEq(array[1].y, 8);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 7, y: 8 };
  }, TypeError, "Setting float64x2 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [ 7, 8 ];
  }, TypeError, "Setting float64x2 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 9;
  }, TypeError, "Setting float64x2 from a number");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

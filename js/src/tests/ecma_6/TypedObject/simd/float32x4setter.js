// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 938728;
var summary = 'float32x4 setting';

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
  assertEq(array[1].w, 8);

  // Test that we are allowed to write float32x4 values into array,
  // but not other things.

  array[1] = float32x4(15, 16, 17, 18);
  assertEq(array[1].w, 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting float32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting float32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting float32x4 from a number");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

test();

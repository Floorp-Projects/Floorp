// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 handles';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var ArrayType = TypedObject.ArrayType;

var float64 = TypedObject.float64;
var Handle = TypedObject.Handle;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = float64x2.array(3);
  var array = new Array([float64x2(1, 2),
                         float64x2(3, 4),
                         float64x2(5, 6)]);

  // Test that trying to create handle into the interior of a
  // float64x2 fails.

  assertThrowsInstanceOf(function() {
    var h = float64.handle(array, 1, "w");
  }, TypeError, "Creating a float64 handle to prop via ctor");

  assertThrowsInstanceOf(function() {
    var h = float64.handle();
    Handle.move(h, array, 1, "w");
  }, TypeError, "Creating a float64 handle to prop via move");

  assertThrowsInstanceOf(function() {
    var h = float64.handle(array, 1, 0);
  }, TypeError, "Creating a float64 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = float64.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a float64 handle to elem via move");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

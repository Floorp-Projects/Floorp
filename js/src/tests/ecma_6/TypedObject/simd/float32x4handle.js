// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 938728;
var summary = 'float32x4 handles';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var float32x4 = TypedObject.float32x4;
var float32 = TypedObject.float32;
var Handle = TypedObject.Handle;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = new ArrayType(float32x4, 3);
  var array = new Array([float32x4(1, 2, 3, 4),
                         float32x4(5, 6, 7, 8),
                         float32x4(9, 10, 11, 12)]);

  // Test that trying to create handle into the interior of a
  // float32x4 fails.

  assertThrowsInstanceOf(function() {
    var h = float32.handle(array, 1, "w");
  }, TypeError, "Creating a float32 handle to prop via ctor");

  assertThrowsInstanceOf(function() {
    var h = float32.handle();
    Handle.move(h, array, 1, "w");
  }, TypeError, "Creating a float32 handle to prop via move");

  assertThrowsInstanceOf(function() {
    var h = float32.handle(array, 1, 0);
  }, TypeError, "Creating a float32 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = float32.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a float32 handle to elem via move");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

test();

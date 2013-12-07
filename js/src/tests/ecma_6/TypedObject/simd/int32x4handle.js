// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 938728;
var summary = 'int32x4 handles';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var ArrayType = TypedObject.ArrayType;
var int32x4 = TypedObject.int32x4;
var int32 = TypedObject.int32;
var Handle = TypedObject.Handle;

function test() {
  print(BUGNUMBER + ": " + summary);

  var Array = int32x4.array(3);
  var array = new Array([int32x4(1, 2, 3, 4),
                         int32x4(5, 6, 7, 8),
                         int32x4(9, 10, 11, 12)]);

  // Test that trying to create handle into the interior of a
  // int32x4 fails.

  assertThrowsInstanceOf(function() {
    var h = int32.handle(array, 1, "w");
  }, TypeError, "Creating a int32 handle to prop via ctor");

  assertThrowsInstanceOf(function() {
    var h = int32.handle();
    Handle.move(h, array, 1, "w");
  }, TypeError, "Creating a int32 handle to prop via move");

  assertThrowsInstanceOf(function() {
    var h = int32.handle(array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int32.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via move");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

test();

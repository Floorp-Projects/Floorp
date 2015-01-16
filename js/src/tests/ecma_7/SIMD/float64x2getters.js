// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 getters';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  // Create a float64x2 and check that the getters work:
  var f = float64x2(11, 22);
  assertEq(f.x, 11);
  assertEq(f.y, 22);

  // Test that the getters work when called reflectively:
  var g = f.__lookupGetter__("x");
  assertEq(g.call(f), 11);

  // Test that getters cannot be applied to various incorrect things:
  assertThrowsInstanceOf(function() {
    g.call({})
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF)
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new TypedObject.StructType({x: TypedObject.float64,
                                        y: TypedObject.float64});
    var v = new T({x: 11, y: 22});
    g.call(v)
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new int32x4(1, 2, 3, 4);
    g.call(t)
  }, TypeError, "Getter applicable to int32x4");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 938728;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var {StructType, int32} = TypedObject;
var summary = 'int32x4 getters';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  // Create a int32x4 and check that the getters work:
  var f = int32x4(11, 22, 33, 44);
  assertEq(f.x, 11);
  assertEq(f.y, 22);
  assertEq(f.z, 33);
  assertEq(f.w, 44);

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
    var T = new StructType({x: int32, y: int32, z: int32, w: int32});
    var v = new T({x: 11, y: 22, z: 33, w: 44});
    g.call(v)
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new float32x4(1, 2, 3, 4);
    g.call(t)
  }, TypeError, "Getter applicable to float32x4");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

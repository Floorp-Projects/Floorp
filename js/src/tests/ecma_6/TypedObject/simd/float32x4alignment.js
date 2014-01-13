// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 938728;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var summary = 'float32x4 alignment';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;

function test() {
  print(BUGNUMBER + ": " + summary);

  assertEq(float32x4.byteLength, 16);
  assertEq(float32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: float32x4});
  assertEq(Compound.fieldOffsets["c"], 0);
  assertEq(Compound.fieldOffsets["d"], 1);
  assertEq(Compound.fieldOffsets["f"], 16);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;

var summary = 'float64x2 alignment';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;

function test() {
  print(BUGNUMBER + ": " + summary);

  assertEq(float64x2.byteLength, 16);
  assertEq(float64x2.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: float64x2});
  assertEq(Compound.fieldOffsets["c"], 0);
  assertEq(Compound.fieldOffsets["d"], 1);
  assertEq(Compound.fieldOffsets["f"], 16);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

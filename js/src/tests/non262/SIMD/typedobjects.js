// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;
var Bool64x2 = SIMD.Bool64x2;

var {StructType, Handle} = TypedObject;
var {float32, float64, int8, int16, int32, uint8} = TypedObject;

function testFloat32x4Alignment() {
  assertEq(Float32x4.byteLength, 16);
  assertEq(Float32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Float32x4});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testFloat32x4Getters() {
  var f = Float32x4(11, 22, 33, 44);
  assertEq(Float32x4.extractLane(f, 0), 11);
  assertEq(Float32x4.extractLane(f, 1), 22);
  assertEq(Float32x4.extractLane(f, 2), 33);
  assertEq(Float32x4.extractLane(f, 3), 44);

  assertThrowsInstanceOf(() => Float32x4.extractLane(f, 4), RangeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(Int32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, f), TypeError);
}

function testFloat32x4Handles() {
  var Array = Float32x4.array(3);
  var array = new Array([Float32x4(1, 2, 3, 4),
                         Float32x4(5, 6, 7, 8),
                         Float32x4(9, 10, 11, 12)]);

  // Test that trying to create handle into the interior of a
  // Float32x4 fails.
  assertThrowsInstanceOf(function() {
    var h = float32.handle(array, 1, 0);
  }, TypeError, "Creating a float32 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = float32.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a float32 handle to elem via move");
}

function testFloat32x4Reify() {
  var Array = Float32x4.array(3);
  var array = new Array([Float32x4(1, 2, 3, 4),
                         Float32x4(5, 6, 7, 8),
                         Float32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of Float32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(Float32x4.extractLane(f, 3), 8);
  assertEq(Float32x4.extractLane(array[1], 3), 8);
  array[1] = Float32x4(15, 16, 17, 18);
  assertEq(Float32x4.extractLane(f, 3), 8);
  assertEq(Float32x4.extractLane(array[1], 3), 18);
}

function testFloat32x4Setters() {
  var Array = Float32x4.array(3);
  var array = new Array([Float32x4(1, 2, 3, 4),
                         Float32x4(5, 6, 7, 8),
                         Float32x4(9, 10, 11, 12)]);
  assertEq(Float32x4.extractLane(array[1], 3), 8);

  // Test that we are allowed to write Float32x4 values into array,
  // but not other things.

  array[1] = Float32x4(15, 16, 17, 18);
  assertEq(Float32x4.extractLane(array[1], 3), 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting Float32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting Float32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting Float32x4 from a number");
}

function testFloat64x2Alignment() {
  assertEq(Float64x2.byteLength, 16);
  assertEq(Float64x2.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Float64x2});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testFloat64x2Getters() {
  // Create a Float64x2 and check that the getters work:
  var f = Float64x2(11, 22);
  assertEq(Float64x2.extractLane(f, 0), 11);
  assertEq(Float64x2.extractLane(f, 1), 22);

  assertThrowsInstanceOf(() => Float64x2.extractLane(f, 2), RangeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(Float32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, f), TypeError);
}

function testFloat64x2Handles() {
  var Array = Float64x2.array(3);
  var array = new Array([Float64x2(1, 2),
                         Float64x2(3, 4),
                         Float64x2(5, 6)]);

  // Test that trying to create handle into the interior of a
  // Float64x2 fails.
  assertThrowsInstanceOf(function() {
    var h = float64.handle(array, 1, 0);
  }, TypeError, "Creating a float64 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = float64.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a float64 handle to elem via move");
}

function testFloat64x2Reify() {
  var Array = Float64x2.array(3);
  var array = new Array([Float64x2(1, 2),
                         Float64x2(3, 4),
                         Float64x2(5, 6)]);

  // Test that reading array[1] produces a *copy* of Float64x2, not an
  // alias into the array.

  var f = array[1];
  assertEq(Float64x2.extractLane(f, 1), 4);
  assertEq(Float64x2.extractLane(array[1], 1), 4);
  array[1] = Float64x2(7, 8);
  assertEq(Float64x2.extractLane(f, 1), 4);
  assertEq(Float64x2.extractLane(array[1], 1), 8);
}

function testFloat64x2Setters() {
  var Array = Float64x2.array(3);
  var array = new Array([Float64x2(1, 2),
                         Float64x2(3, 4),
                         Float64x2(5, 6)]);
  assertEq(Float64x2.extractLane(array[1], 1), 4);

  // Test that we are allowed to write Float64x2 values into array,
  // but not other things.

  array[1] = Float64x2(7, 8);
  assertEq(Float64x2.extractLane(array[1], 1), 8);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 7, y: 8 };
  }, TypeError, "Setting Float64x2 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [ 7, 8 ];
  }, TypeError, "Setting Float64x2 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 9;
  }, TypeError, "Setting Float64x2 from a number");
}

function testInt8x16Alignment() {
  assertEq(Int8x16.byteLength, 16);
  assertEq(Int8x16.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Int8x16});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt8x16Getters() {
  // Create a Int8x16 and check that the getters work:
  var f = Int8x16(11, 22, 33, 44, 55, 66, 77, 88, 99, 10, 20, 30, 40, 50, 60, 70);
  assertEq(Int8x16.extractLane(f, 0), 11);
  assertEq(Int8x16.extractLane(f, 1), 22);
  assertEq(Int8x16.extractLane(f, 2), 33);
  assertEq(Int8x16.extractLane(f, 3), 44);
  assertEq(Int8x16.extractLane(f, 4), 55);
  assertEq(Int8x16.extractLane(f, 5), 66);
  assertEq(Int8x16.extractLane(f, 6), 77);
  assertEq(Int8x16.extractLane(f, 7), 88);
  assertEq(Int8x16.extractLane(f, 8), 99);
  assertEq(Int8x16.extractLane(f, 9), 10);
  assertEq(Int8x16.extractLane(f, 10), 20);
  assertEq(Int8x16.extractLane(f, 11), 30);
  assertEq(Int8x16.extractLane(f, 12), 40);
  assertEq(Int8x16.extractLane(f, 13), 50);
  assertEq(Int8x16.extractLane(f, 14), 60);
  assertEq(Int8x16.extractLane(f, 15), 70);

  assertThrowsInstanceOf(() => Int8x16.extractLane(f, 16), RangeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(Int32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, f), TypeError);
}

function testInt8x16Handles() {
  var Array = Int8x16.array(3);
  var array = new Array([Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that trying to create handle into the interior of a
  // Int8x16 fails.
  assertThrowsInstanceOf(function() {
    var h = int8.handle(array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int8.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via move");
}

function testInt8x16Reify() {
  var Array = Int8x16.array(3);
  var array = new Array([Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that reading array[1] produces a *copy* of Int8x16, not an
  // alias into the array.

  var f = array[1];

  var sj1 = Int8x16.extractLane(f, 3);

  assertEq(sj1, 20);
  assertEq(Int8x16.extractLane(array[1], 3), 20);
  array[1] = Int8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(Int8x16.extractLane(f, 3), 20);
  assertEq(Int8x16.extractLane(array[1], 3), 52);
}

function testInt8x16Setters() {
  var Array = Int8x16.array(3);
  var array = new Array([Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);
  assertEq(Int8x16.extractLane(array[1], 3), 20);

  // Test that we are allowed to write Int8x16 values into array,
  // but not other things.

  array[1] = Int8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(Int8x16.extractLane(array[1], 3), 52);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 49, s1: 50, s2: 51, s3: 52, s4: 53, s5: 54, s6: 55, s7: 56,
                s8: 57, s9: 58, s10: 59, s11: 60, s12: 61, s13: 62, s14: 63, s15: 64};
  }, TypeError, "Setting Int8x16 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64];
  }, TypeError, "Setting Int8x16 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 52;
  }, TypeError, "Setting Int8x16 from a number");
}

function testInt16x8Alignment() {
  assertEq(Int16x8.byteLength, 16);
  assertEq(Int16x8.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Int16x8});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt16x8Getters() {
  // Create a Int16x8 and check that the getters work:
  var f = Int16x8(11, 22, 33, 44, 55, 66, 77, 88);
  assertEq(Int16x8.extractLane(f, 0), 11);
  assertEq(Int16x8.extractLane(f, 1), 22);
  assertEq(Int16x8.extractLane(f, 2), 33);
  assertEq(Int16x8.extractLane(f, 3), 44);
  assertEq(Int16x8.extractLane(f, 4), 55);
  assertEq(Int16x8.extractLane(f, 5), 66);
  assertEq(Int16x8.extractLane(f, 6), 77);
  assertEq(Int16x8.extractLane(f, 7), 88);

  assertThrowsInstanceOf(() => Int16x8.extractLane(f, 8), RangeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(Int32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, f), TypeError);
}

function testInt16x8Handles() {
  var Array = Int16x8.array(3);
  var array = new Array([Int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that trying to create handle into the interior of a
  // Int16x8 fails.
  assertThrowsInstanceOf(function() {
    var h = int16.handle(array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int16.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via move");
}

function testInt16x8Reify() {
  var Array = Int16x8.array(3);
  var array = new Array([Int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that reading array[1] produces a *copy* of Int16x8, not an
  // alias into the array.

  var f = array[1];
  assertEq(Int16x8.extractLane(f, 3), 12);
  assertEq(Int16x8.extractLane(array[1], 3), 12);
  array[1] = Int16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(Int16x8.extractLane(f, 3), 12);
  assertEq(Int16x8.extractLane(array[1], 3), 28);
}

function testInt16x8Setters() {
  var Array = Int16x8.array(3);
  var array = new Array([Int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);
  assertEq(Int16x8.extractLane(array[1], 3), 12);

  // Test that we are allowed to write Int16x8 values into array,
  // but not other things.

  array[1] = Int16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(Int16x8.extractLane(array[1], 3), 28);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 25, s1: 26, s2: 27, s3: 28, s4: 29, s5: 30, s6: 31, s7: 32};
  }, TypeError, "Setting Int16x8 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [25, 26, 27, 28, 29, 30, 31, 32];
  }, TypeError, "Setting Int16x8 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 28;
  }, TypeError, "Setting Int16x8 from a number");
}

function testInt32x4Alignment() {
  assertEq(Int32x4.byteLength, 16);
  assertEq(Int32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Int32x4});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt32x4Getters() {
  // Create a Int32x4 and check that the getters work:
  var f = Int32x4(11, 22, 33, 44);
  assertEq(Int32x4.extractLane(f, 0), 11);
  assertEq(Int32x4.extractLane(f, 1), 22);
  assertEq(Int32x4.extractLane(f, 2), 33);
  assertEq(Int32x4.extractLane(f, 3), 44);

  assertThrowsInstanceOf(() => Int32x4.extractLane(f, 4), RangeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(Float32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, f), TypeError);
}

function testInt32x4Handles() {
  var Array = Int32x4.array(3);
  var array = new Array([Int32x4(1, 2, 3, 4),
                         Int32x4(5, 6, 7, 8),
                         Int32x4(9, 10, 11, 12)]);

  // Test that trying to create handle into the interior of a
  // Int32x4 fails.
  assertThrowsInstanceOf(function() {
    var h = int32.handle(array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int32.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via move");
}

function testInt32x4Reify() {
  var Array = Int32x4.array(3);
  var array = new Array([Int32x4(1, 2, 3, 4),
                         Int32x4(5, 6, 7, 8),
                         Int32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of Int32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(Int32x4.extractLane(f, 3), 8);
  assertEq(Int32x4.extractLane(array[1], 3), 8);
  array[1] = Int32x4(15, 16, 17, 18);
  assertEq(Int32x4.extractLane(f, 3), 8);
  assertEq(Int32x4.extractLane(array[1], 3), 18);
}

function testInt32x4Setters() {
  var Array = Int32x4.array(3);
  var array = new Array([Int32x4(1, 2, 3, 4),
                         Int32x4(5, 6, 7, 8),
                         Int32x4(9, 10, 11, 12)]);
  assertEq(Int32x4.extractLane(array[1], 3), 8);

  // Test that we are allowed to write Int32x4 values into array,
  // but not other things.
  array[1] = Int32x4(15, 16, 17, 18);
  assertEq(Int32x4.extractLane(array[1], 3), 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting Int32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting Int32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting Int32x4 from a number");
}

function testUint8x16Alignment() {
  assertEq(Uint8x16.byteLength, 16);
  assertEq(Uint8x16.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Uint8x16});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testUint8x16Getters() {
  // Create a Uint8x16 and check that the getters work:
  var f = Uint8x16(11, 22, 33, 44, 55, 66, 77, 88, 99, 10, 20, 30, 40, 50, 60, 70);
  assertEq(Uint8x16.extractLane(f, 0), 11);
  assertEq(Uint8x16.extractLane(f, 1), 22);
  assertEq(Uint8x16.extractLane(f, 2), 33);
  assertEq(Uint8x16.extractLane(f, 3), 44);
  assertEq(Uint8x16.extractLane(f, 4), 55);
  assertEq(Uint8x16.extractLane(f, 5), 66);
  assertEq(Uint8x16.extractLane(f, 6), 77);
  assertEq(Uint8x16.extractLane(f, 7), 88);
  assertEq(Uint8x16.extractLane(f, 8), 99);
  assertEq(Uint8x16.extractLane(f, 9), 10);
  assertEq(Uint8x16.extractLane(f, 10), 20);
  assertEq(Uint8x16.extractLane(f, 11), 30);
  assertEq(Uint8x16.extractLane(f, 12), 40);
  assertEq(Uint8x16.extractLane(f, 13), 50);
  assertEq(Uint8x16.extractLane(f, 14), 60);
  assertEq(Uint8x16.extractLane(f, 15), 70);

  assertThrowsInstanceOf(() => Uint8x16.extractLane(f, 16), RangeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(Uint32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Uint8x16.extractLane(f, f), TypeError);
}

function testUint8x16Handles() {
  var Array = Uint8x16.array(3);
  var array = new Array([Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Uint8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Uint8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that trying to create handle into the interior of a
  // Uint8x16 fails.
  assertThrowsInstanceOf(function() {
    var h = int8.handle(array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int8.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via move");
}

function testUint8x16Reify() {
  var Array = Uint8x16.array(3);
  var array = new Array([Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Uint8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Uint8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that reading array[1] produces a *copy* of Uint8x16, not an
  // alias into the array.

  var f = array[1];

  var sj1 = Uint8x16.extractLane(f, 3);

  assertEq(sj1, 20);
  assertEq(Uint8x16.extractLane(array[1], 3), 20);
  array[1] = Uint8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(Uint8x16.extractLane(f, 3), 20);
  assertEq(Uint8x16.extractLane(array[1], 3), 52);
}

function testUint8x16Setters() {
  var Array = Uint8x16.array(3);
  var array = new Array([Uint8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         Uint8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         Uint8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);
  assertEq(Uint8x16.extractLane(array[1], 3), 20);

  // Test that we are allowed to write Uint8x16 values into array,
  // but not other things.

  array[1] = Uint8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(Uint8x16.extractLane(array[1], 3), 52);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 49, s1: 50, s2: 51, s3: 52, s4: 53, s5: 54, s6: 55, s7: 56,
                s8: 57, s9: 58, s10: 59, s11: 60, s12: 61, s13: 62, s14: 63, s15: 64};
  }, TypeError, "Setting Uint8x16 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64];
  }, TypeError, "Setting Uint8x16 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 52;
  }, TypeError, "Setting Uint8x16 from a number");
}

function testUint16x8Alignment() {
  assertEq(Uint16x8.byteLength, 16);
  assertEq(Uint16x8.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Uint16x8});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testUint16x8Getters() {
  // Create a Uint16x8 and check that the getters work:
  var f = Uint16x8(11, 22, 33, 44, 55, 66, 77, 88);
  assertEq(Uint16x8.extractLane(f, 0), 11);
  assertEq(Uint16x8.extractLane(f, 1), 22);
  assertEq(Uint16x8.extractLane(f, 2), 33);
  assertEq(Uint16x8.extractLane(f, 3), 44);
  assertEq(Uint16x8.extractLane(f, 4), 55);
  assertEq(Uint16x8.extractLane(f, 5), 66);
  assertEq(Uint16x8.extractLane(f, 6), 77);
  assertEq(Uint16x8.extractLane(f, 7), 88);

  assertThrowsInstanceOf(() => Uint16x8.extractLane(f, 8), RangeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(Uint32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Uint16x8.extractLane(f, f), TypeError);
}

function testUint16x8Handles() {
  var Array = Uint16x8.array(3);
  var array = new Array([Uint16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Uint16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Uint16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that trying to create handle into the interior of a
  // Uint16x8 fails.
  assertThrowsInstanceOf(function() {
    var h = int16.handle(array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int16.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via move");
}

function testUint16x8Reify() {
  var Array = Uint16x8.array(3);
  var array = new Array([Uint16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Uint16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Uint16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that reading array[1] produces a *copy* of Uint16x8, not an
  // alias into the array.

  var f = array[1];
  assertEq(Uint16x8.extractLane(f, 3), 12);
  assertEq(Uint16x8.extractLane(array[1], 3), 12);
  array[1] = Uint16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(Uint16x8.extractLane(f, 3), 12);
  assertEq(Uint16x8.extractLane(array[1], 3), 28);
}

function testUint16x8Setters() {
  var Array = Uint16x8.array(3);
  var array = new Array([Uint16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         Uint16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         Uint16x8(17, 18, 19, 20, 21, 22, 23, 24)]);
  assertEq(Uint16x8.extractLane(array[1], 3), 12);

  // Test that we are allowed to write Uint16x8 values into array,
  // but not other things.

  array[1] = Uint16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(Uint16x8.extractLane(array[1], 3), 28);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 25, s1: 26, s2: 27, s3: 28, s4: 29, s5: 30, s6: 31, s7: 32};
  }, TypeError, "Setting Uint16x8 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [25, 26, 27, 28, 29, 30, 31, 32];
  }, TypeError, "Setting Uint16x8 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 28;
  }, TypeError, "Setting Uint16x8 from a number");
}

function testUint32x4Alignment() {
  assertEq(Uint32x4.byteLength, 16);
  assertEq(Uint32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: Uint32x4});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testUint32x4Getters() {
  // Create a Uint32x4 and check that the getters work:
  var f = Uint32x4(11, 22, 33, 44);
  assertEq(Uint32x4.extractLane(f, 0), 11);
  assertEq(Uint32x4.extractLane(f, 1), 22);
  assertEq(Uint32x4.extractLane(f, 2), 33);
  assertEq(Uint32x4.extractLane(f, 3), 44);

  assertThrowsInstanceOf(() => Uint32x4.extractLane(f, 4), RangeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(Float32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Uint32x4.extractLane(f, f), TypeError);
}

function testUint32x4Handles() {
  var Array = Uint32x4.array(3);
  var array = new Array([Uint32x4(1, 2, 3, 4),
                         Uint32x4(5, 6, 7, 8),
                         Uint32x4(9, 10, 11, 12)]);

  // Test that trying to create handle into the interior of a
  // Uint32x4 fails.
  assertThrowsInstanceOf(function() {
    var h = int32.handle(array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int32.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int32 handle to elem via move");
}

function testUint32x4Reify() {
  var Array = Uint32x4.array(3);
  var array = new Array([Uint32x4(1, 2, 3, 4),
                         Uint32x4(5, 6, 7, 8),
                         Uint32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of Uint32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(Uint32x4.extractLane(f, 3), 8);
  assertEq(Uint32x4.extractLane(array[1], 3), 8);
  array[1] = Uint32x4(15, 16, 17, 18);
  assertEq(Uint32x4.extractLane(f, 3), 8);
  assertEq(Uint32x4.extractLane(array[1], 3), 18);
}

function testUint32x4Setters() {
  var Array = Uint32x4.array(3);
  var array = new Array([Uint32x4(1, 2, 3, 4),
                         Uint32x4(5, 6, 7, 8),
                         Uint32x4(9, 10, 11, 12)]);
  assertEq(Uint32x4.extractLane(array[1], 3), 8);

  // Test that we are allowed to write Uint32x4 values into array,
  // but not other things.
  array[1] = Uint32x4(15, 16, 17, 18);
  assertEq(Uint32x4.extractLane(array[1], 3), 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting Uint32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting Uint32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting Uint32x4 from a number");
}

function testBool8x16Getters() {
  // Create a Bool8x16 and check that the getters work:
  var f = Bool8x16(true, false, true, false, true, false, true, false, true, true, false, false, true, true, false, false);
  assertEq(Bool8x16.extractLane(f, 0), true);
  assertEq(Bool8x16.extractLane(f, 1), false);
  assertEq(Bool8x16.extractLane(f, 2), true);
  assertEq(Bool8x16.extractLane(f, 3), false);
  assertEq(Bool8x16.extractLane(f, 4), true);
  assertEq(Bool8x16.extractLane(f, 5), false);
  assertEq(Bool8x16.extractLane(f, 6), true);
  assertEq(Bool8x16.extractLane(f, 7), false);
  assertEq(Bool8x16.extractLane(f, 8), true);
  assertEq(Bool8x16.extractLane(f, 9), true);
  assertEq(Bool8x16.extractLane(f, 10), false);
  assertEq(Bool8x16.extractLane(f, 11), false);
  assertEq(Bool8x16.extractLane(f, 12), true);
  assertEq(Bool8x16.extractLane(f, 13), true);
  assertEq(Bool8x16.extractLane(f, 14), false);
  assertEq(Bool8x16.extractLane(f, 15), false);

  assertThrowsInstanceOf(() => Bool8x16.extractLane(f, 16), RangeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(Float32x4(1, 2, 3, 4), 0), TypeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Bool8x16.extractLane(f, f), TypeError);
}

function testBool8x16Reify() {
  var Array = Bool8x16.array(3);
  var array = new Array([Bool8x16(true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false),
                         Bool8x16(false, true, false, true, false, true, false, true, false, true, false, true, false, true, false, true),
                         Bool8x16(true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false)]);

  // Test that reading array[1] produces a *copy* of Bool8x16, not an
  // alias into the array.

  var f = array[1];
  assertEq(Bool8x16.extractLane(f, 2), false);
  assertEq(Bool8x16.extractLane(array[1], 2), false);
  assertEq(Bool8x16.extractLane(f, 3), true);
  assertEq(Bool8x16.extractLane(array[1], 3), true);
  array[1] = Bool8x16(true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false);
  assertEq(Bool8x16.extractLane(f, 3), true);
  assertEq(Bool8x16.extractLane(array[1], 3), false);
}

function testBool8x16Setters() {
  var Array = Bool8x16.array(3);
  var array = new Array([Bool8x16(true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false),
                         Bool8x16(false, true, false, true, false, true, false, true, false, true, false, true, false, true, false, true),
                         Bool8x16(true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false)]);

  assertEq(Bool8x16.extractLane(array[1], 3), true);
  // Test that we are allowed to write Bool8x16 values into array,
  // but not other things.
  array[1] = Bool8x16(true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false);
  assertEq(Bool8x16.extractLane(array[1], 3), false);
  assertThrowsInstanceOf(function() {
    array[1] = {s0: true, s1: true, s2: true, s3: true, s4: true, s5: true, s6: true, s7: true,
                s8: false, s9: false, s10: false, s11: false, s12: false, s13: false, s14: false, s15: false};
  }, TypeError, "Setting Bool8x16 from an object");
  assertThrowsInstanceOf(function() {
    array[1] = [true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false];
  }, TypeError, "Setting Bool8x16 from an array");
  assertThrowsInstanceOf(function() {
    array[1] = false;
  }, TypeError, "Setting Bool8x16 from a boolean");
}

function testBool16x8Getters() {
  // Create a Bool8x16 and check that the getters work:
  var f = Bool16x8(true, false, true, false, true, true, false, false);
  assertEq(Bool16x8.extractLane(f, 0), true);
  assertEq(Bool16x8.extractLane(f, 1), false);
  assertEq(Bool16x8.extractLane(f, 2), true);
  assertEq(Bool16x8.extractLane(f, 3), false);
  assertEq(Bool16x8.extractLane(f, 4), true);
  assertEq(Bool16x8.extractLane(f, 5), true);
  assertEq(Bool16x8.extractLane(f, 6), false);
  assertEq(Bool16x8.extractLane(f, 7), false);

  assertThrowsInstanceOf(() => Bool16x8.extractLane(f, 8), RangeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(Float32x4(1, 2, 3, 4), 0), TypeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Bool16x8.extractLane(f, f), TypeError);
}

function testBool16x8Reify() {
  var Array = Bool16x8.array(3);
  var array = new Array([Bool16x8(true, false, true, false, true, false, true, false),
                         Bool16x8(false, true, false, true, false, true, false, true),
                         Bool16x8(true, true, true, false, true, true, true, false)]);
  // Test that reading array[1] produces a *copy* of Bool16x8, not an
  // alias into the array.
  var f = array[1];
  assertEq(Bool16x8.extractLane(f, 2), false);
  assertEq(Bool16x8.extractLane(array[1], 2), false);
  assertEq(Bool16x8.extractLane(f, 3), true);
  assertEq(Bool16x8.extractLane(array[1], 3), true);
  array[1] = Bool16x8(true, false, true, false, true, false, true, false);
  assertEq(Bool16x8.extractLane(f, 3), true);
  assertEq(Bool16x8.extractLane(array[1], 3), false);
}

function testBool16x8Setters() {
  var Array = Bool16x8.array(3);
  var array = new Array([Bool16x8(true, false, true, false, true, false, true, false),
                         Bool16x8(false, true, false, true, false, true, false, true),
                         Bool16x8(true, true, true, false, true, true, true, false)]);


  assertEq(Bool16x8.extractLane(array[1], 3), true);
  // Test that we are allowed to write Bool16x8 values into array,
  // but not other things.
  array[1] = Bool16x8(true, false, true, false, true, false, true, false);
  assertEq(Bool16x8.extractLane(array[1], 3), false);
  assertThrowsInstanceOf(function() {
    array[1] = {s0: false, s1: true, s2: false, s3: true, s4: false, s5: true, s6: false, s7: true};
  }, TypeError, "Setting Bool16x8 from an object");
  assertThrowsInstanceOf(function() {
    array[1] = [true, false, false, true, true, true, false, false];
  }, TypeError, "Setting Bool16x8 from an array");
  assertThrowsInstanceOf(function() {
    array[1] = false;
  }, TypeError, "Setting Bool16x8 from a boolean");
}

function testBool32x4Getters() {
  // Create a Bool32x4 and check that the getters work:
  var f = Bool32x4(true, false, false, true);
  assertEq(Bool32x4.extractLane(f, 0), true);
  assertEq(Bool32x4.extractLane(f, 1), false);
  assertEq(Bool32x4.extractLane(f, 2), false);
  assertEq(Bool32x4.extractLane(f, 3), true);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(f, 4), RangeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(Float32x4(1, 2, 3, 4), 0), TypeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Bool32x4.extractLane(f, f), TypeError);
}

function testBool32x4Reify() {
  var Array = Bool32x4.array(3);
  var array = new Array([Bool32x4(true, false, false, true),
                         Bool32x4(true, false, true, false),
                         Bool32x4(true, true, true, false)]);

  // Test that reading array[1] produces a *copy* of Bool32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(Bool32x4.extractLane(f, 2), true);
  assertEq(Bool32x4.extractLane(array[1], 2), true);
  assertEq(Bool32x4.extractLane(f, 3), false);
  assertEq(Bool32x4.extractLane(array[1], 3), false);
  array[1] = Bool32x4(false, true, false, true);
  assertEq(Bool32x4.extractLane(f, 3), false);
  assertEq(Bool32x4.extractLane(array[1], 3), true);
}

function testBool32x4Setters() {
  var Array = Bool32x4.array(3);
  var array = new Array([Bool32x4(true, false, false, true),
                         Bool32x4(true, false, true, false),
                         Bool32x4(true, true, true, false)]);


  assertEq(Bool32x4.extractLane(array[1], 3), false);
  // Test that we are allowed to write Bool32x4 values into array,
  // but not other things.
  array[1] = Bool32x4(false, true, false, true);
  assertEq(Bool32x4.extractLane(array[1], 3), true);
  assertThrowsInstanceOf(function() {
    array[1] = {x: false, y: true, z: false, w: true};
  }, TypeError, "Setting Bool32x4 from an object");
  assertThrowsInstanceOf(function() {
    array[1] = [true, false, false, true];
  }, TypeError, "Setting Bool32x4 from an array");
  assertThrowsInstanceOf(function() {
    array[1] = false;
  }, TypeError, "Setting Bool32x4 from a number");
}

function testBool64x2Getters() {
  // Create a Bool64x2 and check that the getters work:
  var f = Bool64x2(true, false);
  assertEq(Bool64x2.extractLane(f, 0), true);
  assertEq(Bool64x2.extractLane(f, 1), false);

  assertThrowsInstanceOf(() => Bool64x2.extractLane(f, 2), RangeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(f, -1), RangeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(f, 0.5), RangeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(f, {}), RangeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(Bool32x4(1,2,3,4), 0), TypeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(1, 0), TypeError);
  assertThrowsInstanceOf(() => Bool64x2.extractLane(f, f), TypeError);
}

function testBool64x2Reify() {
  var Array = Bool64x2.array(3);
  var array = new Array([Bool64x2(true, false),
                         Bool64x2(false, true),
                         Bool64x2(true, true)]);

  // Test that reading array[1] produces a *copy* of Bool64x2, not an
  // alias into the array.

  var f = array[1];
  assertEq(Bool64x2.extractLane(f, 1), true);
  assertEq(Bool64x2.extractLane(array[1], 1), true);
  array[1] = Bool64x2(false, false);
  assertEq(Bool64x2.extractLane(f, 1), true);
  assertEq(Bool64x2.extractLane(array[1], 1), false);
}

function testBool64x2Setters() {
  var Array = Bool64x2.array(3);
  var array = new Array([Bool64x2(true, false),
                         Bool64x2(false, true),
                         Bool64x2(true, true)]);
  assertEq(Bool64x2.extractLane(array[1], 1), true);

  // Test that we are allowed to write Bool64x2 values into array,
  // but not other things.

  array[1] = Bool64x2(false, false);
  assertEq(Bool64x2.extractLane(array[1], 1), false);

  assertThrowsInstanceOf(function() {
    array[1] = {x: false, y: false };
  }, TypeError, "Setting Bool64x2 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [ false, false ];
  }, TypeError, "Setting Bool64x2 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 9;
  }, TypeError, "Setting Bool64x2 from a number");
}


function test() {

  testFloat32x4Alignment();
  testFloat32x4Getters();
  testFloat32x4Handles();
  testFloat32x4Reify();
  testFloat32x4Setters();

  testFloat64x2Alignment();
  testFloat64x2Getters();
  testFloat64x2Handles();
  testFloat64x2Reify();
  testFloat64x2Setters();

  testInt8x16Alignment();
  testInt8x16Getters();
  testInt8x16Handles();
  testInt8x16Reify();
  testInt8x16Setters();

  testInt16x8Alignment();
  testInt16x8Getters();
  testInt16x8Handles();
  testInt16x8Reify();
  testInt16x8Setters();

  testInt32x4Alignment();
  testInt32x4Getters();
  testInt32x4Handles();
  testInt32x4Reify();
  testInt32x4Setters();

  testUint8x16Alignment();
  testUint8x16Getters();
  testUint8x16Handles();
  testUint8x16Reify();
  testUint8x16Setters();

  testUint16x8Alignment();
  testUint16x8Getters();
  testUint16x8Handles();
  testUint16x8Reify();
  testUint16x8Setters();

  testUint32x4Alignment();
  testUint32x4Getters();
  testUint32x4Handles();
  testUint32x4Reify();
  testUint32x4Setters();

  testBool8x16Getters();
  testBool8x16Reify();
  testBool8x16Setters();

  testBool16x8Getters();
  testBool16x8Reify();
  testBool16x8Setters();

  testBool32x4Getters();
  testBool32x4Reify();
  testBool32x4Setters();

  testBool64x2Getters();
  testBool64x2Reify();
  testBool64x2Setters();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();

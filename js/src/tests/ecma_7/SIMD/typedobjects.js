// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

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

  assertThrowsInstanceOf(() => Float32x4.extractLane(f, 4), TypeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, -1), TypeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, 0.5), TypeError);
  assertThrowsInstanceOf(() => Float32x4.extractLane(f, {}), TypeError);
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

  assertThrowsInstanceOf(() => Float64x2.extractLane(f, 2), TypeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, -1), TypeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, 0.5), TypeError);
  assertThrowsInstanceOf(() => Float64x2.extractLane(f, {}), TypeError);
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

  assertThrowsInstanceOf(() => Int8x16.extractLane(f, 16), TypeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, -1), TypeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, 0.5), TypeError);
  assertThrowsInstanceOf(() => Int8x16.extractLane(f, {}), TypeError);
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
  assertEq(Int8x16.extractLane(f, 3), 20);
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

  assertThrowsInstanceOf(() => Int16x8.extractLane(f, 8), TypeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, -1), TypeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, 0.5), TypeError);
  assertThrowsInstanceOf(() => Int16x8.extractLane(f, {}), TypeError);
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

  assertThrowsInstanceOf(() => Int32x4.extractLane(f, 4), TypeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, -1), TypeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, 0.5), TypeError);
  assertThrowsInstanceOf(() => Int32x4.extractLane(f, {}), TypeError);
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

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();

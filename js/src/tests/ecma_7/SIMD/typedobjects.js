// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;

var {StructType, Handle} = TypedObject;
var {float32, float64, int8, int16, int32, uint8} = TypedObject;

function testFloat32x4Alignment() {
  assertEq(float32x4.byteLength, 16);
  assertEq(float32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: float32x4});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testFloat32x4Getters() {
  // Create a float32x4 and check that the getters work:
  var f = float32x4(11, 22, 33, 44);
  assertEq(f.x, 11);
  assertEq(f.y, 22);
  assertEq(f.z, 33);
  assertEq(f.w, 44);

  // Test that the getters work when called reflectively:
  var g = f.__lookupGetter__("x");
  assertEq(g.call(f), 11);

  // Test that getters cannot be applied to various incorrect things:
  assertThrowsInstanceOf(function() {
    g.call({});
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF);
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new StructType({x: float32,
                            y: float32,
                            z: float32,
                            w: float32});
    var v = new T({x: 11, y: 22, z: 33, w: 44});
    g.call(v);
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new int32x4(1, 2, 3, 4);
    g.call(t);
  }, TypeError, "Getter applicable to int32x4");
}

function testFloat32x4Handles() {
  var Array = float32x4.array(3);
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
}

function testFloat32x4Reify() {
  var Array = float32x4.array(3);
  var array = new Array([float32x4(1, 2, 3, 4),
                         float32x4(5, 6, 7, 8),
                         float32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of float32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.w, 8);
  assertEq(array[1].w, 8);
  array[1] = float32x4(15, 16, 17, 18);
  assertEq(f.w, 8);
  assertEq(array[1].w, 18);
}

function testFloat32x4Setters() {
  var Array = float32x4.array(3);
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
}

function testFloat64x2Alignment() {
  assertEq(float64x2.byteLength, 16);
  assertEq(float64x2.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: float64x2});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testFloat64x2Getters() {
  // Create a float64x2 and check that the getters work:
  var f = float64x2(11, 22);
  assertEq(f.x, 11);
  assertEq(f.y, 22);

  // Test that the getters work when called reflectively:
  var g = f.__lookupGetter__("x");
  assertEq(g.call(f), 11);

  // Test that getters cannot be applied to various incorrect things:
  assertThrowsInstanceOf(function() {
    g.call({});
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF);
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new StructType({x: float64,
                            y: float64});
    var v = new T({x: 11, y: 22});
    g.call(v);
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new int32x4(1, 2, 3, 4);
    g.call(t);
  }, TypeError, "Getter applicable to int32x4");
}

function testFloat64x2Handles() {
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
}

function testFloat64x2Reify() {
  var Array = float64x2.array(3);
  var array = new Array([float64x2(1, 2),
                         float64x2(3, 4),
                         float64x2(5, 6)]);

  // Test that reading array[1] produces a *copy* of float64x2, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.y, 4);
  assertEq(array[1].y, 4);
  array[1] = float64x2(7, 8);
  assertEq(f.y, 4);
  assertEq(array[1].y, 8);
}

function testFloat64x2Setters() {
  var Array = float64x2.array(3);
  var array = new Array([float64x2(1, 2),
                         float64x2(3, 4),
                         float64x2(5, 6)]);
  assertEq(array[1].y, 4);

  // Test that we are allowed to write float64x2 values into array,
  // but not other things.

  array[1] = float64x2(7, 8);
  assertEq(array[1].y, 8);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 7, y: 8 };
  }, TypeError, "Setting float64x2 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [ 7, 8 ];
  }, TypeError, "Setting float64x2 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 9;
  }, TypeError, "Setting float64x2 from a number");
}

function testInt8x16Alignment() {
  assertEq(int8x16.byteLength, 16);
  assertEq(int8x16.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: int8x16});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt8x16Getters() {
  // Create a int8x16 and check that the getters work:
  var f = int8x16(11, 22, 33, 44, 55, 66, 77, 88, 99, 10, 20, 30, 40, 50, 60, 70);
  assertEq(f.s0, 11);
  assertEq(f.s1, 22);
  assertEq(f.s2, 33);
  assertEq(f.s3, 44);
  assertEq(f.s4, 55);
  assertEq(f.s5, 66);
  assertEq(f.s6, 77);
  assertEq(f.s7, 88);
  assertEq(f.s8, 99);
  assertEq(f.s9, 10);
  assertEq(f.s10, 20);
  assertEq(f.s11, 30);
  assertEq(f.s12, 40);
  assertEq(f.s13, 50);
  assertEq(f.s14, 60);
  assertEq(f.s15, 70);

  // Test that the getters work when called reflectively:
  var g = f.__lookupGetter__("s0");
  assertEq(g.call(f), 11);

  // Test that getters cannot be applied to various incorrect things:
  assertThrowsInstanceOf(function() {
    g.call({})
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF)
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new StructType({s0: int8, s1: int8, s2: int8, s3: int8, s4: int8, s5: int8, s6: int8, s7: int8,
                            s8: int8, s9: int8, s10: int8, s11: int8, s12: int8, s13: int8, s14: int8, s15: int8});
    var v = new T({s0: 11, s1: 22, s2: 33, s3: 44, s4: 55, s5: 66, s6: 77, s7: 88,
                   s8: 99, s9: 10, s10: 20, s11: 30, s12: 40, s13: 50, s14: 60, s15: 70});
    g.call(v)
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new int16x8(1, 2, 3, 4, 5, 6, 7, 8);
    g.call(t)
  }, TypeError, "Getter applicable to int16x8");
}

function testInt8x16Handles() {
  var Array = int8x16.array(3);
  var array = new Array([int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that trying to create handle into the interior of a
  // int8x16 fails.

  assertThrowsInstanceOf(function() {
    var h = int8.handle(array, 1, "s3");
  }, TypeError, "Creating a int8 handle to prop via ctor");

  assertThrowsInstanceOf(function() {
    var h = int8.handle();
    Handle.move(h, array, 1, "s3");
  }, TypeError, "Creating a int8 handle to prop via move");

  assertThrowsInstanceOf(function() {
    var h = int8.handle(array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int8.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int8 handle to elem via move");
}

function testInt8x16Reify() {
  var Array = int8x16.array(3);
  var array = new Array([int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);

  // Test that reading array[1] produces a *copy* of int8x16, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.s3, 20);
  assertEq(array[1].s3, 20);
  array[1] = int8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(f.s3, 20);
  assertEq(array[1].s3, 52);
}

function testInt8x16Setters() {
  var Array = int8x16.array(3);
  var array = new Array([int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
                         int8x16(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32),
                         int8x16(33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)]);
  assertEq(array[1].s3, 20);

  // Test that we are allowed to write int8x16 values into array,
  // but not other things.

  array[1] = int8x16(49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
  assertEq(array[1].s3, 52);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 49, s1: 50, s2: 51, s3: 52, s4: 53, s5: 54, s6: 55, s7: 56,
                s8: 57, s9: 58, s10: 59, s11: 60, s12: 61, s13: 62, s14: 63, s15: 64};
  }, TypeError, "Setting int8x16 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64];
  }, TypeError, "Setting int8x16 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 52;
  }, TypeError, "Setting int8x16 from a number");
}

function testInt16x8Alignment() {
  assertEq(int16x8.byteLength, 16);
  assertEq(int16x8.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: int16x8});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt16x8Getters() {
  // Create a int16x8 and check that the getters work:
  var f = int16x8(11, 22, 33, 44, 55, 66, 77, 88);
  assertEq(f.s0, 11);
  assertEq(f.s1, 22);
  assertEq(f.s2, 33);
  assertEq(f.s3, 44);
  assertEq(f.s4, 55);
  assertEq(f.s5, 66);
  assertEq(f.s6, 77);
  assertEq(f.s7, 88);

  // Test that the getters work when called reflectively:
  var g = f.__lookupGetter__("s0");
  assertEq(g.call(f), 11);

  // Test that getters cannot be applied to various incorrect things:
  assertThrowsInstanceOf(function() {
    g.call({})
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF)
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new StructType({s0: int16, s1: int16, s2: int16, s3: int16, s4: int16, s5: int16, s6: int16, s7: int16});
    var v = new T({s0: 11, s1: 22, s2: 33, s3: 44, s4: 55, s5: 66, s6: 77, s7: 88});
    g.call(v)
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new int16x8(1, 2, 3, 4, 5, 6, 7, 8);
    g.call(t)
  }, TypeError, "Getter applicable to int16x8");
}

function testInt16x8Handles() {
  var Array = int16x8.array(3);
  var array = new Array([int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that trying to create handle into the interior of a
  // int16x8 fails.

  assertThrowsInstanceOf(function() {
    var h = int16.handle(array, 1, "s3");
  }, TypeError, "Creating a int16 handle to prop via ctor");

  assertThrowsInstanceOf(function() {
    var h = int16.handle();
    Handle.move(h, array, 1, "s3");
  }, TypeError, "Creating a int16 handle to prop via move");

  assertThrowsInstanceOf(function() {
    var h = int16.handle(array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via ctor");

  assertThrowsInstanceOf(function() {
    var h = int16.handle();
    Handle.move(h, array, 1, 0);
  }, TypeError, "Creating a int16 handle to elem via move");
}

function testInt16x8Reify() {
  var Array = int16x8.array(3);
  var array = new Array([int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);

  // Test that reading array[1] produces a *copy* of int16x8, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.s3, 12);
  assertEq(array[1].s3, 12);
  array[1] = int16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(f.s3, 12);
  assertEq(array[1].s3, 28);
}

function testInt16x8Setters() {
  var Array = int16x8.array(3);
  var array = new Array([int16x8(1, 2, 3, 4, 5, 6, 7, 8),
                         int16x8(9, 10, 11, 12, 13, 14, 15, 16),
                         int16x8(17, 18, 19, 20, 21, 22, 23, 24)]);
  assertEq(array[1].s3, 12);

  // Test that we are allowed to write int16x8 values into array,
  // but not other things.

  array[1] = int16x8(25, 26, 27, 28, 29, 30, 31, 32);
  assertEq(array[1].s3, 28);

  assertThrowsInstanceOf(function() {
    array[1] = {s0: 25, s1: 26, s2: 27, s3: 28, s4: 29, s5: 30, s6: 31, s7: 32};
  }, TypeError, "Setting int16x8 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [25, 26, 27, 28, 29, 30, 31, 32];
  }, TypeError, "Setting int16x8 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 28;
  }, TypeError, "Setting int16x8 from a number");
}

function testInt32x4Alignment() {
  assertEq(int32x4.byteLength, 16);
  assertEq(int32x4.byteAlignment, 16);

  var Compound = new StructType({c: uint8, d: uint8, f: int32x4});
  assertEq(Compound.fieldOffsets.c, 0);
  assertEq(Compound.fieldOffsets.d, 1);
  assertEq(Compound.fieldOffsets.f, 16);
}

function testInt32x4Getters() {
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
    g.call({});
  }, TypeError, "Getter applicable to random objects");
  assertThrowsInstanceOf(function() {
    g.call(0xDEADBEEF);
  }, TypeError, "Getter applicable to integers");
  assertThrowsInstanceOf(function() {
    var T = new StructType({x: int32, y: int32, z: int32, w: int32});
    var v = new T({x: 11, y: 22, z: 33, w: 44});
    g.call(v);
  }, TypeError, "Getter applicable to structs");
  assertThrowsInstanceOf(function() {
    var t = new float32x4(1, 2, 3, 4);
    g.call(t);
  }, TypeError, "Getter applicable to float32x4");
}

function testInt32x4Handles() {
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
}

function testInt32x4Reify() {
  var Array = int32x4.array(3);
  var array = new Array([int32x4(1, 2, 3, 4),
                         int32x4(5, 6, 7, 8),
                         int32x4(9, 10, 11, 12)]);

  // Test that reading array[1] produces a *copy* of int32x4, not an
  // alias into the array.

  var f = array[1];
  assertEq(f.w, 8);
  assertEq(array[1].w, 8);
  array[1] = int32x4(15, 16, 17, 18);
  assertEq(f.w, 8);
  assertEq(array[1].w, 18);
}

function testInt32x4Setters() {
  var Array = int32x4.array(3);
  var array = new Array([int32x4(1, 2, 3, 4),
                         int32x4(5, 6, 7, 8),
                         int32x4(9, 10, 11, 12)]);
  assertEq(array[1].w, 8);

  // Test that we are allowed to write int32x4 values into array,
  // but not other things.

  array[1] = int32x4(15, 16, 17, 18);
  assertEq(array[1].w, 18);

  assertThrowsInstanceOf(function() {
    array[1] = {x: 15, y: 16, z: 17, w: 18};
  }, TypeError, "Setting int32x4 from an object");

  assertThrowsInstanceOf(function() {
    array[1] = [15, 16, 17, 18];
  }, TypeError, "Setting int32x4 from an array");

  assertThrowsInstanceOf(function() {
    array[1] = 22;
  }, TypeError, "Setting int32x4 from a number");
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

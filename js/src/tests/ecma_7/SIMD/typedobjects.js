// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var {StructType, Handle} = TypedObject;
var {float32, float64, int32, uint8} = TypedObject;

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

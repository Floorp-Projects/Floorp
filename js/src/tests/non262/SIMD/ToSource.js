// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {
    var Float32x4 = SIMD.Float32x4;
    var f = Float32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Float32x4(11, 22, 33, 44)");

    var Float64x2 = SIMD.Float64x2;
    var f = Float64x2(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Float64x2(11, 22)");

    var Int8x16 = SIMD.Int8x16;
    var f = Int8x16(11, 22, 33, 44, -11, -22, -33, -44, 1, 2, 3, 4, -1, -2, -3, -4);
    assertEq(f.toSource(), "SIMD.Int8x16(11, 22, 33, 44, -11, -22, -33, -44, 1, 2, 3, 4, -1, -2, -3, -4)");

    var Int16x8 = SIMD.Int16x8;
    var f = Int16x8(11, 22, 33, 44, -11, -22, -33, -44);
    assertEq(f.toSource(), "SIMD.Int16x8(11, 22, 33, 44, -11, -22, -33, -44)");

    var Int32x4 = SIMD.Int32x4;
    var f = Int32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Int32x4(11, 22, 33, 44)");

    var Uint8x16 = SIMD.Uint8x16;
    var f = Uint8x16(11, 22, 33, 44, 245, 234, 223, 212, 1, 2, 3, 4, 255, 254, 0, 250);
    assertEq(f.toSource(), "SIMD.Uint8x16(11, 22, 33, 44, 245, 234, 223, 212, 1, 2, 3, 4, 255, 254, 0, 250)");

    var Uint16x8 = SIMD.Uint16x8;
    var f = Uint16x8(11, 22, 33, 44, 65535, 65534, 65533, 65532);
    assertEq(f.toSource(), "SIMD.Uint16x8(11, 22, 33, 44, 65535, 65534, 65533, 65532)");

    var Uint32x4 = SIMD.Uint32x4;
    var f = Uint32x4(11, 22, 4294967295, 4294967294);
    assertEq(f.toSource(), "SIMD.Uint32x4(11, 22, 4294967295, 4294967294)");

    var Bool8x16 = SIMD.Bool8x16;
    var f = Bool8x16(true, true, false, false, false, true, true, false, true, true, true, true, false, false, false, false);
    assertEq(f.toSource(), "SIMD.Bool8x16(true, true, false, false, false, true, true, false, true, true, true, true, false, false, false, false)");

    var Bool16x8 = SIMD.Bool16x8;
    var f = Bool16x8(true, true, false, false, true, false, false, true);
    assertEq(f.toSource(), "SIMD.Bool16x8(true, true, false, false, true, false, false, true)");

    var Bool32x4 = SIMD.Bool32x4;
    var f = Bool32x4(true, true, false, false);
    assertEq(f.toSource(), "SIMD.Bool32x4(true, true, false, false)");

    var Bool64x2 = SIMD.Bool64x2;
    var f = Bool64x2(true, false);
    assertEq(f.toSource(), "SIMD.Bool64x2(true, false)");

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {
    var Float32x4 = SIMD.Float32x4;
    var f = Float32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Float32x4(11, 22, 33, 44)");

    var Int32x4 = SIMD.Int32x4;
    var f = Int32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Int32x4(11, 22, 33, 44)");

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

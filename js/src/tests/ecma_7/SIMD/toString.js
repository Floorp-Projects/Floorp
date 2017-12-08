// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {
    var Float32x4 = SIMD.Float32x4;
    var f = Float32x4(11, 22, NaN, -44);
    assertEq(f.toString(), "SIMD.Float32x4(11, 22, NaN, -44)");

    // Polyfill check should show that we already have a toString.
    assertEq(Float32x4.prototype.hasOwnProperty("toString"), true);

    // This toString method type checks its argument.
    var ts = Float32x4.prototype.toString;
    assertThrowsInstanceOf(() => ts.call(5), TypeError);
    assertThrowsInstanceOf(() => ts.call({}), TypeError);

    // Can't convert SIMD objects to numbers.
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Float64x2 = SIMD.Float64x2;
    var f = Float64x2(11, 22);
    assertEq(f.toString(), "SIMD.Float64x2(11, 22)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Int8x16 = SIMD.Int8x16;
    var f = Int8x16(11, 22, 33, 44, -11, -22, -33, -44, 1, 2, 3, 4, -1, -2, -3, -4);
    assertEq(f.toString(), "SIMD.Int8x16(11, 22, 33, 44, -11, -22, -33, -44, 1, 2, 3, 4, -1, -2, -3, -4)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Int16x8 = SIMD.Int16x8;
    var f = Int16x8(11, 22, 33, 44, -11, -22, -33, -44);
    assertEq(f.toString(), "SIMD.Int16x8(11, 22, 33, 44, -11, -22, -33, -44)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Int32x4 = SIMD.Int32x4;
    var f = Int32x4(11, 22, 33, 44);
    assertEq(f.toString(), "SIMD.Int32x4(11, 22, 33, 44)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Uint8x16 = SIMD.Uint8x16;
    var f = Uint8x16(11, 22, 33, 44, 245, 234, 223, 212, 1, 2, 3, 4, 255, 254, 0, 250);
    assertEq(f.toString(), "SIMD.Uint8x16(11, 22, 33, 44, 245, 234, 223, 212, 1, 2, 3, 4, 255, 254, 0, 250)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Uint16x8 = SIMD.Uint16x8;
    var f = Uint16x8(11, 22, 33, 44, 65535, 65534, 65533, 65532);
    assertEq(f.toString(), "SIMD.Uint16x8(11, 22, 33, 44, 65535, 65534, 65533, 65532)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Uint32x4 = SIMD.Uint32x4;
    var f = Uint32x4(11, 22, 4294967295, 4294967294);
    assertEq(f.toString(), "SIMD.Uint32x4(11, 22, 4294967295, 4294967294)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Bool8x16 = SIMD.Bool8x16;
    var f = Bool8x16(true, true, false, false, false, true, true, false, true, true, true, true, false, false, false, false);
    assertEq(f.toString(), "SIMD.Bool8x16(true, true, false, false, false, true, true, false, true, true, true, true, false, false, false, false)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Bool16x8 = SIMD.Bool16x8;
    var f = Bool16x8(true, true, false, false, true, false, false, true);
    assertEq(f.toString(), "SIMD.Bool16x8(true, true, false, false, true, false, false, true)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Bool32x4 = SIMD.Bool32x4;
    var f = Bool32x4(true, true, false, false);
    assertEq(f.toString(), "SIMD.Bool32x4(true, true, false, false)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    var Bool64x2 = SIMD.Bool64x2;
    var f = Bool64x2(true, false);
    assertEq(f.toString(), "SIMD.Bool64x2(true, false)");
    assertThrowsInstanceOf(() => +f, TypeError);
    assertThrowsInstanceOf(() => f.valueOf(), TypeError);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {
    var Float32x4 = SIMD.Float32x4;
    var f = Float32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Float32x4(11, 22, 33, 44)");

    var Int32x4 = SIMD.Int32x4;
    var f = Int32x4(11, 22, 33, 44);
    assertEq(f.toSource(), "SIMD.Int32x4(11, 22, 33, 44)");

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

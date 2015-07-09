if (typeof SIMD === "undefined")
    quit();

var Float32x4 = SIMD.Float32x4;
var f = Float32x4(11, 22, 33, 44);
assertEq(f.toSource(), "float32x4(11, 22, 33, 44)");

var Int32x4 = SIMD.Int32x4;
var f = Int32x4(11, 22, 33, 44);
assertEq(f.toSource(), "int32x4(11, 22, 33, 44)");

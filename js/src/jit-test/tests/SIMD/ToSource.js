if (typeof SIMD === "undefined")
    quit();

var float32x4 = SIMD.float32x4;
var f = float32x4(11, 22, 33, 44);
assertEq(f.toSource(), "float32x4(11, 22, 33, 44)");

var int32x4 = SIMD.int32x4;
var f = int32x4(11, 22, 33, 44);
assertEq(f.toSource(), "int32x4(11, 22, 33, 44)");

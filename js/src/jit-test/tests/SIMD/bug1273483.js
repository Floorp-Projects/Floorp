if (typeof SIMD === 'undefined')
    quit();

Int8x16 = SIMD.Int8x16;
var Int32x4 = SIMD.Int32x4;
function testSwizzleForType(type) { return type(); }
testSwizzleForType(Int8x16);
function testSwizzleInt32x4() { return testSwizzleForType(Int32x4); }
testSwizzleInt32x4();

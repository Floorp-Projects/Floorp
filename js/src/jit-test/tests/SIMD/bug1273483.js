if (typeof SIMD === 'undefined')
    quit();

Int8x16 = SIMD.Int8x16;
var Int32x4 = SIMD.Int32x4;
function testSwizzleForType(type) type();
testSwizzleForType(Int8x16);
function testSwizzleInt32x4() testSwizzleForType(Int32x4);
testSwizzleInt32x4();

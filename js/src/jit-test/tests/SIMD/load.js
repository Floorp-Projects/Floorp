load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 40);

function f() {
    var f32 = new Float32Array(16);
    for (var i = 0; i < 16; i++)
        f32[i] = i + 1;

    var f64 = new Float64Array(f32.buffer);
    var i32 = new Int32Array(f32.buffer);
    var u32 = new Uint32Array(f32.buffer);
    var i16 = new Int16Array(f32.buffer);
    var u16 = new Uint16Array(f32.buffer);
    var i8  = new Int8Array(f32.buffer);
    var u8  = new Uint8Array(f32.buffer);

    function testLoad() {
        assertEqX4(SIMD.float32x4.load(f64, 0),      [1,2,3,4]);
        assertEqX4(SIMD.float32x4.load(f32, 1),      [2,3,4,5]);
        assertEqX4(SIMD.float32x4.load(i32, 2),      [3,4,5,6]);
        assertEqX4(SIMD.float32x4.load(i16, 3 << 1), [4,5,6,7]);
        assertEqX4(SIMD.float32x4.load(u16, 4 << 1), [5,6,7,8]);
        assertEqX4(SIMD.float32x4.load(i8 , 5 << 2), [6,7,8,9]);
        assertEqX4(SIMD.float32x4.load(u8 , 6 << 2), [7,8,9,10]);

        assertEqX4(SIMD.float32x4.load(f64, (16 >> 1) - (4 >> 1)), [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(f32, 16 - 4),               [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(i32, 16 - 4),               [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(i16, (16 << 1) - (4 << 1)), [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(u16, (16 << 1) - (4 << 1)), [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(i8,  (16 << 2) - (4 << 2)), [13,14,15,16]);
        assertEqX4(SIMD.float32x4.load(u8,  (16 << 2) - (4 << 2)), [13,14,15,16]);
    }

    function testLoadX() {
        assertEqX4(SIMD.float32x4.loadX(f64, 0),      [1,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(f32, 1),      [2,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i32, 2),      [3,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i16, 3 << 1), [4,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(u16, 4 << 1), [5,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i8 , 5 << 2), [6,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(u8 , 6 << 2), [7,0,0,0]);

        assertEqX4(SIMD.float32x4.loadX(f64, (16 >> 1) - (4 >> 1)), [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(f32, 16 - 4),               [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i32, 16 - 4),               [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i16, (16 << 1) - (4 << 1)), [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(u16, (16 << 1) - (4 << 1)), [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(i8,  (16 << 2) - (4 << 2)), [13,0,0,0]);
        assertEqX4(SIMD.float32x4.loadX(u8,  (16 << 2) - (4 << 2)), [13,0,0,0]);
    }

    function testLoadXY() {
        assertEqX4(SIMD.float32x4.loadXY(f64, 0),      [1,2,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(f32, 1),      [2,3,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i32, 2),      [3,4,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i16, 3 << 1), [4,5,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(u16, 4 << 1), [5,6,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i8 , 5 << 2), [6,7,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(u8 , 6 << 2), [7,8,0,0]);

        assertEqX4(SIMD.float32x4.loadXY(f64, (16 >> 1) - (4 >> 1)), [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(f32, 16 - 4),               [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i32, 16 - 4),               [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i16, (16 << 1) - (4 << 1)), [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(u16, (16 << 1) - (4 << 1)), [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(i8,  (16 << 2) - (4 << 2)), [13,14,0,0]);
        assertEqX4(SIMD.float32x4.loadXY(u8,  (16 << 2) - (4 << 2)), [13,14,0,0]);
    }

    function testLoadXYZ() {
        assertEqX4(SIMD.float32x4.loadXYZ(f64, 0),      [1,2,3,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(f32, 1),      [2,3,4,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i32, 2),      [3,4,5,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i16, 3 << 1), [4,5,6,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(u16, 4 << 1), [5,6,7,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i8 , 5 << 2), [6,7,8,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(u8 , 6 << 2), [7,8,9,0]);

        assertEqX4(SIMD.float32x4.loadXYZ(f64, (16 >> 1) - (4 >> 1)), [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(f32, 16 - 4),               [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i32, 16 - 4),               [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i16, (16 << 1) - (4 << 1)), [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(u16, (16 << 1) - (4 << 1)), [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(i8,  (16 << 2) - (4 << 2)), [13,14,15,0]);
        assertEqX4(SIMD.float32x4.loadXYZ(u8,  (16 << 2) - (4 << 2)), [13,14,15,0]);
    }

    for (var i = 0; i < 150; i++) {
        testLoad();
        testLoadX();
        testLoadXY();
        testLoadXYZ();
    }
}

f();

function testBailout(uglyDuckling) {
    var f32 = new Float32Array(16);
    for (var i = 0; i < 16; i++)
        f32[i] = i + 1;

    var i8  = new Int8Array(f32.buffer);

    for (var i = 0; i < 150; i++) {
        var caught = false;
        try {
            SIMD.float32x4.load(i8, (i < 149) ? 0 : uglyDuckling);
        } catch (e) {
            print(e);
            assertEq(e instanceof RangeError, true);
            caught = true;
        }
        assertEq(i < 149 || caught, true);
    }
}

print('Testing range checks...');
testBailout(-1);
testBailout(-15);
testBailout(12 * 4 + 1);

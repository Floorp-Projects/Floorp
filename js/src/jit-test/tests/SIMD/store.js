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

    var f4 = SIMD.Float32x4(42, 43, 44, 45);

    function check(n) {
        assertEq(f32[0], 42);
        assertEq(f32[1], n > 1 ? 43 : 2);
        assertEq(f32[2], n > 2 ? 44 : 3);
        assertEq(f32[3], n > 3 ? 45 : 4);

        f32[0] = 1;
        f32[1] = 2;
        f32[2] = 3;
        f32[3] = 4;
    }

    function testStore() {
        SIMD.Float32x4.store(f64, 0, f4);
        check(4);
        SIMD.Float32x4.store(f32, 0, f4);
        check(4);
        SIMD.Float32x4.store(i32, 0, f4);
        check(4);
        SIMD.Float32x4.store(u32, 0, f4);
        check(4);
        SIMD.Float32x4.store(i16, 0, f4);
        check(4);
        SIMD.Float32x4.store(u16, 0, f4);
        check(4);
        SIMD.Float32x4.store(i8, 0, f4);
        check(4);
        SIMD.Float32x4.store(u8, 0, f4);
        check(4);
    }

    function testStore1() {
        SIMD.Float32x4.store1(f64, 0, f4);
        check(1);
        SIMD.Float32x4.store1(f32, 0, f4);
        check(1);
        SIMD.Float32x4.store1(i32, 0, f4);
        check(1);
        SIMD.Float32x4.store1(u32, 0, f4);
        check(1);
        SIMD.Float32x4.store1(i16, 0, f4);
        check(1);
        SIMD.Float32x4.store1(u16, 0, f4);
        check(1);
        SIMD.Float32x4.store1(i8, 0, f4);
        check(1);
        SIMD.Float32x4.store1(u8, 0, f4);
        check(1);
    }

    function testStore2() {
        SIMD.Float32x4.store2(f64, 0, f4);
        check(2);
        SIMD.Float32x4.store2(f32, 0, f4);
        check(2);
        SIMD.Float32x4.store2(i32, 0, f4);
        check(2);
        SIMD.Float32x4.store2(u32, 0, f4);
        check(2);
        SIMD.Float32x4.store2(i16, 0, f4);
        check(2);
        SIMD.Float32x4.store2(u16, 0, f4);
        check(2);
        SIMD.Float32x4.store2(i8, 0, f4);
        check(2);
        SIMD.Float32x4.store2(u8, 0, f4);
        check(2);
    }

    function testStore3() {
        SIMD.Float32x4.store3(f64, 0, f4);
        check(3);
        SIMD.Float32x4.store3(f32, 0, f4);
        check(3);
        SIMD.Float32x4.store3(i32, 0, f4);
        check(3);
        SIMD.Float32x4.store3(u32, 0, f4);
        check(3);
        SIMD.Float32x4.store3(i16, 0, f4);
        check(3);
        SIMD.Float32x4.store3(u16, 0, f4);
        check(3);
        SIMD.Float32x4.store3(i8, 0, f4);
        check(3);
        SIMD.Float32x4.store3(u8, 0, f4);
        check(3);
    }

    for (var i = 0; i < 150; i++) {
        testStore();
        testStore1();
        testStore2();
        testStore3();
    }
}

f();

function testBailout(uglyDuckling) {
    var f32 = new Float32Array(16);
    for (var i = 0; i < 16; i++)
        f32[i] = i + 1;

    var i8  = new Int8Array(f32.buffer);

    var f4 = SIMD.Float32x4(42, 43, 44, 45);

    for (var i = 0; i < 150; i++) {
        var caught = false;
        try {
            SIMD.Float32x4.store(i8, (i < 149) ? 0 : (16 << 2) - (4 << 2) + 1, f4);
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

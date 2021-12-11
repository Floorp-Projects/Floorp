// |jit-test| slow

// Ensure Warp JIT code handles |new Int32Array(INT32_MAX)| correctly.

function test(len) {
    var ta;
    for (var i = 0; i < 2149; i++) {
        if (i % 1024 === 100) {
            // Constant length.
            ta = new Int32Array(0x7fff_ffff);
            assertEq(ta.length, 0x7fff_ffff);
            ta[0x7fff_fffe] = i;
            assertEq(ta[0x7fff_fffe], i);

            // Variable length.
            ta = new Int32Array(len - i);
            assertEq(ta.length, len - i);
            ta[ta.length - 1] = i;
            assertEq(ta[ta.length - 1], i);
        }
    }
}
test(0x7fff_ffff);

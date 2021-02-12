// |jit-test| test-also=--spectre-mitigations=off

const gb = 1 * 1024 * 1024 * 1024;
const ab = new ArrayBuffer(7 * gb);

// Function called with Uint8Arrays of different sizes.
function test(u8arr) {
    var length = u8arr.length;
    u8arr[0] = 87;

    function testExpectedOOB() {
        var base = length - 10;
        u8arr[base]++;
        for (var i = 0; i < 15; i++) {
            var val = u8arr[base + i];
            u8arr[base + i + 1] = (val|0) + 1;
        }
    }
    for (var i = 0; i < 500; i++) {
        testExpectedOOB();
    }
    assertEq(u8arr[length - 1], 253);

    function testNegativeInt32Index() {
        var val = 0;
        for (var i = 0; i < 1500; i++) {
            var idx = (i < 1450) - 1; // First 0, then -1.
            val = u8arr[idx];
        }
        assertEq(val, undefined);
    }
    testNegativeInt32Index();

    function testNegativeDoubleIndex() {
        var val = 0;
        for (var i = 0; i < 1500; i++) {
            var idx = numberToDouble(+(i < 1450)) - 1; // First 0.0, then -1.0.
            val = u8arr[idx];
            assertEq(val, i < 1450 ? 87 : undefined);
        }
    }
    testNegativeDoubleIndex();

    function testConstantDoubleIndex() {
        for (var i = 0; i < 1500; i++) {
            var idxInBounds = 4294967100;
            assertEq(u8arr[idxInBounds], 0);
            u8arr[idxInBounds] = 1;
            assertEq(u8arr[idxInBounds], 1);
            u8arr[idxInBounds] = 0;
            var idxOOB = 7516192768;
            assertEq(u8arr[idxOOB], undefined);
            var idxFractional = 7516192766 - 0.1;
            assertEq(u8arr[idxFractional], undefined);
            var idxNeg0 = -0;
            assertEq(u8arr[idxNeg0], 87);
            var idxNaN = NaN;
            assertEq(u8arr[idxNaN], undefined);
        }
    }
    testConstantDoubleIndex();

    function testDoubleIndexWeird() {
        var arr = [0.0, -0, 3.14, NaN, Infinity, -Infinity,
                Number.MIN_SAFE_INTEGER, Number.MAX_SAFE_INTEGER];
        for (var i = 0; i < 1500; i++) {
            var which = i % arr.length;
            var idx = arr[which];
            assertEq(u8arr[idx], which < 2 ? 87 : undefined);
        }
    }
    testDoubleIndexWeird();

    // Uses LCompare.
    function testHasElement1() {
        for (var i = 0; i < 1500; i++) {
            var idx = (length - 500) + i;
            assertEq(idx in u8arr, idx < length);
            assertEq(-1 in u8arr, false);
            assertEq(10737418240 in u8arr, false);
            assertEq(0x7fff_ffff in u8arr, true);
            assertEq(0xffff_ffff in u8arr, true);
        }
    }
    testHasElement1();

    // Uses LCompareAndBranch.
    function testHasElement2() {
        for (var i = 0; i < 1500; i++) {
            var idx = (length - 500) + i;
            if (idx in u8arr) {
                assertEq(idx < length, true);
            } else {
                assertEq(idx >= length, true);
            }
            var count = 0;
            if (-1 in u8arr) {
                count++;
            }
            if (10737418240 in u8arr) {
                count++;
            }
            if (0x7fff_ffff in u8arr) {
            } else {
                count++;
            }
            if (0xffff_ffff in u8arr) {
            } else {
                count++;
            }
            assertEq(count, 0);
        }
    }
    testHasElement2();
}
test(new Uint8Array(ab)); // 7 GB
test(new Uint8Array(ab, 0, 4 * gb)); // 4 GB

// |jit-test| test-also=--spectre-mitigations=off

// Ensure |index * ByteSize| overflowing int32_t is handled correctly in the
// backend.

const ab = new ArrayBuffer(7 * 1024 * 1024 * 1024);

function testInt16() {
    var ta = new Int16Array(ab);
    for (var i = 0; i < 2000; i++) {
        var idx = 1073741824; // 2147483648 / 2, offset doesn't fit in int32_t.
        assertEq(ta[idx], i);
        ++ta[idx];

        idx = 1073741823; // Largest offset that fits in int32_t.
        assertEq(ta[idx], i * 2);
        ta[idx] += 2;
    }
    ta[1073741823] = 0;
    ta[1073741824] = 0;
}
testInt16();

function testInt32() {
    var ta = new Int32Array(ab);
    for (var i = 0; i < 2000; i++) {
        var idx = 536870912; // 2147483648 / 4, offset doesn't fit in int32_t.
        assertEq(ta[idx], i);
        ++ta[idx];

        idx = 536870911; // Largest offset that fits in int32_t.
        assertEq(ta[idx], i * 2);
        ta[idx] += 2;
    }
    ta[536870911] = 0;
    ta[536870912] = 0;
}
testInt32();

function testFloat64() {
    var ta = new Float64Array(ab);
    for (var i = 0; i < 2000; i++) {
        var idx = 268435456; // 2147483648 / 8
        assertEq(ta[idx], i);
        ++ta[idx];

        idx = 268435455; // Largest offset that fits in int32_t.
        assertEq(ta[idx], i * 2);
        ta[idx] += 2;
    }
    ta[268435455] = 0;
    ta[268435456] = 0;
}
testFloat64();

function testBigInt() {
    var ta = new BigInt64Array(ab);
    for (var i = 0; i < 2000; i++) {
        var idx = 268435456; // 2147483648 / 8
        assertEq(ta[idx], BigInt(i));
        ++ta[idx];

        idx = 268435455; // Largest offset that fits in int32_t.
        assertEq(ta[idx], BigInt(i * 2));
        ta[idx] += 2n;
    }
    ta[268435455] = 0n;
    ta[268435456] = 0n;
}
testBigInt();

function testInt16Atomics() {
    var ta = new Int16Array(ab);
    for (var i = 0; i < 2000; i++) {
        var idx = 1073741824; // 2147483648 / 2, offset doesn't fit in int32_t.
        assertEq(Atomics.load(ta, idx), i);
        Atomics.add(ta, idx, 1);
        Atomics.exchange(ta, idx, 2);
        assertEq(ta[idx], 2);
        assertEq(Atomics.compareExchange(ta, idx, 2, 3), 2);
        Atomics.store(ta, idx, i + 1);

        idx = 1073741823; // Largest offset that fits in int32_t.
        assertEq(Atomics.load(ta, idx), i);
        Atomics.add(ta, idx, 1);
        Atomics.exchange(ta, idx, 2);
        assertEq(ta[idx], 2);
        assertEq(Atomics.compareExchange(ta, idx, 2, 3), 2);
        Atomics.store(ta, idx, i + 1);
    }
    ta[1073741823] = 0;
    ta[1073741824] = 0;
}
testInt16Atomics();

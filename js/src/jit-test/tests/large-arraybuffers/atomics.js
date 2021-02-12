// |jit-test| skip-if: !this.SharedArrayBuffer || !this.Atomics

let gb = 1 * 1024 * 1024 * 1024;
let buflen = 4 * gb + 64;
let sab = new SharedArrayBuffer(buflen);
assertEq(sab.byteLength, buflen);

function testBasic(base) {
    var uint8 = new Uint8Array(sab);
    var uint8Part = new Uint8Array(sab, base, 64);

    for (var i = 0; i < 50; i++) {
        var index = base + i;
        uint8Part[i] = 123;
        assertEq(uint8[index], 123);

        // Binary ops.
        assertEq(Atomics.add(uint8, index, 1), 123);
        assertEq(Atomics.and(uint8, index, 0xf), 124);
        assertEq(Atomics.or(uint8, index, 0xf), 12);
        assertEq(Atomics.xor(uint8, index, 0xee), 0xf);
        assertEq(Atomics.sub(uint8, index, 100), 225);
        assertEq(uint8Part[i], 125);

        // compareExchange.
        assertEq(Atomics.compareExchange(uint8, index, 125, 90), 125);
        assertEq(Atomics.compareExchange(uint8, index, 125, 90), 90);
        assertEq(uint8Part[i], 90);

        // exchange.
        assertEq(Atomics.exchange(uint8, index, 42), 90);
        assertEq(uint8Part[i], 42);

        // load/store.
        assertEq(Atomics.load(uint8, index), 42);
        assertEq(Atomics.store(uint8, index, 99), 99);
        assertEq(uint8Part[i], 99);
    }
}
for (let i = 0; i <= 4; i++) {
    testBasic(i * gb);
}

function testWait() {
    let int32 = new Int32Array(sab);
    let index = int32.length - 1;
    assertEq(int32[index], 0);
    assertEq(Atomics.wait(int32, index, 1), "not-equal");
    int32[index] = 12345;
    assertEq(Atomics.wait(int32, index, 12345, 1), "timed-out");
    assertEq(Atomics.notify(int32, index), 0);

    let int32WithOffset = new Int32Array(sab, int32.byteLength - 4);
    assertEq(int32WithOffset[0], 12345);
    assertEq(Atomics.wait(int32WithOffset, 0, 12345, 1), "timed-out");
}
testWait();

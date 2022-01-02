// Basic smoke tests for large ArrayBuffers.

let gb = 1 * 1024 * 1024 * 1024;

function test1() {
    let ab = new ArrayBuffer(7 * gb);
    assertEq(ab.byteLength, 7 * gb);

    let taInt16 = new Int16Array(ab);
    assertEq(taInt16.byteOffset, 0);
    assertEq(taInt16.byteLength, 7 * gb);
    assertEq(taInt16.length, 3.5 * gb);

    let taInt16LastGb = new Int16Array(ab, 6 * gb);
    assertEq(taInt16LastGb.byteOffset, 6 * gb);
    assertEq(taInt16LastGb.byteLength, 1 * gb);
    assertEq(taInt16LastGb.length, 0.5 * gb);

    taInt16LastGb[0] = -99;
    assertEq(taInt16[3 * gb], -99);

    let taUint8 = new Uint8Array(ab);
    assertEq(taUint8.length, 7 * gb);
    assertEq(taUint8[7 * gb - 1], 0);

    taUint8[7 * gb - 1] = 42;
    taUint8[7 * gb - 1]++;
    ++taUint8[7 * gb - 1];
    assertEq(taUint8[7 * gb - 1], 44);

    let dv = new DataView(ab);
    assertEq(dv.getInt16(6 * gb, true), -99);
    assertEq(dv.getUint8(7 * gb - 1, true), 44);

    dv.setInt16(6 * gb + 2, 123, true);
    assertEq(taInt16LastGb[1], 123);
}
test1();

function test2() {
    let taInt8 = new Int8Array(4 * gb);
    assertEq(taInt8.length, 4 * gb);
    taInt8[4 * gb - 4] = 42;
    assertEq(taInt8[4 * gb - 4], 42);

    let dv = new DataView(taInt8.buffer);
    assertEq(dv.getInt32(4 * gb - 4, true), 42);
    assertEq(dv.getBigInt64(4 * gb - 8, true), 180388626432n);
}
test2();

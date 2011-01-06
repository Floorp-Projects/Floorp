// vim: set ts=4 sw=4 tw=99 et:

function testSetTypedInt8Array(k) {
    var ar = new Int8Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = k + 800;
    var t = k + 555;
    var t = ar[k+7] = t & 5;
    ar[0] = 12;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], -12);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], 32);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 1);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedUint8ClampedArray(k) {
    var ar = new Uint8ClampedArray(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = k + 800;
    var t = k + 555;
    var L = ar[k+7] = t & 5;
    var Q = ar[k+7] = t + 5;
    ar[0] = 12;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = -500;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], 0);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], 255);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 255);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedUint8Array(k) {
    var ar = new Uint8Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = k + 800;
    var t = k + 555;
    var L = ar[k+7] = t + 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], 244);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], 32);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 48);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedInt16Array(k) {
    var ar = new Int16Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 800 * 800 * 913;
    var t = k + 555;
    var L = ar[k+7] = t + 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500000;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], -24288);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], -32768);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 560);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedUint16Array(k) {
    var ar = new Uint16Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 800 * 800 * 913;
    var t = k + 555;
    var L = ar[k+7] = t + 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500000;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], 41248);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], 32768);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 560);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedInt32Array(k) {
    var ar = new Int32Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 800 * 800 * 800 * 800;
    var t = k + 555;
    var L = ar[k+7] = t + 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], 500);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], -234881024);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 560);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

function testSetTypedUint32Array(k) {
    var ar = new Uint32Array(8);
    ar[k+5] = { };
    ar[k+6] = ar;
    ar[k+4] = (k + 800) * 800 * 800 * 800 * 800;
    var t = k + 555;
    var L = ar[k+7] = t + 5;
    ar[0] = 12.3;
    ar[8] = 500;
    ar[k+8] = 1200;
    ar[k+1] = 500;
    ar[k+2] = "3";
    ar[k+3] = true;
    assertEq(ar[0], 12);
    assertEq(ar[1], 500);
    assertEq(ar[2], 3);
    assertEq(ar[3], 1);
    assertEq(ar[4], 4060086272);
    assertEq(ar[5], 0);
    assertEq(ar[6], 0);
    assertEq(ar[7], 560);
    assertEq(ar[8], undefined);
    assertEq(ar[k+8], undefined);
}

for (var i = 0; i <= 10; i++) {
    testSetTypedInt8Array(0);
    testSetTypedUint8Array(0);
    testSetTypedUint8ClampedArray(0);
    testSetTypedInt16Array(0);
    testSetTypedUint16Array(0);
    testSetTypedInt32Array(0);
    testSetTypedUint32Array(0);
    if (i == 5)
        gc();
}


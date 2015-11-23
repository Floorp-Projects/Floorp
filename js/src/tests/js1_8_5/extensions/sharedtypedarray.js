// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Minimal test cases.  Note that on 64-bit a SharedArrayBuffer is
// very expensive under these rules - a 4GB area is reserved for it.
// So don't go allocating a ton of them.
//
// These tests cannot test that sharing works across workers.  There
// are or will be tests, in dom/workers, that do that.

if (!this.SharedArrayBuffer)
    quit(0);

var b;

function testSharedArrayBuffer() {
    b = new SharedArrayBuffer("4096"); // Test string conversion, too
    assertEq(b instanceof SharedArrayBuffer, true);
    assertEq(b.byteLength, 4096);

    assertEq(!!SharedArrayBuffer.isView, true);

    b.fnord = "Hi there";
    assertEq(b.fnord, "Hi there");

    SharedArrayBuffer.prototype.abracadabra = "no wishing for wishes!";
    assertEq(b.abracadabra, "no wishing for wishes!");

    // can "convert" a buffer (really works as an assertion)
    assertEq(SharedArrayBuffer(b), b);

    // can't convert any other object
    assertThrowsInstanceOf(() => SharedArrayBuffer({}), TypeError);

    // byteLength can be invoked as per normal, indirectly
    assertEq(Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype,"byteLength").get.call(b), 4096);

    // however byteLength is not generic
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(SharedArrayBuffer.prototype,"byteLength").get.call({}), TypeError);

}

function testSharedTypedArray() {
    var x1 = new Int8Array(b);
    var x2 = new Int32Array(b);

    assertEq(SharedArrayBuffer.isView(x1), true);
    assertEq(SharedArrayBuffer.isView(x2), true);
    assertEq(SharedArrayBuffer.isView({}), false);

    assertEq(x1.buffer, b);
    assertEq(x2.buffer, b);

    assertEq(x1.byteLength, b.byteLength);
    assertEq(x2.byteLength, b.byteLength);

    assertEq(x1.byteOffset, 0);
    assertEq(x2.byteOffset, 0);

    assertEq(x1.length, b.byteLength);
    assertEq(x2.length, b.byteLength / 4);

    var x3 = new Int8Array(b, 20);
    assertEq(x3.length, b.byteLength - 20);
    assertEq(x3.byteOffset, 20);

    var x3 = new Int32Array(b, 20, 10);
    assertEq(x3.length, 10);
    assertEq(x3.byteOffset, 20);

    // Mismatched type
    assertThrowsInstanceOf(() => Int8Array(x2), TypeError);

    // Unconvertable object
    assertThrowsInstanceOf(() => Int8Array({}), TypeError);

    // negative start
    assertThrowsInstanceOf(() => new Int8Array(b, -7), RangeError);

    // not congruent mod element size
    assertThrowsInstanceOf(() => new Int32Array(b, 3), TypeError); // Bug 1227207: should be RangeError

    // start out of range
    assertThrowsInstanceOf(() => new Int32Array(b, 4104), TypeError); // Ditto

    // end out of range
    assertThrowsInstanceOf(() => new Int32Array(b, 4092, 2), TypeError); // Ditto

    // Views alias the storage
    x2[0] = -1;
    assertEq(x1[0], -1);
    x1[0] = 1;
    x1[1] = 1;
    x1[2] = 1;
    x1[3] = 1;
    assertEq(x2[0], 0x01010101);

    assertEq(x2[5], 0);
    x3[0] = -1;
    assertEq(x2[5], -1);

    // Out-of-range accesses yield undefined
    assertEq(x2[1023], 0);
    assertEq(x2[1024], undefined);
    assertEq(x2[2047], undefined);
}

function testSharedTypedArrayMethods() {
    var v = new Int32Array(b);
    for ( var i=0 ; i < v.length ; i++ )
        v[i] = i;

    // Rudimentary tests - they don't test any boundary conditions

    // subarray
    var w = v.subarray(10, 20);
    assertEq(w.length, 10);
    for ( var i=0 ; i < w.length ; i++ )
        assertEq(w[i], v[i+10]);
    for ( var i=0 ; i < w.length ; i++ )
        w[i] = -w[i];
    for ( var i=0 ; i < w.length ; i++ )
        assertEq(w[i], v[i+10]);

    // copyWithin
    for ( var i=0 ; i < v.length ; i++ )
        v[i] = i;
    v.copyWithin(10, 20, 30);
    for ( var i=0 ; i < 10 ; i++ )
        assertEq(v[i], i);
    for ( var i=10 ; i < 20 ; i++ )
        assertEq(v[i], v[i+10]);

    // set
    for ( var i=0 ; i < v.length ; i++ )
        v[i] = i;
    v.set([-1,-2,-3,-4,-5,-6,-7,-8,-9,-10], 5);
    for ( var i=5 ; i < 15 ; i++ )
        assertEq(v[i], -i+4);

    // In the following two cases the two arrays will reference the same buffer,
    // so there will be an overlapping copy.
    //
    // Case 1: Read from lower indices than are written
    v.set(v.subarray(0, 7), 1);
    assertEq(v[0], 0);
    assertEq(v[1], 0);
    assertEq(v[2], 1);
    assertEq(v[3], 2);
    assertEq(v[4], 3);
    assertEq(v[5], 4);
    assertEq(v[6], -1);
    assertEq(v[7], -2);
    assertEq(v[8], -4);
    assertEq(v[9], -5);

    // Case 2: Read from higher indices than are written
    v.set(v.subarray(1, 5), 0);
    assertEq(v[0], 0);
    assertEq(v[1], 1);
    assertEq(v[2], 2);
    assertEq(v[3], 3);
    assertEq(v[4], 3);
    assertEq(v[5], 4);
    assertEq(v[6], -1);
    assertEq(v[7], -2);
    assertEq(v[8], -4);
    assertEq(v[9], -5);
}

function testClone1() {
    var sab1 = b;
    var blob = serialize(sab1, [sab1]);
    var sab2 = deserialize(blob);
    assertEq(sharedAddress(sab1), sharedAddress(sab2));
}

function testClone2() {
    var sab = b;
    var ia1 = new Int32Array(sab);
    var blob = serialize(ia1, [sab]);
    var ia2 = deserialize(blob);
    assertEq(ia1.length, ia2.length);
    assertEq(ia1.buffer instanceof SharedArrayBuffer, true);
    assertEq(sharedAddress(ia1.buffer), sharedAddress(ia2.buffer));
    ia1[10] = 37;
    assertEq(ia2[10], 37);
}

testSharedArrayBuffer();
testSharedTypedArray();
testSharedTypedArrayMethods();
testClone1();
testClone2();

reportCompare(0, 0, 'ok');

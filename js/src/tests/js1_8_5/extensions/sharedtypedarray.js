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
//
// Structured cloning is not tested here (there are no APIs).

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
    assertEq(SharedInt8Array.prototype.BYTES_PER_ELEMENT, 1);
    assertEq(SharedUint8Array.prototype.BYTES_PER_ELEMENT, 1);
    assertEq(SharedUint8ClampedArray.prototype.BYTES_PER_ELEMENT, 1);
    assertEq(SharedInt16Array.prototype.BYTES_PER_ELEMENT, 2);
    assertEq(SharedUint16Array.prototype.BYTES_PER_ELEMENT, 2);
    assertEq(SharedInt32Array.prototype.BYTES_PER_ELEMENT, 4);
    assertEq(SharedUint32Array.prototype.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat32Array.prototype.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat32Array.prototype.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat64Array.prototype.BYTES_PER_ELEMENT, 8);
    assertEq(SharedFloat64Array.prototype.BYTES_PER_ELEMENT, 8);

    assertEq(SharedInt8Array.BYTES_PER_ELEMENT, 1);
    assertEq(SharedUint8Array.BYTES_PER_ELEMENT, 1);
    assertEq(SharedUint8ClampedArray.BYTES_PER_ELEMENT, 1);
    assertEq(SharedInt16Array.BYTES_PER_ELEMENT, 2);
    assertEq(SharedUint16Array.BYTES_PER_ELEMENT, 2);
    assertEq(SharedInt32Array.BYTES_PER_ELEMENT, 4);
    assertEq(SharedUint32Array.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat32Array.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat32Array.BYTES_PER_ELEMENT, 4);
    assertEq(SharedFloat64Array.BYTES_PER_ELEMENT, 8);
    assertEq(SharedFloat64Array.BYTES_PER_ELEMENT, 8);

    // Distinct prototypes for distinct types
    SharedInt8Array.prototype.hello = "hi there";
    assertEq(SharedUint8Array.prototype.hello, undefined);

    var x1 = new SharedInt8Array(b);
    var x2 = new SharedInt32Array(b);

    assertEq(SharedArrayBuffer.isView(x1), true);
    assertEq(SharedArrayBuffer.isView(x2), true);
    assertEq(SharedArrayBuffer.isView({}), false);
    assertEq(SharedArrayBuffer.isView(new Int32Array(10)), false);

    assertEq(x1.buffer, b);
    assertEq(x2.buffer, b);

    assertEq(Object.getOwnPropertyDescriptor(SharedInt8Array.prototype,"buffer").get.call(x1), x1.buffer);
    assertEq(Object.getOwnPropertyDescriptor(SharedInt32Array.prototype,"buffer").get.call(x2), x2.buffer);

    // Not generic
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(SharedInt8Array.prototype,"buffer").get.call({}), TypeError);

    // Not even to other shared typed arrays
    assertThrowsInstanceOf(() => Object.getOwnPropertyDescriptor(SharedInt8Array.prototype,"buffer").get.call(x2), TypeError);

    assertEq(x1.byteLength, b.byteLength);
    assertEq(x2.byteLength, b.byteLength);

    assertEq(x1.byteOffset, 0);
    assertEq(x2.byteOffset, 0);

    assertEq(x1.length, b.byteLength);
    assertEq(x2.length, b.byteLength / 4);

    // Conversions that should work
    assertEq(SharedInt8Array(x1), x1);
    assertEq(SharedInt32Array(x2), x2);

    var x3 = new SharedInt8Array(b, 20);
    assertEq(x3.length, b.byteLength - 20);
    assertEq(x3.byteOffset, 20);

    var x3 = new SharedInt32Array(b, 20, 10);
    assertEq(x3.length, 10);
    assertEq(x3.byteOffset, 20);

    // Mismatched type
    assertThrowsInstanceOf(() => SharedInt8Array(x2), TypeError);

    // Unconvertable object
    assertThrowsInstanceOf(() => SharedInt8Array({}), TypeError);

    // negative start
    assertThrowsInstanceOf(() => new SharedInt8Array(b, -7), RangeError);

    // not congruent mod element size
    assertThrowsInstanceOf(() => new SharedInt32Array(b, 3), RangeError);

    // start out of range
    assertThrowsInstanceOf(() => new SharedInt32Array(b, 4104), RangeError);

    // end out of range
    assertThrowsInstanceOf(() => new SharedInt32Array(b, 4092, 2), RangeError);

    var b2 = new SharedInt32Array(1024); // Should create a new buffer
    assertEq(b2.length, 1024);
    assertEq(b2.byteLength, 4096);
    assertEq(b2.byteOffset, 0);
    assertEq(b2.buffer != b, true);

    var b3 = new SharedInt32Array("1024"); // Should create a new buffer
    assertEq(b3.length, 1024);
    assertEq(b3.byteLength, 4096);
    assertEq(b3.byteOffset, 0);
    assertEq(b3.buffer != b, true);

    // bad length
    assertThrowsInstanceOf(() => new SharedInt32Array({}), TypeError);

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
    var v = new SharedInt32Array(b);
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

if (typeof SharedArrayBuffer === "function") {
    testSharedArrayBuffer();
    testSharedTypedArray();
    testSharedTypedArrayMethods();
}

reportCompare(0, 0, 'ok');

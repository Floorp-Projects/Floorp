load(libdir + "asserts.js");

var gb = 1 * 1024 * 1024 * 1024;
var ta = new Uint8Array(4 * gb + 10);

function testSetFromTyped() {
    var ta2 = new Int8Array(10);
    ta2[0] = 23;
    ta2[9] = -10;
    ta.set(ta2, 4 * gb);
    assertEq(ta[4 * gb + 0], 23);
    assertEq(ta[4 * gb + 9], 246);
}
testSetFromTyped();

function testSetFromOther() {
    ta.set([12, 34], 4 * gb + 4);
    assertEq(ta[4 * gb + 4], 12);
    assertEq(ta[4 * gb + 5], 34);
}
testSetFromOther();

function testCopyWithin() {
    // Large |start|.
    ta[ta.length - 1] = 3;
    ta.copyWithin(0, 4 * gb);
    assertEq(ta[9], 3);

    // Large relative |start|.
    ta[ta.length - 1] = 4;
    ta.copyWithin(0, -10);
    assertEq(ta[9], 4);

    // Large |start| and |end|.
    ta[ta.length - 3] = 5;
    ta[ta.length - 2] = 66;
    ta[1] = 1;
    ta.copyWithin(0, ta.length - 3, ta.length - 2);
    assertEq(ta[0], 5);
    assertEq(ta[1], 1);

    // Large |target| and |start|.
    ta.copyWithin(4 * gb + 5, 4 * gb + 7);
    assertEq(ta[4 * gb + 6], 66);

    // Large |target|.
    ta[6] = 117;
    ta.copyWithin(4 * gb);
    assertEq(ta[4 * gb + 6], 117);
}
testCopyWithin();

function testSlice() {
    // Large |start| and |end|.
    ta[4 * gb + 0] = 11;
    ta[4 * gb + 1] = 22;
    ta[4 * gb + 2] = 33;
    ta[4 * gb + 3] = 44;
    var ta2 = ta.slice(4 * gb, 4 * gb + 4);
    assertEq(ta2.toString(), "11,22,33,44");

    // Large |start|.
    ta[ta.length - 3] = 99;
    ta[ta.length - 2] = 88;
    ta[ta.length - 1] = 77;
    ta2 = ta.slice(4 * gb + 8);
    assertEq(ta2.toString(), "88,77");

    // Relative values.
    ta2 = ta.slice(-3, -1);
    assertEq(ta2.toString(), "99,88");

    // Large relative values.
    ta[0] = 100;
    ta[1] = 101;
    ta[2] = 102;
    ta2 = ta.slice(-ta.length, -ta.length + 3);
    assertEq(ta2.toString(), "100,101,102");
}
testSlice();

function testSubarray() {
    // Large |start| and |end|.
    ta[4 * gb + 0] = 11;
    ta[4 * gb + 1] = 22;
    ta[4 * gb + 2] = 33;
    ta[4 * gb + 3] = 44;
    var ta2 = ta.subarray(4 * gb, 4 * gb + 4);
    assertEq(ta2.toString(), "11,22,33,44");

    // Large |start|.
    ta[ta.length - 3] = 99;
    ta[ta.length - 2] = 88;
    ta[ta.length - 1] = 77;
    ta2 = ta.subarray(4 * gb + 8);
    assertEq(ta2.toString(), "88,77");

    // Relative values.
    ta2 = ta.subarray(-3, -1);
    assertEq(ta2.toString(), "99,88");

    // Large relative values.
    ta[0] = 100;
    ta[1] = 101;
    ta[2] = 102;
    ta2 = ta.subarray(-ta.length, -ta.length + 3);
    assertEq(ta2.toString(), "100,101,102");
}
testSubarray();

function testIterators() {
    var ex;

    ex = null;
    try {
        for (var p in ta) {}
    } catch (e) {
        ex = e;
    }
    assertEq(ex, "out of memory");

    ex = null;
    try {
        Object.getOwnPropertyNames(ta);
    } catch (e) {
        ex = e;
    }
    assertEq(ex, "out of memory");
}
testIterators();

function testArraySliceSparse() {
    // Length mustn't exceed UINT32_MAX.
    var len = 4 * gb - 1;
    var ta2 = new Int8Array(ta.buffer, 0, len);
    ta2[len - 1] = 1;

    // The SliceSparse optimisation is only used for native objects which have the
    // "indexed" flag set.
    var o = {
        length: len,
        100000: 0, // sparse + indexed
        __proto__: ta2,
    };

    // Collect sufficient elements to trigger the SliceSparse optimisation.
    var r = Array.prototype.slice.call(o, -2000);
    assertEq(r.length, 2000);
    assertEq(r[r.length - 1], 1);
}
testArraySliceSparse();

function testArrayIterator() {
    for (var i = 0; i < 20; i++) {
        ta[i] = i;
    }
    var sum = 0;
    var i = 0;
    for (var x of ta) {
        if (i++ > 20) {
            break;
        }
        sum += x;
    }
    assertEq(sum, 190);
}
testArrayIterator();

function testArrayBufferSlice() {
    // Large |start| and |end|.
    ta[4 * gb + 0] = 11;
    ta[4 * gb + 1] = 22;
    ta[4 * gb + 2] = 33;
    ta[4 * gb + 3] = 44;
    var ta2 = new Uint8Array(ta.buffer.slice(4 * gb, 4 * gb + 4));
    assertEq(ta2.toString(), "11,22,33,44");

    // Large |start|.
    ta[ta.length - 3] = 99;
    ta[ta.length - 2] = 88;
    ta[ta.length - 1] = 77;
    ta2 = new Uint8Array(ta.buffer.slice(4 * gb + 8));
    assertEq(ta2.toString(), "88,77");

    // Relative values.
    ta2 = new Uint8Array(ta.buffer.slice(-3, -1));
    assertEq(ta2.toString(), "99,88");

    // Large relative values.
    ta[0] = 100;
    ta[1] = 101;
    ta[2] = 102;
    ta2 = new Uint8Array(ta.buffer.slice(-ta.length, -ta.length + 3));
    assertEq(ta2.toString(), "100,101,102");
}
testArrayBufferSlice();

function testFromObjectTooLargeLength() {
    assertThrowsInstanceOf(() => new Uint8Array({length: 9 * gb}), RangeError);
    assertThrowsInstanceOf(() => ta.set({length: 9 * gb}), RangeError);
}
testFromObjectTooLargeLength();

// Test cases for subclasses of SharedArrayBuffer.

if (!this.SharedArrayBuffer)
    quit(0);

load(libdir + "asserts.js");

// Basic subclassing.

class MySharedArrayBuffer1 extends SharedArrayBuffer {
    constructor(n) { super(n) }
}

let mv1 = new MySharedArrayBuffer1(1024);
assertEq(mv1 instanceof SharedArrayBuffer, true);
assertEq(mv1 instanceof MySharedArrayBuffer1, true);
assertEq(mv1.byteLength, 1024);

// Can construct views on the subclasses and read/write elements.

let mva1 = new Int8Array(mv1);
assertEq(mva1.length, mv1.byteLength);
assertEq(mva1.buffer, mv1);

for ( let i=1 ; i < mva1.length ; i++ )
    mva1[i] = i;

for ( let i=1 ; i < mva1.length ; i++ )
    assertEq(mva1[i], (i << 24) >> 24);

// Passing modified arguments to superclass to get a different length.

class MySharedArrayBuffer2 extends SharedArrayBuffer {
    constructor(n) { super(n-1) }
}

let mv2 = new MySharedArrayBuffer2(10);
assertEq(mv2 instanceof SharedArrayBuffer, true);
assertEq(mv2 instanceof MySharedArrayBuffer2, true);
assertEq(mv2.byteLength, 9);

// Returning a different object altogether.

class MySharedArrayBuffer3 extends SharedArrayBuffer {
    constructor(n) {
	return new Array(n);
    }
}

let mv3 = new MySharedArrayBuffer3(10);
assertEq(mv3 instanceof Array, true);
assertEq(mv3 instanceof MySharedArrayBuffer3, false);
assertEq(mv3.length, 10);

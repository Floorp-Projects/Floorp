// SharedArrayBuffer.prototype.slice

if (!this.SharedArrayBuffer)
    quit(0);

load(libdir + "asserts.js");

let buf = new SharedArrayBuffer(1024);
let bufAsI8 = new Int8Array(buf);
for ( let i=0 ; i < buf.length ; i++ )
    bufAsI8[i] = i;

let base = 10;
let len = 10;

let buf2 = buf.slice(base, base+len);

// Smells right?
assertEq(buf2 instanceof SharedArrayBuffer, true);
assertEq(buf2.byteLength, len);

// Data got copied correctly?
let buf2AsI8 = new Int8Array(buf2);
for ( let i=0 ; i < buf2AsI8.length ; i++ )
    assertEq(buf2AsI8[i], bufAsI8[base+i]);

// Storage not shared?
let correct = bufAsI8[base];
bufAsI8[base]++;
assertEq(buf2AsI8[0], correct);

// Start beyond end
let notail = buf.slice(buf.byteLength+1);
assertEq(notail.byteLength, 0);

// Negative start
let tail = buf.slice(-5, buf.byteLength);
assertEq(tail.byteLength, 5);
let tailAsI8 = new Int8Array(tail);
for ( let i=0 ; i < tailAsI8.length ; i++ )
    assertEq(tailAsI8[i], bufAsI8[buf.byteLength-5+i]);

// Negative end
let head = buf.slice(0, -5);
assertEq(head.byteLength, buf.byteLength-5);
let headAsI8 = new Int8Array(head);
for ( let i=0 ; i < headAsI8.length ; i++ )
    assertEq(headAsI8[i], bufAsI8[i]);

// Subtyping
class MySharedArrayBuffer1 extends SharedArrayBuffer {
    constructor(n) { super(n) }
}

let myBuf = new MySharedArrayBuffer1(1024);

let myBufAsI8 = new Int8Array(myBuf);
for ( let i=0 ; i < myBuf.length ; i++ )
    myBufAsI8[i] = i;

let myBufSlice = myBuf.slice(0, 20);

assertEq(myBufSlice instanceof MySharedArrayBuffer1, true);

assertEq(myBufSlice.byteLength, 20);

let myBufSliceAsI8 = new Int8Array(myBufSlice);
for ( let i=0 ; i < myBufSlice.length ; i++ )
    assertEq(myBufAsI8[i], myBufSliceAsI8[i]);

// Error mode: the method requires an object
assertThrowsInstanceOf(() => buf.slice.call(false, 0, 1), TypeError);

// Error mode: the method is not generic.
assertThrowsInstanceOf(() => buf.slice.call([1,2,3], 0, 1), TypeError);

// Error mode (step 15): the buffer constructed on behalf of slice
// is too short.

class MySharedArrayBuffer2 extends SharedArrayBuffer {
    constructor(n) { super(n-1) }
}

let myBuf2 = new MySharedArrayBuffer2(10);

assertThrowsInstanceOf(() => myBuf2.slice(0, 5), TypeError);

// Error mode (step 13): the buffer constructed on behalf of slice
// is not a SharedArrayBuffer.

let subvert = false;

class MySharedArrayBuffer3 extends SharedArrayBuffer {
    constructor(n) {
	super(n);
	if (subvert)
	    return new Array(n);
    }
}

let myBuf3 = new MySharedArrayBuffer3(10);

subvert = true;
assertThrowsInstanceOf(() => myBuf3.slice(0, 5), TypeError);

// Error mode (step 14): the buffer constructed on behalf of slice
// is the same as the input buffer.

let sneaky = null;

class MySharedArrayBuffer4 extends SharedArrayBuffer {
    constructor(n) {
	super(n);
	if (sneaky)
	    return sneaky;
    }
}

let myBuf4 = new MySharedArrayBuffer4(10);

sneaky = myBuf4;
assertThrowsInstanceOf(() => myBuf4.slice(0, 5), TypeError);

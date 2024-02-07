// |jit-test| --enable-arraybuffer-resizable; skip-if: !this.SharedArrayBuffer?.prototype?.grow

var gsab = new SharedArrayBuffer(4, {maxByteLength: 16});

// Test byte lengths are correct.
assertEq(gsab.byteLength, 4);
assertEq(gsab.maxByteLength, 16);

// Pass |gsab| to the mailbox.
setSharedObject(gsab);

// Retrieve again from the mailbox to create a new growable shared array buffer
// which points to the same memory.
var gsab2 = getSharedObject();

assertEq(gsab !== gsab2, true, "different objects expected");

// Byte lengths are correct for both objects.
assertEq(gsab.byteLength, 4);
assertEq(gsab.maxByteLength, 16);
assertEq(gsab2.byteLength, 4);
assertEq(gsab2.maxByteLength, 16);

// Grow the original object.
gsab.grow(6);

// Byte lengths are correct for both objects.
assertEq(gsab.byteLength, 6);
assertEq(gsab.maxByteLength, 16);
assertEq(gsab2.byteLength, 6);
assertEq(gsab2.maxByteLength, 16);

// Grow the copy.
gsab2.grow(8);

// Byte lengths are correct for both objects.
assertEq(gsab.byteLength, 8);
assertEq(gsab.maxByteLength, 16);
assertEq(gsab2.byteLength, 8);
assertEq(gsab2.maxByteLength, 16);

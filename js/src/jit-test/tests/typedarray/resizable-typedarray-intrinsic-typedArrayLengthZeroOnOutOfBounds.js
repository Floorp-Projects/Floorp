// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

load(libdir + "asserts.js");

const TypedArrayLengthZeroOnOutOfBounds = getSelfHostedValue("TypedArrayLengthZeroOnOutOfBounds");

function testTypedArrayLength() {
  let ab = new ArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayLengthZeroOnOutOfBounds(ta), 100 - (i & 3));
  }
}
testTypedArrayLength();

function testTypedArrayLengthOutOfBounds() {
  let ab = new ArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab, 0, 10),
    new Int8Array(ab, 1, 10),
    new Int8Array(ab, 2, 10),
    new Int8Array(ab, 3, 10),
  ];

  // Resize to zero to make all views out-of-bounds.
  ab.resize(0);

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayLengthZeroOnOutOfBounds(ta), 0);
  }
}
testTypedArrayLengthOutOfBounds();

function testTypedArrayLengthDetached() {
  let ab = new ArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab, 0, 10),
    new Int8Array(ab, 1, 10),
    new Int8Array(ab, 2, 10),
    new Int8Array(ab, 3, 10),
  ];

  // Detach the buffer.
  ab.transfer();

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayLengthZeroOnOutOfBounds(ta), 0);
  }
}
testTypedArrayLengthDetached();

function testTypedArrayLengthWithShared() {
  let ab = new SharedArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayLengthZeroOnOutOfBounds(ta), 100 - (i & 3));
  }
}
testTypedArrayLengthWithShared();

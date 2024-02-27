// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

const TypedArrayByteOffset = getSelfHostedValue("TypedArrayByteOffset");

function testTypedArrayByteOffset() {
  let ab = new ArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayByteOffset(ta), i & 3);
  }
}
testTypedArrayByteOffset();

function testTypedArrayByteOffsetOutOfBounds() {
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
    assertEq(TypedArrayByteOffset(ta), i & 3);
  }
}
testTypedArrayByteOffsetOutOfBounds();

function testTypedArrayByteOffsetDetached() {
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
    assertEq(TypedArrayByteOffset(ta), 0);
  }
}
testTypedArrayByteOffsetDetached();

function testTypedArrayByteOffsetWithShared() {
  let ab = new SharedArrayBuffer(100, {maxByteLength: 100});
  let typedArrays = [
    new Int8Array(ab),
    new Int8Array(ab, 1),
    new Int8Array(ab, 2),
    new Int8Array(ab, 3),
  ];

  for (let i = 0; i < 200; ++i) {
    let ta = typedArrays[i & 3];
    assertEq(TypedArrayByteOffset(ta), i & 3);
  }
}
testTypedArrayByteOffsetWithShared();

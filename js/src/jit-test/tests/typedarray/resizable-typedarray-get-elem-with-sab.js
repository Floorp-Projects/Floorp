// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize||!this.SharedArrayBuffer

const TypedArrays = [
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array,
];

function test(TA) {
  const length = 4;
  const byteLength = length * TA.BYTES_PER_ELEMENT;

  let rab = new SharedArrayBuffer(byteLength, {maxByteLength: byteLength});
  let actual = new TA(rab);
  let expected = new TA(length);
  let type = expected[0].constructor;

  for (let i = 0; i < length; ++i) {
    actual[i] = type(i * i);
    expected[i] = type(i * i);
  }

  // In-bounds access
  for (let i = 0; i < 200; ++i) {
    let index = i % length;
    assertEq(actual[index], expected[index]);
  }

  // Out-of-bounds access
  for (let i = 0; i < 200; ++i) {
    let index = i % (length + 4);
    assertEq(actual[index], expected[index]);
  }
}

for (let TA of TypedArrays) {
  // Copy test function to ensure monomorphic ICs.
  let copy = Function(`return ${test}`)();

  copy(TA);
}

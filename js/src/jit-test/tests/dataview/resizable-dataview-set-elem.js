// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

load(libdir + "dataview.js");

const TypedArrays = [
  Int8Array,
  Uint8Array,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  BigInt64Array,
  BigUint64Array,
];

function test(TA) {
  const length = 4;
  const byteLength = length * TA.BYTES_PER_ELEMENT;

  let rab = new ArrayBuffer(byteLength, {maxByteLength: byteLength});
  let actual = new TA(rab);
  let expected = new TA(length);
  let type = expected[0].constructor;

  let dv = new DataView(rab);
  for (let i = 0; i < 200; ++i) {
    let index = i % length;
    let byteIndex = index * TA.BYTES_PER_ELEMENT;

    let v = type(i);
    dv.setElem(byteIndex, v, nativeIsLittleEndian);
    expected[index] = v;

    assertEq(actual[index], expected[index]);
  }
}

for (let TA of TypedArrays) {
  let setter = "set" + typeName(TA);

  // Copy test function to ensure monomorphic ICs.
  let copy = Function(`return ${test}`.replaceAll("setElem", setter))();

  copy(TA);
}

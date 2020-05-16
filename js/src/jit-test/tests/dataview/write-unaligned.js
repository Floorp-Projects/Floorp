// Test unaligned write access.

load(libdir + "dataview.js");

// Create a new test function for each scalar type.
function createWrite(data) {
  const name = typeName(data.type);
  const offset = 1;

  return Function("data", `
    const {values, littleEndian, bigEndian} = data;

    // Load from array so that Ion doesn't treat as constants.
    const True = [true, 1];
    const False = [false, 0];

    const src = new ${data.type.name}(values);

    const ab = new ArrayBuffer(${data.type.BYTES_PER_ELEMENT + offset});
    const dv = new DataView(ab);

    const srcUint8 = new Uint8Array(src.buffer);
    const dstUint8 = new Uint8Array(ab);

    function assertSameContents(idx, msg) {
      for (let i = 0; i < ${data.type.BYTES_PER_ELEMENT}; ++i) {
        assertEq(dstUint8[i + ${offset}], srcUint8[i + idx * ${data.type.BYTES_PER_ELEMENT}]);
      }
    }

    for (let i = 0; i < 100; ++i) {
      let j = i % values.length;

      // Skip over NaNs to avoid false-negatives due to NaN canonicalisation.
      if (${name === "Float32" || name === "Float64"}) {
        if (Number.isNaN(bigEndian[j]) || Number.isNaN(littleEndian[j])) {
          continue;
        }
      }

      dstUint8.fill(0);
      dv.set${name}(${offset}, bigEndian[j]);
      assertSameContents(j, "default");

      dstUint8.fill(0);
      dv.set${name}(${offset}, littleEndian[j], true);
      assertSameContents(j, "little");

      dstUint8.fill(0);
      dv.set${name}(${offset}, bigEndian[j], false);
      assertSameContents(j, "big");

      dstUint8.fill(0);
      dv.set${name}(${offset}, littleEndian[j], True[i & 1]);
      assertSameContents(j, "little, dynamic");

      dstUint8.fill(0);
      dv.set${name}(${offset}, bigEndian[j], False[i & 1]);
      assertSameContents(j, "big, dynamic");
    }
  `);
}

for (let data of createTestData()) {
  let f = createWrite(data);

  for (let i = 0; i < 2; ++i) {
    f(data);
  }
}

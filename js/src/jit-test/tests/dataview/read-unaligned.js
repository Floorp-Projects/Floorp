// Test unaligned read access.

load(libdir + "dataview.js");

// Create a new test function for each scalar type.
function createRead(data) {
  const name = typeName(data.type);
  const offset = 1;

  return Function("data", `
    const {values, littleEndian, bigEndian} = data;

    // Load from array so that Ion doesn't treat as constants.
    const True = [true, 1];
    const False = [false, 0];

    const ab = new ArrayBuffer(${data.values.length * data.type.BYTES_PER_ELEMENT + offset});
    const dv = new DataView(ab);

    new ${data.type.name}(ab, 0, ${data.values.length}).set(values);

    new Uint8Array(ab).copyWithin(${offset}, 0);

    for (let i = 0; i < 100; ++i) {
      let j = i % values.length;
      let index = j * ${data.type.BYTES_PER_ELEMENT} + ${offset};

      let v1 = dv.get${name}(index);
      assertEq(v1, bigEndian[j]);

      let v2 = dv.get${name}(index, true);
      assertEq(v2, littleEndian[j]);

      let v3 = dv.get${name}(index, false);
      assertEq(v3, bigEndian[j]);

      let v4 = dv.get${name}(index, True[i & 1]);
      assertEq(v4, littleEndian[j]);

      let v5 = dv.get${name}(index, False[i & 1]);
      assertEq(v5, bigEndian[j]);
    }
  `);
}

for (let data of createTestData()) {
  let f = createRead(data);

  for (let i = 0; i < 2; ++i) {
    f(data);
  }
}

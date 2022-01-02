function typeName(typedArrayCtor) {
  return typedArrayCtor.name.slice(0, -"Array".length);
}

const nativeIsLittleEndian = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

function toEndianess(type, v, littleEndian) {
  // Disable Ion compilation to call the native, non-inlined DataView functions.
  with ({}); // no-ion
  assertEq(inIon() !== true, true);

  let dv = new DataView(new ArrayBuffer(type.BYTES_PER_ELEMENT));

  let name = typeName(type);
  dv[`set${name}`](0, v, nativeIsLittleEndian);
  return dv[`get${name}`](0, littleEndian);
}

function toLittleEndian(type, v) {
  return toEndianess(type, v, true);
}

function toBigEndian(type, v) {
  return toEndianess(type, v, false);
}

// Shared test data for various DataView tests.
function createTestData() {
  const tests = [
    {
      type: Int8Array,
      values: [-128, -127, -2, -1, 0, 1, 2, 126, 127],
    },
    {
      type: Uint8Array,
      values: [0, 1, 2, 126, 127, 128, 254, 255],
    },
    {
      type: Int16Array,
      values: [-32768, -32767, -2, -1, 0, 1, 2, 32766, 32767],
    },
    {
      type: Uint16Array,
      values: [0, 1, 2, 32766, 32767, 32768, 65534, 65535],
    },
    {
      type: Int32Array,
      values: [-2147483648, -2147483647, -2, -1, 0, 1, 2, 2147483646, 2147483647],
    },
    {
      type: Uint32Array,
      values: [0, 1, 2, 2147483646, 2147483647],  // Representable as Int32
    },
    {
      type: Uint32Array,
      values: [0, 1, 2, 2147483646, 2147483647, 2147483648, 4294967294, 4294967295],
    },
    {
      type: Float32Array,
      values: [-NaN, -Infinity, -0.5, -0, +0, 0.5, Infinity, NaN],
    },
    {
      type: Float64Array,
      values: [-NaN, -Infinity, -0.5, -0, +0, 0.5, Infinity, NaN],
    },
    {
      type: BigInt64Array,
      values: [-9223372036854775808n, -9223372036854775807n, -2n, -1n, 0n, 1n, 2n, 9223372036854775806n, 9223372036854775807n],
    },
    {
      type: BigUint64Array,
      values: [0n, 1n, 2n, 9223372036854775806n, 9223372036854775807n, 9223372036854775808n, 18446744073709551614n, 18446744073709551615n],
    },
  ];

  tests.forEach(data => {
    data.littleEndian = data.values.map(v => toLittleEndian(data.type, v));
    data.bigEndian = data.values.map(v => toBigEndian(data.type, v));
  });

  return tests;
}

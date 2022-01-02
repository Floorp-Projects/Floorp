// Test NaN canonicalisation when reading from a DataView.

load(libdir + "dataview.js");

// Float32
function testF32() {
  function writeBE(ui32, value) {
    let ui8 = new Uint8Array(ui32.buffer);

    ui8[0] = (value >> 24) & 0xff;
    ui8[1] = (value >> 16) & 0xff;
    ui8[2] = (value >> 8) & 0xff;
    ui8[3] = (value >> 0) & 0xff;
  }

  function writeLE(ui32, value) {
    let ui8 = new Uint8Array(ui32.buffer);

    ui8[0] = (value >> 0) & 0xff;
    ui8[1] = (value >> 8) & 0xff;
    ui8[2] = (value >> 16) & 0xff;
    ui8[3] = (value >> 24) & 0xff;
  }

  // Smallest and largest SNaNs and QNaNs, with and without sign-bit set.
  const NaNs = [
    0x7F80_0001, 0x7FBF_FFFF, 0x7FC0_0000, 0x7FFF_FFFF,
    0xFF80_0001, 0xFFBF_FFFF, 0xFFC0_0000, 0xFFFF_FFFF,
  ];

  const canonicalNaN = new Uint32Array(new Float32Array([NaN]).buffer)[0];

  // Load from array so that Ion doesn't treat as constants.
  const True = [true, 1];
  const False = [false, 0];

  function f() {
    let src_ui32 = new Uint32Array(1);

    let dst_f32 = new Float32Array(1);
    let dst_ui32 = new Uint32Array(dst_f32.buffer);

    let dv = new DataView(src_ui32.buffer);

    for (let i = 0; i < 100; ++i) {
      let nan = NaNs[i % NaNs.length];

      // Write to typed array, implicitly using native endian.
      src_ui32[0] = nan;
      dst_f32[0] = dv.getFloat32(0, nativeIsLittleEndian);
      assertEq(dst_ui32[0], canonicalNaN);

      // Write and read using big endian. |isLittleEndian| parameter is absent.
      writeBE(src_ui32, nan);
      dst_f32[0] = dv.getFloat32(0);
      assertEq(dst_ui32[0], canonicalNaN);

      // Write and read using big endian. |isLittleEndian| parameter is a constant.
      writeBE(src_ui32, nan);
      dst_f32[0] = dv.getFloat32(0, false);
      assertEq(dst_ui32[0], canonicalNaN);

      // Write and read using little endian. |isLittleEndian| parameter is a constant.
      writeLE(src_ui32, nan);
      dst_f32[0] = dv.getFloat32(0, true);
      assertEq(dst_ui32[0], canonicalNaN);

      // Write and read using big endian.
      writeBE(src_ui32, nan);
      dst_f32[0] = dv.getFloat32(0, False[i & 1]);
      assertEq(dst_ui32[0], canonicalNaN);

      // Write and read using little endian.
      writeLE(src_ui32, nan);
      dst_f32[0] = dv.getFloat32(0, True[i & 1]);
      assertEq(dst_ui32[0], canonicalNaN);
    }
  }

  for (let i = 0; i < 2; ++i) f();
}
testF32();

// Float64
function testF64() {
  function writeBE(ui64, value) {
    let ui8 = new Uint8Array(ui64.buffer);

    ui8[0] = Number((value >> 56n) & 0xffn);
    ui8[1] = Number((value >> 48n) & 0xffn);
    ui8[2] = Number((value >> 40n) & 0xffn);
    ui8[3] = Number((value >> 32n) & 0xffn);
    ui8[4] = Number((value >> 24n) & 0xffn);
    ui8[5] = Number((value >> 16n) & 0xffn);
    ui8[6] = Number((value >> 8n) & 0xffn);
    ui8[7] = Number((value >> 0n) & 0xffn);
  }

  function writeLE(ui64, value) {
    let ui8 = new Uint8Array(ui64.buffer);

    ui8[0] = Number((value >> 0n) & 0xffn);
    ui8[1] = Number((value >> 8n) & 0xffn);
    ui8[2] = Number((value >> 16n) & 0xffn);
    ui8[3] = Number((value >> 24n) & 0xffn);
    ui8[4] = Number((value >> 32n) & 0xffn);
    ui8[5] = Number((value >> 40n) & 0xffn);
    ui8[6] = Number((value >> 48n) & 0xffn);
    ui8[7] = Number((value >> 56n) & 0xffn);
  }

  // Smallest and largest SNaNs and QNaNs, with and without sign-bit set.
  const NaNs = [
    0x7FF0_0000_0000_0001n, 0x7FF7_FFFF_FFFF_FFFFn, 0x7FF8_0000_0000_0000n, 0x7FFF_FFFF_FFFF_FFFFn,
    0xFFF0_0000_0000_0001n, 0xFFF7_FFFF_FFFF_FFFFn, 0xFFF8_0000_0000_0000n, 0xFFFF_FFFF_FFFF_FFFFn,
  ];

  const canonicalNaN = new BigUint64Array(new Float64Array([NaN]).buffer)[0];

  // Load from array so that Ion doesn't treat as constants.
  const True = [true, 1];
  const False = [false, 0];

  function f() {
    let src_ui64 = new BigUint64Array(1);

    let dst_f64 = new Float64Array(1);
    let dst_ui64 = new BigUint64Array(dst_f64.buffer);

    let dv = new DataView(src_ui64.buffer);

    for (let i = 0; i < 100; ++i) {
      let nan = NaNs[i % NaNs.length];

      src_ui64[0] = nan;
      dst_f64[0] = dv.getFloat64(0, nativeIsLittleEndian);
      assertEq(dst_ui64[0], canonicalNaN);

      // Write and read using big endian. |isLittleEndian| parameter is absent.
      writeBE(src_ui64, nan);
      dst_f64[0] = dv.getFloat64(0);
      assertEq(dst_ui64[0], canonicalNaN);

      // Write and read using big endian. |isLittleEndian| parameter is a constant.
      writeBE(src_ui64, nan);
      dst_f64[0] = dv.getFloat64(0, false);
      assertEq(dst_ui64[0], canonicalNaN);

      // Write and read using little endian. |isLittleEndian| parameter is a constant.
      writeLE(src_ui64, nan);
      dst_f64[0] = dv.getFloat64(0, true);
      assertEq(dst_ui64[0], canonicalNaN);

      // Write and read using big endian.
      writeBE(src_ui64, nan);
      dst_f64[0] = dv.getFloat64(0, False[i & 1]);
      assertEq(dst_ui64[0], canonicalNaN);

      // Write and read using little endian.
      writeLE(src_ui64, nan);
      dst_f64[0] = dv.getFloat64(0, True[i & 1]);
      assertEq(dst_ui64[0], canonicalNaN);
    }
  }

  for (let i = 0; i < 2; ++i) f();
}
testF64();

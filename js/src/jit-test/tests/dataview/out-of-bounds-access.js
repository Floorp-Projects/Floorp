// Out-of-bounds accesses are detected when inlining DataView.

function testRead() {
  const xs = [0x11_22_33_44, 0x55_66_77_88];

  let dv = new DataView(new ArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
  dv.setInt32(0 * Int32Array.BYTES_PER_ELEMENT, xs[0], true);
  dv.setInt32(1 * Int32Array.BYTES_PER_ELEMENT, xs[1], true);

  function f(dv, q) {
    for (let i = 0; i <= 1000; ++i) {
      // Perform an out-of-bounds read in the last iteration.
      let k = (i & 1) * Int32Array.BYTES_PER_ELEMENT + (i === 1000 && q == 2 ? 7 : 0);

      let v = dv.getInt32(k, true);
      assertEq(v, xs[i & 1]);
    }
  }

  try {
    for (var i = 0; i <= 2; ++i) {
      f(dv, i);
    }
  } catch (e) {
    assertEq(e instanceof RangeError, true, e.message);
    assertEq(i, 2);
  }
}
testRead();

function testWrite() {
  const xs = [0x11_22_33_44, 0x55_66_77_88];

  let dv = new DataView(new ArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 2));
  let ui8 = new Uint8Array(dv.buffer);

  function f(dv, q) {
    for (let i = 0; i <= 1000; ++i) {
      // Perform an out-of-bounds read in the last iteration.
      let k = (i & 1) * Int32Array.BYTES_PER_ELEMENT + (i === 1000 && q == 2 ? 7 : 0);
      let x = xs[i & 1];

      dv.setInt32(k, x);

      assertEq(ui8[0 + (i & 1) * Int32Array.BYTES_PER_ELEMENT], (x >> 24) & 0xff);
      assertEq(ui8[1 + (i & 1) * Int32Array.BYTES_PER_ELEMENT], (x >> 16) & 0xff);
      assertEq(ui8[2 + (i & 1) * Int32Array.BYTES_PER_ELEMENT], (x >> 8) & 0xff);
      assertEq(ui8[3 + (i & 1) * Int32Array.BYTES_PER_ELEMENT], (x >> 0) & 0xff);
    }
  }

  try {
    for (var i = 0; i <= 2; ++i) {
      f(dv, i);
    }
  } catch (e) {
    assertEq(e instanceof RangeError, true, e.message);
    assertEq(i, 2);
  }
}
testWrite();

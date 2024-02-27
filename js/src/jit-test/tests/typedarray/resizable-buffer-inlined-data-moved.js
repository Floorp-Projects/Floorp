// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

function fillArrayBuffer(rab) {
  let fill = new Int8Array(rab);
  for (let i = 0; i < fill.length; ++i) fill[i] = i + 1;
}

function test() {
  let rab = new ArrayBuffer(4, {maxByteLength: 4});
  let ta = new Int8Array(rab, 2, 2);

  fillArrayBuffer(rab);

  assertEq(ta[0], 3);
  assertEq(ta[1], 4);

  // Shrink to make |ta| out-of-bounds.
  rab.resize(3);

  // Request GC to move inline data.
  gc();

  // Grow to make |ta| in-bounds again.
  rab.resize(4);

  assertEq(ta[0], 3);
  assertEq(ta[1], 0);
}
test();


function testAutoLength() {
  let rab = new ArrayBuffer(4, {maxByteLength: 4});
  let ta = new Int8Array(rab, 2);

  fillArrayBuffer(rab);

  assertEq(ta[0], 3);
  assertEq(ta[1], 4);

  // Shrink to make |ta| out-of-bounds.
  rab.resize(1);

  // Request GC to move inline data.
  gc();

  // Grow to make |ta| in-bounds again.
  rab.resize(4);

  assertEq(ta[0], 0);
  assertEq(ta[1], 0);
}
testAutoLength();

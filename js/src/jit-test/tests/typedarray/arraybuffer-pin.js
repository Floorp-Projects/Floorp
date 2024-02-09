// |jit-test| --enable-arraybuffer-resizable

load(libdir + "asserts.js");

var ab_inline = new ArrayBuffer(4);
assertEq(pinArrayBufferOrViewLength(ab_inline), true);
assertEq(pinArrayBufferOrViewLength(ab_inline), false);
assertErrorMessage(() => detachArrayBuffer(ab_inline), RangeError, /change pinned length/);
assertEq(pinArrayBufferOrViewLength(ab_inline, false), true);
detachArrayBuffer(ab_inline);

var ab_big = new ArrayBuffer(1000);
assertEq(pinArrayBufferOrViewLength(ab_big), true);
assertEq(pinArrayBufferOrViewLength(ab_big), false);
assertErrorMessage(() => detachArrayBuffer(ab_big), RangeError, /change pinned length/);
assertEq(pinArrayBufferOrViewLength(ab_big, false), true);
detachArrayBuffer(ab_big);

if (ArrayBuffer.prototype.resize) {
  var rab_small = new ArrayBuffer(4, { maxByteLength: 20 });
  assertEq(pinArrayBufferOrViewLength(rab_small), true);
  assertEq(pinArrayBufferOrViewLength(rab_small), false);
  assertErrorMessage(() => detachArrayBuffer(rab_small), RangeError, /change pinned length/);
  assertErrorMessage(() => rab_small.resize(18), RangeError, /change pinned length/);
  assertEq(pinArrayBufferOrViewLength(rab_small, false), true);
  assertEq(rab_small.byteLength, 4);
  rab_small.resize(18);
  assertEq(rab_small.byteLength, 18);
  detachArrayBuffer(rab_small);
} else {
  print("Skipped test: resizable ArrayBuffers unavailable");
}

if (ArrayBuffer.prototype.resize) {
  var rab_big = new ArrayBuffer(200, { maxByteLength: 1000 });
  assertEq(pinArrayBufferOrViewLength(rab_big), true);
  assertEq(pinArrayBufferOrViewLength(rab_big), false);
  assertErrorMessage(() => detachArrayBuffer(rab_big), RangeError, /change pinned length/);
  assertErrorMessage(() => rab_big.resize(400), RangeError, /change pinned length/);
  assertEq(pinArrayBufferOrViewLength(rab_big, false), true);
  assertEq(rab_big.byteLength, 200);
  rab_big.resize(400);
  assertEq(rab_big.byteLength, 400);
  detachArrayBuffer(rab_big);
} else {
  print("Skipped test: resizable ArrayBuffers unavailable");
}

var sab = new SharedArrayBuffer(4);
assertEq(pinArrayBufferOrViewLength(sab), false);
assertEq(pinArrayBufferOrViewLength(sab), false);
assertErrorMessage(() => serialize([sab], [sab]), TypeError, /Shared memory objects.*transfer/);
assertEq(pinArrayBufferOrViewLength(sab, false), false);
assertErrorMessage(() => detachArrayBuffer(sab), TypeError, /ArrayBuffer.*required/);

if (SharedArrayBuffer.prototype.grow) {
  var gab = new SharedArrayBuffer(4, { maxByteLength: 1000 });
  assertEq(pinArrayBufferOrViewLength(gab), false);
  assertEq(pinArrayBufferOrViewLength(gab), false);
  assertErrorMessage(() => detachArrayBuffer(gab), TypeError, /ArrayBuffer.*required/);
  // assertErrorMessage(() => gab.grow(400), RangeError, /change pinned length/);
  assertEq(pinArrayBufferOrViewLength(gab, false), false);
  assertEq(gab.byteLength, 4);
  gab.grow(400);
  assertEq(gab.byteLength, 400);
  assertErrorMessage(() => detachArrayBuffer(gab), TypeError, /ArrayBuffer.*required/);
} else {
  print("Skipped test: growable SharedArrayBuffers unavailable");
}

// |jit-test| --enable-arraybuffer-resizable; skip-if: !ArrayBuffer.prototype.resize

const scopes = [
  "SameProcess",
  "DifferentProcess",
  "DifferentProcessForIndexedDB",
];

function testArrayBufferTransferable(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;
  var maxByteLength = 2 * byteLength;

  var ab = new ArrayBuffer(byteLength, {maxByteLength});
  assertEq(ab.resizable, true);
  assertEq(ab.byteLength, byteLength);
  assertEq(ab.maxByteLength, maxByteLength);

  var ta = new Uint8Array(ab);
  ta.set([33, 44, 55, 66]);
  assertEq(ta.toString(), "33,44,55,66");

  var clonebuf = serialize(ab, [ab], {scope});
  var ab2 = deserialize(clonebuf);
  assertEq(ab2 instanceof ArrayBuffer, true);
  assertEq(new Uint8Array(ab2).toString(), "33,44,55,66");
  assertEq(ab2.resizable, true);
  assertEq(ab2.byteLength, byteLength);
  assertEq(ab2.maxByteLength, maxByteLength);

  assertEq(ab.detached, true);
  assertEq(ab2.detached, false);

  ab2.resize(maxByteLength);
  assertEq(ab2.byteLength, maxByteLength);
}
scopes.forEach(testArrayBufferTransferable);

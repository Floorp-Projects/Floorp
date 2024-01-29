// |jit-test| --enable-arraybuffer-resizable; skip-if: !SharedArrayBuffer.prototype.grow

const scopes = [
  "SameProcess",
];

const g = newGlobal({enableCoopAndCoep: true});

const options = {
  SharedArrayBuffer: "allow",
};

function testInt32Array(scope) {
  var length = 4;
  var byteLength = length * Int32Array.BYTES_PER_ELEMENT;
  var maxByteLength = 2 * byteLength;

  var ab = new SharedArrayBuffer(byteLength, {maxByteLength});
  assertEq(ab.growable, true);
  assertEq(ab.byteLength, byteLength);
  assertEq(ab.maxByteLength, maxByteLength);

  var ta1 = new Int32Array(ab);
  assertEq(ta1.byteLength, byteLength);
  ta1.set([1, 87654321, -123]);
  assertEq(ta1.toString(), "1,87654321,-123,0");

  var clonebuf = g.serialize(ta1, undefined, {scope, ...options});
  var ta2 = g.deserialize(clonebuf, {...options});
  assertEq(ta2 instanceof g.Int32Array, true);
  assertEq(ta2.byteLength, byteLength);
  assertEq(ta2.toString(), "1,87654321,-123,0");
  assertEq(ta2.buffer.growable, true);
  assertEq(ta2.buffer.byteLength, byteLength);
  assertEq(ta2.buffer.maxByteLength, maxByteLength);

  ta2.buffer.grow(maxByteLength);
  assertEq(ta2.byteLength, maxByteLength);
}
scopes.forEach(testInt32Array);

function testFloat64Array(scope) {
  var length = 4;
  var byteLength = length * Float64Array.BYTES_PER_ELEMENT;
  var maxByteLength = 2 * byteLength;

  var ab = new SharedArrayBuffer(byteLength, {maxByteLength});
  assertEq(ab.growable, true);
  assertEq(ab.byteLength, byteLength);
  assertEq(ab.maxByteLength, maxByteLength);

  var ta1 = new Float64Array(ab);
  assertEq(ta1.byteLength, byteLength);
  ta1.set([NaN, 3.14, 0, 0]);
  assertEq(ta1.toString(), "NaN,3.14,0,0");

  var clonebuf = g.serialize(ta1, undefined, {scope, ...options});
  var ta2 = g.deserialize(clonebuf, {...options});
  assertEq(ta2 instanceof g.Float64Array, true);
  assertEq(ta2.byteLength, byteLength);
  assertEq(ta2.toString(), "NaN,3.14,0,0");
  assertEq(ta2.buffer.growable, true);
  assertEq(ta2.buffer.byteLength, byteLength);
  assertEq(ta2.buffer.maxByteLength, maxByteLength);

  ta2.buffer.grow(maxByteLength);
  assertEq(ta2.byteLength, maxByteLength);
}
scopes.forEach(testFloat64Array);

function testDataView(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;
  var maxByteLength = 2 * byteLength;

  var ab = new SharedArrayBuffer(byteLength, {maxByteLength});
  assertEq(ab.growable, true);
  assertEq(ab.byteLength, byteLength);
  assertEq(ab.maxByteLength, maxByteLength);

  var ta = new Uint8Array(ab);
  ta.set([5, 0, 255]);
  assertEq(ta.toString(), "5,0,255,0");
  var dv1 = new DataView(ab);
  assertEq(dv1.byteLength, byteLength);

  var clonebuf = g.serialize(dv1, undefined, {scope, ...options});
  var dv2 = g.deserialize(clonebuf, {...options});
  assertEq(dv2 instanceof g.DataView, true);
  assertEq(dv2.byteLength, byteLength);
  assertEq(new Uint8Array(dv2.buffer).toString(), "5,0,255,0");
  assertEq(dv2.buffer.growable, true);
  assertEq(dv2.buffer.byteLength, byteLength);
  assertEq(dv2.buffer.maxByteLength, maxByteLength);

  dv2.buffer.grow(maxByteLength);
  assertEq(dv2.byteLength, maxByteLength);
}
scopes.forEach(testDataView);

function testArrayBuffer(scope) {
  var length = 4;
  var byteLength = length * Uint8Array.BYTES_PER_ELEMENT;
  var maxByteLength = 2 * byteLength;

  var ab = new SharedArrayBuffer(byteLength, {maxByteLength});
  assertEq(ab.growable, true);
  assertEq(ab.byteLength, byteLength);
  assertEq(ab.maxByteLength, maxByteLength);

  var ta = new Uint8Array(ab);
  ta.set([33, 44, 55, 66]);
  assertEq(ta.toString(), "33,44,55,66");

  var clonebuf = g.serialize(ab, undefined, {scope, ...options});
  var ab2 = g.deserialize(clonebuf, {...options});
  assertEq(ab2 instanceof g.SharedArrayBuffer, true);
  assertEq(new Uint8Array(ab2).toString(), "33,44,55,66");
  assertEq(ab2.growable, true);
  assertEq(ab2.byteLength, byteLength);
  assertEq(ab2.maxByteLength, maxByteLength);

  ab2.grow(maxByteLength);
  assertEq(ab2.byteLength, maxByteLength);
}
scopes.forEach(testArrayBuffer);

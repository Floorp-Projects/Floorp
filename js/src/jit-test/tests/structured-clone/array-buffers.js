// Tests for ArrayBuffer, TypedArray, and DataView encoding/decoding. 

var clonebuffer = serialize("dummy");

// ========= V2 =========

function testV2Int32Array() {
    var buf = new Uint8Array([3,0,0,0,0,0,241,255,3,0,0,0,16,0,255,255,4,0,0,0,0,0,0,0,12,0,0,0,9,0,255,255,1,0,0,0,177,127,57,5,133,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0]);
    clonebuffer.clonebuffer = buf.buffer;
    var ta = deserialize(clonebuffer);
    assertEq(ta instanceof Int32Array, true);
    assertEq(ta.toString(), "1,87654321,-123");
}
testV2Int32Array();

function testV2Float64Array() {
    var buf = new Uint8Array([3,0,0,0,0,0,241,255,4,0,0,0,16,0,255,255,7,0,0,0,0,0,0,0,32,0,0,0,9,0,255,255,0,0,0,0,0,0,248,127,31,133,235,81,184,30,9,64,0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    clonebuffer.clonebuffer = buf.buffer;
    var ta = deserialize(clonebuffer);
    assertEq(ta instanceof Float64Array, true);
    assertEq(ta.toString(), "NaN,3.14,0,0");
}
testV2Float64Array();

function testV2DataView() {
    var buf = new Uint8Array([3,0,0,0,0,0,241,255,3,0,0,0,21,0,255,255,3,0,0,0,9,0,255,255,5,0,255,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    clonebuffer.clonebuffer = buf.buffer;
    var dv = deserialize(clonebuffer);
    assertEq(dv instanceof DataView, true);
    assertEq(new Uint8Array(dv.buffer).toString(), "5,0,255");
}
testV2DataView();

function testV2ArrayBuffer() {
    var buf = new Uint8Array([3,0,0,0,0,0,241,255,4,0,0,0,9,0,255,255,33,44,55,66,0,0,0,0]);
    clonebuffer.clonebuffer = buf.buffer;
    var ab = deserialize(clonebuffer);
    assertEq(ab instanceof ArrayBuffer, true);
    assertEq(new Uint8Array(ab).toString(), "33,44,55,66");
}
testV2ArrayBuffer();

// ========= Current =========

function testInt32Array() {
    var ta1 = new Int32Array([1, 87654321, -123]);
    var clonebuf = serialize(ta1, undefined, {scope: "DifferentProcessForIndexedDB"});
    var ta2 = deserialize(clonebuf);
    assertEq(ta2 instanceof Int32Array, true);
    assertEq(ta2.toString(), "1,87654321,-123");
}
testInt32Array();

function testFloat64Array() {
    var ta1 = new Float64Array([NaN, 3.14, 0, 0]);
    var clonebuf = serialize(ta1, undefined, {scope: "DifferentProcessForIndexedDB"});
    var ta2 = deserialize(clonebuf);
    assertEq(ta2 instanceof Float64Array, true);
    assertEq(ta2.toString(), "NaN,3.14,0,0");
}
testFloat64Array();

function testDataView() {
    var ta = new Uint8Array([5, 0, 255]);
    var dv1 = new DataView(ta.buffer);
    var clonebuf = serialize(dv1, undefined, {scope: "DifferentProcessForIndexedDB"});
    var dv2 = deserialize(clonebuf);
    assertEq(dv2 instanceof DataView, true);
    assertEq(new Uint8Array(dv2.buffer).toString(), "5,0,255");
}
testDataView();

function testArrayBuffer() {
    var ta = new Uint8Array([33, 44, 55, 66]);
    var clonebuf = serialize(ta.buffer, undefined, {scope: "DifferentProcessForIndexedDB"});
    var ab = deserialize(clonebuf);
    assertEq(ab instanceof ArrayBuffer, true);
    assertEq(new Uint8Array(ab).toString(), "33,44,55,66");
}
testArrayBuffer();

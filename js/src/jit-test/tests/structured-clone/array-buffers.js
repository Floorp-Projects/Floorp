// Tests for ArrayBuffer, TypedArray, and DataView encoding/decoding. 

var clonebuffer = serialize("dummy");

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

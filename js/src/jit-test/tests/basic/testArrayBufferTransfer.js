load(libdir + "asserts.js");
load(libdir + "asm.js");

// Currently, ArrayBuffer.transfer is #ifdef NIGHTLY_BUILD. When
// ArrayBuffer.transfer is enabled on release, this test should be removed.
if (!ArrayBuffer.transfer)
    quit();

var XF = ArrayBuffer.transfer;

assertEq(typeof XF, "function");
assertEq(XF.length, 2);

// arg 1 errors
assertThrowsInstanceOf(()=>XF(), Error);
assertThrowsInstanceOf(()=>XF(undefined), Error);
assertThrowsInstanceOf(()=>XF(null), Error);
assertThrowsInstanceOf(()=>XF({}), Error);
assertThrowsInstanceOf(()=>XF(new Int32Array(1)), Error);
var buf = new ArrayBuffer(1);
neuter(buf, 'change-data');
assertThrowsInstanceOf(()=>XF(buf), TypeError);

// arg 2 errors
var buf = new ArrayBuffer(1);
assertThrowsInstanceOf(()=>XF(buf, -1), Error);
assertThrowsInstanceOf(()=>XF(buf, {valueOf() { return -1 }}), Error);
assertThrowsInstanceOf(()=>XF(buf, {toString() { return "-1" }}), Error);
assertThrowsValue(()=>XF(buf, {valueOf() { throw "wee" }}), "wee");

// arg 2 is coerced via ToInt32
var buf = new ArrayBuffer(1);
assertThrowsInstanceOf(()=>XF(buf, Math.pow(2,31)), Error);
buf = XF(buf, Math.pow(2,32));
assertEq(buf.byteLength, 0);
buf = XF(buf, Math.pow(2,32) + 10);
assertEq(buf.byteLength, 10);

// on undefined second argument, stay the same size:
var buf1 = new ArrayBuffer(0);
var buf2 = XF(buf1);
assertEq(buf1.byteLength, 0);
assertEq(buf2.byteLength, 0);
assertThrowsInstanceOf(()=>XF(buf1), TypeError);

var buf1 = new ArrayBuffer(3);
var buf2 = XF(buf1);
assertEq(buf1.byteLength, 0);
assertEq(buf2.byteLength, 3);
assertThrowsInstanceOf(()=>XF(buf1), TypeError);

var buf1 = new ArrayBuffer(9);
var buf2 = XF(buf1, undefined);
assertEq(buf1.byteLength, 0);
assertEq(buf2.byteLength, 9);
assertThrowsInstanceOf(()=>XF(buf1), TypeError);

// test going to from various sizes
function test(N1, N2) {
    var buf1 = new ArrayBuffer(N1);
    var i32 = new Int32Array(buf1);
    for (var i = 0; i < i32.length; i++)
        i32[i] = i;

    var buf2 = XF(buf1, N2);

    assertEq(buf1.byteLength, 0);
    assertEq(i32.length, 0);
    assertEq(buf2.byteLength, N2);
    var i32 = new Int32Array(buf2);
    for (var i = 0; i < Math.min(N1, N2)/4; i++)
        assertEq(i32[i], i);
    for (var i = Math.min(N1, N2)/4; i < i32.length; i++) {
        assertEq(i32[i], 0);
        i32[i] = -i;
    }
}
test(0, 0);
test(0, 4);
test(4, 0);
test(4, 4);
test(0, 1000);
test(4, 1000);
test(1000, 0);
test(1000, 4);
test(1000, 1000);

// asm.js:
function testAsmJS(N1, N2) {
    var buf1 = new ArrayBuffer(N1);
    asmLink(asmCompile('stdlib', 'ffis', 'buf', USE_ASM + "var i32=new stdlib.Int32Array(buf); function f() {} return f"), this, null, buf1);
    var i32 = new Int32Array(buf1);
    for (var i = 0; i < i32.length; i+=100)
        i32[i] = i;

    var buf2 = XF(buf1, N2);

    assertEq(buf1.byteLength, 0);
    assertEq(i32.length, 0);
    assertEq(buf2.byteLength, N2);
    var i32 = new Int32Array(buf2);
    var i = 0;
    for (; i < Math.min(N1, N2)/4; i+=100)
        assertEq(i32[i], i);
    for (; i < i32.length; i+=100) {
        assertEq(i32[i], 0);
        i32[i] = -i;
    }
}
testAsmJS(BUF_MIN, 0);
testAsmJS(BUF_MIN, BUF_MIN);
testAsmJS(BUF_MIN, 2*BUF_MIN);
testAsmJS(2*BUF_MIN, BUF_MIN);

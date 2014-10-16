load(libdir + "asm.js");
load(libdir + "asserts.js");

if (!isAsmJSCompilationAvailable())
    quit();

var m = asmCompile('stdlib', 'foreign', 'buffer',
                  `"use asm";
                   var i32 = new stdlib.Int32Array(buffer);
                   function set(i,j) {
                       i=i|0;
                       j=j|0;
                       i32[i>>2] = j;
                   }
                   function get(i) {
                       i=i|0;
                       return i32[i>>2]|0
                   }
                   return {get:get, set:set}`);

var buffer = new ArrayBuffer(BUF_MIN);
var {get, set} = asmLink(m, this, null, buffer);
set(4, 42);
assertEq(get(4), 42);
neuter(buffer, "change-data");
neuter(buffer, "same-data");
assertThrowsInstanceOf(() => get(4), InternalError);

var buf1 = new ArrayBuffer(BUF_MIN);
var buf2 = new ArrayBuffer(BUF_MIN);
var {get:get1, set:set1} = asmLink(m, this, null, buf1);
var {get:get2, set:set2} = asmLink(m, this, null, buf2);
set1(0, 13);
set2(0, 42);
neuter(buf1, "change-data");
assertThrowsInstanceOf(() => get1(0), InternalError);
assertEq(get2(0), 42);

var m = asmCompile('stdlib', 'foreign', 'buffer',
                  `"use asm";
                   var i32 = new stdlib.Int32Array(buffer);
                   var ffi = foreign.ffi;
                   function inner(i) {
                       i=i|0;
                       ffi();
                       return i32[i>>2]|0
                   }
                   return inner`);

var buffer = new ArrayBuffer(BUF_MIN);
function ffi1() { neuter(buffer, "change-data"); }
var inner = asmLink(m, this, {ffi:ffi1}, buffer);
assertThrowsInstanceOf(() => inner(8), InternalError);

var byteLength = Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);
var m = asmCompile('stdlib', 'foreign', 'buffer',
                  `"use asm";
                   var ffi = foreign.ffi;
                   var I32 = stdlib.Int32Array;
                   var i32 = new I32(buffer);
                   var len = stdlib.byteLength;
                   function changeHeap(newBuffer) {
                       if (len(newBuffer) & 0xffffff || len(newBuffer) <= 0xffffff || len(newBuffer) > 0x80000000)
                           return false;
                       i32 = new I32(newBuffer);
                       buffer = newBuffer;
                       return true;
                   }
                   function get(i) {
                       i=i|0;
                       return i32[i>>2]|0;
                   }
                   function inner(i) {
                       i=i|0;
                       ffi();
                       return get(i)|0;
                   }
                   return {changeHeap:changeHeap, get:get, inner:inner}`);

var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
var buf2 = new ArrayBuffer(BUF_CHANGE_MIN);
var buf3 = new ArrayBuffer(BUF_CHANGE_MIN);
var buf4 = new ArrayBuffer(BUF_CHANGE_MIN);
new Int32Array(buf2)[13] = 42;
new Int32Array(buf3)[13] = 1024;
new Int32Array(buf4)[13] = 1337;

function ffi2() { neuter(buf1, "change-data"); assertEq(changeHeap(buf2), true); }
var {changeHeap, get:get2, inner} = asmLink(m, this, {ffi:ffi2}, buf1);
assertEq(inner(13*4), 42);

function ffi3() {
    assertEq(get2(13*4), 42);
    assertEq(get2(BUF_CHANGE_MIN), 0)
    assertEq(get3(13*4), 42);
    assertEq(get3(BUF_CHANGE_MIN), 0)
    neuter(buf2, "change-data");
    assertThrowsInstanceOf(()=>get2(13*4), InternalError);
    assertThrowsInstanceOf(()=>get2(BUF_CHANGE_MIN), InternalError);
    assertThrowsInstanceOf(()=>get3(13*4), InternalError);
    assertThrowsInstanceOf(()=>get3(BUF_CHANGE_MIN), InternalError);
    assertEq(changeHeap(buf3), true);
    assertThrowsInstanceOf(()=>get2(13*4), InternalError);
    assertThrowsInstanceOf(()=>get2(BUF_CHANGE_MIN), InternalError);
    assertEq(get3(13*4), 1024);
    assertEq(get3(BUF_CHANGE_MIN), 0);
    assertEq(changeHeap(buf4), true);
}
var {changeHeap, get:get3, inner} = asmLink(m, this, {ffi:ffi3}, buf2);
assertEq(inner(13*4), 1337);
assertThrowsInstanceOf(()=>get2(0), InternalError);
assertEq(get3(BUF_CHANGE_MIN), 0);
assertEq(get3(13*4), 1337);

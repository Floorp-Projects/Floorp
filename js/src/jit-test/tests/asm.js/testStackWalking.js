load(libdir + "asm.js");
load(libdir + "asserts.js");

var callFFI1 = asmCompile('global', 'ffis', USE_ASM + "var ffi=ffis.ffi; function asmfun1() { return ffi(1)|0 } return asmfun1");
var callFFI2 = asmCompile('global', 'ffis', USE_ASM + "var ffi=ffis.ffi; function asmfun2() { return ffi(2)|0 } return asmfun2");

var stack;
function dumpStack(i) { stack = new Error().stack; return i+11 }

var asmfun1 = asmLink(callFFI1, null, {ffi:dumpStack});
assertEq(asmfun1(), 12);
assertEq(stack.indexOf("asmfun1") === -1, false);

var asmfun2 = asmLink(callFFI2, null, {ffi:function ffi(i){return asmfun1()+20}});
assertEq(asmfun2(), 32);
assertEq(stack.indexOf("asmfun1") == -1, false);
assertEq(stack.indexOf("asmfun2") == -1, false);
assertEq(stack.indexOf("asmfun2") > stack.indexOf("asmfun1"), true);

var caught = false;
try {
    asmLink(asmCompile(USE_ASM + "function asmRec() { asmRec() } return asmRec"))();
} catch (e) {
    caught = true;
}
assertEq(caught, true);

var caught = false;
try {
    callFFI1(null, {ffi:Object.preventExtensions})();
} catch (e) {
    caught = true;
}
assertEq(caught, true);

assertEq(asmLink(callFFI1, null, {ffi:eval})(), 1);
assertEq(asmLink(callFFI1, null, {ffi:Function})(), 0);
assertEq(asmLink(callFFI1, null, {ffi:Error})(), 0);

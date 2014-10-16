// |jit-test| exitstatus: 6;
load(libdir + "asm.js");

// This test may iloop for valid reasons if not compiled with asm.js (namely,
// inlining may allow the heap load to be hoisted out of the loop).
if (!isAsmJSCompilationAvailable())
    quit(6);

setJitCompilerOption("signals.enable", 0);

var byteLength =
  Function.prototype.call.bind(Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get);

var buf1 = new ArrayBuffer(BUF_CHANGE_MIN);
new Int32Array(buf1)[0] = 13;
var buf2 = new ArrayBuffer(BUF_CHANGE_MIN);
new Int32Array(buf2)[0] = 42;

// Test changeHeap from interrupt (as if that could ever happen...)
var m = asmCompile('glob', 'ffis', 'b', USE_ASM +
                   `var I32=glob.Int32Array; var i32=new I32(b);
                    var len=glob.byteLength;
                    function changeHeap(b2) { if(len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 0x80000000) return false; i32=new I32(b2); b=b2; return true }
                    function f() {}
                    function loop(i) { i=i|0; while((i32[i>>2]|0) == 13) { f() } }
                    return {loop:loop, changeHeap:changeHeap}`);
var { loop, changeHeap } = asmLink(m, this, null, buf1);
timeout(1, function() { assertEq(changeHeap(buf2), false); return false });
loop(0);

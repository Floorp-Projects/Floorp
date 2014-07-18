// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

setJitCompilerOption("signals.enable", 0);
var f = asmLink(asmCompile(USE_ASM + "/* no-signals */ function f(i) { i=i|0; if (!i) return; f((i-1)|0); f((i-1)|0); f((i-1)|0); f((i-1)|0); f((i-1)|0); } return f"));
timeout(1);
f(100);
assertEq(true, false);

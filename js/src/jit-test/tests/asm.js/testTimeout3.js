// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")

load(libdir + "asm.js");

var f = asmLink(asmCompile(USE_ASM + "function f(i) { i=i|0; if (!i) return; f((i-1)|0); f((i-1)|0); f((i-1)|0); f((i-1)|0); f((i-1)|0); } return f"));
timeout(1);
f(100);
assertEq(true, false);

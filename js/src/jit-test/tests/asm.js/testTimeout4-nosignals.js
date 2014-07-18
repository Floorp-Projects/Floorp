// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

setJitCompilerOption("signals.enable", 0);
var g = asmLink(asmCompile(USE_ASM + "/* no-signals */ function f(d) { d=+d; d=d*.1; d=d/.4; return +d } function g() { while(1) { +f(1.1) } } return g"));
timeout(1);
g();
assertEq(true, false);

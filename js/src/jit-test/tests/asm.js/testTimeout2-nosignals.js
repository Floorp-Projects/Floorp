// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

suppressSignalHandlers(true);
var g = asmLink(asmCompile(USE_ASM + "function g() { while(1) {} } return g"));
timeout(1);
g();
assertEq(true, false);

// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration()['wasi']

load(libdir + "asm.js");

var g = asmLink(asmCompile(USE_ASM + "function g() { while(1) {} } return g"));
timeout(1);
g();
assertEq(true, false);

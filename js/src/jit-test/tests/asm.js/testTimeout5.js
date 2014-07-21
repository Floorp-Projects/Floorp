// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

enableSPSProfiling();

var f = asmLink(asmCompile('glob', 'ffis', 'buf', USE_ASM + "function f() { var i=0; while (1) { i=(i+1)|0 } } return f"));
timeout(1);
if (getBuildConfiguration()["arm-simulator"])
    enableSingleStepProfiling();
f();
assertEq(true, false);

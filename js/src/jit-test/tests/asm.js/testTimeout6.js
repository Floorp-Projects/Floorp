// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")

load(libdir + "asm.js");

enableGeckoProfiling();

var f = asmLink(asmCompile('glob', 'ffis', 'buf', USE_ASM + "function g() { var i=0; while (1) { i=(i+1)|0 } } function f() { g() } return f"));
timeout(1);
if (getBuildConfiguration("arm-simulator"))
    enableSingleStepProfiling();
f();
assertEq(true, false);

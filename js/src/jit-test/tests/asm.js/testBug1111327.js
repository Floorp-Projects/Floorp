// |jit-test| skip-if: !getBuildConfiguration("arm-simulator")
// Single-step profiling currently only works in the ARM simulator

load(libdir + "asm.js");

enableGeckoProfiling();
enableSingleStepProfiling();
var m = asmCompile(USE_ASM + 'function f() {} return f');
asmLink(m)();
asmLink(m)();

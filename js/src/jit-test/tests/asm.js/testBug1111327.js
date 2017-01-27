load(libdir + "asm.js");

// Single-step profiling currently only works in the ARM simulator
if (!getBuildConfiguration()["arm-simulator"])
    quit();

enableGeckoProfiling();
enableSingleStepProfiling();
var m = asmCompile(USE_ASM + 'function f() {} return f');
asmLink(m)();
asmLink(m)();

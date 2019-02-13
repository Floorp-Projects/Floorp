// |jit-test| skip-if: !this.getJitCompilerOptions() || !this.getJitCompilerOptions()['ion.enable']
load(libdir + "asserts.js");
var oldOpts = getJitCompilerOptions();
for (var k in oldOpts)
    setJitCompilerOption(k, oldOpts[k]);
var newOpts = getJitCompilerOptions();
assertDeepEq(oldOpts, newOpts);

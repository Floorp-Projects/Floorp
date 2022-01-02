// |jit-test| error:Enabling or disabling the Baseline Interpreter
let blInterpEnabled = getJitCompilerOptions()['blinterp.enable'] === 1;
setJitCompilerOption("blinterp.enable", blInterpEnabled ? 0 : 1);

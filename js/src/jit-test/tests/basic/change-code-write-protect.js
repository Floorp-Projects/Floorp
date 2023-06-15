// |jit-test| error:Can't change code write protection at runtime
var value = getJitCompilerOptions()["write-protect-code"];
setJitCompilerOption("write-protect-code", Number(!value));

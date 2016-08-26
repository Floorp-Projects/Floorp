// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

setCachingEnabled(true);

var jco = getJitCompilerOptions();
if (!isCachingEnabled() || !isAsmJSCompilationAvailable())
    quit(6);

// Modules compiled without signal handlers should still work even if signal
// handlers have been reactivated.
suppressSignalHandlers(true);

var code = USE_ASM + "function f() {} function g() { while(1) { f() } } return g";

var m = asmCompile(code);
assertEq(isAsmJSModule(m), true);
assertEq(isAsmJSModuleLoadedFromCache(m), false);

suppressSignalHandlers(false);

var m = asmCompile(code);
assertEq(isAsmJSModule(m), true);
assertEq(isAsmJSModuleLoadedFromCache(m), false);

var g = asmLink(m);
timeout(1);
g();
assertEq(true, false);

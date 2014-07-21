// |jit-test| exitstatus: 6;

load(libdir + "asm.js");

var jco = getJitCompilerOptions();
if (jco["signals.enable"] === 0 || !isCachingEnabled() || !isAsmJSCompilationAvailable())
    quit(6);

// Modules compiled without signal handlers should still work even if signal
// handlers have been reactivated.
setJitCompilerOption("signals.enable", 0);

var code = USE_ASM + "function f() {} function g() { while(1) { f() } } return g";

var m = asmCompile(code);
assertEq(isAsmJSModule(m), true);
assertEq(isAsmJSModuleLoadedFromCache(m), false);

setJitCompilerOption("signals.enable", 1);

var m = asmCompile(code);
assertEq(isAsmJSModule(m), true);
assertEq(isAsmJSModuleLoadedFromCache(m), true);

var g = asmLink(m);
timeout(1);
g();
assertEq(true, false);

// Test a big fat asm.js module. First load/compile/cache bullet.js in a
// separate process and then load it again in this process, which should be a
// cache hit.

if (!isAsmJSCompilationAvailable())
    quit();

// Note: if you get some failure in this test, it probably has to do with
// bullet.js and not the nestedShell() call, so try first commenting out
// nestedShell() (and the loadedFromCache assertion) to see if the error
// reproduces.
var code = "setIonCheckGraphCoherency(false); load('" + libdir + "bullet.js'); runBullet()";
nestedShell("--js-cache", "--no-js-cache-per-process", "--execute=" + code);
setIonCheckGraphCoherency(false);
load(libdir + 'bullet.js');
var results = runBullet();
assertEq(results.asmJSValidated, true);
assertEq(results.loadedFromCache, true);

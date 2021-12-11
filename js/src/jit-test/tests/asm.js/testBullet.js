// |jit-test| skip-if: !isAsmJSCompilationAvailable()

setIonCheckGraphCoherency(false);
load(libdir + 'bullet.js');
var results = runBullet();
assertEq(results.asmJSValidated, true);

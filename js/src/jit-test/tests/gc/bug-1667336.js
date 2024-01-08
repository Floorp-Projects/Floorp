// |jit-test| slow; --ion-offthread-compile=off; skip-if: helperThreadCount() === 0

var g = newGlobal();
gczeal(9, 1);
gczeal(11, 2);
g.offThreadCompileToStencil("");
setJitCompilerOption("offthread-compilation.enable", 1);

for (let i = 0 ; i < 3000; i++) {
  loadFile();
}

function loadFile() {
  try {
    x;
  } catch(exc) {}
}

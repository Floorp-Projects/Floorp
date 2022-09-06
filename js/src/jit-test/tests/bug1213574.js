// |jit-test| skip-if: helperThreadCount() === 0

var lfGlobal = newGlobal();
lfGlobal.offThreadCompileToStencil(`{ let x; throw 42; }`);
var stencil = lfGlobal.finishOffThreadStencil();
try {
    lfGlobal.evalStencil(stencil);
} catch (e) {
}

lfGlobal.offThreadCompileToStencil(`function f() { { let x = 42; return x; } }`);
stencil = lfGlobal.finishOffThreadStencil();
try {
    lfGlobal.evalStencil(stencil);
} catch (e) {
}

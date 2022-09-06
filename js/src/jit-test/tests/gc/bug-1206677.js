// |jit-test| skip-if: !('oomTest' in this) || helperThreadCount() === 0

var lfGlobal = newGlobal();
for (lfLocal in this) {
    if (!(lfLocal in lfGlobal)) {
        lfGlobal[lfLocal] = this[lfLocal];
    }
}
const script = 'oomTest(() => getBacktrace({args: true, locals: "123795", thisprops: true}));';
lfGlobal.offThreadCompileToStencil(script);
var stencil = lfGlobal.finishOffThreadStencil();
lfGlobal.evalStencil(stencil);

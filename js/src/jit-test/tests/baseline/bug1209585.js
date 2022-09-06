// |jit-test| skip-if: helperThreadCount() === 0 || !('oomAtAllocation' in this)

if ("gczeal" in this)
    gczeal(0);

eval("g=function() {}")
var lfGlobal = newGlobal();
for (lfLocal in this) {
    if (!(lfLocal in lfGlobal)) {
        lfGlobal[lfLocal] = this[lfLocal];
    }
}
lfGlobal.offThreadCompileToStencil(`
if (!("oomAtAllocation" in this && "resetOOMFailure" in this))
    gczeal(0);
function oomTest(f) {
    var i = 1;
    do {
        try {
            oomAtAllocation(i);
            f();
            more = resetOOMFailure();
        } catch (e) {
            more = resetOOMFailure();
        }
        i++;
    } while(more);
}
var g = newGlobal();
oomTest(function() { new revocable(); });
`);
try {
    var stencil = lfGlobal.finishOffThreadStencil();
    lfGlobal.evalStencil(stencil);
} catch(e) {
    // This can happen if we OOM while bailing out in Ion.
    assertEq(e, "out of memory");
}

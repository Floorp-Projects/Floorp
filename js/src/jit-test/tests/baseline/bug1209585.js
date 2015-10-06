if (helperThreadCount() == 0)
    quit();

if (!("oomAtAllocation" in this && "resetOOMFailure" in this))
    quit();

if ("gczeal" in this)
    gczeal(0);

eval("g=function() {}")
var lfGlobal = newGlobal();
for (lfLocal in this) {
    if (!(lfLocal in lfGlobal)) {
        lfGlobal[lfLocal] = this[lfLocal];
    }
}
lfGlobal.offThreadCompileScript(`
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
lfGlobal.runOffThreadScript();

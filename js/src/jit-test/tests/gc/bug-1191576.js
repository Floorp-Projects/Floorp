// |jit-test| allow-oom

if (!('gczeal' in this && 'oomAfterAllocations' in this))
    quit();

var lfcode = new Array();
gczeal(14);
loadFile(`
for (let e of Object.values(newGlobal())) {
    if (oomAfterAllocations(100))
        continue;
}
`);
function loadFile(lfVarx) {
    for (lfLocal in this) {}
    evaluate(lfVarx);
}

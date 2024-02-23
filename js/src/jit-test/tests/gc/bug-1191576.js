// |jit-test| allow-oom

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

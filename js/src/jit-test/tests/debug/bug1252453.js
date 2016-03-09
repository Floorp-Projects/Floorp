// |jit-test| --no-threads

lfcode = new Array();
gczeal(8,2);
lfcode.push(`
const root = newGlobal();
const dbg = new Debugger;
const wrappedRoot = dbg.addDebuggee(root);
dbg.memory.trackingAllocationSites = 1;
root.eval("(" + function immediate() { '_'  << foo } + "())");
`);
file = lfcode.shift();
loadFile(file);
function loadFile(lfVarx) {
    try {
        function newFunc(x) Function(x)();
        newFunc(lfVarx)();
    } catch (lfVare) {
        print(lfVare)
    }
}

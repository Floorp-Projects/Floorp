// |jit-test| skip-if: !('oomTest' in this)

// Adapted from randomly chosen test: js/src/jit-test/tests/debug/Memory-drainAllocationsLog-13.js
//
// This triggers OOMs that will cause weak marking mode to abort.
const root = newGlobal({
    newCompartment: true
});
root.eval("dbg = new Debugger()");
root.dbg.addDebuggee(this);
root.dbg.memory.trackingAllocationSites = true;
// jsfunfuzz-generated
relazifyFunctions('compartment');
print(/x/);
oomTest((function() {
    String.prototype.localeCompare()
}), {
    keepFailing: true
});

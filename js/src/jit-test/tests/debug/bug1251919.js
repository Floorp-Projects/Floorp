// |jit-test| error: out of memory

if (!('oomTest' in this))
    throw new Error("out of memory");

// jsfunfuzz-generated
fullcompartmentchecks(true);
// Adapted from randomly chosen test: js/src/jit-test/tests/debug/bug-1248162.js
var dbg = new Debugger;
dbg.onNewGlobalObject = function() {};
oomTest(function() {
    newGlobal();
})

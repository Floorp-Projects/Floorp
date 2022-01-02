// |jit-test| skip-if: !hasFunction["gczeal"]
// Removing and adding debuggees during an incremental sweep should not confuse
// the generatorFrames map.

// Let any ongoing incremental GC finish, and then clear any ambient zeal
// settings established by the harness (the JS_GC_ZEAL env var, shell arguments,
// etc.)
gczeal(0);

let g = newGlobal({newCompartment: true});
g.eval('function* f() { yield 123; }');

let dbg = Debugger(g);
dbg.onEnterFrame = frame => {
    dbg.removeDebuggee(g);
    dbg.addDebuggee(g);
};

// Select GCZeal mode 10 (IncrementalMultipleSlices: Incremental GC in many
// slices) and use long slices, to make sure that the debuggee removal occurs
// during a slice.
gczeal(10, 0);
gcslice(1);
while (gcstate() !== "NotAcctive" && gcstate() !== "Sweep") {
  gcslice(1000);
}

let genObj = g.f();
genObj.return();
assertEq(gcstate(), "Sweep");

// Test that we can create 700 cross compartment wrappers to nursery objects
// without triggering a minor GC.
gczeal(0);
let g = newGlobal({newCompartment: true});
evalcx("function f(x) { return {x: x}; }", g);
gc();
let initial = gcparam("gcNumber");
for (let i = 0; i < 700; i++)
    g.f(i);
let final = gcparam("gcNumber");
assertEq(final, initial);

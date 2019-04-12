// |jit-test| --no-ion; --no-baseline; skip-if: !('gcstate' in this && 'oomAfterAllocations' in this)

gczeal(0);

// Create a bunch of ObjectGroups with TypeNewScript attached.
const count = 1000;
let c = [];
let a = [];
for (let i = 0; i < count; i++) {
    c[i] = function() { this.a = 1; this.b = 0; this.c = 2; };
    a[i] = new c[i];
}

// Start an incremental GC and run until we're about to sweep type information.
assertEq(gcstate(), "NotActive");
gczeal(20);
startgc(1);

// Run incremental slices with simulated OOM set up to provoke OOM when sweeping
// types.
assertEq(gcstate(), "Sweep");
gczeal(10);
unsetgczeal(20);
while (gcstate() == "Sweep") {
    oomAfterAllocations(2);
    gcslice(1);
    resetOOMFailure();
}

// Ensure our type information stays alive.
let x = c.length + a.length;

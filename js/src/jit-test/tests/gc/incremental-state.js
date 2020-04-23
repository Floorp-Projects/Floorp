// |jit-test| skip-if: !hasFunction["gczeal"]

function assert(x) {
  assertEq(true, x);
}

// Test expected state changes during collection.
gczeal(0);

// Non-incremental GC.
gc();
assertEq(gcstate(), "NotActive");

// Incremental GC in minimal slice. Note that finalization always uses zero-
// sized slices while background finalization is on-going, so we need to loop.
gcslice(1000000);
assert(gcstate() !== "Mark");
finishgc();
assertEq(gcstate(), "NotActive");

// Incremental GC in multiple slices: if marking takes more than one slice,
// we yield before we start sweeping.
gczeal(0);
gcslice(1);
assertEq(gcstate(), "Mark");
gcslice(1000000);
assertEq(gcstate(), "Mark");
gcslice(1000000);
assert(gcstate() !== "Mark");
finishgc();

// Zeal mode 8: Incremental GC in two slices:
//   1) mark roots
//   2) mark and sweep
gczeal(8, 0);
gcslice(1);
assertEq(gcstate(), "Mark");
gcslice(1);
assertEq(gcstate(), "NotActive");

// Zeal mode 9: Incremental GC in two slices:
//   1) mark roots and marking
//   2) new marking and sweeping
gczeal(9, 0);
gcslice(1);
assertEq(gcstate(), "Mark");
gcslice(1);
assertEq(gcstate(), "NotActive");

// Zeal mode 10: Incremental GC in multiple slices (always yeilds before
// sweeping). This test uses long slices to prove that this zeal mode yields
// in sweeping, where normal IGC (above) does not.
gczeal(10, 0);
gcslice(1000000);
assertEq(gcstate(), "Sweep");
gcslice(1000000);
assert(gcstate() !== "Sweep");
finishgc();

// Two-slice zeal modes that yield once during sweeping.
for (let mode of [ 17, 19 ]) {
    print(mode);
    gczeal(mode, 0);
    gcslice(1);
    assertEq(gcstate(), "Sweep");
    gcslice(1);
    assertEq(gcstate(), "NotActive");
}

// Two-slice zeal modes that yield per-zone during sweeping.
const sweepingZealModes = [ 20, 21, 22, 23 ];
for (let mode of sweepingZealModes) {
    print(mode);
    gczeal(mode, 0);
    gcslice(1);
    while (gcstate() === "Sweep")
        gcslice(1);
    assertEq(gcstate(), "NotActive");
}

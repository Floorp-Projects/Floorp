// Test expected state changes during collection.
if (!("gcstate" in this))
    quit();

gczeal(0);

// Non-incremental GC.
gc();
assertEq(gcstate(), "NotActive");

// Incremental GC in minimal slice. Note that finalization always uses zero-
// sized slices while background finalization is on-going, so we need to loop.
gcslice(1000000);
while (gcstate() == "Finalize") { gcslice(1); }
while (gcstate() == "Decommit") { gcslice(1); }
assertEq(gcstate(), "NotActive");

// Incremental GC in multiple slices: if marking takes more than one slice,
// we yield before we start sweeping.
gczeal(0);
gcslice(1);
assertEq(gcstate(), "Mark");
gcslice(1000000);
assertEq(gcstate(), "Mark");
gcslice(1000000);
while (gcstate() == "Finalize") { gcslice(1); }
while (gcstate() == "Decommit") { gcslice(1); }
assertEq(gcstate(), "NotActive");

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
while (gcstate() == "Finalize") { gcslice(1); }
while (gcstate() == "Decommit") { gcslice(1); }
assertEq(gcstate(), "NotActive");

// Zeal mode 17: Incremental GC in two slices:
//   1) mark everything and start sweeping
//   2) finish sweeping
gczeal(17, 0);
gcslice(1);
assertEq(gcstate(), "Sweep");
gcslice(1);
assertEq(gcstate(), "NotActive");

// Test expected state changes during collection.
if (!("gcstate" in this))
    quit();

gczeal(0);

// Non-incremental GC.
gc();
assertEq(gcstate(), "none");

// Incremental GC in minimal slice. Note that finalization always uses zero-
// sized slices while background finalization is on-going, so we need to loop.
gcslice(1000000);
while (gcstate() == "finalize") { gcslice(1); }
while (gcstate() == "decommit") { gcslice(1); }
assertEq(gcstate(), "none");

// Incremental GC in multiple slices: if marking takes more than one slice,
// we yield before we start sweeping.
gczeal(0);
gcslice(1);
assertEq(gcstate(), "mark");
gcslice(1000000);
assertEq(gcstate(), "mark");
gcslice(1000000);
while (gcstate() == "finalize") { gcslice(1); }
while (gcstate() == "decommit") { gcslice(1); }
assertEq(gcstate(), "none");

// Zeal mode 8: Incremental GC in two main slices:
//   1) mark roots
//   2) mark and sweep
//   *) finalize.
gczeal(8, 0);
gcslice(1);
assertEq(gcstate(), "mark");
gcslice(1);
while (gcstate() == "finalize") { gcslice(1); }
while (gcstate() == "decommit") { gcslice(1); }
assertEq(gcstate(), "none");

// Zeal mode 9: Incremental GC in two main slices:
//   1) mark roots and marking
//   2) new marking and sweeping
//   *) finalize.
gczeal(9, 0);
gcslice(1);
assertEq(gcstate(), "mark");
gcslice(1);
while (gcstate() == "finalize") { gcslice(1); }
while (gcstate() == "decommit") { gcslice(1); }
assertEq(gcstate(), "none");

// Zeal mode 10: Incremental GC in multiple slices (always yeilds before
// sweeping). This test uses long slices to prove that this zeal mode yields
// in sweeping, where normal IGC (above) does not.
gczeal(10, 0);
gcslice(1000000);
assertEq(gcstate(), "sweep");
gcslice(1000000);
while (gcstate() == "finalize") { gcslice(1); }
while (gcstate() == "decommit") { gcslice(1); }
assertEq(gcstate(), "none");

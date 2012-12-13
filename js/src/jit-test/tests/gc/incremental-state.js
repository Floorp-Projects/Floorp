/*
 * Test expected state changes during collection.
 */

if ("gcstate" in this) {
    assertEq(gcstate(), "none");

    /* Non-incremental GC. */
    gc();
    assertEq(gcstate(), "none");

    /* Incremental GC in one slice. */
    gcslice(1000000);
    assertEq(gcstate(), "none");

    /* 
     * Incremental GC in multiple slices: if marking takes more than one slice,
     * we yield before we start sweeping.
     */
    gcslice(1);
    assertEq(gcstate(), "mark");
    gcslice(1000000);
    assertEq(gcstate(), "mark");
    gcslice(1000000);
    assertEq(gcstate(), "none");

    /* Zeal mode 8: Incremental GC in two slices: 1) mark roots 2) finish collection. */
    gczeal(8);
    gcslice(1);
    assertEq(gcstate(), "mark");
    gcslice(1);
    assertEq(gcstate(), "none");

    /* Zeal mode 9: Incremental GC in two slices: 1) mark all 2) new marking and finish. */
    gczeal(9);
    gcslice(1);
    assertEq(gcstate(), "mark");
    gcslice(1);
    assertEq(gcstate(), "none");

    /* Zeal mode 10: Incremental GC in multiple slices (always yeilds before sweeping). */
    gczeal(10);
    gcslice(1000000);
    assertEq(gcstate(), "sweep");
    gcslice(1000000);
    assertEq(gcstate(), "none");
}

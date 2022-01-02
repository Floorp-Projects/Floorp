/* Exercise the path where we want to collect a new compartment in the middle of incremental GC. */

var g1 = newGlobal();
var g2 = newGlobal();

schedulezone(g1);
gcslice(0); // Start IGC, but don't mark anything.
schedulezone(g2);
gcslice(1);
gcslice();

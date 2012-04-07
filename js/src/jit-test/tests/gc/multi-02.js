/* Exercise the path where we want to collect a new compartment in the middle of incremental GC. */

var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');

schedulegc(g1);
gcslice(0);
schedulegc(g2);
gcslice(1);
gcslice();

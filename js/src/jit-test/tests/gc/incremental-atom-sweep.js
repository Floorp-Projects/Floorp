/*
 * Exercise finding dead atoms in the atom set before they are removed by
 * SweepAtomState() - in AtomizeInline().
 */
o = {};
for (var i = 0 ; i < 10 ; ++i) {
    o["atom" + (i + 2)] = 1;
    delete o["atom" + (i + 1)];
    o["atom" + i] = 1;
    gcslice(1000000);
}

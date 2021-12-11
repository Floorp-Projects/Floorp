// In Nanojit, loads and stores have a maximum displacement of 16-bits.  Any
// displacements larger than that should be split off into a separate
// instruction that adds the displacement to the base pointer.  This
// program tests if this is done correctly.
//
// x.y ends up having a dslot offset of 79988, because of the 20000 array
// elements before it.  If Nanojit incorrectly stores this offset into a
// 16-bit value it will truncate to 14452 (because 79988 - 65536 == 14452).
// This means that the increments in the second loop will be done to one of
// the array elements instead of x.y.  And so x.y's final value will be
// (99 + 8) instead of 1099.
//
// Note that setting x.y to 99 and checking its value at the end will
// access the correct location because those lines are interpreted.  Phew.

var x = {}
for (var i = 0; i < 20000; i++)
    x[i] = 0;
x.y = 99;            // not traced, correctly accessed

for (var i = 0; i < 1000; ++i) {
    x.y++;           // traced, will access an array elem if disp was truncated
}
assertEq(x.y, 1099); // not traced, correctly accessed


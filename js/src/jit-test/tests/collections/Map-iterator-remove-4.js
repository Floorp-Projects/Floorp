// Multiple live iterators on the same Map can cope with removing entries.

load(libdir + "eqArrayHelper.js");
load(libdir + "asserts.js");

// Make a map.
var map = Map();
var SIZE = 7;
for (var j = 0; j < SIZE; j++)
    map.set(j, j);

// Make lots of iterators pointing to entry 2 of the map.
var NITERS = 5;
var iters = [];
for (var i = 0; i < NITERS; i++) {
    var iter = map.iterator();
    assertEqArray(iter.next(), [0, 0]);
    assertEqArray(iter.next(), [1, 1]);
    iters[i] = iter;
}

// Remove half of the map entries.
for (var j = 0; j < SIZE; j += 2)
    map.delete(j);

// Make sure all the iterators still work.
for (var i = 0; i < NITERS; i++) {
    var iter = iters[i];
    for (var j = 3; j < SIZE; j += 2)
        assertEqArray(iter.next(), [j, j]);
    assertThrowsValue(function () { iter.next(); }, StopIteration);
}

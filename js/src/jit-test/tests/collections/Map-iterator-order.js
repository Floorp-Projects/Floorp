// Map iterators produces entries in the order they were inserted.

load(libdir + "eqArrayHelper.js");

var map = new Map();
for (var i = 7; i !== 1; i = i * 7 % 1117)
    map.set("" + i, i);
assertEq(map.size, 557);

i = 7;
for (var pair of map) {
    assertEqArray(pair, ["" + i, i]);
    i = i * 7 % 1117;
}
assertEq(i, 1);

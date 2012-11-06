// Set iterators produces entries in the order they were inserted.

var set = Set();
var i;
for (i = 7; i !== 1; i = i * 7 % 1117)
    set.add(i);
assertEq(set.size, 557);

i = 7;
for (var v of set) {
    assertEq(v, i);
    i = i * 7 % 1117;
}
assertEq(i, 1);

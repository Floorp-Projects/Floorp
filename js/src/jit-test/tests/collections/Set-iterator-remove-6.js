// Removing many Set entries does not cause a live iterator to skip any of the
// entries that were not removed. (Compacting a Set must not be observable to
// script.)

load(libdir + "asserts.js");

var set = Set();
for (var i = 0; i < 32; i++)
    set.add(i);
var iter = set.iterator();
assertEq(iter.next(), 0);
for (var i = 0; i < 30; i++)
    set.delete(i);
assertEq(set.size(), 2);
for (var i = 32; i < 100; i++)
    set.add(i);  // eventually triggers compaction

for (var i = 30; i < 100; i++)
    assertEq(iter.next(), i);
assertThrowsValue(function () { iter.next(); }, StopIteration);

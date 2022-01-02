load(libdir + "asserts.js");

function f(tag) { return {[tag]: 1}; }
a = [];
for (var i = 0; i < 2000; i++)
    a[i] = f("first");

for (var i = 0; i < 2000; i++)
    assertEq(a[i].first, 1);

for (var i = 0; i < 2000; i++)
    a[i] = f("second");

for (var i = 0; i < 2000; i++)
    assertEq(a[i].second, 1);

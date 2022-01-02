// Array.of basics

load(libdir + "asserts.js");

var a = Array.of();
assertEq(a.length, 0);

a = Array.of(undefined, null, 3.14, []);
assertDeepEq(a, [undefined, null, 3.14, []]);

a = [];
for (var i = 0; i < 1000; i++)
    a[i] = i;
assertDeepEq(Array.of.apply({}, a), a);

// Iterators from another compartment work with both their own .next method
// with the other compartment's .next method.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var g = newGlobal();
g.eval(`var it = [1, 2][Symbol.iterator]();`);
assertIteratorNext(g.it, 1);
assertDeepEq([][Symbol.iterator]().next.call(g.it), { value: 2, done: false })

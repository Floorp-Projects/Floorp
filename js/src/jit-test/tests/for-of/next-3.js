// Iterators from another compartment work with their own .next method
// when called from another compartment, but not with the other
// compartment's .next method.

// FIXME: 'next' should work cross-realm.  Bug 924059.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var g = newGlobal();
g.eval("var it = [1, 2]['" + std_iterator + "']();");
assertIteratorResult(g.it.next(), 1, false);
assertThrowsInstanceOf([][std_iterator]().next.bind(g.it), TypeError)

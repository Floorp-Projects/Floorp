// The .next method of array iterators works across compartment boundaries.

load(libdir + "asserts.js");
var g = newGlobal('new-compartment');
g.eval("var it = [1, 2].iterator();");
assertEq(g.it.next(), 1);
assertEq([].iterator().next.call(g.it), 2);
assertThrowsValue([].iterator().next.bind(g.it), StopIteration);

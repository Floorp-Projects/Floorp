// map.iterator() and iter.next() are non-generic but work on cross-compartment wrappers.

load(libdir + "asserts.js");
var g = newGlobal();

var iterator_fn = Set.prototype.iterator;
assertThrowsInstanceOf(function () { iterator_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { iterator_fn.call(Map()); }, TypeError);
var setw = g.eval("Set(['x', 'y'])");
assertEq(iterator_fn.call(setw).next(), "x");

var next_fn = Set().iterator().next;
assertThrowsInstanceOf(function () { next_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { next_fn.call(Map().iterator()); }, TypeError);
var iterw = setw.iterator();
assertEq(next_fn.call(iterw), "x");
assertEq(next_fn.call(iterw), "y");
assertThrowsValue(function () { next_fn.call(iterw); }, g.StopIteration);

// map.iterator() and iter.next() are non-generic but work on cross-compartment wrappers.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var g = newGlobal();

var iterator_fn = Set.prototype[std_iterator];
assertThrowsInstanceOf(function () { iterator_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { iterator_fn.call(Map()); }, TypeError);
var setw = g.eval("Set(['x', 'y'])");
assertIteratorNext(iterator_fn.call(setw), "x");

var next_fn = Set()[std_iterator]().next;
assertThrowsInstanceOf(function () { next_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { next_fn.call(Map()[std_iterator]()); }, TypeError);
var iterw = setw[std_iterator]();
assertIteratorResult(next_fn.call(iterw), "x", false);
assertIteratorResult(next_fn.call(iterw), "y", false);
assertIteratorResult(next_fn.call(iterw), undefined, true);

// map.iterator() and iter.next() are non-generic but work on cross-compartment wrappers.

load(libdir + "asserts.js");
load(libdir + "eqArrayHelper.js");
var g = newGlobal('new-compartment');

var iterator_fn = Map.prototype.iterator;
assertThrowsInstanceOf(function () { iterator_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { iterator_fn.call(Set()); }, TypeError);
var mapw = g.eval("Map([['x', 1], ['y', 2]])");
assertEqArray(iterator_fn.call(mapw).next(), ["x", 1]);

var next_fn = Map().iterator().next;
assertThrowsInstanceOf(function () { next_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { next_fn.call(Set().iterator()); }, TypeError);
var iterw = mapw.iterator();
assertEqArray(next_fn.call(iterw), ["x", 1]);
assertEqArray(next_fn.call(iterw), ["y", 2]);
assertThrowsValue(function () { next_fn.call(iterw); }, g.StopIteration);

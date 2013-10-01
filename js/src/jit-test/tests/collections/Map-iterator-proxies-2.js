// map.iterator() and iter.next() are non-generic but work on cross-compartment wrappers.

load(libdir + "asserts.js");
load(libdir + "eqArrayHelper.js");
load(libdir + "iteration.js");

var g = newGlobal();

var iterator_fn = Map.prototype[std_iterator];
assertThrowsInstanceOf(function () { iterator_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { iterator_fn.call(Set()); }, TypeError);
var mapw = g.eval("Map([['x', 1], ['y', 2]])");
assertEqArray(iterator_fn.call(mapw).next().value, ["x", 1]);

var next_fn = Map()[std_iterator]().next;
assertThrowsInstanceOf(function () { next_fn.call({}); }, TypeError);
assertThrowsInstanceOf(function () { next_fn.call(Set()[std_iterator]()); }, TypeError);
var iterw = mapw[std_iterator]();
assertEqArray(next_fn.call(iterw).value, ["x", 1]);
assertEqArray(next_fn.call(iterw).value, ["y", 2]);
assertEq(next_fn.call(iterw).done, true);

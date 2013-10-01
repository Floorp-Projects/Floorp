// for-of is defined in terms of basic operations on objects, particularly
// [[Get]] for properties named "iterator" and "next", and [[Call]]. These
// "semantics" tests check that for-of really does appear to be implemented in
// terms of those more basic operations, as required by the spec, even in
// unusual cases.

// Deleting Array.prototype.iterator makes for-of stop working on arrays.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

delete Array.prototype[std_iterator];
assertThrowsInstanceOf(function () { for (var x of []) ; }, TypeError);

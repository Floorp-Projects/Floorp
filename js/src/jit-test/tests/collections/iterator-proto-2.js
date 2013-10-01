// Iterators of different collection types have different prototypes.

load(libdir + "iteration.js");

var aproto = Object.getPrototypeOf(Array()[std_iterator]());
var mproto = Object.getPrototypeOf(Map()[std_iterator]());
var sproto = Object.getPrototypeOf(Set()[std_iterator]());
assertEq(aproto !== mproto, true);
assertEq(aproto !== sproto, true);
assertEq(mproto !== sproto, true);
assertEq(aproto.next !== mproto.next, true);
assertEq(aproto.next !== sproto.next, true);
assertEq(mproto.next !== sproto.next, true);

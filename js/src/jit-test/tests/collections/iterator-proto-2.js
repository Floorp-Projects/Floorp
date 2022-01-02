// Iterators of different collection types have different prototypes.

load(libdir + "iteration.js");

var aproto = Object.getPrototypeOf(Array()[Symbol.iterator]());
var mproto = Object.getPrototypeOf((new Map())[Symbol.iterator]());
var sproto = Object.getPrototypeOf((new Set())[Symbol.iterator]());
assertEq(aproto !== mproto, true);
assertEq(aproto !== sproto, true);
assertEq(mproto !== sproto, true);
assertEq(aproto.next !== mproto.next, true);
assertEq(aproto.next !== sproto.next, true);
assertEq(mproto.next !== sproto.next, true);

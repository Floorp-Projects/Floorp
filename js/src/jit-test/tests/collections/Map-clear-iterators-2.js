// A Map iterator continues to visit entries added after a clear().

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var m = new Map([["a", 1]]);
var it = m[Symbol.iterator]();
assertIteratorNext(it, ["a", 1]);
m.clear();
m.set("b", 2);
assertIteratorNext(it, ["b", 2]);
assertIteratorDone(it, undefined);

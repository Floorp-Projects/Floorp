// for-of works on a cross-compartment wrapper of a Map.

var g = newGlobal();
var mw = g.eval("new Map([['a', 1], ['b', 2]])");
var log = '';
for (let [k, v] of mw)
    log += k + v;
assertEq(log, "a1b2");

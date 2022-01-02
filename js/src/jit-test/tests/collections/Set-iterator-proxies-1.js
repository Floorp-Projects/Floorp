// for-of works on a cross-compartment wrapper of a Set.

var g = newGlobal();
var mw = g.eval("new Set(['a', 'b', 1, 2])");
var log = '';
for (let x of mw)
    log += x;
assertEq(log, "ab12");

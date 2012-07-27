// for-of works on a cross-compartment wrapper of a Set.

var g = newGlobal('new-compartment');
var mw = g.eval("Set(['a', 'b', 1, 2])");
var log = '';
for (let x of mw)
    log += x;
assertEq(log, "ab12");

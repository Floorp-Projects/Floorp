// hasDebuggee tests.

var g1 = newGlobal('new-compartment'), g1w;
g1.eval("var g2 = newGlobal('same-compartment')");
var g2 = g1.g2;
var g1w, g2w;

var dbg = new Debugger;
function checkHas(hasg1, hasg2) {
    assertEq(dbg.hasDebuggee(g1), hasg1);
    if (typeof g1w === 'object')
        assertEq(dbg.hasDebuggee(g1w), hasg1);
    assertEq(dbg.hasDebuggee(g2), hasg2);
    if (typeof g2w === 'object')
        assertEq(dbg.hasDebuggee(g2w), hasg2);
}

checkHas(false, false);
g1w = dbg.addDebuggee(g1);
checkHas(true, false);
g2w = dbg.addDebuggee(g2);
checkHas(true, true);
dbg.removeDebuggee(g1w);
checkHas(false, true);
dbg.removeDebuggee(g2);
checkHas(false, false);

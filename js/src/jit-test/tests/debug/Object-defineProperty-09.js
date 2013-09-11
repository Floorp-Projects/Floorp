// defineProperty can't re-define non-configurable properties.
// Also: when defineProperty throws, the exception is native to the debugger
// compartment, not a wrapper.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
gw.defineProperty("p", {value: 1});
g.p = 4;
assertEq(g.p, 1);

var threw;
try {
    gw.defineProperty("p", {value: 2});
    threw = false;
} catch (exc) {
    threw = true;
    assertEq(exc instanceof TypeError, true);
    assertEq(typeof exc.message, "string");
    assertEq(typeof exc.stack, "string");
}
assertEq(threw, true);

assertEq(g.p, 1);

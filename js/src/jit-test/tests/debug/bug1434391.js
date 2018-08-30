if (!('oomTest' in this))
    quit();

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
oomTest(function () {
    assertEq(gw.executeInGlobal("(42).toString(0)").throw.errorMessageName, "JSMSG_BAD_RADIX");
});

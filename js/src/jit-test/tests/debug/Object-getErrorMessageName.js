// Debugger.Object.prototype.getErrorMessageName returns the error message name
// associated with some error object.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

assertEq(gw.executeInGlobal("(42).toString(0)").throw.errorMessageName, "JSMSG_BAD_RADIX");

// Custom errors have no error message name.
assertEq(gw.executeInGlobal("throw new Error()").throw.errorMessageName, undefined);

// Ensure that the method works across globals.
g.eval(`var g = newGlobal();
        g.eval('var err; try { (42).toString(0); } catch (e) { err = e; }');
        var err2 = g.err;`);
assertEq(gw.executeInGlobal("throw err2").throw.errorMessageName, "JSMSG_BAD_RADIX");

// Ensure that non-error objects return undefined.
const Args = [
    "1",
    "'blah'",
    "({})",
    "[]",
    "() => 1"
]

for (let arg of Args)
    assertEq(gw.executeInGlobal(`${arg}`).return.errorMessageName, undefined);

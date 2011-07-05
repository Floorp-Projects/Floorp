// Handle proto-less objects passed to addDebuggee.

var g = newGlobal('new-compartment');
var obj = g.eval("Object.create(null)");
var dbg = new Debugger;

// hasDebuggee and removeDebuggee must tolerate proto-less objects.
assertEq(dbg.hasDebuggee(obj), false);

// addDebuggee may either succeed or throw a TypeError. Don't care.
var added;
try {
    dbg.addDebuggee(obj);
    added = true;
} catch (exc) {
    if (!(exc instanceof TypeError))
        throw exc;
    added = false;
}

assertEq(dbg.hasDebuggee(obj), added);
assertEq(dbg.getDebuggees().length, added ? 1 : 0);
assertEq(dbg.removeDebuggee(obj), undefined);
assertEq(dbg.hasDebuggee(obj), false);

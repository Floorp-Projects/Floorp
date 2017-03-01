if (helperThreadCount() == 0)
    quit();

// The new Debugger here should throw but we don't have a way to verify this
// (exceptions that worker threads throw do not cause the test to fail).
evalInCooperativeThread('cooperativeYield(); var dbg = new Debugger();');

var dbg = new Debugger;
assertEq(dbg.addAllGlobalsAsDebuggees(), undefined);

function assertThrows(f) {
    var exception = false;
    try { f(); } catch (e) { exception = true; }
    assertEq(exception, true);
}

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {};
assertThrows(() => evalInCooperativeThread("var x = 3"));
assertThrows(cooperativeYield);

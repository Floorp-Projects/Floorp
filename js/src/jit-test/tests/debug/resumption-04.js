// |jit-test| error: already executing generator
// Forced return from a generator frame.

var g = newGlobal();
var val = {value: '!', done: false};
g.debuggeeGlobal = this;
g.eval("var dbg = new Debugger();" +
       "var gw = dbg.addDebuggee(debuggeeGlobal);" +
       "var val = gw.makeDebuggeeValue(debuggeeGlobal.val);" +
       "dbg.onDebuggerStatement = function () { return {return: val}; };");

function* gen() {
    yield '1';
    debugger;  // Force return here. The value is ignored.
    yield '2';
}

var iter = gen();
assertEq(iter.next().value, "1");
assertEq(iter.next().value, "!");
iter.next();
assertEq(0, 1);

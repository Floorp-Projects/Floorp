// |jit-test| debug
// Forced return from a generator frame.

var g = newGlobal('new-compartment');
g.debuggeeGlobal = this;
g.eval("var dbg = new Debugger(debuggeeGlobal);" +
       "dbg.onDebuggerStatement = function () { return {return: '!'}; };");

function gen() {
    yield '1';
    debugger;  // Force return here. The value is ignored.
    yield '2';
}
var x = [v for (v in gen())];
assertEq(x.join(","), "1");

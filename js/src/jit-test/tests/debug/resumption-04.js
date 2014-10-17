// |jit-test| error: already executing generator
// Forced return from a generator frame.

var g = newGlobal();
g.debuggeeGlobal = this;
g.eval("var dbg = new Debugger(debuggeeGlobal);" +
       "dbg.onDebuggerStatement = function () { return {return: '!'}; };");

function gen() {
    yield '1';
    debugger;  // Force return here. The value is ignored.
    yield '2';
}

var iter = gen();
assertEq(iter.next(), "1");
assertEq(iter.next(), "!");
iter.next();
assertEq(0, 1);

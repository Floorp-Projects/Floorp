// Ensure that uncaught exceptions thrown in onExceptionUnwind do not
// propagate outward into debuggee execution.
var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var frame;
dbg.onExceptionUnwind = function (f, x) {
    frame = f;
    assertEq(frame.onStack, true);
    throw 'unhandled';
};
dbg.onDebuggerStatement = function(f) {
    assertEq(f.eval('throw 42').throw, 42);
    assertEq(frame.onStack, false);
};
g.eval('debugger');

// Don't fail just because we reported an uncaught exception.
quit(0);

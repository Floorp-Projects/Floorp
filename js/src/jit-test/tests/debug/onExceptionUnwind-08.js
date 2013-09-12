// Ensure that ScriptDebugEpilogue gets called when onExceptionUnwind
// throws an uncaught exception.
var g = newGlobal();
var dbg = Debugger(g);
var frame;
dbg.onExceptionUnwind = function (f, x) {
    frame = f;
    assertEq(frame.live, true);
    throw 'unhandled';
};
dbg.onDebuggerStatement = function(f) {
    assertEq(f.eval('throw 42'), null);
    assertEq(frame.live, false);
};
g.eval('debugger');

// Don't fail just because we reported an uncaught exception.
quit(0);

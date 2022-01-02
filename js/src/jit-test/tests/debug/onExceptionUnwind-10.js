// Ensure that ScriptDebugEpilogue gets called when onExceptionUnwind
// terminates execution.
var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
var frame;
dbg.onExceptionUnwind = function (f, x) {
    frame = f;
    assertEq(frame.type, 'eval');
    assertEq(frame.onStack, true);
    terminate();
};
dbg.onDebuggerStatement = function(f) {
    assertEq(f.eval('throw 42'), null);
    assertEq(frame.onStack, false);
};
g.eval('debugger');

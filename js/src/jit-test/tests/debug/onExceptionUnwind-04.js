// onExceptionUnwind is not called for exceptions thrown and handled in the debugger.
var g = newGlobal();
var dbg = Debugger(g);
g.log = '';
dbg.onDebuggerStatement = function (frame) {
    try {
        throw new Error("oops");
    } catch (exc) {
        g.log += exc.message;
    }
};
dbg.onExceptionUnwind = function (frame) {
    g.log += 'BAD';
};

g.eval("debugger; log += ' ok';");
assertEq(g.log, 'oops ok');

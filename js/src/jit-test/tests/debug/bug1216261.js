// |jit-test| exitstatus: 3

if (!('oomAfterAllocations' in this))
    quit(3);

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onDebuggerStatement = function(frame) {
    oomAfterAllocations(5);
    // OOMs here, and possibly again in the error reporter when trying to
    // report the OOM, so the shell just exits with code 3.
    frame.older.eval("escaped = function() { return y }");
}
g.eval("function h() { debugger }");
g.eval("(function () { var y = {p:42}; h(); yield })().next();");

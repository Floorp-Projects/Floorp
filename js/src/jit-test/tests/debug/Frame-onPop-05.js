var g = newGlobal();
var dbg = new Debugger(g);
g.debuggerGlobal = this;
var log;

dbg.onEnterFrame = function handleEnter(f) {
    log += '(';
    f.onPop = function handlePop(c) {
        log += ')';
        assertEq(c.throw, "election");
    };
};
dbg.onExceptionUnwind = function handleExceptionUnwind(f, x) {
    log += 'u';
    assertEq(x, "election");
};

log = '';
try {
    g.eval("try { throw 'election'; } finally { debuggerGlobal.log += 'f'; }");
} catch (x) {
    log += 'c';
    assertEq(x, 'election');
}
assertEq(log, '(ufu)c');

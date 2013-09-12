// dbg.getNewestFrame in an onPop handler returns the frame being popped.
var g = newGlobal();
g.eval("function f() { debugger; }");
g.eval("function g() { f(); }");
g.eval("function h() { g(); }");
g.eval("function i() { h(); }");

var dbg = new Debugger(g);
var log;
dbg.onEnterFrame = function handleEnter(f) {
    log += "(" + f.callee.name;
    f.onPop = function handlePop(c) {
        log += ")" + f.callee.name;  
        assertEq(dbg.getNewestFrame(), this);
    };
};
log = '';
g.i();
assertEq(log, "(i(h(g(f)f)g)h)i");

// When a termination is propagated out of multiple frames, their onPop
// handlers are called in the correct order, and no onExceptionUnwind
// handlers are called.
var g = newGlobal();
g.eval("function f() { terminate(); }");
g.eval("function g() { f(); }");
g.eval("function h() { g(); }");
g.eval("function i() { h(); }");

var dbg = new Debugger(g);
var log;
var count = 0;
function makePopHandler(label, resumption) {
    return function handlePop(completion) { 
        log += label;
        assertEq(completion, null);
        return resumption;
    };
}
dbg.onEnterFrame = function handleEnter(f) {
    log += "(" + f.callee.name;
    f.onPop = makePopHandler(f.callee.name + ")",
                             count++ == 0 ? { return: 'king' } : undefined);
};
dbg.onExceptionUnwind = function handleExceptionUnwind(f, x) {
    log += 'u';  
};
log = '';
assertEq(g.i(), 'king');
assertEq(log, "(i(h(g(ff)g)h)i)");

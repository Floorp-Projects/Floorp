// When an exception is propagated out of multiple frames, their onPop
// and onExceptionUnwind handlers are called in the correct order.
var g = newGlobal('new-compartment');
g.eval("function f() { throw 'mud'; }");
g.eval("function g() { f(); }");
g.eval("function h() { g(); }");
g.eval("function i() { h(); }");

var dbg = new Debugger(g);
var log;
function makePopHandler(label) {
    return function handlePop(completion) { 
        log += label;
        assertEq(completion.throw, "mud");
    };
}
dbg.onEnterFrame = function handleEnter(f) {
    log += "(" + f.callee.name;
    f.onPop = makePopHandler(")" + f.callee.name);  
};
dbg.onExceptionUnwind = function handleExceptionUnwind(f, x) {
    assertEq(x, 'mud');
    log += "u" + f.callee.name;
};
log = '';
try {
    g.i();
} catch (x) {
    log += 'c';
    assertEq(x, "mud");
}
assertEq(log, "(i(h(g(fuf)fug)guh)hui)ic");

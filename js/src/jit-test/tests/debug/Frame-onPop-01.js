// When multiple frames have onPop handlers, they are called in the correct order.
var g = newGlobal();
g.eval("function f() { debugger; }");
g.eval("function g() { f(); }");
g.eval("function h() { g(); }");
g.eval("function i() { h(); }");

var dbg = new Debugger(g);
var log;
function logger(frame, mark) { 
    return function (completion) {
        assertEq(this, frame);
        assertEq('return' in completion, true);
        log += mark;
    };
}
dbg.onEnterFrame = function handleEnter(f) {
    log += "(" + f.callee.name;
    // Note that this establishes a distinct function object as each
    // frame's onPop handler. Thus, a pass proves that each frame is
    // remembering its handler separately.
    f.onPop = logger(f, f.callee.name + ")");  
};
dbg.onDebuggerStatement = function handleDebugger(f) {
    log += 'd';
};
log = '';
g.i();
assertEq(log, "(i(h(g(fdf)g)h)i)");

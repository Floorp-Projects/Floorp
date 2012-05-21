// frame.eval can evaluate code in a frame pushed in another context. Bug 749697.

// In other words, the debugger can see all frames on the stack, even though
// each frame is attached to a particular JSContext and multiple JSContexts may
// have frames on the stack.

var g = newGlobal('new-compartment');
g.eval('function f(a) { debugger; evaluate("debugger;", {newContext: true}); }');

var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame1) {
    dbg.onDebuggerStatement = function (frame2) {
        assertEq(frame1.eval("a").return, 31);
        hits++;
    };
};

g.f(31);
assertEq(hits, 1);

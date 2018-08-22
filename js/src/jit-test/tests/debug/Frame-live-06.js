// frame.live is false for generator frames after they return.

let g = newGlobal();
g.eval("function* f() { debugger; }");

let dbg = Debugger(g);
let savedFrame;

dbg.onDebuggerStatement = frame => {
    savedFrame = frame;
    assertEq(frame.callee.name, "f");
    assertEq(frame.live, true);
    frame.onPop = function() {
        assertEq(frame.live, true);
    };
};
g.f().next();

assertEq(savedFrame.live, false);
try {
    savedFrame.older;
    throw new Error("expected exception, none thrown");
} catch (exc) {
    assertEq(exc.message, "Debugger.Frame is not live");
}


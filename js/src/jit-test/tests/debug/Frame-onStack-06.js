// frame.onStack is false for generator frames after they return.

let g = newGlobal({newCompartment: true});
g.eval("function* f() { debugger; }");

let dbg = Debugger(g);
let savedFrame;

dbg.onDebuggerStatement = frame => {
    savedFrame = frame;
    assertEq(frame.callee.name, "f");
    assertEq(frame.onStack, true);
    frame.onPop = function() {
        assertEq(frame.onStack, true);
    };
};
g.f().next();

assertEq(savedFrame.onStack, false);
try {
    savedFrame.older;
    throw new Error("expected exception, none thrown");
} catch (exc) {
    assertEq(exc.message, "Debugger.Frame is not on stack or suspended");
}


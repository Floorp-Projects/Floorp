// If onEnterFrame terminates a generator, the Frame is left in a sane but inactive state.

load(libdir + "asserts.js");

let g = newGlobal({newCompartment: true});
g.eval("function* f(x) { yield x; }");
let dbg = new Debugger;
let gw = dbg.addDebuggee(g);

let genFrame = null;
dbg.onDebuggerStatement = frame => {
    dbg.onEnterFrame = frame => {
        if (frame.callee == gw.getOwnPropertyDescriptor("f").value) {
            genFrame = frame;
            return null;
        }
    };
    assertEq(frame.eval("f(0);"), null);
};

g.eval("debugger;");

assertEq(genFrame instanceof Debugger.Frame, true);
assertEq(genFrame.onStack, false);
assertThrowsInstanceOf(() => genFrame.callee, Error);

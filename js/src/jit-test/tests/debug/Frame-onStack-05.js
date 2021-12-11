// frame.onStack is false for frames removed after their compartments stopped being debuggees.

load(libdir + 'asserts.js');

var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = Debugger(g1, g2);
var hits = 0;
var snapshot = [];
dbg.onDebuggerStatement = function (frame) {
    if (hits++ === 0) {
        assertEq(frame.eval("x();"), null);
    } else {
        for (var f = frame; f; f = f.older) {
            if (f.type === "call" && f.script !== null)
                snapshot.push(f);
        }
        dbg.removeDebuggee(g2);
        return null;
    }
};

g1.eval("function z() { debugger; }");
g2.z = g1.z;
g2.eval("function y() { z(); }");
g2.eval("function x() { y(); }");
assertEq(g2.eval("debugger; 'ok';"), "ok");
assertEq(hits, 2);
assertEq(snapshot.length, 3);
for (var i = 0; i < snapshot.length; i++) {
    assertEq(snapshot[i].onStack, false);
    assertThrowsInstanceOf(() => frame.script, Error);
}

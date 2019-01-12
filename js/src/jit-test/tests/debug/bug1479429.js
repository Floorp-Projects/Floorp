// Bug 1479429 - Methods throw on out-of-range bytecode offsets.

load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onDebuggerStatement = function(frame) {
    assertThrowsInstanceOf(
        () => frame.script.getPredecessorOffsets(0x400000),
        TypeError);
    assertThrowsInstanceOf(
        () => frame.script.getSuccessorOffsets(-1),
        TypeError);
}
g.eval("debugger;");

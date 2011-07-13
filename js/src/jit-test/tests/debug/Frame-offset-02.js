// frame.offset gives different values at different points in a script.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var s = undefined, a = []
dbg.onDebuggerStatement = function (frame) {
    if (s === undefined)
        s = frame.script;
    else
        assertEq(s, frame.script);
    assertEq(frame.offset !== undefined, true);
    assertEq(a.indexOf(frame.offset), -1);
    a.push(frame.offset);
};
g.eval("debugger; debugger; debugger;");
assertEq(a.length, 3);

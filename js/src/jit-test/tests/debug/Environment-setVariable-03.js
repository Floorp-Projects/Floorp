// setVariable cannot create new global variables.
// (Other kinds of environment are tested in Environment-variables.js.)

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertThrowsInstanceOf(function () { frame.environment.setVariable("x", 7); }, TypeError);
    hits++;
};
g.eval("debugger");
assertEq("x" in g, false);
assertEq(hits, 1);


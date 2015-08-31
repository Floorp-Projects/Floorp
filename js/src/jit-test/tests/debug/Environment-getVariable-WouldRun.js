// getVariable that would trigger a getter does not crash or explode.
// It should throw WouldRunDebuggee, but that isn't implemented yet.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    assertThrowsInstanceOf(function () {
        frame.environment.parent.getVariable("x");
    }, Error);
    hits++;
};
g.eval("Object.defineProperty(this, 'x', {get: function () { throw new Error('fail'); }});\n" +
       "debugger;");
assertEq(hits, 1);

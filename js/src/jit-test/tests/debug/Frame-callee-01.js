// Frame.prototype.callee for a normal function frame.

load(libdir + "asserts.js");

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
g.eval(`
function f() {}
`);

const fObj = dbg.makeGlobalObjectReference(g).makeDebuggeeValue(g.f);
let frame;
let callee;
dbg.onEnterFrame = function(f) {
  frame = f;
  callee = frame.callee;
};

g.f();

assertEq(frame instanceof Debugger.Frame, true);
assertEq(callee instanceof Debugger.Object, true);
assertEq(callee, fObj);

// The frame has finished evaluating, so the callee is no longer accessible.
assertThrowsInstanceOf(() => frame.callee, Error);

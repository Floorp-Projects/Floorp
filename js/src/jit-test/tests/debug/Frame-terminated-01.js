// Check `.terminated` functionality for normal functions.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
function f(){}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.terminated, false);
};

g.f();

assertEq(frame instanceof Debugger.Frame, true);
assertEq(frame.terminated, true);

// Check `.terminated` functionality for generator functions.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
function* f(){}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.terminated, false);
};

const it = g.f();

assertEq(frame instanceof Debugger.Frame, true);
assertEq(frame.terminated, false);

it.next();

assertEq(frame.terminated, true);

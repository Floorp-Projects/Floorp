// Debugger.Frame.prototype.type on a normal function.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = Debugger(g);

g.eval(`
function f() {}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.type, "call");
};

g.f();

assertEq(!!frame, true);
// Throws because the frame has terminated.
assertThrowsInstanceOf(() => frame.type, Error);

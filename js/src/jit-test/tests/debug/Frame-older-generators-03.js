// Test that Debugger.Frame.prototype.older works on suspended generators.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
function* f() {}
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

const it = g.f();

assertEq(frame.older, null);

it.next();

assertThrowsInstanceOf(() => frame.older, Error);

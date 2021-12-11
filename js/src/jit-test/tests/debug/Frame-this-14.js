// Test that Debugger.Frame.prototype.this works on a suspended generator
// function.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

g.eval(`
var context = {};
var f = function*() {
  return this;
}.bind(context);
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  assertEq(frame.this, gDO.makeDebuggeeValue(g.context));
  dbg.onEnterFrame = undefined;
};

const it = g.f();

assertEq(!!frame, true);
assertEq(frame.this, gDO.makeDebuggeeValue(g.context));

it.next();

assertThrowsInstanceOf(() => frame.this, Error);

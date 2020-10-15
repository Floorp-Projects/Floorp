// Test that Debugger.Frame.prototype.this works on a suspended async
// function.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger();
const gDO = dbg.addDebuggee(g);

g.eval(`
var context = {};
var f = async function() {
  await Promise.resolve();
  return this;
}.bind(context);
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  assertEq(frame.this, gDO.makeDebuggeeValue(g.context));
  dbg.onEnterFrame = undefined;
};

(async () => {
  const promise = g.f();

  assertEq(!!frame, true);
  assertEq(frame.this, gDO.makeDebuggeeValue(g.context));

  await promise;

  assertThrowsInstanceOf(() => frame.this, Error);
})();

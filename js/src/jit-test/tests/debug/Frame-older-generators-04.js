// Test that Debugger.Frame.prototype.older works on suspended async functions.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
async function f() {
  await Promise.resolve();
}
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

(async () => {
  const promise = g.f();

  assertEq(frame.older, null);

  await promise;

  assertThrowsInstanceOf(() => frame.older, Error);
})();

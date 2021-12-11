// Test that Debugger.Frame.prototype.older works on suspended async generators.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
async function* f() {
  await Promise.resolve();
}
`);

let frame;
dbg.onEnterFrame = f => {
  frame = f;
  dbg.onEnterFrame = undefined;
};

(async () => {
  const it = g.f();

  assertEq(frame.older, null);

  const promise = it.next();

  assertEq(frame.older, null);

  await promise;

  assertThrowsInstanceOf(() => frame.older, Error);
})();

// Debugger.Frame.prototype.type on an async function.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = Debugger(g);

g.eval(`
async function f() {
  await Promise.resolve();
}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.type, "call");
};

(async () => {
  const promise = g.f();

  assertEq(frame.type, "call");
  frame = null;

  await promise;

  assertEq(!!frame, true);
  // Throws because the frame has terminated.
  assertThrowsInstanceOf(() => frame.type, Error);
})();

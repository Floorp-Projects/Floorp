// Check `.terminated` functionality for async generator functions.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
async function* f(){
  await Promise.resolve();
}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.terminated, false);
};

(async () => {
  const it = g.f();

  assertEq(frame instanceof Debugger.Frame, true);
  assertEq(frame.terminated, false);

  const promise = it.next();

  assertEq(frame.terminated, false);

  await promise;

  assertEq(frame.terminated, true);
})();

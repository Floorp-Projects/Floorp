// Check `.terminated` functionality for async functions.

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

g.eval(`
async function f(){
  await Promise.resolve();
}
`);

let frame;
dbg.onEnterFrame = function(f) {
  frame = f;
  assertEq(frame.terminated, false);
};

(async () => {
  const promise = g.f();

  assertEq(frame instanceof Debugger.Frame, true);
  assertEq(frame.terminated, false);

  await promise;

  assertEq(frame.terminated, true);
})();

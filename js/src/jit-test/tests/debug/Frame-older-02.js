// An explicit async stack should interrupt a Debugger.Frame chain.

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
let done = false;
dbg.onDebuggerStatement = function (frame) {
  // The frame has no "older" frame because the explicit async stack
  // attached to the async function takes priority over the real
  // parent frame that is tracked in the frame iterator.
  assertEq(!!frame.older, false);

  done = true;
};

g.eval(`
let draining = false;
async function run() {
  await Promise.resolve();

  // Make sure that the test is running within "drainJobQueue()".
  assertEq(draining, true);
  debugger;
}

(function main() {
  run();

  // Force resumption of the "run" function.
  draining = true;
  drainJobQueue();
  draining = false;
})();
`);
assertEq(done, true);

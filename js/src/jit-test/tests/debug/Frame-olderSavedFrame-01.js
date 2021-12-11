// An explicit async stack should be available via olderSavedFrame.
// This test makes sure that "main" shows up as an async saved frame
// instead of "older", even though "drainJobQueue()" is running inside of
// "main()" directly.

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
let done = false;
dbg.onDebuggerStatement = function (frame) {
  // This frame has an "olderSavedFrame" because "run()" is an async function
  // and those have explicit async stacks attached to them.
  const parent = frame.olderSavedFrame;
  assertEq(typeof parent.source, "string");
  assertEq(parent.line, 12);
  assertEq(parent.column, 3);
  assertEq(parent.asyncCause, "async");
  assertEq(parent.functionDisplayName, "main");
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

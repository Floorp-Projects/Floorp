// An explicit async stack should be available via olderSavedFrame.
// This test makes sure that "main" shows up as an async saved frame properly
// when the promise is resumed normally outside "main()".

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);
let done = false;
dbg.onDebuggerStatement = function (frame) {
  // This frame has an "olderSavedFrame" because "run()" is an async function
  // and those have explicit async stacks attached to them.
  const parent = frame.olderSavedFrame;
  assertEq(typeof parent.source, "string");
  assertEq(parent.line, 9);
  assertEq(parent.column, 3);
  assertEq(parent.asyncCause, "async");
  assertEq(parent.functionDisplayName, "main");
  done = true;
};

g.eval(`
let draining = false;
async function run() {
  await Promise.resolve();
  debugger;
}

(function main() {
  run();
})();
drainJobQueue();
`);
assertEq(done, true);

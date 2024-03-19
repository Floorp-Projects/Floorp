// catchTermination should undo the quit() operation and let the remaining jobs
// run.

evaluate(`
  quit();
`, {
  catchTermination : true
});

const global = newGlobal({ newCompartment: true });

let called = false;
const dbg = new Debugger(global);
dbg.onDebuggerStatement = function (frame) {
  Promise.resolve(42).then(v => { called = true; });
};
global.eval(`
  debugger;
`);

drainJobQueue();

assertEq(called, true);

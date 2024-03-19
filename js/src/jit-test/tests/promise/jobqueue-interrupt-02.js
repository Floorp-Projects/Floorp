// quit() while draining job queue leaves the remaining jobs untouched.

const global = newGlobal({ newCompartment:true });
const dbg = Debugger(global);
dbg.onDebuggerStatement = function() {
  Promise.resolve().then(() => {
    quit();
  });
  Promise.resolve().then(() => {
    // This shouldn't be called.
    assertEq(true, false);
  });
};
global.eval("debugger");

// Changing onPop while the function is dead is allowed.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

let steps = new Set();
dbg.onDebuggerStatement = function(frame) {
  // Setting 'onPop' while alive is allowed.
  steps.add("debugger 1");
  assertEq(frame.onStack, true);
  frame.onPop = function() {
    steps.add("onpop 1");
  };

  dbg.onDebuggerStatement = function() {
    // Clear the 'onPop' while dead.
    steps.add("debugger 2");
    assertEq(frame.onStack, false);

    // Clearing 'onPop' while dead is allowed.
    frame.onPop = undefined;

      // Setting 'onPop' while dead is allowed.
    frame.onPop = function() {
      steps.add("onpop 2");
    };
  };
};

g.eval(
  `
    function myGen() {
      debugger;
    }

    const g = myGen();

    debugger;
  `
);

assertDeepEq(Array.from(steps), [
  "debugger 1",
  "onpop 1",
  "debugger 2",
]);

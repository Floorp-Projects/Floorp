// Changing onPop while the generator is suspended/dead is allowed.

load(libdir + "asserts.js");

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

let steps = new Set();
dbg.onDebuggerStatement = function(frame) {
  // Setting 'onStep' while alive is allowed.
  steps.add("debugger 1");
  assertEq(frame.onStack, true);
  frame.onPop = function() {
    steps.add("onpop 1");
  };

  dbg.onDebuggerStatement = function() {
    // Clear the 'onPop' while suspended.
    steps.add("debugger 2");
    assertEq(frame.onStack, false);

    // Clearing 'onPop' while suspended is allowed.
    frame.onPop = undefined;

      // Setting 'onPop' while suspended is allowed.
    frame.onPop = function() {
      steps.add("onpop 2");
    };

    dbg.onDebuggerStatement = function() {
      steps.add("debugger 3");
      assertEq(frame.onStack, false);

      // Clearing 'onPop' while dead is allowed.
      frame.onPop = undefined;

        // Setting 'onPop' while dead is allowed.
      frame.onPop = function() {
        steps.add("onpop 3");
      };
    };
  };
};

g.eval(
  `
    function* myGen() {
      debugger;
      yield;
    }

    const g = myGen();
    g.next();

    debugger;
    g.next();

    debugger;
  `
);

assertDeepEq(Array.from(steps), [
  "debugger 1",
  "onpop 1",
  "debugger 2",
  "onpop 2",
  "debugger 3",
]);

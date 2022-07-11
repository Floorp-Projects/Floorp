// Bug 1117242: Test calling SavedFrame getters from globals that don't subsume
// that frame's principals.

const {addDebuggerToGlobal} = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

const lowP = Services.scriptSecurityManager.createNullPrincipal({});
const midP = [lowP, "http://other.com"];
const highP = Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);

const low  = new Cu.Sandbox(lowP);
const mid  = new Cu.Sandbox(midP);
const high = new Cu.Sandbox(highP);

function run_test() {
  // Test that the priveleged view of a SavedFrame from a subsumed compartment
  // is the same view that the subsumed compartment gets. Create the following
  // chain of function calls (with some intermediate system-principaled frames
  // due to implementation):
  //
  //     low.lowF -> mid.midF -> high.highF -> high.saveStack
  //
  // Where high.saveStack gets monkey patched to create stacks in each of our
  // sandboxes.

  Cu.evalInSandbox("function highF() { return saveStack(); }", high);

  mid.highF = () => high.highF();
  Cu.evalInSandbox("function midF() { return highF(); }", mid);

  low.midF = () => mid.midF();
  Cu.evalInSandbox("function lowF() { return midF(); }", low);

  const expected = [
    {
      sandbox: low,
      frames: ["lowF"],
    },
    {
      sandbox: mid,
      frames: ["midF", "lowF"],
    },
    {
      sandbox: high,
      frames: ["getSavedFrameInstanceFromSandbox",
               "saveStack",
               "highF",
               "run_test/mid.highF",
               "midF",
               "run_test/low.midF",
               "lowF",
               "run_test",
               "_execute_test",
               null],
    }
  ];

  for (let { sandbox, frames } of expected) {
    high.saveStack = function saveStack() {
      return getSavedFrameInstanceFromSandbox(sandbox);
    };

    const xrayStack = low.lowF();
    equal(xrayStack.functionDisplayName, "getSavedFrameInstanceFromSandbox",
          "Xrays should always be able to see everything.");

    let waived = Cu.waiveXrays(xrayStack);
    do {
      ok(frames.length,
         "There should still be more expected frames while we have actual frames.");
      equal(waived.functionDisplayName, frames.shift(),
            "The waived wrapper should give us the stack's compartment's view.");
      waived = waived.parent;
    } while (waived);
  }
}

// Get a SavedFrame instance from inside the given sandbox.
//
// We can't use Cu.getJSTestingFunctions().saveStack() because Cu isn't
// available to sandboxes that don't have the system principal. The easiest way
// to get the SavedFrame is to use the Debugger API to track allocation sites
// and then do an allocation.
function getSavedFrameInstanceFromSandbox(sandbox) {
  const dbg = new Debugger(sandbox);

  dbg.memory.trackingAllocationSites = true;
  Cu.evalInSandbox("new Object", sandbox);
  const allocs = dbg.memory.drainAllocationsLog();
  dbg.memory.trackingAllocationSites = false;

  ok(allocs[0], "We should observe the allocation");
  const { frame } = allocs[0];

  if (sandbox !== high) {
    ok(Cu.isXrayWrapper(frame), "`frame` should be an xray...");
    equal(Object.prototype.toString.call(Cu.waiveXrays(frame)),
          "[object SavedFrame]",
          "...and that xray should wrap a SavedFrame");
  }

  return frame;
}


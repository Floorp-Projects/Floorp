/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the ChromeUtils interface.

if (typeof Debugger != "function") {
  const { addDebuggerToGlobal } = Cu.import("resource://gre/modules/jsdebugger.jsm", {});
  addDebuggerToGlobal(this);
}

function run_test() {
  ok(ChromeUtils, "Should be able to get the ChromeUtils interface");

  testBadParameters();
  testGoodParameters();

  do_test_finished();
}

function testBadParameters() {
  throws(() => ChromeUtils.saveHeapSnapshot(),
         "Should throw if arguments aren't passed in.");

  throws(() => ChromeUtils.saveHeapSnapshot(null),
         "Should throw if boundaries isn't an object.");

  throws(() => ChromeUtils.saveHeapSnapshot({}),
         "Should throw if the boundaries object doesn't have any properties.");

  throws(() => ChromeUtils.saveHeapSnapshot({ runtime: true,
                                              globals: [this] }),
         "Should throw if the boundaries object has more than one property.");

  throws(() => ChromeUtils.saveHeapSnapshot({ debugger: {} }),
         "Should throw if the debuggees object is not a Debugger object");

  throws(() => ChromeUtils.saveHeapSnapshot({ globals: [{}] }),
         "Should throw if the globals array contains non-global objects.");

  throws(() => ChromeUtils.saveHeapSnapshot({ runtime: false }),
         "Should throw if runtime is supplied and is not true.");

  throws(() => ChromeUtils.saveHeapSnapshot({ globals: null }),
         "Should throw if globals is not an object.");

  throws(() => ChromeUtils.saveHeapSnapshot({ globals: {} }),
         "Should throw if globals is not an array.");

  throws(() => ChromeUtils.saveHeapSnapshot({ debugger: Debugger.prototype }),
         "Should throw if debugger is the Debugger.prototype object.");

  throws(() => ChromeUtils.saveHeapSnapshot({ get globals() { return [this]; } }),
         "Should throw if boundaries property is a getter.");
}

const makeNewSandbox = () =>
  Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());

function testGoodParameters() {
  let sandbox = makeNewSandbox();
  let dbg = new Debugger(sandbox);

  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot for a debuggee global.");

  dbg = new Debugger;
  let sandboxes = Array(10).fill(null).map(makeNewSandbox);
  sandboxes.forEach(sb => dbg.addDebuggee(sb));

  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot for many debuggee globals.");

  dbg = new Debugger;
  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot with no debuggee globals.");

  ChromeUtils.saveHeapSnapshot({ globals: [this] });
  ok(true, "Should be able to save a snapshot for a specific global.");

  ChromeUtils.saveHeapSnapshot({ runtime: true });
  ok(true, "Should be able to save a snapshot of the full runtime.");
}

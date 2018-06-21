/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the ChromeUtils interface.
// eslint-disable-next-line
if (typeof Debugger != "function") {
  const { addDebuggerToGlobal } = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm", {});
  addDebuggerToGlobal(this);
}

function run_test() {
  ok(ChromeUtils, "Should be able to get the ChromeUtils interface");

  testBadParameters();
  testGoodParameters();

  do_test_finished();
}

function testBadParameters() {
  Assert.throws(() => ChromeUtils.saveHeapSnapshot(),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if arguments aren't passed in.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot(null),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if boundaries isn't an object.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({}),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if the boundaries object doesn't have any properties.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ runtime: true,
                                                     globals: [this] }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if the boundaries object has more than one property.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ debugger: {} }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if the debuggees object is not a Debugger object");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ globals: [{}] }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if the globals array contains non-global objects.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ runtime: false }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if runtime is supplied and is not true.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ globals: null }),
                /TypeError:.*can't be converted to a sequence/,
                "Should throw if globals is not an object.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ globals: {} }),
                /TypeError:.*can't be converted to a sequence/,
                "Should throw if globals is not an array.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ debugger: Debugger.prototype }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if debugger is the Debugger.prototype object.");

  Assert.throws(() => ChromeUtils.saveHeapSnapshot({ get globals() {
    return [this];
  } }),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Should throw if boundaries property is a getter.");
}

const makeNewSandbox = () =>
  Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());

function testGoodParameters() {
  const sandbox = makeNewSandbox();
  let dbg = new Debugger(sandbox);

  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot for a debuggee global.");

  dbg = new Debugger();
  const sandboxes = Array(10).fill(null).map(makeNewSandbox);
  sandboxes.forEach(sb => dbg.addDebuggee(sb));

  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot for many debuggee globals.");

  dbg = new Debugger();
  ChromeUtils.saveHeapSnapshot({ debugger: dbg });
  ok(true, "Should be able to save a snapshot with no debuggee globals.");

  ChromeUtils.saveHeapSnapshot({ globals: [this] });
  ok(true, "Should be able to save a snapshot for a specific global.");

  ChromeUtils.saveHeapSnapshot({ runtime: true });
  ok(true, "Should be able to save a snapshot of the full runtime.");
}

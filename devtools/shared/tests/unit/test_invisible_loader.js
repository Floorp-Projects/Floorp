/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

/**
 * Ensure that sandboxes created via the Dev Tools loader respect the
 * invisibleToDebugger flag.
 */
function run_test() {
  visible_loader();
  invisible_loader();
}

function visible_loader() {
  let loader = new DevToolsLoader();
  loader.invisibleToDebugger = false;
  loader.require("devtools/shared/indentation");

  let dbg = new Debugger();
  let sandbox = loader._provider.loader.sharedGlobalSandbox;

  try {
    dbg.addDebuggee(sandbox);
    do_check_true(true);
  } catch (e) {
    do_throw("debugger could not add visible value");
  }

  // Check that for common loader used for tabs, promise modules is Promise.jsm
  // Which is required to support unhandled promises rejection in mochitests
  const promise = Cu.import("resource://gre/modules/Promise.jsm", {}).Promise;
  do_check_eq(loader.require("promise"), promise);
}

function invisible_loader() {
  let loader = new DevToolsLoader();
  loader.invisibleToDebugger = true;
  loader.require("devtools/shared/indentation");

  let dbg = new Debugger();
  let sandbox = loader._provider.loader.sharedGlobalSandbox;

  try {
    dbg.addDebuggee(sandbox);
    do_throw("debugger added invisible value");
  } catch (e) {
    do_check_true(true);
  }

  // But for browser toolbox loader, promise is loaded as a regular modules out
  // of Promise-backend.js, that to be invisible to the debugger and not step
  // into it.
  const promise = loader.require("promise");
  const promiseModule = loader._provider.loader.modules["resource://gre/modules/Promise-backend.js"];
  do_check_eq(promise, promiseModule.exports);
}

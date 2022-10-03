/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { addDebuggerToGlobal } = ChromeUtils.import(
  "resource://gre/modules/jsdebugger.jsm"
);
addDebuggerToGlobal(globalThis);

/**
 * Ensure that sandboxes created via the Dev Tools loader respect the
 * invisibleToDebugger flag.
 */
function run_test() {
  visible_loader();
  invisible_loader();
}

function visible_loader() {
  const loader = new DevToolsLoader({
    invisibleToDebugger: false,
  });
  loader.require("resource://devtools/shared/indentation.js");

  const dbg = new Debugger();
  const sandbox = loader.loader.sharedGlobalSandbox;

  try {
    dbg.addDebuggee(sandbox);
    Assert.ok(true);
  } catch (e) {
    do_throw("debugger could not add visible value");
  }
}

function invisible_loader() {
  const loader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  loader.require("resource://devtools/shared/indentation.js");

  const dbg = new Debugger();
  const sandbox = loader.loader.sharedGlobalSandbox;

  try {
    dbg.addDebuggee(sandbox);
    do_throw("debugger added invisible value");
  } catch (e) {
    Assert.ok(true);
  }
}

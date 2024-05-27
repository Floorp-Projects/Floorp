/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { addDebuggerToGlobal } = ChromeUtils.importESModule(
  "resource://gre/modules/jsdebugger.sys.mjs"
);
addDebuggerToGlobal(globalThis);

/**
 * Ensure that sandboxes created via the Dev Tools loader respect the
 * invisibleToDebugger flag.
 */
function run_test() {
  visible_loader();
  invisible_loader();
  // TODO: invisibleToDebugger should be deprecated in favor of
  // useDistinctSystemPrincipalLoader, but we might move out from the loader
  // to using only standard imports instead.
  distinct_system_principal_loader();
}

function visible_loader() {
  const loader = new DevToolsLoader({
    invisibleToDebugger: false,
  });
  loader.require("resource://devtools/shared/indentation.js");

  const dbg = new Debugger();
  const sandbox = loader.loader.sharedGlobal;

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
  const sandbox = loader.loader.sharedGlobal;

  try {
    dbg.addDebuggee(sandbox);
    do_throw("debugger added invisible value");
  } catch (e) {
    Assert.ok(true);
  }
}

function distinct_system_principal_loader() {
  const {
    useDistinctSystemPrincipalLoader,
    releaseDistinctSystemPrincipalLoader,
  } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs",
    { global: "shared" }
  );

  const requester = {};
  const loader = useDistinctSystemPrincipalLoader(requester);
  loader.require("resource://devtools/shared/indentation.js");

  const dbg = new Debugger();
  const sandbox = loader.loader.sharedGlobal;

  try {
    dbg.addDebuggee(sandbox);
    do_throw("debugger added invisible value");
  } catch (e) {
    Assert.ok(true);
  }
  releaseDistinctSystemPrincipalLoader(requester);
}

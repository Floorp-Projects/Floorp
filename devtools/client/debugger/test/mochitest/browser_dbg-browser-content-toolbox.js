/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

/**
 * Tests that the debugger is succesfully loaded in the Browser Content Toolbox.
 */

const {
  gDevToolsBrowser
} = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  clearDebuggerPreferences();

  info("Open a tab pointing to doc-scripts.html");
  await addTab(EXAMPLE_URL + "doc-scripts.html");

  info("Open the Browser Content Toolbox");
  let toolbox = await gDevToolsBrowser.openContentProcessToolbox(gBrowser);

  info("Select the debugger");
  await toolbox.selectTool("jsdebugger");

  let dbg = createDebuggerContext(toolbox);
  ok(dbg, "Debugger context is available");

  info("Create a breakpoint");
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);

  info("Disable the breakpoint");
  await disableBreakpoint(dbg, 0);
  let bp = findBreakpoint(dbg, "simple2", 3);
  is(bp.disabled, true, "breakpoint is disabled");

  info("Enable the breakpoint");
  await enableBreakpoint(dbg, 0);
  bp = findBreakpoint(dbg, "simple2", 3);
  is(bp.disabled, false, "breakpoint is enabled");

  info("Close the browser toolbox window");
  let onToolboxDestroyed = toolbox.once("destroyed");
  toolbox.win.top.close();
  await onToolboxDestroyed;

  info("Toolbox is destroyed");
});

function toggleBreakpoint(dbg, index) {
  const bp = findAllElements(dbg, "breakpointItems")[index];
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg.store, "SET_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

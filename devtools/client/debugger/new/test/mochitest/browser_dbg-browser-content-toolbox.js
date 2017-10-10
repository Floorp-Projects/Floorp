/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the debugger is succesfully loaded in the Browser Content Toolbox.
 */

const {gDevToolsBrowser} = require("devtools/client/framework/devtools-browser");

function toggleBreakpoint(dbg, index) {
  const bp = findElement(dbg, "breakpointItem", index);
  const input = bp.querySelector("input");
  input.click();
}

async function disableBreakpoint(dbg, index) {
  const disabled = waitForDispatch(dbg, "DISABLE_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await disabled;
}

async function enableBreakpoint(dbg, index) {
  const enabled = waitForDispatch(dbg, "ENABLE_BREAKPOINT");
  toggleBreakpoint(dbg, index);
  await enabled;
}

function findBreakpoint(dbg, url, line) {
  const { selectors: { getBreakpoint }, getState } = dbg;
  const source = findSource(dbg, url);
  return getBreakpoint(getState(), { sourceId: source.id, line });
}

add_task(async function() {
  clearDebuggerPreferences();

  info("Open a tab pointing to doc-scripts.html")
  let tab = await addTab(EXAMPLE_URL + "doc-scripts.html");

  info("Open the Browser Content Toolbox")
  let toolbox = await gDevToolsBrowser.openContentProcessToolbox(gBrowser);

  info("Wait for the debugger to be ready");
  await toolbox.getPanelWhenReady("jsdebugger");

  let dbg = createDebuggerContext(toolbox);
  ok(dbg, "Debugger context is available");

  info("Create a breakpoint");
  await selectSource(dbg, "simple2");
  await addBreakpoint(dbg, "simple2", 3);

  info("Disable the breakpoint");
  await disableBreakpoint(dbg, 1);
  let bp = findBreakpoint(dbg, "simple2", 3);
  is(bp.disabled, true, "breakpoint is disabled");

  info("Enable the breakpoint");
  await enableBreakpoint(dbg, 1);
  bp = findBreakpoint(dbg, "simple2", 3);
  is(bp.disabled, false, "breakpoint is enabled");

  info("Close the browser toolbox window");
  let onToolboxDestroyed = toolbox.once("destroyed");
  toolbox.win.top.close();
  await onToolboxDestroyed;

  info("Toolbox is destroyed");
});
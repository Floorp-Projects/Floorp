/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a function in a source-mapped file displays its
// original source in the debugger. See Bug 1433373.

"use strict";

requestLongerTimeout(5);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-click-function-to-mapped-source.html";

const TEST_ORIGINAL_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-click-function-to-source.js";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Log a function");
  const onLoggedFunction = waitForMessageByType(
    hud,
    "function foo",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.foo();
  });
  const { node } = await onLoggedFunction;
  const jumpIcon = node.querySelector(".jump-definition");
  ok(jumpIcon, "A jump to definition button is rendered, as expected");

  info("Click on the jump to definition button.");
  jumpIcon.click();

  info("Wait for the Debugger panel to open.");
  const toolbox = hud.toolbox;
  await toolbox.getPanelWhenReady("jsdebugger");

  const dbg = createDebuggerContext(toolbox);
  await waitForSelectedSource(dbg, TEST_ORIGINAL_URI);
  await waitForSelectedLocation(dbg, 9);

  const pendingLocation = dbg.selectors.getPendingSelectedLocation();
  const { url, line, column } = pendingLocation;

  is(url, TEST_ORIGINAL_URI, "Debugger is open at the expected file");
  is(line, 9, "Debugger is open at the expected line");
  // If we loaded the original file, we'd have column 12 for the function's
  // start position, but 9 is correct for the location in the source map.
  is(column, 9, "Debugger is open at the expected column");
});

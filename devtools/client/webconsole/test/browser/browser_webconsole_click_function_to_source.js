/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a function displays its source in the debugger. See Bug 1050691.

"use strict";

requestLongerTimeout(5);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/" +
  "test-click-function-to-source.html";

const TEST_SCRIPT_URI =
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
  await waitForSelectedSource(dbg, TEST_SCRIPT_URI);

  const pendingLocation = dbg.selectors.getPendingSelectedLocation();
  const { line, column } = pendingLocation;
  is(line, 9, "Debugger is open at the expected line");
  is(column, 12, "Debugger is open at the expected column");
});

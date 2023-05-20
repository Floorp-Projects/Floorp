/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a function in a pretty-printed file displays its
// original source in the debugger. See Bug 1590824.

"use strict";

requestLongerTimeout(5);

const TEST_ROOT =
  "http://example.com/browser/devtools/client/webconsole/test/browser/";

const TEST_URI = TEST_ROOT + "test-click-function-to-prettyprinted-source.html";
const TEST_GENERATED_URI =
  TEST_ROOT + "test-click-function-to-source.unmapped.min.js";
const TEST_PRETTYPRINTED_URI = TEST_GENERATED_URI + ":formatted";

add_task(async function () {
  await clearDebuggerPreferences();

  info("Open the console");
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.toolbox;

  const onLoggedFunction = waitForMessageByType(
    hud,
    "function foo",
    ".console-api"
  );
  invokeInTab("foo");
  const { node } = await onLoggedFunction;
  const jumpIcon = node.querySelector(".jump-definition");
  ok(jumpIcon, "A jump to definition button is rendered, as expected");

  info("Click on the jump to definition button");
  jumpIcon.click();
  await toolbox.getPanelWhenReady("jsdebugger");
  const dbg = createDebuggerContext(toolbox);
  await waitForSelectedSource(dbg, TEST_GENERATED_URI);

  info("Pretty-print the minified source");
  clickElement(dbg, "prettyPrintButton");
  await waitForSelectedSource(dbg, TEST_PRETTYPRINTED_URI);

  info("Switch back to the console");
  await toolbox.selectTool("webconsole");
  info("Click on the jump to definition button a second time");
  jumpIcon.click();

  info("Wait for the Debugger panel to open");
  await waitForSelectedSource(dbg, TEST_PRETTYPRINTED_URI);
  await waitForSelectedLocation(dbg, 2);

  const location = dbg.selectors.getPendingSelectedLocation();
  // Pretty-printed source maps don't have column positions
  const { url, line } = location;

  is(url, TEST_PRETTYPRINTED_URI, "Debugger is open at the expected file");
  is(line, 2, "Debugger is open at the expected line");
});

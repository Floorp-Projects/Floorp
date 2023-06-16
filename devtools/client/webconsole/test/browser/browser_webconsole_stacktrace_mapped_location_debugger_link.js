/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that clicking on a location in a stacktrace for a source-mapped file displays its
// original source in the debugger. See Bug 1587839.

"use strict";

requestLongerTimeout(2);

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  "test-console-stacktrace-mapped.html";

const TEST_ORIGINAL_FILENAME = "test-sourcemap-original.js";

const TEST_ORIGINAL_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/" +
  TEST_ORIGINAL_FILENAME;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Print a stacktrace");
  const onLoggedStacktrace = waitForMessageByType(
    hud,
    "console.trace",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.logTrace();
  });
  const { node } = await onLoggedStacktrace;

  info("Wait until the original frames are displayed");
  await waitFor(() =>
    Array.from(node.querySelectorAll(".stacktrace .filename"))
      .map(frameEl => frameEl.textContent)
      .includes(TEST_ORIGINAL_FILENAME)
  );

  info("Click on the frame.");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    node.querySelector(".stacktrace .location")
  );

  info("Wait for the Debugger panel to open.");
  const toolbox = hud.toolbox;
  await toolbox.getPanelWhenReady("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  info("Wait for selected source");
  await waitForSelectedSource(dbg, TEST_ORIGINAL_URI);
  await waitForSelectedLocation(dbg, 15);

  const pendingLocation = dbg.selectors.getPendingSelectedLocation();
  const { url, line } = pendingLocation;

  is(url, TEST_ORIGINAL_URI, "Debugger is open at the expected file");
  is(line, 15, "Debugger is open at the expected line");
});

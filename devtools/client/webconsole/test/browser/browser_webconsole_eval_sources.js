/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-eval-sources.html";

async function clickFirstStackElement(hud, message, needsExpansion) {
  if (needsExpansion) {
    const button = message.querySelector(".collapse-button");
    ok(button, "has button");
    button.click();
  }

  let frame;
  await waitUntil(() => {
    frame = message.querySelector(".frame");
    return !!frame;
  });

  EventUtils.sendMouseEvent({ type: "mousedown" }, frame);

  await once(hud.ui, "source-in-debugger-opened");
}

// Test that stack/message links in console API and error messages originating
// from eval code go to a source in the debugger. This should work even when the
// console is opened first.
add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  let messageNode = await waitFor(() => findMessage(hud, "BAR"));
  await clickFirstStackElement(hud, messageNode, true);

  const dbg = toolbox.getPanel("jsdebugger");

  is(
    dbg._selectors.getSelectedSource(dbg._getState()).url,
    null,
    "expected source url"
  );

  await testOpenInDebugger(hud, toolbox, "FOO", false);
  await testOpenInDebugger(hud, toolbox, "BAR", false);

  // Test that links in the API work when the eval source has a sourceURL property
  // which is not considered to be a valid URL.
  await testOpenInDebugger(hud, toolbox, "BAZ", false);

  // Test that stacks in console.trace() calls work.
  messageNode = await waitFor(() => findMessage(hud, "TRACE"));
  await clickFirstStackElement(hud, messageNode, false);

  is(
    /my-foo.js/.test(dbg._selectors.getSelectedSource(dbg._getState()).url),
    true,
    "expected source url"
  );
});

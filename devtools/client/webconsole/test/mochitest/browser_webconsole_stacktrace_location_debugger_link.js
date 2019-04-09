/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that message source links for js errors and console API calls open in
// the jsdebugger when clicked.

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/Component not initialized/);
PromiseTestUtils.whitelistRejectionsGlobally(/this\.worker is null/);

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-stacktrace-location-debugger-link.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const target = await TargetFactory.forTab(gBrowser.selectedTab);
  const toolbox = gDevTools.getToolbox(target);

  await testOpenInDebugger(hud, toolbox, "console.trace()");
  await testOpenInDebugger(hud, toolbox, "myErrorObject");
});

async function testOpenInDebugger(hud, toolbox, text) {
  info(`Testing message with text "${text}"`);
  const messageNode = await waitFor(() => findMessage(hud, text));
  const framesNode = await waitFor(() => messageNode.querySelector(".frames"));

  const frameNodes = framesNode.querySelectorAll(".frame");
  is(frameNodes.length, 3,
    "The message does have the expected number of frames in the stacktrace");

  for (const frameNode of frameNodes) {
    await checkClickOnNode(hud, toolbox, frameNode);

    info("Selecting the console again");
    await toolbox.selectTool("webconsole");
  }
}

async function checkClickOnNode(hud, toolbox, frameNode) {
  info("checking click on node location");
  const onSourceInDebuggerOpened = once(hud.ui, "source-in-debugger-opened");
  EventUtils.sendMouseEvent({ type: "mousedown" }, frameNode.querySelector(".location"));
  await onSourceInDebuggerOpened;

  const url = frameNode.querySelector(".filename").textContent;
  const dbg = toolbox.getPanel("jsdebugger");
  is(
    dbg._selectors.getSelectedSource(dbg._getState()).url,
    url,
    `Debugger is opened at expected source url (${url})`
  );
}

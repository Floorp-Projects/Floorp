/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { HUDService } = require("devtools/client/webconsole/hudservice");

// This functionality was copied from devtools/client/webconsole/test/mochitest/head.js,
// since this test straddles both the web console and the debugger. I couldn't
// figure out how to load that script directly here.

function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(
    messages,
    (el) => el.textContent.includes(text)
  );
  return elements;
}

async function openContextMenu(hud, element) {
  const onConsoleMenuOpened = hud.ui.consoleOutput.once("menu-open");
  synthesizeContextMenuEvent(element);
  await onConsoleMenuOpened;
  const doc = hud.ui.consoleOutput.owner.chromeWindow.document;
  return doc.getElementById("webconsole-menu");
}

function hideContextMenu(hud) {
  const doc = hud.ui.consoleOutput.owner.chromeWindow.document;
  const popup = doc.getElementById("webconsole-menu");
  if (!popup) {
    return Promise.resolve();
  }

  const onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  return onPopupHidden;
}

// Test basic console time warping functionality in web replay.
async function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_error.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let console = await openToolboxForTab(tab, "webconsole");
  let hud = console.getCurrentPanel().hud;
  let messages = findMessages(hud, "Number 5");
  ok(messages.length == 1, "Found one message");
  let message = messages.pop();

  let menuPopup = await openContextMenu(hud, message);
  let timeWarpItem = menuPopup.querySelector("#console-menu-time-warp");
  ok(timeWarpItem, "Time warp menu item is available");
  timeWarpItem.click();
  await hideContextMenu(hud);

  await once(Services.ppmm, "TimeWarpFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();

  await checkEvaluateInTopFrame(client, "number", 5);

  // Initially we are paused inside the 'new Error()' call on line 19. The
  // first reverse step takes us to the start of that line.
  await reverseStepOverToLine(client, 19);

  await reverseStepOverToLine(client, 18);
  await setBreakpoint(client, "doc_rr_error.html", 12);
  await rewindToLine(client, 12);
  await checkEvaluateInTopFrame(client, "number", 4);
  await resumeToLine(client, 12);
  await checkEvaluateInTopFrame(client, "number", 5);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}

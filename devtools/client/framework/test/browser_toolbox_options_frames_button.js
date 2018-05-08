/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the frames button is always visible when the user is on the options panel.
// Test that the button is disabled if the current target has no frames.
// Test that the button is enabled otherwise.

const TEST_URL = "data:text/html;charset=utf8,test frames button visibility";
const TEST_URL_FRAMES = TEST_URL + "<iframe src=\"data:text/plain,iframe\"></iframe>";
const FRAME_BUTTON_PREF = "devtools.command-button-frames.enabled";

add_task(async function() {
  // Hide the button by default.
  await pushPref(FRAME_BUTTON_PREF, false);

  let tab = await addTab(TEST_URL);
  let target = TargetFactory.forTab(tab);

  info("Open the toolbox on the Options panel");
  let toolbox = await gDevTools.showToolbox(target, "options");
  let doc = toolbox.doc;

  let optionsPanel = toolbox.getCurrentPanel();

  let framesButton = doc.getElementById("command-button-frames");
  ok(!framesButton, "Frames button is not rendered.");

  let optionsDoc = optionsPanel.panelWin.document;
  let framesButtonCheckbox = optionsDoc.getElementById("command-button-frames");
  framesButtonCheckbox.click();

  framesButton = doc.getElementById("command-button-frames");
  ok(framesButton, "Frames button is rendered.");
  ok(framesButton.disabled, "Frames button is disabled.");

  info("Leave the options panel, the frames button should not be rendered.");
  await toolbox.selectTool("webconsole");
  framesButton = doc.getElementById("command-button-frames");
  ok(!framesButton, "Frames button is no longer rendered.");

  info("Go back to the options panel, the frames button should rendered.");
  await toolbox.selectTool("options");
  framesButton = doc.getElementById("command-button-frames");
  ok(framesButton, "Frames button is rendered again.");

  info("Navigate to a page with frames, the frames button should be enabled.");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, TEST_URL_FRAMES);

  framesButton = doc.getElementById("command-button-frames");
  ok(framesButton, "Frames button is still rendered.");

  await waitUntil(() => !framesButton.disabled);
  ok(!framesButton.disabled, "Frames button is not disabled.");

  Services.prefs.clearUserPref(FRAME_BUTTON_PREF);
});

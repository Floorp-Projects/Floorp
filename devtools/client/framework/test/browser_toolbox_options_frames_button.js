/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the frames button is always visible when the user is on the options panel.
// Test that the button is disabled if the current target has no frames.
// Test that the button is enabled otherwise.

const TEST_URL = "data:text/html;charset=utf8,test frames button visibility";
const TEST_IFRAME_URL = "data:text/plain,iframe";
const TEST_IFRAME_URL2 = "data:text/plain,iframe2";
const TEST_URL_FRAMES =
  TEST_URL +
  `<iframe src="${TEST_IFRAME_URL}"></iframe>` +
  `<iframe src="${TEST_IFRAME_URL2}"></iframe>`;
const FRAME_BUTTON_PREF = "devtools.command-button-frames.enabled";

add_task(async function() {
  // Hide the button by default.
  await pushPref(FRAME_BUTTON_PREF, false);

  const tab = await addTab(TEST_URL);

  info("Open the toolbox on the Options panel");
  const toolbox = await gDevTools.showToolboxForTab(tab, { toolId: "options" });
  const doc = toolbox.doc;

  const optionsPanel = toolbox.getCurrentPanel();

  let framesButton = doc.getElementById("command-button-frames");
  ok(!framesButton, "Frames button is not rendered.");

  const optionsDoc = optionsPanel.panelWin.document;
  const framesButtonCheckbox = optionsDoc.getElementById(
    "command-button-frames"
  );
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

  // Do not run the rest of this test when both fission and EFT is disabled as
  // it prevents creating a target for the iframe
  if (!isFissionEnabled() || !isEveryFrameTargetEnabled()) {
    return;
  }

  info("Navigate to a page with frames, the frames button should be enabled.");
  await navigateTo(TEST_URL_FRAMES);

  framesButton = doc.getElementById("command-button-frames");
  ok(framesButton, "Frames button is still rendered.");

  await waitFor(() => {
    framesButton = doc.getElementById("command-button-frames");
    return framesButton && !framesButton.disabled;
  });

  const { targetCommand } = toolbox.commands;
  const iframeTarget = targetCommand
    .getAllTargets([targetCommand.TYPES.FRAME])
    .find(target => target.url == TEST_IFRAME_URL);
  ok(iframeTarget, "Found the target for the iframe");

  ok(
    !framesButton.classList.contains("checked"),
    "Before selecting an iframe, the button is not checked"
  );
  await toolbox.commands.targetCommand.selectTarget(iframeTarget);
  ok(
    framesButton.classList.contains("checked"),
    "After selecting an iframe, the button is checked"
  );

  info("Remove this first iframe, which is currently selected");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.querySelector("iframe").remove();
  });

  await waitFor(() => {
    return targetCommand.selectedTargetFront == targetCommand.targetFront;
  }, "Wait for the selected target to be back on the top target");

  ok(
    !framesButton.classList.contains("checked"),
    "The button is back unchecked after having removed the selected iframe"
  );

  Services.prefs.clearUserPref(FRAME_BUTTON_PREF);
});

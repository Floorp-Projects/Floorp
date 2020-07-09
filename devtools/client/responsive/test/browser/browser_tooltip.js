/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CONTENT = `<h1 title="test title">test h1</h1>`;
const TEST_URL = `data:text/html;charset=utf-8,${TEST_CONTENT}`;

// Test for the tooltip coordinate on the browsing document in RDM.

addRDMTask(TEST_URL, async ({ ui }) => {
  // On ubuntu1804, the test fails if the real mouse cursor is on the test document.
  // See Bug 1600183
  info("Disable non test mouse event");
  window.windowUtils.disableNonTestMouseEvents(true);
  registerCleanupFunction(() => {
    window.windowUtils.disableNonTestMouseEvents(false);
  });

  info("Create a promise which waits until the tooltip will be shown");
  const tooltip = ui.browserWindow.gBrowser.ownerDocument.getElementById(
    "remoteBrowserTooltip"
  );
  const onTooltipShown = BrowserTestUtils.waitForEvent(tooltip, "popupshown");

  info("Show a tooltip");
  await spawnViewportTask(ui, {}, async () => {
    const target = content.document.querySelector("h1");
    await EventUtils.synthesizeMouse(
      target,
      1,
      1,
      { type: "mouseover", isSynthesized: false },
      content
    );
    await EventUtils.synthesizeMouse(
      target,
      2,
      1,
      { type: "mousemove", isSynthesized: false },
      content
    );
    await EventUtils.synthesizeMouse(
      target,
      3,
      1,
      { type: "mousemove", isSynthesized: false },
      content
    );
  });

  info("Wait for showing the tooltip");
  await onTooltipShown;

  info("Test the X coordinate of the tooltip");
  isnot(tooltip.screenX, 0, "The X coordinate of tooltip should not be 0");
});

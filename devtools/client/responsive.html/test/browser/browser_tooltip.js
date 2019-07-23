/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CONTENT = `<h1 title="test title">test h1</h1>`;
const TEST_URL = `data:text/html;charset=utf-8,${ encodeURIComponent(TEST_CONTENT) }`;

// Test for the tooltip coordinate on the browsing document in RDM.

addRDMTask(TEST_URL, async ({ ui }) => {
  info("Show a tooltip");
  await spawnViewportTask(ui, {}, () => {
    const target = content.document.querySelector("h1");
    const { x: targetX, y: targetY } = target.getClientRects()[0];
    content.windowUtils.sendMouseEvent("mouseover", targetX, targetY, 0, 0, 0);
    content.windowUtils.sendMouseEvent("mousemove", targetX + 1, targetY, 0, 0, 0);
  });

  info("Wait for showing the tooltip");
  const tooltip =
    ui.browserWindow.gBrowser.ownerDocument.getElementById("remoteBrowserTooltip");
  await waitUntil(() => tooltip.state === "open");

  info("Test the X coordinate of the tooltip");
  isnot(tooltip.screenX, 0, "The X coordinate of tooltip should not be 0");
});

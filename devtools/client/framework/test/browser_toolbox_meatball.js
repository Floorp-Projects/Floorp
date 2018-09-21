/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Sanity test for meatball menu.
//
// We also use this to test the common Menu* components since we don't currently
// have a means of testing React components in isolation.

const { focusableSelector } = require("devtools/client/shared/focus");
const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(tab, "inspector", Toolbox.HostType.BOTTOM);

  info("Check opening meatball menu by clicking the menu button");
  await openMeatballMenuWithClick(toolbox);
  const menuDockToBottom = toolbox.doc.getElementById("toolbox-meatball-menu-dock-bottom");
  ok(menuDockToBottom.getAttribute("aria-checked") === "true",
     "menuDockToBottom has checked");

  info("Check closing meatball menu by clicking outside the popup area");
  await closeMeatballMenuWithClick(toolbox);

  info("Check moving the focus element with key event");
  await openMeatballMenuWithClick(toolbox);
  checkKeyHandling(toolbox);

  info("Check closing meatball menu with escape key");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, toolbox.win);
  await waitForMeatballMenuToClose(toolbox);

  // F1 should trigger the settings panel and close the menu at the same time.
  info("Check closing meatball menu with F1 key");
  await openMeatballMenuWithClick(toolbox);
  EventUtils.synthesizeKey("VK_F1", {}, toolbox.win);
  await waitForMeatballMenuToClose(toolbox);
});

async function openMeatballMenuWithClick(toolbox) {
  const meatballButton = toolbox.doc.getElementById("toolbox-meatball-menu-button");
  await waitUntil(() => meatballButton.style.pointerEvents !== "none");
  EventUtils.synthesizeMouseAtCenter(meatballButton, {}, toolbox.win);

  const panel = toolbox.doc.querySelectorAll(".tooltip-xul-wrapper");
  const shownListener = new Promise(res => {
    panel[0].addEventListener("popupshown", res, { once: true });
  });

  const menuPanel = toolbox.doc.getElementById("toolbox-meatball-menu-button-panel");
  ok(menuPanel, "meatball panel is available");

  info("Waiting for the menu panel to be displayed");

  await shownListener;
  await waitUntil(() => menuPanel.classList.contains("tooltip-visible"));
}

async function closeMeatballMenuWithClick(toolbox) {
  const meatballButton = toolbox.doc.getElementById("toolbox-meatball-menu-button");
  await waitUntil(() => toolbox.win.getComputedStyle(meatballButton).pointerEvents === "none");
  meatballButton.click();

  const menuPanel = toolbox.doc.getElementById("toolbox-meatball-menu-button-panel");
  ok(menuPanel, "meatball panel is available");

  info("Waiting for the menu panel to be hidden");
  await waitUntil(() => !menuPanel.classList.contains("tooltip-visible"));
}

async function waitForMeatballMenuToClose(toolbox) {
  const menuPanel = toolbox.doc.getElementById("toolbox-meatball-menu-button-panel");
  ok(menuPanel, "meatball panel is available");

  info("Waiting for the menu panel to be hidden");
  await waitUntil(() => !menuPanel.classList.contains("tooltip-visible"));
}

function checkKeyHandling(toolbox) {
  const selectable =
    toolbox.doc.getElementById("toolbox-meatball-menu").querySelectorAll(focusableSelector);

  EventUtils.synthesizeKey("VK_DOWN", {}, toolbox.win);
  is(toolbox.doc.activeElement, selectable[0], "First item selected with down key.");
  EventUtils.synthesizeKey("VK_UP", {}, toolbox.win);
  is(toolbox.doc.activeElement, selectable[selectable.length - 1], "End item selected with up key.");
  EventUtils.synthesizeKey("VK_HOME", {}, toolbox.win);
  is(toolbox.doc.activeElement, selectable[0], "First item selected with home key.");
  EventUtils.synthesizeKey("VK_END", {}, toolbox.win);
  is(toolbox.doc.activeElement, selectable[selectable.length - 1], "End item selected with down key.");
}

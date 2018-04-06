/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a button to access tools hidden by toolbar overflow is displayed when the
// toolbar starts to present an overflow.
let { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  let tab = await addTab("about:blank");

  info("Open devtools on the Inspector in a separate window");
  let toolbox = await openToolboxForTab(tab, "inspector", Toolbox.HostType.WINDOW);

  let hostWindow = toolbox.win.parent;
  let originalWidth = hostWindow.outerWidth;
  let originalHeight = hostWindow.outerHeight;

  info("Resize devtools window to a width that should not trigger any overflow");
  let onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(640, 300);
  await onResize;

  let allToolsButton = toolbox.doc.querySelector(".all-tools-menu");
  ok(!allToolsButton, "The all tools button is not displayed");

  info("Resize devtools window to a width that should trigger an overflow");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(300, 300);
  await onResize;

  info("Wait until the all tools button is available");
  await waitUntil(() => toolbox.doc.querySelector(".all-tools-menu"));

  allToolsButton = toolbox.doc.querySelector(".all-tools-menu");
  ok(allToolsButton, "The all tools button is displayed");

  info("Open the all-tools-menupopup and verify that the inspector button is checked");
  let menuPopup = await openAllToolsMenu(toolbox);

  let inspectorButton = toolbox.doc.querySelector("#all-tools-menupopup-inspector");
  ok(inspectorButton, "The inspector button is available");
  ok(inspectorButton.getAttribute("checked"), "The inspector button is checked");

  let consoleButton = toolbox.doc.querySelector("#all-tools-menupopup-webconsole");
  ok(consoleButton, "The console button is available");
  ok(!consoleButton.getAttribute("checked"), "The console button is not checked");

  info("Switch to the webconsole using the all-tools-menupopup popup");
  let onSelected = toolbox.once("webconsole-selected");
  consoleButton.click();
  await onSelected;

  info("Closing the all-tools-menupopup popup");
  let onPopupHidden = once(menuPopup, "popuphidden");
  menuPopup.hidePopup();
  await onPopupHidden;

  info("Re-open the all-tools-menupopup and verify that the console button is checked");
  menuPopup = await openAllToolsMenu(toolbox);

  inspectorButton = toolbox.doc.querySelector("#all-tools-menupopup-inspector");
  ok(!inspectorButton.getAttribute("checked"), "The inspector button is not checked");

  consoleButton = toolbox.doc.querySelector("#all-tools-menupopup-webconsole");
  ok(consoleButton.getAttribute("checked"), "The console button is checked");

  info("Restore the original window size");
  hostWindow.resizeTo(originalWidth, originalHeight);
});

async function openAllToolsMenu(toolbox) {
  let allToolsButton = toolbox.doc.querySelector(".all-tools-menu");
  EventUtils.synthesizeMouseAtCenter(allToolsButton, {}, toolbox.win);

  let menuPopup = toolbox.doc.querySelector("#all-tools-menupopup");
  ok(menuPopup, "all-tools-menupopup is available");

  info("Waiting for the menu popup to be displayed");
  await waitUntil(() => menuPopup && menuPopup.state === "open");

  return menuPopup;
}

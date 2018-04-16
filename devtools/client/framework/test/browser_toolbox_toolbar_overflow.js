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

  info("Open devtools on the Inspector in a bottom dock");
  let toolbox = await openToolboxForTab(tab, "inspector", Toolbox.HostType.BOTTOM);

  let hostWindow = toolbox.win.parent;
  let originalWidth = hostWindow.outerWidth;
  let originalHeight = hostWindow.outerHeight;

  info("Resize devtools window to a width that should not trigger any overflow");
  let onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(1350, 300);
  await onResize;
  waitUntil(() => {
    // Wait for all buttons are displayed.
    return toolbox.panelDefinitions.length !==
      toolbox.doc.querySelectorAll(".devtools-tab").length;
  });

  let chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  ok(!chevronMenuButton, "The chevron menu button is not displayed");

  info("Resize devtools window to a width that should trigger an overflow");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(800, 300);
  await onResize;
  waitUntil(() => !toolbox.doc.querySelector(".tools-chevron-menu"));

  info("Wait until the chevron menu button is available");
  await waitUntil(() => toolbox.doc.querySelector(".tools-chevron-menu"));

  chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  ok(chevronMenuButton, "The chevron menu button is displayed");

  info("Open the tools-chevron-menupopup and verify that the inspector button is checked");
  let menuPopup = await openChevronMenu(toolbox);

  let inspectorButton = toolbox.doc.querySelector("#tools-chevron-menupopup-inspector");
  ok(!inspectorButton, "The chevron menu doesn't have the inspector button.");

  let consoleButton = toolbox.doc.querySelector("#tools-chevron-menupopup-webconsole");
  ok(!consoleButton, "The chevron menu doesn't have the console button.");

  let storageButton = toolbox.doc.querySelector("#tools-chevron-menupopup-storage");
  ok(storageButton, "The chevron menu has the storage button.");

  info("Switch to the performance using the tools-chevron-menupopup popup");
  let onSelected = toolbox.once("storage-selected");
  storageButton.click();
  await onSelected;

  info("Closing the tools-chevron-menupopup popup");
  let onPopupHidden = once(menuPopup, "popuphidden");
  menuPopup.hidePopup();
  await onPopupHidden;

  info("Restore the original window size");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originalWidth, originalHeight);
  await onResize;
});

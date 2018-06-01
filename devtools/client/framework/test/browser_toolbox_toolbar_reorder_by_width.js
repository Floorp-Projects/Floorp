/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test will:
//
// * Confirm that currently selected button to access tools will not hide due to overflow.
//   In this case, a button which is located on the left of a currently selected will hide.
// * Confirm that a button to access tool will hide when registering a new panel.
//
// Note that this test is based on the tab ordinal is fixed.
// i.e. After changed by Bug 1226272, this test might fail.

const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const tab = await addTab("about:blank");

  info("Open devtools on the Storage in a sidebar.");
  const toolbox = await openToolboxForTab(tab, "storage", Toolbox.HostType.BOTTOM);

  const win = getWindow(toolbox);
  const { outerWidth: originalWindowWidth, outerHeight: originalWindowHeight } = win;
  registerCleanupFunction(() => {
    win.resizeTo(originalWindowWidth, originalWindowHeight);
  });

  info("Waiting for the window to be resized");
  await resizeWindow(toolbox, 800);

  info("Wait until the tools menu button is available");
  await waitUntil(() => toolbox.doc.querySelector(".tools-chevron-menu"));

  const toolsMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  ok(toolsMenuButton, "The tools menu button is displayed");

  info("Confirm that selected tab is not hidden.");
  const storageButton = toolbox.doc.querySelector("#toolbox-tab-storage");
  ok(storageButton, "The storage tab is on toolbox.");

  // Reset window size for 2nd test.
  await resizeWindow(toolbox, originalWindowWidth);
});

add_task(async function() {
  const tab = await addTab("about:blank");

  info("Open devtools on the Storage in a sidebar.");
  const toolbox = await openToolboxForTab(tab, "storage", Toolbox.HostType.BOTTOM);

  info("Resize devtools window to a width that should trigger an overflow");
  await resizeWindow(toolbox, 800);

  info("Regist a new tab");
  const onRegistered = toolbox.once("tool-registered");
  gDevTools.registerTool({
    id: "test-tools",
    label: "Test Tools",
    isMenu: true,
    isTargetSupported: () => true,
    build: function() {},
  });
  await onRegistered;

  info("Open the tools menu button.");
  let popup = await openChevronMenu(toolbox);

  info("The registered new tool tab should be in the tools menu.");
  let testToolsButton = toolbox.doc.querySelector("#tools-chevron-menupopup-test-tools");
  ok(testToolsButton, "The tools menu has a registered new tool button.");

  info("Closing the tools-chevron-menupopup popup");
  let onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  await onPopupHidden;

  info("Unregistering test-tools");
  const onUnregistered = toolbox.once("tool-unregistered");
  gDevTools.unregisterTool("test-tools");
  await onUnregistered;

  info("Open the tools menu button.");
  popup = await openChevronMenu(toolbox);

  info("An unregistered new tool tab should not be in the tools menu.");
  testToolsButton = toolbox.doc.querySelector("#tools-chevron-menupopup-test-tools");
  ok(!testToolsButton, "The tools menu doesn't have a unregistered new tool button.");

  info("Closing the tools-chevron-menupopup popup");
  onPopupHidden = once(popup, "popuphidden");
  popup.hidePopup();
  await onPopupHidden;
});

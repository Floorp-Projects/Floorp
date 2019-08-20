/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a button to access tools hidden by toolbar overflow is displayed when the
// toolbar starts to present an overflow.
const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  const tab = await addTab("about:blank");

  info("Open devtools on the Inspector in a bottom dock");
  const toolbox = await openToolboxForTab(
    tab,
    "inspector",
    Toolbox.HostType.BOTTOM
  );

  const hostWindow = toolbox.topWindow;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;

  info(
    "Resize devtools window to a width that should not trigger any overflow"
  );
  let onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(1350, 300);
  await onResize;

  info("Wait for all buttons to be displayed");
  await waitUntil(() => {
    return (
      toolbox.panelDefinitions.length ===
      toolbox.doc.querySelectorAll(".devtools-tab").length
    );
  });

  let chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  ok(!chevronMenuButton, "The chevron menu button is not displayed");

  info("Resize devtools window to a width that should trigger an overflow");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(800, 300);
  await onResize;
  await waitUntil(() => !toolbox.doc.querySelector(".tools-chevron-menu"));

  info("Wait until the chevron menu button is available");
  await waitUntil(() => toolbox.doc.querySelector(".tools-chevron-menu"));

  chevronMenuButton = toolbox.doc.querySelector(".tools-chevron-menu");
  ok(chevronMenuButton, "The chevron menu button is displayed");

  info(
    "Open the tools-chevron-menupopup and verify that the inspector button is checked"
  );
  await openChevronMenu(toolbox);

  const inspectorButton = toolbox.doc.querySelector(
    "#tools-chevron-menupopup-inspector"
  );
  ok(!inspectorButton, "The chevron menu doesn't have the inspector button.");

  const consoleButton = toolbox.doc.querySelector(
    "#tools-chevron-menupopup-webconsole"
  );
  ok(!consoleButton, "The chevron menu doesn't have the console button.");

  const storageButton = toolbox.doc.querySelector(
    "#tools-chevron-menupopup-storage"
  );
  ok(storageButton, "The chevron menu has the storage button.");

  info("Switch to the performance using the tools-chevron-menupopup popup");
  const onSelected = toolbox.once("storage-selected");
  storageButton.click();
  await onSelected;

  info("Restore the original window size");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originalWidth, originalHeight);
  await onResize;
});

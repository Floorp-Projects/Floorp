/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that opening the History view using the default toolbar button works
 * also while the view is displayed in the main menu.
 */
add_task(async function test_history_after_appMenu() {
  // First add the button to the toolbar and wait for it to show up:
  CustomizableUI.addWidgetToArea("history-panelmenu", "nav-bar");
  registerCleanupFunction(() =>
    CustomizableUI.removeWidgetFromArea("history-panelmenu")
  );
  await waitForElementShown(document.getElementById("history-panelmenu"));

  let historyView = PanelMultiView.getViewNode(document, "PanelUI-history");
  let libraryView = PanelMultiView.getViewNode(document, "appMenu-libraryView");

  // Open the main menu.
  await gCUITestUtils.openMainMenu();

  if (!PanelUI.protonAppMenuEnabled) {
    // Show the Library view as a subview of the main menu.
    document.getElementById("appMenu-library-button").click();
    await BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  }

  // Show the History view as a subview of the main menu.
  document
    .getElementById(
      PanelUI.protonAppMenuEnabled
        ? "appMenu-history-button"
        : "appMenu-library-history-button"
    )
    .click();
  await BrowserTestUtils.waitForEvent(historyView, "ViewShown");

  // Show the History view as the main view of the History panel.
  document.getElementById("history-panelmenu").click();
  await BrowserTestUtils.waitForEvent(historyView, "ViewShown");

  // Close the history panel.
  let historyPanel = historyView.closest("panel");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "popuphidden");
  historyPanel.hidePopup();
  await promise;
});

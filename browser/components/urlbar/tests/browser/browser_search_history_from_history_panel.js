/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function searchHistoryFromHistoryPanel() {
  // Add Button to toolbar
  CustomizableUI.addWidgetToArea(
    "history-panelmenu",
    CustomizableUI.AREA_NAVBAR,
    0
  );
  registerCleanupFunction(() => {
    resetCUIAndReinitUrlbarInput();
  });

  let historyButton = document.getElementById("history-panelmenu");
  ok(historyButton, "History button appears in Panel Menu");

  historyButton.click();

  let historyPanel = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
  await promise;
  ok(historyPanel.getAttribute("visible"), "History Panel is in view");

  // Click on 'Search Bookmarks'
  let searchHistoryButton = document.getElementById("appMenuSearchHistory");
  ok(
    BrowserTestUtils.isVisible(
      searchHistoryButton,
      "'Search History Button' is visible."
    )
  );
  EventUtils.synthesizeMouseAtCenter(searchHistoryButton, {});

  await new Promise(resolve => {
    window.gURLBar.controller.addQueryListener({
      onViewOpen() {
        window.gURLBar.controller.removeQueryListener(this);
        resolve();
      },
    });
  });

  // Verify URLBar is in search mode with correct restriction
  is(
    gURLBar.searchMode?.source,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    "Addressbar in correct mode."
  );
  gURLBar.searchMode = null;
  gURLBar.blur();
});

add_task(async function searchHistoryFromAppMenuHistoryButton() {
  // Open main menu and click on 'History' button
  await gCUITestUtils.openMainMenu();
  let historyButton = document.getElementById("appMenu-history-button");
  historyButton.click();

  let historyPanel = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
  await promise;
  ok(historyPanel.getAttribute("visible"), "History Panel is in view");

  // Click on 'Search Bookmarks'
  let searchHistoryButton = document.getElementById("appMenuSearchHistory");
  ok(
    BrowserTestUtils.isVisible(
      searchHistoryButton,
      "'Search History Button' is visible."
    )
  );
  EventUtils.synthesizeMouseAtCenter(searchHistoryButton, {});

  await new Promise(resolve => {
    window.gURLBar.controller.addQueryListener({
      onViewOpen() {
        window.gURLBar.controller.removeQueryListener(this);
        resolve();
      },
    });
  });

  // Verify URLBar is in search mode with correct restriction
  is(
    gURLBar.searchMode?.source,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    "Addressbar in correct mode."
  );
});

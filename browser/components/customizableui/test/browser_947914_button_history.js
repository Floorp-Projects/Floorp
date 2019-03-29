/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(async function() {
  info("Check history button existence and functionality");
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test.
  gBrowser.selectedTab.focus();
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PATH + "dummy_history_item.html");
  BrowserTestUtils.removeTab(tab);

  tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PATH); // will 404, but we don't care.

  CustomizableUI.addWidgetToArea("history-panelmenu", CustomizableUI.AREA_FIXED_OVERFLOW_PANEL);
  registerCleanupFunction(() => CustomizableUI.reset());

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let historyButton = document.getElementById("history-panelmenu");
  ok(historyButton, "History button appears in Panel Menu");

  let historyPanel = document.getElementById("PanelUI-history");
  let promise = BrowserTestUtils.waitForEvent(historyPanel, "ViewShown");
  historyButton.click();
  await promise;
  ok(historyPanel.getAttribute("visible"), "History Panel is in view");

  let browserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let panelHiddenPromise = promiseOverflowHidden(window);

  let historyItems = document.getElementById("appMenu_historyMenu");
  let historyItemForURL = historyItems.querySelector("toolbarbutton.bookmark-item[label='Happy History Hero']");
  ok(historyItemForURL, "Should have a history item for the history we just made.");
  EventUtils.synthesizeMouseAtCenter(historyItemForURL, {});
  await browserLoaded;
  is(gBrowser.currentURI.spec, TEST_PATH + "dummy_history_item.html", "Should have expected page load");

  await panelHiddenPromise;
  BrowserTestUtils.removeTab(tab);
  info("Menu panel was closed");
});

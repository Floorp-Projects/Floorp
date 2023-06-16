/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests ensures that the urlbar adaptive behavior updates
 * when using switch to tab in the address bar dropdown.
 */

"use strict";

add_setup(async function () {
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });
});

add_task(async function test_adaptive_with_search_term_and_switch_tab() {
  await PlacesUtils.history.clear();
  let urls = [
    "https://example.com/",
    "https://example.com/#cat",
    "https://example.com/#cake",
    "https://example.com/#car",
  ];

  info(`Load tabs in same order as urls`);
  let tabs = [];
  for (let url of urls) {
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, url, false, true);
    gBrowser.loadTabs([url], {
      inBackground: true,
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    let tab = await tabPromise;
    tabs.push(tab);
  }

  info(`Switch to tab 0`);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  info("Wait for autocomplete");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ca",
  });

  let result1 = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.notEqual(result1.url, urls[1], `${urls[1]} url should not be first`);

  info(`Scroll down to select the ${urls[1]} entry using keyboard`);
  let result2 = await UrlbarTestUtils.getDetailsOfResultAt(
    window,
    UrlbarTestUtils.getSelectedRowIndex(window)
  );

  while (result2.url != urls[1]) {
    EventUtils.synthesizeKey("KEY_ArrowDown");
    result2 = await UrlbarTestUtils.getDetailsOfResultAt(
      window,
      UrlbarTestUtils.getSelectedRowIndex(window)
    );
  }

  Assert.equal(
    result2.type,
    UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
    "Selected entry should be tab switch"
  );
  Assert.equal(result2.url, urls[1]);

  info("Visiting tab 1");
  EventUtils.synthesizeKey("KEY_Enter");
  Assert.equal(gBrowser.selectedTab, tabs[1], "Should have switched to tab 1");

  info("Switch back to tab 0");
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  info("Wait for autocomplete");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "ca",
  });

  let result3 = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result3.url, urls[1], `${urls[1]} url should be first`);

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

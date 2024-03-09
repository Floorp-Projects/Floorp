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
  let waitForVisits = PlacesTestUtils.waitForNotification(
    "page-visited",
    events => events.some(e => e.url === urls[3])
  );
  for (let url of urls) {
    tabs.push(await BrowserTestUtils.openNewForegroundTab({ gBrowser, url }));
  }
  // Ensure visits have been added.
  await waitForVisits;

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

add_task(
  async function test_adaptive_nonadaptive_container_dedupe_switch_tab() {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["privacy.userContext.enabled", true],
        ["browser.urlbar.switchTabs.searchAllContainers", true],
      ],
    });
    // Add a url both to history and input history, ensure that the Muxer will
    // properly dedupe the 2 entries, also with containers involved.
    await PlacesUtils.history.clear();
    const url = "https://example.com/";

    let promiseVisited = PlacesTestUtils.waitForNotification(
      "page-visited",
      events => events.some(e => e.url === url)
    );
    let tab = BrowserTestUtils.addTab(gBrowser, url, { userContextId: 1 });
    await promiseVisited;

    async function queryAndCheckOneSwitchTabResult() {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "xampl",
      });
      Assert.equal(
        2,
        UrlbarTestUtils.getResultCount(window),
        "Check number of results"
      );
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
      Assert.equal(url, result.url, `Url is the first non-heuristic result`);
      Assert.equal(
        UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
        result.type,
        "Should be a switch tab result"
      );
      Assert.equal(
        1,
        result.result.payload.userContextId,
        "Should use the expected container"
      );
    }
    info("Check the tab is returned as history by a search.");
    await queryAndCheckOneSwitchTabResult();
    info("Add the same url to input history.");
    await UrlbarUtils.addToInputHistory(url, "xampl");
    info("Repeat the query.");
    await queryAndCheckOneSwitchTabResult();
    BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
  }
);

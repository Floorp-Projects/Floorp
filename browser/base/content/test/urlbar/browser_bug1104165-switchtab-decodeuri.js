add_task(async function test_switchtab_decodeuri() {
  info("Opening first tab");
  const TEST_URL = "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html#test%7C1";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Wait for autocomplete");
  await promiseAutocompleteResultPopup("dummy_page");

  info("Select autocomplete popup entry");
  EventUtils.synthesizeKey("VK_DOWN", {});
  ok(gURLBar.value.startsWith("moz-action:switchtab"), "switch to tab entry found");

  info("switch-to-tab");
  await new Promise((resolve, reject) => {
    // In case of success it should switch tab.
    gBrowser.tabContainer.addEventListener("TabSelect", function() {
      is(gBrowser.selectedTab, tab, "Should have switched to the right tab");
      resolve();
    }, {once: true});
    EventUtils.synthesizeKey("VK_RETURN", { });
  });

  gBrowser.removeCurrentTab();
  await PlacesTestUtils.clearHistory();
});

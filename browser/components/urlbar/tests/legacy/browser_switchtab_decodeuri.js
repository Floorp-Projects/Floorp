const TEST_PATH = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://example.org");
const TEST_URL = `${TEST_PATH}dummy_page.html#test%7C1`;

add_task(async function test_switchtab_decodeuri() {
  info("Opening first tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  info("Opening and selecting second tab");
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  info("Wait for autocomplete");
  await promiseAutocompleteResultPopup("dummy_page");

  info("Select autocomplete popup entry");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  ok(gURLBar.value.startsWith("moz-action:switchtab"), "switch to tab entry found");

  info("switch-to-tab");
  await new Promise((resolve, reject) => {
    // In case of success it should switch tab.
    gBrowser.tabContainer.addEventListener("TabSelect", function() {
      is(gBrowser.selectedTab, tab, "Should have switched to the right tab");
      resolve();
    }, {once: true});
    EventUtils.synthesizeKey("KEY_Enter");
  });

  gBrowser.removeCurrentTab();
  await PlacesUtils.history.clear();
});

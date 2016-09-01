add_task(function* test_switchtab_decodeuri() {
  info("Opening first tab");
  let tab = gBrowser.addTab("http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html#test%7C1");
  yield promiseTabLoadEvent(tab);

  info("Opening and selecting second tab");
  let newTab = gBrowser.selectedTab = gBrowser.addTab();

  info("Wait for autocomplete")
  yield promiseAutocompleteResultPopup("dummy_page");

  info("Select autocomplete popup entry");
  EventUtils.synthesizeKey("VK_DOWN", {});
  ok(gURLBar.value.startsWith("moz-action:switchtab"), "switch to tab entry found");

  info("switch-to-tab");
  yield new Promise((resolve, reject) => {
    // In case of success it should switch tab.
    gBrowser.tabContainer.addEventListener("TabSelect", function select() {
      gBrowser.tabContainer.removeEventListener("TabSelect", select, false);
      is(gBrowser.selectedTab, tab, "Should have switched to the right tab");
      resolve();
    }, false);
    EventUtils.synthesizeKey("VK_RETURN", { });
  });

  gBrowser.removeCurrentTab();
  yield PlacesTestUtils.clearHistory();
});

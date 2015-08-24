function* wait_while_tab_is_busy(tab) {
  yield BrowserTestUtils.waitForEvent(tab, "TabAttrModified", false,
    event => event.detail.changed.indexOf("busy") >= 0 &&
             !tab.hasAttribute("busy")
  );
}

// This function waits for the tab to stop being busy instead of waiting for it
// to load, since the canViewSource change happens at that time.
let with_new_tab_opened = Task.async(function* (options, taskFn) {
  let tab = yield BrowserTestUtils.openNewForegroundTab(options.gBrowser, options.url, false);
  yield wait_while_tab_is_busy(tab);
  yield taskFn(tab.linkedBrowser);
  gBrowser.removeTab(tab);
});

add_task(function*() {
  yield new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": [
                                ["view_source.tab", true],
                              ]}, resolve);
  });
});

add_task(function* test_regular_page() {
  function* test_expect_view_source_enabled(browser) {
    ok(!XULBrowserWindow.canViewSource.hasAttribute("disabled"),
       "View Source should be enabled");
  }

  yield with_new_tab_opened({
    gBrowser,
    url: "http://example.com",
  }, test_expect_view_source_enabled);
});

add_task(function* test_view_source_page() {
  function* test_expect_view_source_disabled(browser) {
    ok(XULBrowserWindow.canViewSource.hasAttribute("disabled"),
       "View Source should be disabled");
  }

  yield with_new_tab_opened({
    gBrowser,
    url: "view-source:http://example.com",
  }, test_expect_view_source_disabled);
});

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

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, test_expect_view_source_enabled);
});

add_task(function* test_view_source_page() {
  function* test_expect_view_source_disabled(browser) {
    ok(XULBrowserWindow.canViewSource.hasAttribute("disabled"),
       "View Source should be disabled");
  }

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "view-source:http://example.com",
  }, test_expect_view_source_disabled);
});

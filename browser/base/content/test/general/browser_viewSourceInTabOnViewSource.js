function wait_while_tab_is_busy() {
  return new Promise(resolve => {
    let progressListener = {
      onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
          gBrowser.removeProgressListener(this);
          setTimeout(resolve, 0);
        }
      }
    };
    gBrowser.addProgressListener(progressListener);
  });
}

// This function waits for the tab to stop being busy instead of waiting for it
// to load, since the canViewSource change happens at that time.
var with_new_tab_opened = async function(options, taskFn) {
  let busyPromise = wait_while_tab_is_busy();
  let tab = await BrowserTestUtils.openNewForegroundTab(options.gBrowser, options.url, false);
  await busyPromise;
  await taskFn(tab.linkedBrowser);
  gBrowser.removeTab(tab);
};

add_task(async function() {
  await SpecialPowers.pushPrefEnv({"set": [
                                    ["view_source.tab", true],
                                  ]});
});

add_task(async function test_regular_page() {
  function test_expect_view_source_enabled(browser) {
    ok(!XULBrowserWindow.canViewSource.hasAttribute("disabled"),
       "View Source should be enabled");
  }

  await with_new_tab_opened({
    gBrowser,
    url: "http://example.com",
  }, test_expect_view_source_enabled);
});

add_task(async function test_view_source_page() {
  function test_expect_view_source_disabled(browser) {
    ok(XULBrowserWindow.canViewSource.hasAttribute("disabled"),
       "View Source should be disabled");
  }

  await with_new_tab_opened({
    gBrowser,
    url: "view-source:http://example.com",
  }, test_expect_view_source_disabled);
});

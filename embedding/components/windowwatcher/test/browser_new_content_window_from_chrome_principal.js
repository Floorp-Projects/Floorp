"use strict";

/**
 * Tests that if chrome-privileged code calls .open() on an
 * unprivileged window, that the principal in the newly
 * opened window is appropriately set.
 */
add_task(function* test_chrome_opens_window() {
  // This magic value of 2 means that by default, when content tries
  // to open a new window, it'll actually open in a new window instead
  // of a new tab.
  yield SpecialPowers.pushPrefEnv({"set": [
    ["browser.link.open_newwindow", 2],
  ]});

  let newWinPromise = BrowserTestUtils.waitForNewWindow(true, "http://example.com/");

  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    content.open("http://example.com/", "_blank");
  });

  let win = yield newWinPromise;
  let browser = win.gBrowser.selectedBrowser;

  yield ContentTask.spawn(browser, null, function*() {
    Assert.ok(!content.document.nodePrincipal.isSystemPrincipal,
              "We should not have a system principal.")
    Assert.equal(content.document.nodePrincipal.origin,
                 "http://example.com",
                 "Should have the example.com principal");
  });

  yield BrowserTestUtils.closeWindow(win);
});
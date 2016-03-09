"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

/**
 * Monkey patches TabCrashHandler.getDumpID to return null in order to test
 * about:tabcrashed when a dump is not available.
 */
add_task(function* setup() {
  let originalGetDumpID = TabCrashHandler.getDumpID;
  TabCrashHandler.getDumpID = function(browser) { return null; };
  registerCleanupFunction(() => {
    TabCrashHandler.getDumpID = originalGetDumpID;
  });
});

/**
 * Tests tab crash page when a dump is not available.
 */
add_task(function* test_without_dump() {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    let tab = gBrowser.getTabForBrowser(browser);
    yield BrowserTestUtils.crashBrowser(browser);

    let tabRemovedPromise = BrowserTestUtils.removeTab(tab, { dontRemove: true });

    yield ContentTask.spawn(browser, null, function*() {
      let doc = content.document;
      Assert.ok(!doc.documentElement.classList.contains("crashDumpAvailable"),
         "doesn't have crash dump");

      let container = doc.getElementById("crash-reporter-container");
      Assert.ok(container, "has crash-reporter-container");
      Assert.ok(container.hidden, "crash-reporter-container is hidden");

      doc.getElementById("closeTab").click();
    });

    yield tabRemovedPromise;
  });
});

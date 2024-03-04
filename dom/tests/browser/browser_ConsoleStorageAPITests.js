/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI =
  "http://example.com/browser/dom/tests/browser/test-console-api.html";

function tearDown() {
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

add_task(async function () {
  // Don't cache removed tabs, so "clear console cache on tab close" triggers.
  await SpecialPowers.pushPrefEnv({ set: [["browser.tabs.max_tabs_undo", 0]] });

  registerCleanupFunction(tearDown);

  info(
    "Open a keepalive tab in the background to make sure we don't accidentally kill the content process"
  );
  var keepaliveTab = await BrowserTestUtils.addTab(
    gBrowser,
    "data:text/html,<meta charset=utf8>Keep Alive Tab"
  );

  info("Open the main tab to run the test in");
  var tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);
  var browser = gBrowser.selectedBrowser;

  const windowId = await ContentTask.spawn(browser, null, async function () {
    let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(
      Ci.nsIConsoleAPIStorage
    );

    let observerPromise = new Promise(resolve => {
      let apiCallCount = 0;
      function observe() {
        apiCallCount++;
        info(`Received ${apiCallCount} console log events`);
        if (apiCallCount == 4) {
          ConsoleAPIStorage.removeLogEventListener(observe);
          resolve();
        }
      }

      info("Setting up observer");
      ConsoleAPIStorage.addLogEventListener(
        observe,
        Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal)
      );
    });

    info("Emit a few console API logs");
    content.console.log("this", "is", "a", "log", "message");
    content.console.info("this", "is", "a", "info", "message");
    content.console.warn("this", "is", "a", "warn", "message");
    content.console.error("this", "is", "a", "error", "message");

    info("Wait for the corresponding log event");
    await observerPromise;

    const innerWindowId = content.windowGlobalChild.innerWindowId;
    const events = ConsoleAPIStorage.getEvents(innerWindowId).filter(
      message =>
        message.arguments[0] === "this" &&
        message.arguments[1] === "is" &&
        message.arguments[2] === "a" &&
        message.arguments[4] === "message"
    );
    is(events.length, 4, "The storage service got the messages we emitted");

    info("Ensure clearEvents does remove the events from storage");
    ConsoleAPIStorage.clearEvents();
    is(ConsoleAPIStorage.getEvents(innerWindowId).length, 0, "Cleared Storage");

    return content.windowGlobalChild.innerWindowId;
  });

  await SpecialPowers.spawn(browser, [], function () {
    // make sure a closed window's events are in fact removed from
    // the storage cache
    content.console.log("adding a new event");
  });

  info("Close the window");
  gBrowser.removeTab(tab, { animate: false });
  // Ensure actual window destruction is not delayed (too long).
  SpecialPowers.DOMWindowUtils.garbageCollect();

  // Spawn the check in the keepaliveTab, so that we can read the ConsoleAPIStorage correctly
  gBrowser.selectedTab = keepaliveTab;
  browser = gBrowser.selectedBrowser;

  // Spin the event loop to make sure everything is cleared.
  await SpecialPowers.spawn(browser, [], () => {});

  await SpecialPowers.spawn(browser, [windowId], function (windowId) {
    var ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"].getService(
      Ci.nsIConsoleAPIStorage
    );
    is(
      ConsoleAPIStorage.getEvents(windowId).length,
      0,
      "tab close is clearing the cache"
    );
  });
});

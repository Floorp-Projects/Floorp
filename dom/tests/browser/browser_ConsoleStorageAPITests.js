/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI_NAV = "http://example.com/browser/dom/tests/browser/";

function tearDown()
{
  while (gBrowser.tabs.length > 1)
    gBrowser.removeCurrentTab();
}

add_task(function*()
{
  // Don't cache removed tabs, so "clear console cache on tab close" triggers.
  Services.prefs.setIntPref("browser.tabs.max_tabs_undo", 0);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.tabs.max_tabs_undo");
  });

  registerCleanupFunction(tearDown);

  // Open a keepalive tab in the background to make sure we don't accidentally
  // kill the content process
  var keepaliveTab = gBrowser.addTab("about:blank");

  // Open the main tab to run the test in
  var tab = gBrowser.addTab("about:blank");
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;

  let observerPromise = ContentTask.spawn(browser, null, function(opt) {
    const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";
    let ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
          .getService(Ci.nsIConsoleAPIStorage);

    return new Promise(resolve => {
      let apiCallCount = 0;
      let ConsoleObserver = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

        observe: function(aSubject, aTopic, aData) {
          if (aTopic == "console-storage-cache-event") {
            apiCallCount++;
            if (apiCallCount == 4) {
              let windowId = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;

              Services.obs.removeObserver(this, "console-storage-cache-event");
              Assert.ok(ConsoleAPIStorage.getEvents(windowId).length >= 4, "Some messages found in the storage service");
              ConsoleAPIStorage.clearEvents();
              Assert.equal(ConsoleAPIStorage.getEvents(windowId).length, 0, "Cleared Storage");

              resolve(windowId);
            }
          }
        }
      };

      Services.obs.addObserver(ConsoleObserver, "console-storage-cache-event", false);

      // Redirect the browser to the test URI
      content.window.location = TEST_URI;
    });
  });

  let win;
  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function test_executeSoon() {
      win = browser.contentWindow;
      win.console.log("this", "is", "a", "log message");
      win.console.info("this", "is", "a", "info message");
      win.console.warn("this", "is", "a", "warn message");
      win.console.error("this", "is", "a", "error message");
    });
  }, false);

  let windowId = yield observerPromise;
  // make sure a closed window's events are in fact removed from
  // the storage cache
  win.console.log("adding a new event");

  // Close the window.
  gBrowser.removeTab(tab, {animate: false});
  // Ensure actual window destruction is not delayed (too long).
  SpecialPowers.DOMWindowUtils.garbageCollect();

  // Spawn the check in the keepaliveTab, so that we can read the ConsoleAPIStorage correctly
  gBrowser.selectedTab = keepaliveTab;
  browser = gBrowser.selectedBrowser;

  // Ensure the "inner-window-destroyed" event is processed,
  // so the storage cache is cleared. We do this in the content
  // process because that is where the event will be processed.
  yield ContentTask.spawn(browser, null, function() {
    yield new Promise(resolve => setTimeout(resolve, 0));
  });

  yield ContentTask.spawn(browser, windowId, function(windowId) {
    var ConsoleAPIStorage = Cc["@mozilla.org/consoleAPI-storage;1"]
          .getService(Ci.nsIConsoleAPIStorage);
    Assert.equal(ConsoleAPIStorage.getEvents(windowId).length, 0, "tab close is clearing the cache");
  });
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";
const TEST_URI_NAV = "http://example.com/browser/dom/tests/browser/";

Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm");

var apiCallCount;

var ConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function CO_init()
  {
    Services.obs.addObserver(this, "console-storage-cache-event", false);
    apiCallCount = 0;
  },

  observe: function CO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "console-storage-cache-event") {
      apiCallCount ++;
      if (apiCallCount == 4) {
        try {
                  var tab = gBrowser.selectedTab;
        let browser = gBrowser.selectedBrowser;
        let win = XPCNativeWrapper.unwrap(browser.contentWindow);
        let windowID = getWindowId(win);
        let messages = ConsoleAPIStorage.getEvents(windowID);

        ok(messages.length >= 4, "Some messages found in the storage service");

        ConsoleAPIStorage.clearEvents();
        messages = ConsoleAPIStorage.getEvents(windowID);
        ok(messages.length == 0, "Cleared Storage, no events found");

        // remove the observer so we don't trigger this test again
        Services.obs.removeObserver(this, "console-storage-cache-event");

        // make sure a closed window's events are in fact removed from the
        // storage cache
        win.console.log("adding a new event");

        // close the window - the storage cache should now be empty
        gBrowser.removeTab(tab, {animate: false});

        window.QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindowUtils).garbageCollect();
        executeSoon(function (){
          // use the old windowID again to see if we have any stray cached messages
          messages = ConsoleAPIStorage.getEvents(windowID);
          ok(messages.length == 0, "0 events found, tab close is clearing the cache");
          finish();
        });
        } catch (ex) {
          dump(ex + "\n\n\n");
          dump(ex.stack + "\n\n\n");
        }
        }

    }
  }
};

function tearDown()
{
   while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
}

function test()
{
  registerCleanupFunction(tearDown);

  ConsoleObserver.init();

  waitForExplicitFinish();

  var tab = gBrowser.addTab(TEST_URI);
  gBrowser.selectedTab = tab;
  var browser = gBrowser.selectedBrowser;
  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function test_executeSoon() {
      var contentWin = browser.contentWindow;

      let win = XPCNativeWrapper.unwrap(contentWin);

      win.console.log("this", "is", "a", "log message");
      win.console.info("this", "is", "a", "info message");
      win.console.warn("this", "is", "a", "warn message");
      win.console.error("this", "is", "a", "error message");
    });
  }, false);
}

function getWindowId(aWindow)
{
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils)
                .currentInnerWindowID;
}

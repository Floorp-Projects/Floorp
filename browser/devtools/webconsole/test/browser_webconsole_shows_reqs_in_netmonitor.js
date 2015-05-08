/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URI = "data:text/html;charset=utf8,Test that the web console " +
                 "displays requests that have been recorded in the " +
                 "netmonitor, even if the console hadn't opened yet.";

const TEST_FILE = "test-network-request.html";
const TEST_PATH = "http://example.com/browser/browser/devtools/webconsole/test/" +
                 TEST_FILE;

const NET_PREF = "devtools.webconsole.filter.networkinfo";
Services.prefs.setBoolPref(NET_PREF, true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(NET_PREF);
});

add_task(function* () {
  let { tab, browser } = yield loadTab(TEST_URI);

  let target = TargetFactory.forTab(tab);
  let toolbox = yield gDevTools.showToolbox(target, "netmonitor");
  info("Network panel is open.");

  yield loadDocument(browser);
  info("Document loaded.");

  // Test that the request appears in the network panel.
  testNetmonitor(toolbox);

  // Test that the request appears in the console.
  let hud = yield openConsole();
  info("Web console is open");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "network message",
        text: TEST_FILE,
        category: CATEGORY_NETWORK,
        severity: SEVERITY_LOG
      }
    ]
  });
});


function loadDocument(browser) {
  let deferred = promise.defer();

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve();
  }, true);
  content.location = TEST_PATH;

  return deferred.promise;
}

function testNetmonitor(toolbox) {
  let monitor = toolbox.getCurrentPanel();
  let { RequestsMenu } = monitor.panelWin.NetMonitorView;
  is(RequestsMenu.itemCount, 1, "Network request appears in the network panel");

  let item = RequestsMenu.getItemAtIndex(0);
  is(item.attachment.method, "GET", "The attached method is correct.");
  is(item.attachment.url, TEST_PATH, "The attached url is correct.");
}

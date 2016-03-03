/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Ensure that clicking the button in the Offline mode neterror page makes the browser go online. See bug 435325. */

var proxyPrefValue;

function test() {
  waitForExplicitFinish();

  // Go offline and disable the proxy and cache, then try to load the test URL.
  Services.io.offline = true;

  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  proxyPrefValue = Services.prefs.getIntPref("network.proxy.type");
  Services.prefs.setIntPref("network.proxy.type", 0);

  Services.prefs.setBoolPref("browser.cache.disk.enable", false);
  Services.prefs.setBoolPref("browser.cache.memory.enable", false);

  gBrowser.selectedTab = gBrowser.addTab("http://example.com/");

  let contentScript = `
    let listener = function () {
      removeEventListener("DOMContentLoaded", listener);
      sendAsyncMessage("Test:DOMContentLoaded", { uri: content.document.documentURI });
    };
    addEventListener("DOMContentLoaded", listener);
  `;

  function pageloaded({ data }) {
    mm.removeMessageListener("Test:DOMContentLoaded", pageloaded);
    checkPage(data);
  }

  let mm = gBrowser.selectedBrowser.messageManager;
  mm.addMessageListener("Test:DOMContentLoaded", pageloaded);
  mm.loadFrameScript("data:," + contentScript, true);
}

function checkPage(data) {
  ok(Services.io.offline, "Setting Services.io.offline to true.");

  is(data.uri.substring(0, 27),
     "about:neterror?e=netOffline", "Loading the Offline mode neterror page.");

  // Re-enable the proxy so example.com is resolved to localhost, rather than
  // the actual example.com.
  Services.prefs.setIntPref("network.proxy.type", proxyPrefValue);

  Services.obs.addObserver(function observer(aSubject, aTopic) {
    ok(!Services.io.offline, "After clicking the Try Again button, we're back " +
                             "online.");
    Services.obs.removeObserver(observer, "network:offline-status-changed", false);
    finish();
  }, "network:offline-status-changed", false);

  ContentTask.spawn(gBrowser.selectedBrowser, null, function* () {
    content.document.getElementById("errorTryAgain").click();
  });
}

registerCleanupFunction(function() {
  Services.prefs.setBoolPref("browser.cache.disk.enable", true);
  Services.prefs.setBoolPref("browser.cache.memory.enable", true);
  Services.io.offline = false;
  gBrowser.removeCurrentTab();
});

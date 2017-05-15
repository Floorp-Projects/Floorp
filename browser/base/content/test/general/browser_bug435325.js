/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Ensure that clicking the button in the Offline mode neterror page makes the browser go online. See bug 435325. */

add_task(async function checkSwitchPageToOnlineMode() {

  // Go offline and disable the proxy and cache, then try to load the test URL.
  Services.io.offline = true;

  // Tests always connect to localhost, and per bug 87717, localhost is now
  // reachable in offline mode.  To avoid this, disable any proxy.
  let proxyPrefValue = SpecialPowers.getIntPref("network.proxy.type");
  await SpecialPowers.pushPrefEnv({"set": [
    ["network.proxy.type", 0],
    ["browser.cache.disk.enable", false],
    ["browser.cache.memory.enable", false],
  ]});

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let netErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);

    await BrowserTestUtils.loadURI(browser, "http://example.com/");
    await netErrorLoaded;

    // Re-enable the proxy so example.com is resolved to localhost, rather than
    // the actual example.com.
    await SpecialPowers.pushPrefEnv({"set": [["network.proxy.type", proxyPrefValue]]});
    let changeObserved = TestUtils.topicObserved("network:offline-status-changed");

    // Click on the 'Try again' button.
    await ContentTask.spawn(browser, null, function* () {
      ok(content.document.documentURI.startsWith("about:neterror?e=netOffline"), "Should be showing error page");
      content.document.getElementById("errorTryAgain").click();
    });

    await changeObserved;
    ok(!Services.io.offline, "After clicking the 'Try Again' button, we're back online.");
  });
});

registerCleanupFunction(function() {
  Services.io.offline = false;
});

"use strict";

let {AddonManager} = Components.utils.import("resource://gre/modules/AddonManager.jsm", {});
let {Extension} = Components.utils.import("resource://gre/modules/Extension.jsm", {});

function* makeAndInstallXPI(id, backgroundScript, loadedURL) {
  let xpi = Extension.generateXPI({
    manifest: {applications: {gecko: {id}}},
    background: backgroundScript,
  });
  SimpleTest.registerCleanupFunction(function cleanupXPI() {
    Services.obs.notifyObservers(xpi, "flush-cache-entry", null);
    xpi.remove(false);
  });

  let loadPromise = BrowserTestUtils.waitForNewTab(gBrowser, loadedURL);


  info(`installing ${xpi.path}`);
  let addon = yield AddonManager.installTemporaryAddon(xpi);
  info("installed");

  // A WebExtension is started asynchronously, we have our test extension
  // open a new tab to signal that the background script has executed.
  let loadTab = yield loadPromise;
  yield BrowserTestUtils.removeTab(loadTab);

  return addon;
}


add_task(function* test_setuninstallurl_badargs() {
  async function backgroundScript() {
    let promises = [
      browser.runtime.setUninstallURL("this is not a url")
        .then(() => {
          browser.test.notifyFail("setUninstallURL should have failed with bad url");
        })
        .catch(error => {
          browser.test.assertTrue(/Invalid URL/.test(error.message), "error message indicates malformed url");
        }),

      browser.runtime.setUninstallURL("file:///etc/passwd")
        .then(() => {
          browser.test.notifyFail("setUninstallURL should have failed with non-http[s] url");
        })
        .catch(error => {
          browser.test.assertTrue(/must have the scheme http or https/.test(error.message), "error message indicates bad scheme");
        }),
    ];

    await Promise.all(promises);
    browser.test.notifyPass("setUninstallURL bad params");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: "(" + backgroundScript.toString() + ")()",
  });
  yield extension.startup();
  yield extension.awaitFinish();
  yield extension.unload();
});

// Test the documented behavior of setUninstallURL() that passing an
// empty string is equivalent to not setting an uninstall URL
// (i.e., no new tab is opened upon uninstall)
add_task(function* test_setuninstall_empty_url() {
  async function backgroundScript() {
    await browser.runtime.setUninstallURL("");
    browser.tabs.create({url: "http://example.com/addon_loaded"});
  }

  let addon = yield makeAndInstallXPI("test_uinstallurl2@tests.mozilla.org",
                                      backgroundScript,
                                      "http://example.com/addon_loaded");

  addon.uninstall(true);
  info("uninstalled");

  // no need to explicitly check for the absence of a new tab,
  // BrowserTestUtils will eventually complain if one is opened.
});

add_task(function* test_setuninstallurl() {
  async function backgroundScript() {
    await browser.runtime.setUninstallURL("http://example.com/addon_uninstalled");
    browser.tabs.create({url: "http://example.com/addon_loaded"});
  }

  let addon = yield makeAndInstallXPI("test_uinstallurl@tests.mozilla.org",
                                      backgroundScript,
                                      "http://example.com/addon_loaded");

  // look for a new tab with the uninstall url.
  let uninstallPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/addon_uninstalled");

  addon.uninstall(true);
  info("uninstalled");

  let uninstalledTab = yield uninstallPromise;
  isnot(uninstalledTab, null, "opened tab with uninstall url");
  yield BrowserTestUtils.removeTab(uninstalledTab);
});

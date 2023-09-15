/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test tests if the service worker is able to intercept the script loading
 * channel of a dedicated worker.
 *
 * On success, the test will not crash.
 */

const SAME_ORIGIN = "https://example.com";

const SAME_ORIGIN_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  SAME_ORIGIN
);

const SW_REGISTER_URL = `${SAME_ORIGIN_ROOT}empty_with_utils.html`;
const SW_SCRIPT_URL = `${SAME_ORIGIN_ROOT}simple_fetch_worker.js`;
const SCRIPT_URL = `${SAME_ORIGIN_ROOT}empty.js`;

async function navigateTab(aTab, aUrl) {
  BrowserTestUtils.startLoadingURIString(aTab.linkedBrowser, aUrl);

  await BrowserTestUtils.waitForLocationChange(gBrowser, aUrl).then(() =>
    BrowserTestUtils.browserStopped(aTab.linkedBrowser)
  );
}

async function runTest(aTestSharedWorker) {
  const tab = gBrowser.selectedTab;

  await navigateTab(tab, SW_REGISTER_URL);

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [SCRIPT_URL, aTestSharedWorker],
    async (scriptUrl, testSharedWorker) => {
      await new Promise(resolve => {
        content.navigator.serviceWorker.onmessage = e => {
          if (e.data == scriptUrl) {
            resolve();
          }
        };

        if (testSharedWorker) {
          let worker = new content.Worker(scriptUrl);
        } else {
          let worker = new content.SharedWorker(scriptUrl);
        }
      });
    }
  );

  ok(true, "The service worker has intercepted the script loading.");
}

add_task(async function setupPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
    ],
  });
});

add_task(async function setupBrowser() {
  // The tab will be used by subsequent test steps via 'gBrowser.selectedTab'.
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_URL,
  });

  registerCleanupFunction(async _ => {
    await navigateTab(tab, SW_REGISTER_URL);

    await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      await content.wrappedJSObject.unregisterAll();
    });

    BrowserTestUtils.removeTab(tab);
  });

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [SW_SCRIPT_URL],
    async scriptUrl => {
      await content.wrappedJSObject.registerAndWaitForActive(scriptUrl);
    }
  );
});

add_task(async function runTests() {
  await runTest(false);
  await runTest(true);
});

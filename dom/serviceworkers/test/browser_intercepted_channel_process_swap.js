/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that navigation loads through intercepted channels result in the
// appropriate process swaps. This appears to only be possible when navigating
// to a cross-origin URL, where that navigation is controlled by a ServiceWorker.

"use strict";

const SAME_ORIGIN = "https://example.com";
const CROSS_ORIGIN = "https://example.org";

const SAME_ORIGIN_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  SAME_ORIGIN
);
const CROSS_ORIGIN_ROOT = SAME_ORIGIN_ROOT.replace(SAME_ORIGIN, CROSS_ORIGIN);

const SW_REGISTER_URL = `${CROSS_ORIGIN_ROOT}empty_with_utils.html`;
const SW_SCRIPT_URL = `${CROSS_ORIGIN_ROOT}intercepted_channel_process_swap_worker.js`;
const URL_BEFORE_NAVIGATION = `${SAME_ORIGIN_ROOT}empty.html`;
const CROSS_ORIGIN_URL = `${CROSS_ORIGIN_ROOT}empty.html`;

const TESTCASES = [
  {
    url: CROSS_ORIGIN_URL,
    description:
      "Controlled cross-origin navigation with network-provided response",
  },
  {
    url: `${CROSS_ORIGIN_ROOT}this-path-does-not-exist?respondWith=${CROSS_ORIGIN_URL}`,
    description:
      "Controlled cross-origin navigation with ServiceWorker-provided response",
  },
];

async function navigateTab(aTab, aUrl) {
  BrowserTestUtils.startLoadingURIString(aTab.linkedBrowser, aUrl);

  await BrowserTestUtils.waitForLocationChange(gBrowser, aUrl).then(() =>
    BrowserTestUtils.browserStopped(aTab.linkedBrowser)
  );
}

async function runTestcase(aTab, aTestcase) {
  info(`Testing ${aTestcase.description}`);

  await navigateTab(aTab, URL_BEFORE_NAVIGATION);

  const [initialPid] = E10SUtils.getBrowserPids(aTab.linkedBrowser);

  await navigateTab(aTab, aTestcase.url);

  const [finalPid] = E10SUtils.getBrowserPids(aTab.linkedBrowser);

  await SpecialPowers.spawn(aTab.linkedBrowser, [], () => {
    Assert.ok(
      content.navigator.serviceWorker.controller,
      `${content.location} should be controlled.`
    );
  });

  Assert.notEqual(
    initialPid,
    finalPid,
    `Navigating from ${URL_BEFORE_NAVIGATION} to ${aTab.linkedBrowser.currentURI.spec} should have resulted in a different PID.`
  );
}

add_task(async function setupPrefs() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });
});

add_task(async function setupBrowser() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: SW_REGISTER_URL,
  });

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [SW_SCRIPT_URL],
    async scriptUrl => {
      await content.wrappedJSObject.registerAndWaitForActive(scriptUrl);
    }
  );
});

add_task(async function runTestcases() {
  for (const testcase of TESTCASES) {
    await runTestcase(gBrowser.selectedTab, testcase);
  }
});

add_task(async function cleanup() {
  const tab = gBrowser.selectedTab;

  await navigateTab(tab, SW_REGISTER_URL);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.unregisterAll();
  });

  BrowserTestUtils.removeTab(tab);
});

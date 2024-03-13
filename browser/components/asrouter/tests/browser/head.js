"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ASRouter: "resource:///modules/asrouter/ASRouter.sys.mjs",
  FeatureCallout: "resource:///modules/asrouter/FeatureCallout.sys.mjs",

  FeatureCalloutBroker:
    "resource:///modules/asrouter/FeatureCalloutBroker.sys.mjs",

  FeatureCalloutMessages:
    "resource:///modules/asrouter/FeatureCalloutMessages.sys.mjs",

  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  QueryCache: "resource:///modules/asrouter/ASRouterTargeting.sys.mjs",
});
const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
// We import sinon here to make it available across all mochitest test files
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Feature callout constants
const calloutId = "feature-callout";
const calloutSelector = `#${calloutId}.featureCallout`;
const calloutCTASelector = `#${calloutId} :is(.primary, .secondary)`;
const calloutDismissSelector = `#${calloutId} .dismiss-button`;

function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({ set: prefs });
}

async function clearHistoryAndBookmarks() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  QueryCache.expireAll();
}

/**
 * Helper function to navigate and wait for page to load
 * https://searchfox.org/mozilla-central/rev/314b4297e899feaf260e7a7d1a9566a218216e7a/testing/mochitest/BrowserTestUtils/BrowserTestUtils.sys.mjs#404
 */
async function waitForUrlLoad(url) {
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);
}

async function waitForCalloutScreen(target, screenId) {
  await BrowserTestUtils.waitForMutationCondition(
    target,
    { childList: true, subtree: true, attributeFilter: ["class"] },
    () => target.querySelector(`${calloutSelector}:not(.hidden) .${screenId}`)
  );
}

async function waitForCalloutRemoved(target) {
  await BrowserTestUtils.waitForMutationCondition(
    target,
    { childList: true, subtree: true },
    () => !target.querySelector(calloutSelector)
  );
}

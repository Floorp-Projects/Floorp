/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the buttons in the onboarding dialog for quick suggest/Firefox Suggest.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const DIALOG_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml";

const LEARN_MORE_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "firefox-suggest";

// When the accept button is clicked, no new pages should load, and the feature
// should be enabled.
add_task(async function accept() {
  await doDialogTest(true, async () => {
    let tabCount = gBrowser.tabs.length;
    let dialogPromise = openDialog("accept");

    info("Calling maybeShowOnboardingDialog");
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();

    info("Waiting for dialog");
    await dialogPromise;

    Assert.equal(
      gBrowser.currentURI.spec,
      "about:blank",
      "Nothing loaded in the current tab"
    );
    Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
  });
});

// When the Not Now link is clicked, no new pages should load, and the feature
// should be enabled.
add_task(async function notNow() {
  await doDialogTest(false, async () => {
    let tabCount = gBrowser.tabs.length;
    let dialogPromise = openDialog("onboardingNotNow");

    info("Calling maybeShowOnboardingDialog");
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();

    info("Waiting for dialog");
    await dialogPromise;

    Assert.equal(
      gBrowser.currentURI.spec,
      "about:blank",
      "Nothing loaded in the current tab"
    );
    Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
  });
});

// When the Customize button is clicked, about:preferences#search should load.
add_task(async function customize() {
  await doDialogTest(false, async () => {
    let dialogPromise = openDialog("extra1");

    // about:preferences#search will load in the current tab since it's
    // about:blank.
    let loadPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    ).then(() => info("Saw load"));

    info("Calling maybeShowOnboardingDialog");
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();

    info("Waiting for dialog");
    await dialogPromise;

    info("Waiting for load");
    await loadPromise;

    Assert.equal(
      gBrowser.currentURI.spec,
      "about:preferences#search",
      "Current tab is about:preferences#search"
    );
  });
});

// When the Learn More button is clicked, the support URL should open in a new
// tab.
add_task(async function learnMore() {
  await doDialogTest(false, async () => {
    let dialogPromise = openDialog("onboardingLearnMore");
    let loadPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      LEARN_MORE_URL
    ).then(tab => {
      info("Saw new tab");
      return tab;
    });

    info("Calling maybeShowOnboardingDialog");
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();

    info("Waiting for dialog");
    await dialogPromise;

    info("Waiting for new tab");
    let tab = await loadPromise;

    Assert.equal(gBrowser.selectedTab, tab, "Current tab is the new tab");
    Assert.equal(
      gBrowser.currentURI.spec,
      LEARN_MORE_URL,
      "Current tab is the support page"
    );
    BrowserTestUtils.removeTab(tab);
  });
});

async function doDialogTest(expectOptIn, callback) {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Set all the required prefs for showing the onboarding dialog.
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.suggest.quicksuggest", false],
        ["browser.urlbar.suggest.quicksuggest.sponsored", false],
        ["browser.urlbar.quicksuggest.enabled", true],
        ["browser.urlbar.quicksuggest.shouldShowOnboardingDialog", true],
        ["browser.urlbar.quicksuggest.showedOnboardingDialog", false],
        ["browser.urlbar.quicksuggest.showOnboardingDialogAfterNRestarts", 0],
      ],
    });
    await callback();

    Assert.equal(
      UrlbarPrefs.get("suggest.quicksuggest"),
      expectOptIn,
      "Main pref has been enabled"
    );
    Assert.equal(
      UrlbarPrefs.get("suggest.quicksuggest.sponsored"),
      expectOptIn,
      "Sponsored pref has been enabled"
    );

    await SpecialPowers.popPrefEnv();
  });
}

async function openDialog(button) {
  await BrowserTestUtils.promiseAlertDialog(button, DIALOG_URI, {
    isSubDialog: true,
  });
  info("Saw dialog");
}

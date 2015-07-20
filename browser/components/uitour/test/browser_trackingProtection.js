"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PREF_INTRO_COUNT = "privacy.trackingprotection.introCount";
const PREF_TP_ENABLED = "privacy.trackingprotection.enabled";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/benignPage.html";
const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/general/trackingPage.html";
const TOOLTIP_PANEL = document.getElementById("UITourTooltip");
const TOOLTIP_ANCHOR = document.getElementById("tracking-protection-icon");

let {UrlClassifierTestUtils} = Cu.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
  Services.prefs.clearUserPref(PREF_TP_ENABLED);
  Services.prefs.clearUserPref(PREF_INTRO_COUNT);
});

function allowOneIntro() {
  Services.prefs.setIntPref(PREF_INTRO_COUNT, TrackingProtection.MAX_INTROS - 1);
}

add_task(function* setup_test() {
  Services.prefs.setBoolPref(PREF_TP_ENABLED, true);
  yield UrlClassifierTestUtils.addTestTrackers();
});

add_task(function* test_benignPage() {
  info("Load a test page not containing tracking elements");
  allowOneIntro();
  yield BrowserTestUtils.withNewTab({gBrowser, url: BENIGN_PAGE}, function*() {
    yield waitForConditionPromise(() => {
      return is_visible(TOOLTIP_PANEL);
    }, "Info panel shouldn't appear on a benign page").
      then(() => ok(false, "Info panel shouldn't appear"),
           () => {
             ok(true, "Info panel didn't appear on a benign page");
           });

  });
});

add_task(function* test_trackingPages() {
  info("Load a test page containing tracking elements");
  allowOneIntro();
  yield BrowserTestUtils.withNewTab({gBrowser, url: TRACKING_PAGE}, function*() {
    yield new Promise((resolve, reject) => {
      waitForPopupAtAnchor(TOOLTIP_PANEL, TOOLTIP_ANCHOR, resolve,
                           "Intro panel should appear");
    });

    is(Services.prefs.getIntPref(PREF_INTRO_COUNT), TrackingProtection.MAX_INTROS, "Check intro count increased");

    let step2URL = Services.urlFormatter.formatURLPref("privacy.trackingprotection.introURL") +
                    "#step2";
    let buttons = document.getElementById("UITourTooltipButtons");

    info("Click the step text and nothing should happen");
    let tabCount = gBrowser.tabs.length;
    yield EventUtils.synthesizeMouseAtCenter(buttons.children[0], {});
    is(gBrowser.tabs.length, tabCount, "Same number of tabs should be open");

    info("Resetting count to test that viewing the tour prevents future panels");
    allowOneIntro();

    let panelHiddenPromise = promisePanelElementHidden(window, TOOLTIP_PANEL);
    let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, step2URL);
    info("Clicking the main button");
    EventUtils.synthesizeMouseAtCenter(buttons.children[1], {});
    let tab = yield tabPromise;
    is(Services.prefs.getIntPref(PREF_INTRO_COUNT), TrackingProtection.MAX_INTROS,
       "Check intro count is at the max after opening step 2");
    is(gBrowser.tabs.length, tabCount + 1, "Tour step 2 tab opened");
    yield panelHiddenPromise;
    ok(true, "Panel hid when the button was clicked");
    yield BrowserTestUtils.removeTab(tab);
  });

  info("Open another tracking page and make sure we don't show the panel again");
  yield BrowserTestUtils.withNewTab({gBrowser, url: TRACKING_PAGE}, function*() {
    yield waitForConditionPromise(() => {
      return is_visible(TOOLTIP_PANEL);
    }, "Info panel shouldn't appear more than MAX_INTROS").
      then(() => ok(false, "Info panel shouldn't appear again"),
           () => {
             ok(true, "Info panel didn't appear more than MAX_INTROS on tracking pages");
           });

  });
});

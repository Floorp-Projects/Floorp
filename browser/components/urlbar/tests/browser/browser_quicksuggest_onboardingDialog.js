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

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

// When the accept button is clicked, the user should be opted in.
add_task(async function accept() {
  await doDialogTest({
    expectOptIn: true,
    callback: async () => {
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
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "enable_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "sponsored_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept",
      },
    ],
  });
});

// When the Not Now link is clicked, the user should remain opted out.
add_task(async function notNow() {
  await doDialogTest({
    expectOptIn: false,
    callback: async () => {
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
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now",
      },
    ],
  });
});

// When the settings button is clicked, the user should remain opted out and
// about:preferences should load.
add_task(async function settings() {
  await doDialogTest({
    expectOptIn: false,
    callback: async () => {
      let dialogPromise = openDialog("extra1");

      // about:preferences will load in the current tab since it's
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
        "about:preferences#privacy",
        "Current tab is about:preferences#privacy"
      );
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "settings",
      },
    ],
  });
});

// When Learn More is clicked, the user should remain opted out and the support
// URL should open in a new tab.
add_task(async function learnMore() {
  await doDialogTest({
    expectOptIn: false,
    callback: async () => {
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
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "learn_more",
      },
    ],
  });
});

// The Esc key should behave like Not now: It should dismiss the dialog without
// opting in.
add_task(async function escKey() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingAcceptButton",
    expectOptIn: false,
    callback: async () => {
      let tabCount = gBrowser.tabs.length;
      EventUtils.synthesizeKey("KEY_Escape");
      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 0
// Expected focused element: accept button
add_task(async function focus_accept() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingAcceptButton",
    expectOptIn: true,
    callback: async () => {
      EventUtils.synthesizeKey("KEY_Enter");
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "enable_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "sponsored_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 1
// Expected focused element: settings button
add_task(async function focus_settings() {
  await doFocusTest({
    tabKeyRepeat: 1,
    expectedFocusID: "onboardingSettingsButton",
    expectOptIn: false,
    callback: async () => {
      // about:preferences will load in the current tab since it's about:blank.
      let loadPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser
      ).then(() => info("Saw load"));

      EventUtils.synthesizeKey("KEY_Enter");

      info("Waiting for load");
      await loadPromise;

      Assert.equal(
        gBrowser.currentURI.spec,
        "about:preferences#privacy",
        "Current tab is about:preferences#privacy"
      );
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "settings",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 2
// Expected focused element: learn more link
add_task(async function focus_learnMore() {
  await doFocusTest({
    tabKeyRepeat: 2,
    expectedFocusID: "onboardingLearnMore",
    expectOptIn: false,
    callback: async () => {
      let loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        LEARN_MORE_URL
      ).then(tab => {
        info("Saw new tab");
        return tab;
      });

      EventUtils.synthesizeKey("KEY_Enter");

      info("Waiting for new tab");
      let tab = await loadPromise;

      Assert.equal(gBrowser.selectedTab, tab, "Current tab is the new tab");
      Assert.equal(
        gBrowser.currentURI.spec,
        LEARN_MORE_URL,
        "Current tab is the support page"
      );
      BrowserTestUtils.removeTab(tab);
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "learn_more",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 3
// Expected focused element: not now link
add_task(async function focus_notNow() {
  await doFocusTest({
    tabKeyRepeat: 3,
    expectedFocusID: "onboardingNotNow",
    expectOptIn: false,
    callback: async () => {
      let tabCount = gBrowser.tabs.length;
      EventUtils.synthesizeKey("KEY_Enter");
      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 4
// Expected focused element: accept button (wraps around)
add_task(async function focus_accept_wraparound() {
  await doFocusTest({
    tabKeyRepeat: 4,
    expectedFocusID: "onboardingAcceptButton",
    expectOptIn: true,
    callback: async () => {
      EventUtils.synthesizeKey("KEY_Enter");
    },
    telemetryEvents: [
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "enable_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "sponsored_toggled",
        object: "enabled",
      },
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept",
      },
    ],
  });
});

async function doDialogTest({ expectOptIn, telemetryEvents, callback }) {
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

    // Setting the prefs just now triggered telemetry events, so clear them
    // before calling the callback.
    Services.telemetry.clearEvents();

    await callback();

    Assert.equal(
      UrlbarPrefs.get("suggest.quicksuggest"),
      expectOptIn,
      "Main pref enabled status"
    );
    Assert.equal(
      UrlbarPrefs.get("suggest.quicksuggest.sponsored"),
      expectOptIn,
      "Sponsored pref enabled status"
    );

    if (telemetryEvents) {
      TelemetryTestUtils.assertEvents(telemetryEvents);
    }

    await SpecialPowers.popPrefEnv();
  });
}

// Whether the tab key can move the focus. On macOS with full keyboard access
// disabled (which is default), this will be false. See `canTabMoveFocus`.
let gCanTabMoveFocus;

async function doFocusTest({
  tabKeyRepeat,
  expectedFocusID,
  expectOptIn,
  telemetryEvents,
  callback,
}) {
  if (AppConstants.platform == "macosx") {
    if (gCanTabMoveFocus === undefined) {
      gCanTabMoveFocus = await canTabMoveFocus();
    }
    if (!gCanTabMoveFocus) {
      Assert.ok(true, "Skipping focus/tabbing test on Mac");
      return;
    }
  }

  await doDialogTest({
    expectOptIn,
    telemetryEvents,
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        DIALOG_URI,
        { isSubDialog: true }
      );

      let maybeShowPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();

      let win = await dialogPromise;
      if (win.document.readyState != "complete") {
        await BrowserTestUtils.waitForEvent(win, "load");
      }

      let doc = win.document;

      Assert.equal(
        doc.activeElement.id,
        "onboardingAcceptButton",
        "Accept button is focused initially"
      );

      if (tabKeyRepeat) {
        EventUtils.synthesizeKey("KEY_Tab", { repeat: tabKeyRepeat });
      }
      Assert.equal(
        doc.activeElement.id,
        expectedFocusID,
        "Expected element is focused: " + expectedFocusID
      );

      await callback();
      await maybeShowPromise;
    },
  });
}

async function openDialog(button = undefined) {
  await BrowserTestUtils.promiseAlertDialog(button, DIALOG_URI, {
    isSubDialog: true,
  });
  info("Saw dialog");
}

/**
 * This is a real hacky way of determining whether the tab key can move focus.
 * Windows and Linux both support it but macOS does not unless full keyboard
 * access is enabled, so practically this is only useful on macOS. Gecko seems
 * to know whether full keyboard access is enabled because it affects focus in
 * Firefox and some code in nsXULElement.cpp and other places mention it, but
 * there doesn't seem to be a way to access that information from JS. There is
 * `Services.focus.elementIsFocusable`, but it returns true regardless of
 * whether full access is enabled.
 *
 * So what we do here is open the dialog and synthesize a tab key. If the focus
 * doesn't change, then we assume moving the focus via the tab key is not
 * supported.
 *
 * Why not just always skip the focus tasks on Mac? Because individual
 * developers (like the one writing this comment) may be running macOS with full
 * keyboard access enabled and want to actually run the tasks on their machines.
 *
 * @returns {boolean}
 */
async function canTabMoveFocus() {
  let canMove = false;
  await doDialogTest({
    expectOptIn: false,
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        DIALOG_URI,
        { isSubDialog: true }
      );

      let maybeShowPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();

      let win = await dialogPromise;
      if (win.document.readyState != "complete") {
        await BrowserTestUtils.waitForEvent(win, "load");
      }

      let doc = win.document;
      let { activeElement } = doc;
      EventUtils.synthesizeKey("KEY_Tab");

      canMove = activeElement != doc.activeElement;

      EventUtils.synthesizeKey("KEY_Escape");
      await maybeShowPromise;
    },
  });
  return canMove;
}

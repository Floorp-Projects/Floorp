/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the buttons in the onboarding dialog for quick suggest/Firefox Suggest.
 */

ChromeUtils.defineESModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
});

const OTHER_DIALOG_URI = getRootDirectory(gTestPath) + "subdialog.xhtml";

// Default-branch pref values in the offline scenario.
const OFFLINE_DEFAULT_PREFS = {
  "suggest.quicksuggest.nonsponsored": true,
  "suggest.quicksuggest.sponsored": true,
  "quicksuggest.dataCollection.enabled": false,
};

let gDefaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
let gUserBranch = Services.prefs.getBranch("browser.urlbar.");

// Allow more time for Mac and Linux machines so they don't time out in verify mode.
if (AppConstants.platform === "macosx") {
  requestLongerTimeout(4);
} else if (AppConstants.platform === "linux") {
  requestLongerTimeout(2);
}

// Whether the tab key can move the focus. On macOS with full keyboard access
// disabled (which is default), this will be false. See `canTabMoveFocus`.
let gCanTabMoveFocus;
add_setup(async function () {
  gCanTabMoveFocus = await canTabMoveFocus();

  // Ensure the test remote settings server is set up. This test doesn't trigger
  // any suggestions but it enables Suggest, which will attempt to sync from
  // remote settings.
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
});

// When the user has already enabled the data-collection pref, the dialog should
// not appear.
add_task(async function dataCollectionAlreadyEnabled() {
  setDialogPrereqPrefs();
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);

  info("Calling maybeShowOnboardingDialog");
  let showed = await QuickSuggest.maybeShowOnboardingDialog();
  Assert.ok(!showed, "The dialog was not shown");

  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
});

// When the current tab is about:welcome, the dialog should not appear.
add_task(async function aboutWelcome() {
  setDialogPrereqPrefs();
  await BrowserTestUtils.withNewTab("about:welcome", async () => {
    info("Calling maybeShowOnboardingDialog");
    let showed = await QuickSuggest.maybeShowOnboardingDialog();
    Assert.ok(!showed, "The dialog was not shown");
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is inside the dialog.
add_task(async function escKey_focusInsideDialog() {
  await doDialogTest({
    callback: async () => {
      const { maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      const tabCount = gBrowser.tabs.length;
      Assert.ok(
        document.activeElement.classList.contains("dialogFrame"),
        "dialogFrame is focused in the browser window"
      );

      info("Close the dialog");
      EventUtils.synthesizeKey("KEY_Escape");

      await maybeShowPromise;

      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    onboardingDialogVersion: JSON.stringify({ version: 1 }),
    onboardingDialogChoice: "dismiss_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_2",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is outside the dialog.
add_task(async function escKey_focusOutsideDialog() {
  await doDialogTest({
    callback: async () => {
      const { maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      document.documentElement.focus();
      Assert.ok(
        !document.activeElement.classList.contains("dialogFrame"),
        "dialogFrame is not focused in the browser window"
      );

      info("Close the dialog");
      EventUtils.synthesizeKey("KEY_Escape");

      await maybeShowPromise;
    },
    onboardingDialogVersion: JSON.stringify({ version: 1 }),
    onboardingDialogChoice: "dismiss_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_2",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in when another
// dialog is queued and shown before the onboarding. This task dismisses the
// other dialog by pressing the Escape key.
add_task(async function escKey_queued_esc() {
  await doQueuedEscKeyTest("KEY_Escape");
});

// The Escape key should dismiss the dialog without opting in when another
// dialog is queued and shown before the onboarding. This task dismisses the
// other dialog by pressing the Enter key.
add_task(async function escKey_queued_enter() {
  await doQueuedEscKeyTest("KEY_Enter");
});

async function doQueuedEscKeyTest(otherDialogKey) {
  await doDialogTest({
    callback: async () => {
      // Create promises that will resolve when each dialog is opened.
      let uris = [OTHER_DIALOG_URI, QuickSuggest.ONBOARDING_URI];
      let [otherOpenedPromise, onboardingOpenedPromise] = uris.map(uri =>
        TestUtils.topicObserved(
          "subdialog-loaded",
          contentWin => contentWin.document.documentURI == uri
        ).then(async ([contentWin]) => {
          if (contentWin.document.readyState != "complete") {
            await BrowserTestUtils.waitForEvent(contentWin, "load");
          }
        })
      );

      info("Queuing dialogs for opening");
      let otherClosedPromise = gDialogBox.open(OTHER_DIALOG_URI);
      let onboardingClosedPromise = QuickSuggest.maybeShowOnboardingDialog();

      info("Waiting for the other dialog to open");
      await otherOpenedPromise;

      info(`Pressing ${otherDialogKey} and waiting for other dialog to close`);
      EventUtils.synthesizeKey(otherDialogKey);
      await otherClosedPromise;

      info("Waiting for the onboarding dialog to open");
      await onboardingOpenedPromise;

      info("Pressing Escape and waiting for onboarding dialog to close");
      EventUtils.synthesizeKey("KEY_Escape");
      await onboardingClosedPromise;
    },
    onboardingDialogVersion: JSON.stringify({ version: 1 }),
    onboardingDialogChoice: "dismiss_1",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_1",
      },
    ],
  });
}

// Tests `dismissed_other` by closing the dialog programmatically.
add_task(async function dismissed_other_on_introduction() {
  await doDialogTest({
    callback: async () => {
      const { maybeShowPromise } = await showOnboardingDialog();
      gDialogBox._dialog.close();
      await maybeShowPromise;
    },
    onboardingDialogVersion: JSON.stringify({ version: 1 }),
    onboardingDialogChoice: "dismiss_1",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_1",
      },
    ],
  });
});

// The default is to wait for no browser restarts to show the onboarding dialog
// on the first restart. This tests that we can override it by configuring the
// `showOnboardingDialogOnNthRestart`
add_task(async function nimbus_override_wait_after_n_restarts() {
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      // Wait for 1 browser restart
      quickSuggestShowOnboardingDialogAfterNRestarts: 1,
    },
    callback: async () => {
      let prefPromise = TestUtils.waitForPrefChange(
        "browser.urlbar.quicksuggest.showedOnboardingDialog",
        value => value === true
      ).then(() => info("Saw pref change"));

      // Simulate 2 restarts. this function is only called by BrowserGlue
      // on startup, the first restart would be where MR1 was shown then
      // we will show onboarding the 2nd restart after that.
      info("Simulating first restart");
      await QuickSuggest.maybeShowOnboardingDialog();

      info("Simulating second restart");
      const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        QuickSuggest.ONBOARDING_URI,
        { isSubDialog: true }
      );
      const maybeShowPromise = QuickSuggest.maybeShowOnboardingDialog();
      const win = await dialogPromise;
      if (win.document.readyState != "complete") {
        await BrowserTestUtils.waitForEvent(win, "load");
      }
      // Close dialog.
      EventUtils.synthesizeKey("KEY_Escape");

      info("Waiting for maybeShowPromise and pref change");
      await Promise.all([maybeShowPromise, prefPromise]);
    },
  });
});

add_task(async function nimbus_skip_onboarding_dialog() {
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestShouldShowOnboardingDialog: false,
    },
    callback: async () => {
      // Simulate 3 restarts.
      for (let i = 0; i < 3; i++) {
        info(`Simulating restart ${i + 1}`);
        await QuickSuggest.maybeShowOnboardingDialog();
      }
      Assert.ok(
        !Services.prefs.getBoolPref(
          "browser.urlbar.quicksuggest.showedOnboardingDialog",
          false
        ),
        "The showed onboarding dialog pref should not be set"
      );
    },
  });
});

add_task(async function nimbus_exposure_event() {
  const testData = [
    {
      experimentType: "modal",
      expectedRecorded: true,
    },
    {
      experimentType: "best-match",
      expectedRecorded: false,
    },
    {
      expectedRecorded: false,
    },
  ];

  for (const { experimentType, expectedRecorded } of testData) {
    info(`Nimbus exposure event test for type:[${experimentType}]`);
    UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

    await QuickSuggestTestUtils.clearExposureEvent();

    await QuickSuggestTestUtils.withExperiment({
      valueOverrides: {
        quickSuggestScenario: "online",
        experimentType,
      },
      callback: async () => {
        info("Calling showOnboardingDialog");
        const { maybeShowPromise } = await showOnboardingDialog();
        EventUtils.synthesizeKey("KEY_Escape");
        await maybeShowPromise;

        info("Check the event");
        await QuickSuggestTestUtils.assertExposureEvent(expectedRecorded);
      },
    });
  }
});

const LOGO_TYPE = {
  FIREFOX: 1,
  MAGGLASS: 2,
  ANIMATION_MAGGLASS: 3,
};

const VARIATION_TEST_DATA = [
  {
    name: "A",
    introductionSection: {
      logoType: LOGO_TYPE.ANIMATION_MAGGLASS,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-1",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
      defaultFocusOrder: [
        "onboardingNext",
        "onboardingClose",
        "onboardingDialog",
        "onboardingNext",
      ],
      actions: ["onboardingClose", "onboardingNext"],
    },
    mainSection: {
      logoType: LOGO_TYPE.MAGGLASS,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-1",
        "main-description": "firefox-suggest-onboarding-main-description-1",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-1",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-1",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
      defaultFocusOrder: [
        "onboardingNext",
        "onboardingAccept",
        "onboardingLearnMore",
        "onboardingReject",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingAccept",
      ],
      acceptFocusOrder: [
        "onboardingAccept",
        "onboardingLearnMore",
        "onboardingSubmit",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingAccept",
      ],
      rejectFocusOrder: [
        "onboardingReject",
        "onboardingSubmit",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingLearnMore",
        "onboardingReject",
      ],
      actions: [
        "onboardingAccept",
        "onboardingReject",
        "onboardingSkipLink",
        "onboardingLearnMore",
      ],
    },
  },
  {
    // We don't need to test the focus order and actions because the layout of
    // variation B-H is as same as A.
    name: "B",
    introductionSection: {
      logoType: LOGO_TYPE.ANIMATION_MAGGLASS,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.MAGGLASS,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-2",
        "main-description": "firefox-suggest-onboarding-main-description-2",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-1",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-1",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "C",
    introductionSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-3",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-3",
        "main-description": "firefox-suggest-onboarding-main-description-3",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-1",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-1",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "D",
    introductionSection: {
      logoType: LOGO_TYPE.ANIMATION_MAGGLASS,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-4",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.MAGGLASS,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-4",
        "main-description": "firefox-suggest-onboarding-main-description-4",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-2",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-2",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "E",
    introductionSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-5",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-5",
        "main-description": "firefox-suggest-onboarding-main-description-5",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-2",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-2",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "F",
    introductionSection: {
      logoType: LOGO_TYPE.ANIMATION_MAGGLASS,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-2",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-6",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.MAGGLASS,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-6",
        "main-description": "firefox-suggest-onboarding-main-description-6",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-2",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-2",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "G",
    introductionSection: {
      logoType: LOGO_TYPE.ANIMATION_MAGGLASS,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-7",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.MAGGLASS,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-7",
        "main-description": "firefox-suggest-onboarding-main-description-7",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-2",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-2",
      },
      visibility: {
        "#main-privacy-first": true,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "H",
    introductionSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
        "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": false,
        ".description-section": false,
        ".pager": true,
      },
    },
    mainSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-8",
        "main-description": "firefox-suggest-onboarding-main-description-8",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-1",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-1",
      },
      visibility: {
        "#main-privacy-first": false,
        ".description-section #onboardingLearnMore": false,
        ".accept #onboardingLearnMore": true,
        ".pager": true,
      },
    },
  },
  {
    name: "100-A",
    introductionSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        onboardingNext: "firefox-suggest-onboarding-introduction-next-button-3",
        "introduction-title": "firefox-suggest-onboarding-main-title-9",
      },
      visibility: {
        "#onboardingLearnMoreOnIntroduction": true,
        ".description-section": true,
        ".pager": true,
      },
      defaultFocusOrder: [
        "onboardingNext",
        "onboardingLearnMoreOnIntroduction",
        "onboardingClose",
        "onboardingDialog",
        "onboardingNext",
      ],
      actions: [
        "onboardingClose",
        "onboardingNext",
        "onboardingLearnMoreOnIntroduction",
      ],
    },
    mainSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-9",
        "main-description": "firefox-suggest-onboarding-main-description-9",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label-2",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-3",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label-2",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-3",
      },
      visibility: {
        "#main-privacy-first": true,
        ".description-section #onboardingLearnMore": true,
        ".accept #onboardingLearnMore": false,
        ".pager": false,
      },
      defaultFocusOrder: [
        "onboardingNext",
        "onboardingLearnMore",
        "onboardingAccept",
        "onboardingReject",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingLearnMore",
      ],
      acceptFocusOrder: [
        "onboardingAccept",
        "onboardingSubmit",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingLearnMore",
        "onboardingAccept",
      ],
      rejectFocusOrder: [
        "onboardingReject",
        "onboardingSubmit",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingLearnMore",
        "onboardingReject",
      ],
      actions: [
        "onboardingAccept",
        "onboardingReject",
        "onboardingSkipLink",
        "onboardingLearnMore",
      ],
    },
  },
  {
    name: "100-B",
    mainSection: {
      logoType: LOGO_TYPE.FIREFOX,
      l10n: {
        "main-title": "firefox-suggest-onboarding-main-title-9",
        "main-description": "firefox-suggest-onboarding-main-description-9",
        "main-accept-option-label":
          "firefox-suggest-onboarding-main-accept-option-label-2",
        "main-accept-option-description":
          "firefox-suggest-onboarding-main-accept-option-description-3",
        "main-reject-option-label":
          "firefox-suggest-onboarding-main-reject-option-label-2",
        "main-reject-option-description":
          "firefox-suggest-onboarding-main-reject-option-description-3",
      },
      visibility: {
        "#main-privacy-first": true,
        ".description-section #onboardingLearnMore": true,
        ".accept #onboardingLearnMore": false,
        ".pager": false,
      },
      // Layout of 100-B is same as 100-A, but since there is no the introduction
      // pane, only the default focus order on the main pane is a bit diffrence.
      defaultFocusOrder: [
        "onboardingLearnMore",
        "onboardingAccept",
        "onboardingReject",
        "onboardingSkipLink",
        "onboardingDialog",
        "onboardingLearnMore",
      ],
    },
  },
];

/**
 * This test checks for differences due to variations in logo type, l10n text,
 * element visibility, order of focus, actions, etc. The designation is on
 * VARIATION_TEST_DATA. The items that can be specified are below.
 *
 * name: Specify the variation name.
 *
 * The following items are specified for each section.
 * (introductionSection, mainSection).
 *
 * logoType:
 *   Specify the expected logo type. Please refer to LOGO_TYPE about the type.
 *
 * l10n:
 *   Specify the expected l10n id applied to elements.
 *
 * visibility:
 *   Specify the expected visibility of elements. The way to specify the element
 *   is using selector.
 *
 * defaultFocusOrder:
 *   Specify the expected focus order right after the section is appeared. The
 *   way to specify the element is using id.
 *
 * acceptFocusOrder:
 *   Specify the expected focus order after selecting accept option.
 *
 * rejectFocusOrder:
 *   Specify the expected focus order after selecting reject option.
 *
 * actions:
 *   Specify the action we want to verify such as clicking the  close button. The
 *   available actions are below.
 * - onboardingClose:
 *     Action of the close button “x” by mouse/keyboard.
 * - onboardingNext:
 *     Action of the next button that transits from the introduction section to
 *     the main section by mouse/keyboard.
 * - onboardingAccept:
 *      Action of the submit button by mouse/keyboard after selecting accept
 *      option by mouse/keyboard.
 * - onboardingReject:
 *      Action of the submit button by mouse/keyboard after selecting reject
 *      option by mouse/keyboard.
 * - onboardingSkipLink:
 *      Action of the skip link by mouse/keyboard.
 * - onboardingLearnMore:
 *      Action of the learn more link by mouse/keyboard.
 * - onboardingLearnMoreOnIntroduction:
 *      Action of the learn more link on the introduction section by
 *      mouse/keyboard.
 */
add_task(async function variation_test() {
  for (const variation of VARIATION_TEST_DATA) {
    info(`Test for variation [${variation.name}]`);

    info("Do layout test");
    await doLayoutTest(variation);

    for (const action of variation.introductionSection?.actions || []) {
      info(
        `${action} test on the introduction section for variation [${variation.name}]`
      );
      await this[action](variation);
    }

    for (const action of variation.mainSection?.actions || []) {
      info(
        `${action} test on the main section for variation [${variation.name}]`
      );
      await this[action](variation, !!variation.introductionSection);
    }
  }
});

async function doLayoutTest(variation) {
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestOnboardingDialogVariation: variation.name,
    },
    callback: async () => {
      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog();

      const introductionSection = win.document.getElementById(
        "introduction-section"
      );
      const mainSection = win.document.getElementById("main-section");

      if (variation.introductionSection) {
        info("Check the section visibility");
        Assert.ok(BrowserTestUtils.isVisible(introductionSection));
        Assert.ok(BrowserTestUtils.isHidden(mainSection));

        info("Check the introduction section");
        await assertSection(introductionSection, variation.introductionSection);

        info("Transition to the main section");
        win.document.getElementById("onboardingNext").click();
        await BrowserTestUtils.waitForCondition(
          () =>
            BrowserTestUtils.isHidden(introductionSection) &&
            BrowserTestUtils.isVisible(mainSection)
        );
      } else {
        info("Check the section visibility");
        Assert.ok(BrowserTestUtils.isHidden(introductionSection));
        Assert.ok(BrowserTestUtils.isVisible(mainSection));
      }

      info("Check the main section");
      await assertSection(mainSection, variation.mainSection);

      info("Close the dialog");
      EventUtils.synthesizeKey("KEY_Escape", {}, win);
      await maybeShowPromise;
    },
  });
}

async function assertSection(sectionElement, expectedSection) {
  info("Check the logo");
  assertLogo(sectionElement, expectedSection.logoType);

  info("Check the l10n");
  assertL10N(sectionElement, expectedSection.l10n);

  info("Check the visibility");
  assertVisibility(sectionElement, expectedSection.visibility);

  if (!gCanTabMoveFocus) {
    Assert.ok(true, "Tab key can't move focus, skipping test for focus order");
    return;
  }

  if (expectedSection.defaultFocusOrder) {
    info("Check the default focus order");
    assertFocusOrder(sectionElement, expectedSection.defaultFocusOrder);
  }

  if (expectedSection.acceptFocusOrder) {
    info("Check the focus order after selecting accept option");
    sectionElement.querySelector("#onboardingAccept").focus();
    EventUtils.synthesizeKey("VK_SPACE", {}, sectionElement.ownerGlobal);
    assertFocusOrder(sectionElement, expectedSection.acceptFocusOrder);
  }

  if (expectedSection.rejectFocusOrder) {
    info("Check the focus order after selecting reject option");
    sectionElement.querySelector("#onboardingReject").focus();
    EventUtils.synthesizeKey("VK_SPACE", {}, sectionElement.ownerGlobal);
    assertFocusOrder(sectionElement, expectedSection.rejectFocusOrder);
  }
}

function assertLogo(sectionElement, expectedLogoType) {
  let expectedLogoImage;
  switch (expectedLogoType) {
    case LOGO_TYPE.FIREFOX: {
      expectedLogoImage = 'url("chrome://branding/content/about-logo.svg")';
      break;
    }
    case LOGO_TYPE.MAGGLASS: {
      expectedLogoImage =
        'url("chrome://browser/content/urlbar/quicksuggestOnboarding_magglass.svg")';
      break;
    }
    case LOGO_TYPE.ANIMATION_MAGGLASS: {
      const mediaQuery = sectionElement.ownerGlobal.matchMedia(
        "(prefers-reduced-motion: no-preference)"
      );
      expectedLogoImage = mediaQuery.matches
        ? 'url("chrome://browser/content/urlbar/quicksuggestOnboarding_magglass_animation.svg")'
        : 'url("chrome://browser/content/urlbar/quicksuggestOnboarding_magglass.svg")';
      break;
    }
    default: {
      Assert.ok(false, `Unexpected image type ${expectedLogoType}`);
      break;
    }
  }

  const logo = sectionElement.querySelector(".logo");
  Assert.ok(BrowserTestUtils.isVisible(logo));
  const logoImage =
    sectionElement.ownerGlobal.getComputedStyle(logo).backgroundImage;
  Assert.equal(logoImage, expectedLogoImage);
}

function assertL10N(sectionElement, expectedL10N) {
  for (const [id, l10n] of Object.entries(expectedL10N)) {
    const element = sectionElement.querySelector("#" + id);
    Assert.equal(element.getAttribute("data-l10n-id"), l10n);
  }
}

function assertVisibility(sectionElement, expectedVisibility) {
  for (const [selector, visibility] of Object.entries(expectedVisibility)) {
    const element = sectionElement.querySelector(selector);
    if (visibility) {
      Assert.ok(BrowserTestUtils.isVisible(element));
    } else {
      if (!element) {
        Assert.ok(true);
        return;
      }
      Assert.ok(BrowserTestUtils.isHidden(element));
    }
  }
}

function assertFocusOrder(sectionElement, expectedFocusOrder) {
  const win = sectionElement.ownerGlobal;

  // Check initial active element.
  Assert.equal(win.document.activeElement.id, expectedFocusOrder[0]);

  for (const next of expectedFocusOrder.slice(1)) {
    EventUtils.synthesizeKey("KEY_Tab", {}, win);
    Assert.equal(win.document.activeElement.id, next);
  }
}

async function onboardingClose(variation) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the close button");
      const closeButton = win.document.getElementById("onboardingClose");
      Assert.ok(BrowserTestUtils.isVisible(closeButton));
      Assert.equal(closeButton.getAttribute("title"), "Close");

      info("Commit the close button");
      userAction(closeButton);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;
    },
    variation,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    onboardingDialogChoice: "close_1",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "close_1",
      },
    ],
  });
}

async function onboardingNext(variation) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the next button");
      const nextButton = win.document.getElementById("onboardingNext");
      Assert.ok(BrowserTestUtils.isVisible(nextButton));

      info("Commit the next button");
      userAction(nextButton);

      const introductionSection = win.document.getElementById(
        "introduction-section"
      );
      const mainSection = win.document.getElementById("main-section");
      await BrowserTestUtils.waitForCondition(
        () =>
          BrowserTestUtils.isHidden(introductionSection) &&
          BrowserTestUtils.isVisible(mainSection),
        "Wait for the transition"
      );

      info("Exit");
      EventUtils.synthesizeKey("KEY_Escape", {}, win);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;
    },
    variation,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    onboardingDialogChoice: "dismiss_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_2",
      },
    ],
  });
}

async function onboardingAccept(variation, skipIntroduction) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the accept option and submit button");
      const acceptOption = win.document.getElementById("onboardingAccept");
      const submitButton = win.document.getElementById("onboardingSubmit");
      Assert.ok(acceptOption);
      Assert.ok(submitButton.disabled);

      info("Select the accept option");
      userAction(acceptOption);

      info("Commit the submit button");
      Assert.ok(!submitButton.disabled);
      userAction(submitButton);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;
    },
    variation,
    skipIntroduction,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    onboardingDialogChoice: "accept_2",
    expectedUserBranchPrefs: {
      "quicksuggest.onboardingDialogVersion": JSON.stringify({ version: 1 }),
      "quicksuggest.dataCollection.enabled": true,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "enabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept_2",
      },
    ],
  });
}

async function onboardingReject(variation, skipIntroduction) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the reject option and submit button");
      const rejectOption = win.document.getElementById("onboardingReject");
      const submitButton = win.document.getElementById("onboardingSubmit");
      Assert.ok(rejectOption);
      Assert.ok(submitButton.disabled);

      info("Select the reject option");
      userAction(rejectOption);

      info("Commit the submit button");
      Assert.ok(!submitButton.disabled);
      userAction(submitButton);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;
    },
    variation,
    skipIntroduction,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    onboardingDialogChoice: "reject_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "reject_2",
      },
    ],
  });
}

async function onboardingSkipLink(variation, skipIntroduction) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the skip link");
      const skipLink = win.document.getElementById("onboardingSkipLink");
      Assert.ok(BrowserTestUtils.isVisible(skipLink));

      info("Commit the skip link");
      const tabCount = gBrowser.tabs.length;
      userAction(skipLink);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

      info("Check the current tab status");
      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    variation,
    skipIntroduction,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    onboardingDialogChoice: "not_now_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now_2",
      },
    ],
  });
}

async function onboardingLearnMore(variation, skipIntroduction) {
  await doLearnMoreTest(
    variation,
    skipIntroduction,
    "onboardingLearnMore",
    "learn_more_2"
  );
}

async function onboardingLearnMoreOnIntroduction(variation, skipIntroduction) {
  await doLearnMoreTest(
    variation,
    skipIntroduction,
    "onboardingLearnMoreOnIntroduction",
    "learn_more_1"
  );
}

async function doLearnMoreTest(variation, skipIntroduction, target, telemetry) {
  await doActionTest({
    callback: async (win, userAction, maybeShowPromise) => {
      info("Check the status of the learn more link");
      const learnMoreLink = win.document.getElementById(target);
      Assert.ok(BrowserTestUtils.isVisible(learnMoreLink));

      info("Commit the learn more link");
      const loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        QuickSuggest.HELP_URL
      ).then(tab => {
        info("Saw new tab");
        return tab;
      });
      userAction(learnMoreLink);

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

      info("Waiting for new tab");
      let tab = await loadPromise;

      info("Check the current tab status");
      Assert.equal(gBrowser.selectedTab, tab, "Current tab is the new tab");
      Assert.equal(
        gBrowser.currentURI.spec,
        QuickSuggest.HELP_URL,
        "Current tab is the support page"
      );
      BrowserTestUtils.removeTab(tab);
    },
    variation,
    skipIntroduction,
    onboardingDialogChoice: telemetry,
    onboardingDialogVersion: JSON.stringify({
      version: 1,
      variation: variation.name.toLowerCase(),
    }),
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: telemetry,
      },
    ],
  });
}

async function doActionTest({
  variation,
  skipIntroduction,
  callback,
  onboardingDialogVersion,
  onboardingDialogChoice,
  expectedUserBranchPrefs,
  telemetryEvents,
}) {
  const userClick = target => {
    info("Click on the target");
    target.click();
  };
  const userEnter = target => {
    target.focus();
    if (target.type === "radio") {
      info("Space on the target");
      EventUtils.synthesizeKey("VK_SPACE", {}, target.ownerGlobal);
    } else {
      info("Enter on the target");
      EventUtils.synthesizeKey("KEY_Enter", {}, target.ownerGlobal);
    }
  };

  for (const userAction of [userClick, userEnter]) {
    UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

    await QuickSuggestTestUtils.withExperiment({
      valueOverrides: {
        quickSuggestScenario: "online",
        quickSuggestOnboardingDialogVariation: variation.name,
      },
      callback: async () => {
        await doDialogTest({
          callback: async () => {
            info("Calling showOnboardingDialog");
            const { win, maybeShowPromise } = await showOnboardingDialog({
              skipIntroduction,
            });

            await callback(win, userAction, maybeShowPromise);
          },
          onboardingDialogVersion,
          onboardingDialogChoice,
          expectedUserBranchPrefs,
          telemetryEvents,
        });
      },
    });
  }
}

async function doDialogTest({
  callback,
  onboardingDialogVersion,
  onboardingDialogChoice,
  telemetryEvents,
  expectedUserBranchPrefs,
}) {
  setDialogPrereqPrefs();

  // Set initial prefs on the default branch.
  let initialDefaultBranch = OFFLINE_DEFAULT_PREFS;
  let originalDefaultBranch = {};
  for (let [name, value] of Object.entries(initialDefaultBranch)) {
    originalDefaultBranch = gDefaultBranch.getBoolPref(name);
    gDefaultBranch.setBoolPref(name, value);
    gUserBranch.clearUserPref(name);
  }

  // Setting the prefs just now triggered telemetry events, so clear them
  // before calling the callback.
  Services.telemetry.clearEvents();

  // Call the callback, which should trigger the dialog and interact with it.
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await callback();
  });

  // Now check all pref values on the default and user branches.
  for (let [name, value] of Object.entries(initialDefaultBranch)) {
    Assert.equal(
      gDefaultBranch.getBoolPref(name),
      value,
      "Default-branch value for pref did not change after modal: " + name
    );

    let effectiveValue;
    if (name in expectedUserBranchPrefs) {
      effectiveValue = expectedUserBranchPrefs[name];
      Assert.equal(
        gUserBranch.getBoolPref(name),
        effectiveValue,
        "User-branch value for pref has expected value: " + name
      );
    } else {
      effectiveValue = value;
      Assert.ok(
        !gUserBranch.prefHasUserValue(name),
        "User-branch value for pref does not exist: " + name
      );
    }

    // For good measure, check the value returned by UrlbarPrefs.
    Assert.equal(
      UrlbarPrefs.get(name),
      effectiveValue,
      "Effective value for pref is correct: " + name
    );
  }

  Assert.equal(
    UrlbarPrefs.get("quicksuggest.onboardingDialogVersion"),
    onboardingDialogVersion,
    "onboardingDialogVersion"
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.onboardingDialogChoice"),
    onboardingDialogChoice,
    "onboardingDialogChoice"
  );
  Assert.equal(
    TelemetryEnvironment.currentEnvironment.settings.userPrefs[
      "browser.urlbar.quicksuggest.onboardingDialogChoice"
    ],
    onboardingDialogChoice,
    "onboardingDialogChoice is correct in TelemetryEnvironment"
  );

  QuickSuggestTestUtils.assertEvents(telemetryEvents);

  Assert.ok(
    UrlbarPrefs.get("quicksuggest.showedOnboardingDialog"),
    "quicksuggest.showedOnboardingDialog is true after showing dialog"
  );

  // Clean up.
  for (let [name, value] of Object.entries(originalDefaultBranch)) {
    gDefaultBranch.setBoolPref(name, value);
  }
  for (let name of Object.keys(expectedUserBranchPrefs)) {
    gUserBranch.clearUserPref(name);
  }
}

/**
 * Show onbaording dialog.
 *
 * @param {object} [options]
 *   The object options.
 * @param {boolean} [options.skipIntroduction]
 *   If true, return dialog with skipping the introduction section.
 * @returns {{ window, maybeShowPromise: Promise }}
 *   win: window object of the dialog.
 *   maybeShowPromise: Promise of QuickSuggest.maybeShowOnboardingDialog().
 */
async function showOnboardingDialog({ skipIntroduction } = {}) {
  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
    null,
    QuickSuggest.ONBOARDING_URI,
    { isSubDialog: true }
  );

  const maybeShowPromise = QuickSuggest.maybeShowOnboardingDialog();

  const win = await dialogPromise;
  if (win.document.readyState != "complete") {
    await BrowserTestUtils.waitForEvent(win, "load");
  }

  // Wait until all listers on onboarding dialog are ready.
  await window._quicksuggestOnboardingReady;

  if (!skipIntroduction) {
    return { win, maybeShowPromise };
  }

  // Trigger the transition by pressing Enter on the Next button.
  EventUtils.synthesizeKey("KEY_Enter");

  const introductionSection = win.document.getElementById(
    "introduction-section"
  );
  const mainSection = win.document.getElementById("main-section");

  await BrowserTestUtils.waitForCondition(
    () =>
      BrowserTestUtils.isHidden(introductionSection) &&
      BrowserTestUtils.isVisible(mainSection)
  );

  return { win, maybeShowPromise };
}

/**
 * Sets all the required prefs for showing the onboarding dialog except for the
 * prefs that are set when the dialog is accepted.
 */
function setDialogPrereqPrefs() {
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", true);
  UrlbarPrefs.set("quicksuggest.showedOnboardingDialog", false);
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
  if (AppConstants.platform != "macosx") {
    return true;
  }

  let canMove = false;
  await doDialogTest({
    callback: async () => {
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      let doc = win.document;
      doc.getElementById("onboardingAccept").focus();
      EventUtils.synthesizeKey("KEY_Tab");

      // Whether or not the focus can move to the link.
      canMove = doc.activeElement.id === "onboardingLearnMore";

      EventUtils.synthesizeKey("KEY_Escape");
      await maybeShowPromise;
    },
    onboardingDialogVersion: JSON.stringify({ version: 1 }),
    onboardingDialogChoice: "dismiss_2",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismiss_2",
      },
    ],
  });

  return canMove;
}

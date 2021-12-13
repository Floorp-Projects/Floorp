/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the buttons in the onboarding dialog for quick suggest/Firefox Suggest.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const ONBOARDING_URI =
  "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml";

const OTHER_DIALOG_URI = getRootDirectory(gTestPath) + "subdialog.xhtml";

// Default-branch pref values in the offline scenario.
const OFFLINE_DEFAULT_PREFS = {
  "suggest.quicksuggest.nonsponsored": true,
  "suggest.quicksuggest.sponsored": true,
  "quicksuggest.dataCollection.enabled": false,
};

let gDefaultBranch = Services.prefs.getDefaultBranch("browser.urlbar.");
let gUserBranch = Services.prefs.getBranch("browser.urlbar.");

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

// When the user has already enabled the data-collection pref, the dialog should
// not appear.
add_task(async function onboardingShouldNotAppear() {
  setDialogPrereqPrefs();

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);

  info("Calling maybeShowOnboardingDialog");
  let showed = await UrlbarQuickSuggest.maybeShowOnboardingDialog();
  Assert.ok(!showed, "The dialog was not shown");

  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
});

// When the accept button is clicked, the user should be opted in.
add_task(async function accept() {
  await doDialogTest({
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
    onboardingDialogChoice: "accept",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": true,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "enabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept",
      },
    ],
  });
});

// When the Not Now link is clicked, the user should remain opted out.
add_task(async function notNow() {
  await doDialogTest({
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
    onboardingDialogChoice: "not_now_link",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now_link",
      },
    ],
  });
});

// When the settings button is clicked, the user should remain opted out and
// about:preferences should load.
add_task(async function settings() {
  await doDialogTest({
    callback: async () => {
      let dialogPromise = openDialog("extra1");

      // about:preferences will load in the current tab since it's about:blank.
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
    onboardingDialogChoice: "settings",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
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
    callback: async () => {
      let dialogPromise = openDialog("onboardingLearnMore");
      let loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        QuickSuggestTestUtils.LEARN_MORE_URL
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
        QuickSuggestTestUtils.LEARN_MORE_URL,
        "Current tab is the support page"
      );
      BrowserTestUtils.removeTab(tab);
    },
    onboardingDialogChoice: "learn_more",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "learn_more",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is inside the dialog.
add_task(async function escKey_focusInsideDialog() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingAcceptButton",
    callback: async () => {
      let tabCount = gBrowser.tabs.length;
      Assert.ok(
        document.activeElement.classList.contains("dialogFrame"),
        "dialogFrame is focused in the browser window"
      );
      EventUtils.synthesizeKey("KEY_Escape");
      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    onboardingDialogChoice: "dismissed_escape_key",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismissed_escape_key",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is outside the dialog.
add_task(async function escKey_focusOutsideDialog() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingAcceptButton",
    callback: async () => {
      document.documentElement.focus();
      Assert.ok(
        !document.activeElement.classList.contains("dialogFrame"),
        "dialogFrame is not focused in the browser window"
      );
      EventUtils.synthesizeKey("KEY_Escape");
    },
    onboardingDialogChoice: "dismissed_escape_key",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismissed_escape_key",
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
      let uris = [OTHER_DIALOG_URI, ONBOARDING_URI];
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
      let onboardingClosedPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();

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
    onboardingDialogChoice: "dismissed_escape_key",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismissed_escape_key",
      },
    ],
  });
}

// Tests `dismissed_other` by closing the dialog programmatically.
add_task(async function dismissed_other() {
  await doDialogTest({
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        ONBOARDING_URI,
        { isSubDialog: true }
      );

      let maybeShowPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();

      let win = await dialogPromise;
      if (win.document.readyState != "complete") {
        await BrowserTestUtils.waitForEvent(win, "load");
      }

      gDialogBox._dialog.close();
      await maybeShowPromise;
    },
    onboardingDialogChoice: "dismissed_other",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismissed_other",
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
    callback: async () => {
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "accept",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": true,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "enabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
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
    onboardingDialogChoice: "settings",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
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
    callback: async () => {
      let loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        QuickSuggestTestUtils.LEARN_MORE_URL
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
        QuickSuggestTestUtils.LEARN_MORE_URL,
        "Current tab is the support page"
      );
      BrowserTestUtils.removeTab(tab);
    },
    onboardingDialogChoice: "learn_more",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
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
    onboardingDialogChoice: "not_now_link",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "not_now_link",
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
    callback: async () => {
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "accept",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": true,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "enabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "accept",
      },
    ],
  });
});

async function doDialogTest({
  onboardingDialogChoice,
  telemetryEvents,
  callback,
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

  // Even with the `clearEvents` call above, events related to remote settings
  // can occur in TV tests during the callback, so pass a filter arg to make
  // sure we get only the events we're interested in.
  TelemetryTestUtils.assertEvents(telemetryEvents, {
    category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
  });

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

// Whether the tab key can move the focus. On macOS with full keyboard access
// disabled (which is default), this will be false. See `canTabMoveFocus`.
let gCanTabMoveFocus;

async function doFocusTest({
  tabKeyRepeat,
  expectedFocusID,
  onboardingDialogChoice,
  telemetryEvents,
  callback,
  expectedUserBranchPrefs,
}) {
  if (gCanTabMoveFocus === undefined) {
    gCanTabMoveFocus = await canTabMoveFocus();
  }
  if (!gCanTabMoveFocus && tabKeyRepeat) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  await doDialogTest({
    onboardingDialogChoice,
    expectedUserBranchPrefs,
    telemetryEvents,
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        ONBOARDING_URI,
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
        gCanTabMoveFocus
          ? "onboardingAcceptButton"
          : "quicksuggestOnboardingDialogWindow",
        "Accept button is focused initially"
      );

      if (tabKeyRepeat) {
        EventUtils.synthesizeKey("KEY_Tab", { repeat: tabKeyRepeat });
      }

      if (!gCanTabMoveFocus) {
        expectedFocusID = "quicksuggestOnboardingDialogWindow";
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

/**
 * Sets all the required prefs for showing the onboarding dialog except for the
 * prefs that are set when the dialog is accepted.
 */
function setDialogPrereqPrefs() {
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", true);
  UrlbarPrefs.set("quicksuggest.showedOnboardingDialog", false);
}

async function openDialog(button = undefined) {
  await BrowserTestUtils.promiseAlertDialog(button, ONBOARDING_URI, {
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
  if (AppConstants.platform != "macosx") {
    return true;
  }

  let canMove = false;
  await doDialogTest({
    callback: async () => {
      let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        ONBOARDING_URI,
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
    onboardingDialogChoice: "dismissed_escape_key",
    expectedUserBranchPrefs: {
      "quicksuggest.dataCollection.enabled": false,
    },
    telemetryEvents: [
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "data_collect_toggled",
        object: "disabled",
      },
      {
        category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
        method: "opt_in_dialog",
        object: "dismissed_escape_key",
      },
    ],
  });
  return canMove;
}

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

// Whether the tab key can move the focus. On macOS with full keyboard access
// disabled (which is default), this will be false. See `canTabMoveFocus`.
let gCanTabMoveFocus;
add_task(async function setup() {
  gCanTabMoveFocus = await canTabMoveFocus();
});

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

// Test for transition from introduction to main.
add_task(async function transition() {
  await doTransitionTest({
    trigger: win => {
      info("Find next button");
      const onboardingNext = win.document.getElementById("onboardingNext");
      info("Click to transition");
      onboardingNext.click();
    },
  });
});

// Test for transition from introduction to main by enter key.
add_task(async function transition_by_enter() {
  await doTransitionTest({
    trigger: () => {
      info("Enter to transition");
      EventUtils.synthesizeKey("KEY_Enter");
    },
  });
});

// When the accept option is selected, the user should be opted in.
add_task(async function accept() {
  await doDialogTest({
    callback: async () => {
      let tabCount = gBrowser.tabs.length;

      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      info("Select accept option");
      win.document.getElementById("onboardingAccept").click();

      info("Submit");
      win.document.getElementById("onboardingSubmit").click();

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    onboardingDialogChoice: "accept_2",
    expectedUserBranchPrefs: {
      "quicksuggest.onboardingDialogVersion": JSON.stringify({ version: 1 }),
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
        object: "accept_2",
      },
    ],
  });
});

// When the reject option is selected, the user should be opted out.
add_task(async function reject() {
  await doDialogTest({
    callback: async () => {
      let tabCount = gBrowser.tabs.length;

      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      info("Select reject option");
      win.document.getElementById("onboardingReject").click();

      info("Submit");
      win.document.getElementById("onboardingSubmit").click();

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    onboardingDialogChoice: "reject_2",
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
        object: "reject_2",
      },
    ],
  });
});

// When the "X" close button is clicked, the user should remain opted out.
add_task(async function close() {
  await doDialogTest({
    callback: async () => {
      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog();

      info("Check the visibility of the close button");
      const closeButton = win.document.getElementById("onboardingClose");
      Assert.ok(BrowserTestUtils.is_visible(closeButton));

      info("Click on the close button");
      closeButton.click();

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;
    },
    onboardingDialogChoice: "close_1",
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
        object: "close_1",
      },
    ],
  });
});

// When the Not Now link is clicked, the user should remain opted out.
add_task(async function skip() {
  await doDialogTest({
    callback: async () => {
      let tabCount = gBrowser.tabs.length;

      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      info("Click on not now link");
      win.document.getElementById("onboardingSkipLink").click();

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

      Assert.equal(
        gBrowser.currentURI.spec,
        "about:blank",
        "Nothing loaded in the current tab"
      );
      Assert.equal(gBrowser.tabs.length, tabCount, "No news tabs were opened");
    },
    onboardingDialogChoice: "not_now_2",
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
        object: "not_now_2",
      },
    ],
  });
});

// When Learn More is clicked, the user should remain opted out and the support
// URL should open in a new tab.
add_task(async function learnMore() {
  await doDialogTest({
    callback: async () => {
      let loadPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        QuickSuggestTestUtils.LEARN_MORE_URL
      ).then(tab => {
        info("Saw new tab");
        return tab;
      });

      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: true,
      });

      info("Click on learn more link");
      win.document.getElementById("onboardingLearnMore").click();

      info("Waiting for maybeShowOnboardingDialog to finish");
      await maybeShowPromise;

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
    onboardingDialogChoice: "learn_more_2",
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
        object: "learn_more_2",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is inside the dialog.
add_task(async function escKey_focusInsideDialog() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingNext",
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
    onboardingDialogChoice: "dismiss_2",
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
        object: "dismiss_2",
      },
    ],
  });
});

// The Escape key should dismiss the dialog without opting in. This task tests
// when Escape is pressed while the focus is outside the dialog.
add_task(async function escKey_focusOutsideDialog() {
  await doFocusTest({
    tabKeyRepeat: 0,
    expectedFocusID: "onboardingNext",
    callback: async () => {
      document.documentElement.focus();
      Assert.ok(
        !document.activeElement.classList.contains("dialogFrame"),
        "dialogFrame is not focused in the browser window"
      );
      EventUtils.synthesizeKey("KEY_Escape");
    },
    onboardingDialogChoice: "dismiss_2",
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
    onboardingDialogChoice: "dismiss_1",
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
        object: "dismiss_1",
      },
    ],
  });
}

// Tests `dismissed_other` by closing the dialog programmatically.
add_task(async function dismissed_other_on_introduction() {
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
    onboardingDialogChoice: "dismiss_1",
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
        object: "dismiss_1",
      },
    ],
  });
});

// Tests tabbing through the dialog on introduction section.
add_task(async function focus_order_on_introduction() {
  if (!gCanTabMoveFocus) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  setDialogPrereqPrefs();

  info("Calling showOnboardingDialog");
  const { win, maybeShowPromise } = await showOnboardingDialog();

  info("Check the first focus");
  Assert.equal(win.document.activeElement.id, "onboardingNext");

  const order = ["onboardingClose", "onboardingNext", "onboardingClose"];
  for (const next of order) {
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(win.document.activeElement.id, next);
  }

  EventUtils.synthesizeKey("KEY_Escape");

  info("Waiting for maybeShowOnboardingDialog to finish");
  await maybeShowPromise;
});

// Tests tabbing through the dialog on main section.
add_task(async function focus_order_on_main() {
  if (!gCanTabMoveFocus) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  setDialogPrereqPrefs();

  info("Calling showOnboardingDialog");
  const { win, maybeShowPromise } = await showOnboardingDialog({
    skipIntroduction: true,
  });

  const order = [
    "onboardingAccept",
    "onboardingLearnMore",
    "onboardingReject",
    "onboardingSkipLink",
    "onboardingAccept",
  ];

  for (const next of order) {
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(win.document.activeElement.id, next);
  }

  EventUtils.synthesizeKey("KEY_Escape");

  info("Waiting for maybeShowOnboardingDialog to finish");
  await maybeShowPromise;
});

// Tests tabbing through the dialog after selecting accept option.
add_task(async function focus_order_with_accept_option() {
  if (!gCanTabMoveFocus) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  setDialogPrereqPrefs();

  info("Calling showOnboardingDialog");
  const { win, maybeShowPromise } = await showOnboardingDialog({
    skipIntroduction: true,
  });

  info("Select onboardingAccept");
  EventUtils.synthesizeKey("KEY_Tab");
  Assert.equal(win.document.activeElement.id, "onboardingAccept");
  EventUtils.synthesizeKey(" ");

  const order = [
    "onboardingLearnMore",
    "onboardingSubmit",
    "onboardingSkipLink",
    "onboardingAccept",
  ];

  for (const next of order) {
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(win.document.activeElement.id, next);
  }

  EventUtils.synthesizeKey("KEY_Escape");

  info("Waiting for maybeShowOnboardingDialog to finish");
  await maybeShowPromise;
});

// Tests tabbing through the dialog after selecting reject option.
add_task(async function focus_order_with_reject_option() {
  if (!gCanTabMoveFocus) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  setDialogPrereqPrefs();

  info("Calling showOnboardingDialog");
  const { win, maybeShowPromise } = await showOnboardingDialog({
    skipIntroduction: true,
  });

  info("Select onboardingReject");
  EventUtils.synthesizeKey("KEY_Tab", { repeat: 3 });
  Assert.equal(win.document.activeElement.id, "onboardingReject");
  EventUtils.synthesizeKey(" ");

  const order = [
    "onboardingSubmit",
    "onboardingSkipLink",
    "onboardingLearnMore",
    "onboardingReject",
  ];

  for (const next of order) {
    EventUtils.synthesizeKey("KEY_Tab");
    Assert.equal(win.document.activeElement.id, next);
  }

  EventUtils.synthesizeKey("KEY_Escape");

  info("Waiting for maybeShowOnboardingDialog to finish");
  await maybeShowPromise;
});

// Tests tabbing through the dialog and pressing enter on introduction pane.
// Tab key count: 1
// Expected focused element: close button
add_task(async function focus_close() {
  await doFocusTest({
    introductionPane: true,
    tabKeyRepeat: 1,
    expectedFocusID: "onboardingClose",
    callback: async () => {
      info("Enter to submit");
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "close_1",
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
        object: "close_1",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 1
// Expected focused element: accept option
add_task(async function focus_accept() {
  await doFocusTest({
    tabKeyRepeat: 1,
    expectedFocusID: "onboardingAccept",
    callback: async () => {
      info("Select accept option");
      EventUtils.synthesizeKey(" ");

      info("Enter to submit");
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "accept_2",
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
        object: "accept_2",
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
    onboardingDialogChoice: "learn_more_2",
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
        object: "learn_more_2",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Tab key count: 3
// Expected focused element: reject option
add_task(async function focus_reject() {
  await doFocusTest({
    tabKeyRepeat: 3,
    expectedFocusID: "onboardingReject",
    callback: async () => {
      info("Select reject option");
      EventUtils.synthesizeKey(" ");

      info("Enter to submit");
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "reject_2",
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
        object: "reject_2",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter. (no option)
// Tab key count: 4
// Expected focused element: skip link
add_task(async function focus_skip() {
  await doFocusTest({
    tabKeyRepeat: 4,
    expectedFocusID: "onboardingSkipLink",
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
    onboardingDialogChoice: "not_now_2",
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
        object: "not_now_2",
      },
    ],
  });
});

// Tests tabbing through the dialog and pressing enter.
// Expected focused element: accept option (wraps around)
add_task(async function focus_accept_wraparound() {
  await doFocusTest({
    tabKeyRepeat: 5,
    expectedFocusID: "onboardingAccept",
    callback: async win => {
      info("Select accept option");
      EventUtils.synthesizeKey(" ");
      EventUtils.synthesizeKey("KEY_Enter");
    },
    onboardingDialogChoice: "accept_2",
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
        object: "accept_2",
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
      await UrlbarQuickSuggest.maybeShowOnboardingDialog();

      info("Simulating second restart");
      const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
        null,
        ONBOARDING_URI,
        { isSubDialog: true }
      );
      const maybeShowPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();
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
        await UrlbarQuickSuggest.maybeShowOnboardingDialog();
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

// Test the UI variation A.
add_task(async function variation_A() {
  await doVariationTest({
    variation: "A",
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-1",
      "main-title": "firefox-suggest-onboarding-main-title-1",
      "main-description": "firefox-suggest-onboarding-main-description-1",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  });
});

// Test the UI variation B.
add_task(async function variation_B() {
  await doVariationTest({
    variation: "B",
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      "main-title": "firefox-suggest-onboarding-main-title-2",
      "main-description": "firefox-suggest-onboarding-main-description-2",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  });
});

// Test the UI variation C.
add_task(async function variation_C() {
  await doVariationTest({
    variation: "C",
    expectedUI: {
      firefoxLogo: true,
    },
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-3",
      "main-title": "firefox-suggest-onboarding-main-title-3",
      "main-description": "firefox-suggest-onboarding-main-description-3",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
  });
});

// Test the UI variation D.
add_task(async function variation_D() {
  await doVariationTest({
    variation: "D",
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-4",
      "main-title": "firefox-suggest-onboarding-main-title-4",
      "main-description": "firefox-suggest-onboarding-main-description-4",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  });
});

// Test the UI variation E.
add_task(async function variation_E() {
  await doVariationTest({
    variation: "E",
    expectedUI: {
      firefoxLogo: true,
    },
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-5",
      "main-title": "firefox-suggest-onboarding-main-title-5",
      "main-description": "firefox-suggest-onboarding-main-description-5",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  });
});

// Test the UI variation F.
add_task(async function variation_F() {
  await doVariationTest({
    variation: "F",
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-2",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-6",
      "main-title": "firefox-suggest-onboarding-main-title-6",
      "main-description": "firefox-suggest-onboarding-main-description-6",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  });
});

// Test the UI variation G.
add_task(async function variation_G() {
  await doVariationTest({
    variation: "G",
    expectedUI: {
      mainPrivacyFirst: true,
    },
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-7",
      "main-title": "firefox-suggest-onboarding-main-title-7",
      "main-description": "firefox-suggest-onboarding-main-description-7",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-2",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-2",
    },
  });
});

// Test the UI variation H.
add_task(async function variation_H() {
  await doVariationTest({
    variation: "H",
    expectedUI: {
      firefoxLogo: true,
    },
    expectedL10N: {
      onboardingNext: "firefox-suggest-onboarding-introduction-next-button-1",
      "introduction-title": "firefox-suggest-onboarding-introduction-title-2",
      "main-title": "firefox-suggest-onboarding-main-title-8",
      "main-description": "firefox-suggest-onboarding-main-description-8",
      "main-accept-option-description":
        "firefox-suggest-onboarding-main-accept-option-description-1",
      "main-reject-option-description":
        "firefox-suggest-onboarding-main-reject-option-description-1",
    },
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

async function doFocusTest({
  tabKeyRepeat,
  expectedFocusID,
  onboardingDialogChoice,
  telemetryEvents,
  callback,
  expectedUserBranchPrefs,
  introductionPane,
}) {
  if (!gCanTabMoveFocus && tabKeyRepeat) {
    Assert.ok(true, "Tab key can't move focus, skipping test");
    return;
  }

  await doDialogTest({
    onboardingDialogChoice,
    expectedUserBranchPrefs,
    telemetryEvents,
    callback: async () => {
      const { win, maybeShowPromise } = await showOnboardingDialog({
        skipIntroduction: !introductionPane,
      });

      let doc = win.document;

      Assert.equal(
        doc.activeElement.id,
        "onboardingNext",
        "onboardingNext is focused initially"
      );

      if (tabKeyRepeat) {
        EventUtils.synthesizeKey("KEY_Tab", { repeat: tabKeyRepeat });
      }

      if (!gCanTabMoveFocus) {
        expectedFocusID = "onboardingNext";
      }

      Assert.equal(
        doc.activeElement.id,
        expectedFocusID,
        "Expected element is focused: " + expectedFocusID
      );

      await callback(win);
      await maybeShowPromise;
    },
  });
}

async function doTransitionTest({ trigger }) {
  setDialogPrereqPrefs();

  info("Calling showOnboardingDialog");
  const { win, maybeShowPromise } = await showOnboardingDialog();

  info("Check initial status");
  const introductionSection = win.document.getElementById(
    "introduction-section"
  );
  const mainSection = win.document.getElementById("main-section");
  Assert.ok(BrowserTestUtils.is_visible(introductionSection));
  Assert.ok(BrowserTestUtils.is_hidden(mainSection));

  // Trigger the transition.
  await trigger(win);

  info("Wait for transition");
  await BrowserTestUtils.waitForCondition(
    () =>
      BrowserTestUtils.is_hidden(introductionSection) &&
      BrowserTestUtils.is_visible(mainSection)
  );
  Assert.ok(true, "The transition is finished successfully");

  info("Close the dialog");
  EventUtils.synthesizeKey("KEY_Escape");
  await maybeShowPromise;
}

async function doVariationTest({
  variation,
  expectedUI = {},
  expectedL10N = {},
}) {
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts", 0);

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestScenario: "online",
      quickSuggestShowOnboardingDialogAfterNRestarts: 0,
      quickSuggestOnboardingDialogVariation: variation,
    },
    callback: async () => {
      info("Calling showOnboardingDialog");
      const { win, maybeShowPromise } = await showOnboardingDialog();

      info("Check the logo");
      const introductionLogoImage = win.getComputedStyle(
        win.document.querySelector("#introduction-section .logo")
      ).backgroundImage;
      const mainLogoImage = win.getComputedStyle(
        win.document.querySelector("#main-section .logo")
      ).backgroundImage;

      if (expectedUI.firefoxLogo) {
        const logoImage = 'url("chrome://branding/content/about-logo.svg")';
        Assert.equal(introductionLogoImage, logoImage);
        Assert.equal(mainLogoImage, logoImage);
      } else {
        const logoImage =
          'url("chrome://browser/content/urlbar/quicksuggestOnboarding_magglass.svg")';
        const animationImage =
          'url("chrome://browser/content/urlbar/quicksuggestOnboarding_magglass_animation.svg")';
        const mediaQuery = window.matchMedia(
          "(prefers-reduced-motion: no-preference)"
        );
        const expectedIntroductionLogoImage = mediaQuery.matches
          ? animationImage
          : logoImage;
        Assert.equal(introductionLogoImage, expectedIntroductionLogoImage);
        Assert.equal(mainLogoImage, logoImage);
      }

      info("Check the l10n attribute");
      for (const [id, l10n] of Object.entries(expectedL10N)) {
        const element = win.document.getElementById(id);
        Assert.equal(element.getAttribute("data-l10n-id"), l10n);
      }

      // Trigger the transition by pressing Enter on the Next button.
      EventUtils.synthesizeKey("KEY_Enter");
      await BrowserTestUtils.waitForCondition(
        () =>
          BrowserTestUtils.is_hidden(
            win.document.getElementById("introduction-section")
          ) &&
          BrowserTestUtils.is_visible(
            win.document.getElementById("main-section")
          )
      );

      info("Check the privacy first message on main pane");
      const mainPrivacyFirst = win.document.querySelector(
        "#main-section .privacy-first"
      );
      if (expectedUI.mainPrivacyFirst) {
        Assert.ok(BrowserTestUtils.is_visible(mainPrivacyFirst));
      } else {
        Assert.ok(BrowserTestUtils.is_hidden(mainPrivacyFirst));
      }

      EventUtils.synthesizeKey("KEY_Escape");
      await maybeShowPromise;

      info("Check the version and variation pref");
      Assert.equal(
        UrlbarPrefs.get("quicksuggest.onboardingDialogVersion"),
        JSON.stringify({ version: 1, variation: variation.toLowerCase() })
      );
    },
  });
}

/**
 * Show onbaording dialog.
 *
 * @param {object}
 *   skipIntroduction: If true, return dialog with skipping the introduction section.
 * @returns {object}
 *   win: window object of the dialog.
 *   maybeShowPromise: Promise of UrlbarQuickSuggest.maybeShowOnboardingDialog().
 */
async function showOnboardingDialog({ skipIntroduction } = {}) {
  const dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
    null,
    ONBOARDING_URI,
    { isSubDialog: true }
  );

  const maybeShowPromise = UrlbarQuickSuggest.maybeShowOnboardingDialog();

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
      BrowserTestUtils.is_hidden(introductionSection) &&
      BrowserTestUtils.is_visible(mainSection)
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
    onboardingDialogChoice: "dismiss_2",
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
        object: "dismiss_2",
      },
    ],
  });

  return canMove;
}

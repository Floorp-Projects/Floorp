/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test QuickActions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  UpdateService: "resource://gre/modules/UpdateService.sys.mjs",
  ActionsProviderQuickActions:
    "resource:///modules/ActionsProviderQuickActions.sys.mjs",
});

const DUMMY_PAGE =
  "https://example.com/browser/browser/base/content/test/general/dummy_page.html";

let testActionCalled = 0;

const assertAction = async name => {
  await BrowserTestUtils.waitForCondition(() =>
    window.document.querySelector(`.urlbarView-action-btn[data-action=${name}]`)
  );
  Assert.ok(true, `We found action "${name}`);
};

const hasQuickActions = win =>
  !!win.document.querySelector(".urlbarView-action-btn");

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.secondaryActions.featureGate", true],
    ],
  });

  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    label: "quickactions-downloads2",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    ActionsProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function basic() {
  info("The action isnt shown when not matched");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nomatch",
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "We did no match anything"
  );

  info("A prefix of the command matches");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
  });

  await assertAction("testaction");

  info("The callback of the action is fired when selected");
  EventUtils.synthesizeKey("KEY_Tab", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  Assert.equal(testActionCalled, 1, "Test action was called");
});

add_task(async function match_in_phrase() {
  ActionsProviderQuickActions.addAction("newtestaction", {
    commands: ["matchingstring"],
    label: "quickactions-downloads2",
  });

  info("The action is matched when at end of input");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Test we match at end of matchingstring",
  });
  await assertAction("newtestaction");
  await UrlbarTestUtils.promisePopupClose(window);
  EventUtils.synthesizeKey("KEY_Escape");
  ActionsProviderQuickActions.removeAction("newtestaction");
});

add_task(async function test_viewsource() {
  info("Check the button status of when the page is not web content");
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
    waitForLoad: true,
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "viewsource",
  });

  info("Do view source action");
  const onLoad = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "view-source:https://example.com/"
  );
  EventUtils.synthesizeKey("KEY_Tab", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  const viewSourceTab = await onLoad;

  info("Do view source action on the view-source page");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "viewsource",
  });

  Assert.equal(
    hasQuickActions(window),
    false,
    "Result for quick actions is hidden"
  );

  // Clean up.
  BrowserTestUtils.removeTab(viewSourceTab);
  BrowserTestUtils.removeTab(tab);
});

async function doAlertDialogTest({ input, dialogContentURI }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
  });

  const onDialog = BrowserTestUtils.promiseAlertDialog(null, null, {
    isSubDialog: true,
    callback: win => {
      Assert.equal(win.location.href, dialogContentURI, "The dialog is opened");
      EventUtils.synthesizeKey("KEY_Escape", {}, win);
    },
  });

  EventUtils.synthesizeKey("KEY_Tab", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);

  await onDialog;
}

add_task(async function test_refresh() {
  await doAlertDialogTest({
    input: "refresh",
    dialogContentURI: "chrome://global/content/resetProfile.xhtml",
  });
});

add_task(async function test_clear() {
  let useOldClearHistoryDialog = Services.prefs.getBoolPref(
    "privacy.sanitize.useOldClearHistoryDialog"
  );
  let dialogURL = useOldClearHistoryDialog
    ? "chrome://browser/content/sanitize.xhtml"
    : "chrome://browser/content/sanitize_v2.xhtml";
  await doAlertDialogTest({
    input: "clear",
    dialogContentURI: dialogURL,
  });
});

async function doUpdateActionTest(isActiveExpected) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "update",
  });

  if (isActiveExpected) {
    await assertAction("update");
  } else {
    Assert.equal(hasQuickActions(window), false, "No QuickActions were shown");
  }
}

add_task(async function test_update() {
  if (!AppConstants.MOZ_UPDATER) {
    await doUpdateActionTest(
      false,
      "Should be disabled since not AppConstants.MOZ_UPDATER"
    );
    return;
  }

  const sandbox = sinon.createSandbox();
  try {
    sandbox
      .stub(UpdateService.prototype, "currentState")
      .get(() => Ci.nsIApplicationUpdateService.STATE_IDLE);
    await doUpdateActionTest(
      false,
      "Should be disabled since current update state is not pending"
    );
    sandbox
      .stub(UpdateService.prototype, "currentState")
      .get(() => Ci.nsIApplicationUpdateService.STATE_PENDING);
    await doUpdateActionTest(
      true,
      "Should be enabled since current update state is pending"
    );
  } finally {
    sandbox.restore();
  }
});

add_task(async function test_whitespace() {
  info("Test with quickactions.showInZeroPrefix pref is false");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quickactions.showInZeroPrefix", false]],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: " ",
  });
  Assert.equal(
    hasQuickActions(window),
    false,
    "Result for quick actions is not shown"
  );
  await SpecialPowers.popPrefEnv();

  info("Test with quickactions.showInZeroPrefix pref is true");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quickactions.showInZeroPrefix", true]],
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  const countForEmpty = window.document.querySelectorAll(
    ".urlbarView-quickaction-button"
  ).length;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: " ",
  });
  const countForWhitespace = window.document.querySelectorAll(
    ".urlbarView-quickaction-button"
  ).length;
  Assert.equal(
    countForEmpty,
    countForWhitespace,
    "Count of quick actions of empty and whitespace are same"
  );
  await SpecialPowers.popPrefEnv();
});

async function clickQuickActionOneoffButton() {
  const oneOffButton = await TestUtils.waitForCondition(() =>
    window.document.getElementById("urlbar-engine-one-off-item-actions")
  );
  Assert.ok(oneOffButton, "One off button is available when preffed on");

  EventUtils.synthesizeMouseAtCenter(oneOffButton, {}, window);
  await UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.ACTIONS,
    entry: "oneoff",
  });
}

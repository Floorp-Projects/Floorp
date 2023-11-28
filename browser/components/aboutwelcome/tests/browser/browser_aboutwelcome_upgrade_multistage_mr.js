"use strict";

const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);
const { SpecialMessageActions } = ChromeUtils.importESModule(
  "resource://messaging-system/lib/SpecialMessageActions.sys.mjs"
);
const {
  assertFirefoxViewTabSelected,
  closeFirefoxViewTab,
  init: FirefoxViewTestUtilsInit,
} = ChromeUtils.importESModule(
  "resource://testing-common/FirefoxViewTestUtils.sys.mjs"
);
FirefoxViewTestUtilsInit(this);

const HOMEPAGE_PREF = "browser.startup.homepage";
const NEWTAB_PREF = "browser.newtabpage.enabled";
const PINPBM_DISABLED_PREF = "browser.startup.upgradeDialog.pinPBM.disabled";

// A bunch of the helper functions here are variants of the helper functions in
// browser_aboutwelcome_multistage_mr.js, because the onboarding
// experience runs in the parent process rather than elsewhere.
// If these start to get used in more than just the two files, it may become
// worth refactoring them to avoid duplicated code, and hoisting them
// into head.js.

let sandbox;

add_setup(async () => {
  requestLongerTimeout(2);

  await setAboutWelcomePref(true);

  sandbox = sinon.createSandbox();
  sandbox.stub(OnboardingMessageProvider, "_doesAppNeedPin").resolves(false);
  sandbox
    .stub(OnboardingMessageProvider, "_doesAppNeedDefault")
    .resolves(false);

  sandbox.stub(SpecialMessageActions, "pinFirefoxToTaskbar").resolves();

  registerCleanupFunction(async () => {
    await popPrefs();
    sandbox.restore();
  });
});

/**
 * Get the content by OnboardingMessageProvider.getUpgradeMessage(),
 * discard any screens whose ids are not in the "screensToTest" array,
 * and then open an upgrade dialog with just those screens.
 *
 * @param {Array} screensToTest
 *    A list of which screen ids to be displayed
 *
 * @returns {Promise<Window>}
 *    Resolves to the window global object for the dialog once it has been
 *    opened
 */
async function openMRUpgradeWelcome(screensToTest) {
  const data = await OnboardingMessageProvider.getUpgradeMessage();

  if (screensToTest) {
    data.content.screens = data.content.screens.filter(screen =>
      screensToTest.includes(screen.id)
    );
  }

  sandbox.stub(OnboardingMessageProvider, "getUpgradeMessage").resolves(data);

  let dialogOpenPromise = BrowserTestUtils.promiseAlertDialogOpen(
    null,
    "chrome://browser/content/spotlight.html",
    { isSubDialog: true }
  );

  Cc["@mozilla.org/browser/browserglue;1"]
    .getService()
    .wrappedJSObject._showUpgradeDialog();

  let browser = await dialogOpenPromise;

  OnboardingMessageProvider.getUpgradeMessage.restore();
  return Promise.resolve(browser);
}

async function clickVisibleButton(browser, selector) {
  await BrowserTestUtils.waitForCondition(
    () => browser.document.querySelector(selector),
    `waiting for selector ${selector}`,
    200, // interval
    100 // maxTries
  );
  browser.document.querySelector(selector).click();
}

async function test_upgrade_screen_content(
  browser,
  expected = [],
  unexpected = []
) {
  for (let selector of expected) {
    await TestUtils.waitForCondition(
      () => browser.document.querySelector(selector),
      `Should render ${selector}`
    );
  }
  for (let selector of unexpected) {
    Assert.ok(
      !browser.document.querySelector(selector),
      `Should not render ${selector}`
    );
  }
}

async function waitForDialogClose(browser) {
  await BrowserTestUtils.waitForCondition(
    () => !browser.top?.document.querySelector(".dialogFrame"),
    "waiting for dialog to close"
  );
}

/**
 * Test homepage/newtab prefs start off as defaults and do not change
 */
add_task(async function test_aboutwelcome_upgrade_mr_prefs_off() {
  let browser = await openMRUpgradeWelcome(["UPGRADE_GET_STARTED"]);

  await test_upgrade_screen_content(
    browser,
    //Expected selectors:
    ["main.UPGRADE_GET_STARTED"],
    //Unexpected selectors:
    ["main.PIN_FIREFOX"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");
  await clickVisibleButton(browser, ".action-buttons button.primary");
  await waitForDialogClose(browser);

  Assert.ok(
    !Services.prefs.prefHasUserValue(HOMEPAGE_PREF),
    "homepage pref should be default"
  );
  Assert.ok(
    !Services.prefs.prefHasUserValue(NEWTAB_PREF),
    "newtab pref should be default"
  );

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
 *Test checkbox if needPrivatePin is true
 */
add_task(async function test_aboutwelcome_upgrade_mr_private_pin() {
  OnboardingMessageProvider._doesAppNeedPin.resolves(true);
  let browser = await openMRUpgradeWelcome(["UPGRADE_PIN_FIREFOX"]);

  await test_upgrade_screen_content(
    browser,
    //Expected selectors:
    ["main.UPGRADE_PIN_FIREFOX", "input#action-checkbox"],
    //Unexpected selectors:
    ["main.UPGRADE_COLORWAY"]
  );
  await clickVisibleButton(browser, ".action-buttons button.primary");
  await waitForDialogClose(browser);

  const pinStub = SpecialMessageActions.pinFirefoxToTaskbar;
  Assert.equal(
    pinStub.callCount,
    2,
    "pinFirefoxToTaskbar should have been called twice"
  );
  Assert.ok(
    // eslint-disable-next-line eqeqeq
    pinStub.firstCall.lastArg != pinStub.secondCall.lastArg,
    "pinFirefoxToTaskbar should have been called once for private, once not"
  );

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
 *Test checkbox shouldn't be shown in get started screen
 */

add_task(async function test_aboutwelcome_upgrade_mr_private_pin_get_started() {
  OnboardingMessageProvider._doesAppNeedPin.resolves(false);

  let browser = await openMRUpgradeWelcome(["UPGRADE_GET_STARTED"]);

  await test_upgrade_screen_content(
    browser,
    //Expected selectors
    ["main.UPGRADE_GET_STARTED"],
    //Unexpected selectors:
    ["input#action-checkbox"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");

  await waitForDialogClose(browser);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
 *Test checkbox shouldn't be shown if needPrivatePin is false
 */
add_task(async function test_aboutwelcome_upgrade_mr_private_pin_not_needed() {
  OnboardingMessageProvider._doesAppNeedPin
    .resolves(true)
    .withArgs(true)
    .resolves(false);

  let browser = await openMRUpgradeWelcome(["UPGRADE_PIN_FIREFOX"]);

  await test_upgrade_screen_content(
    browser,
    //Expected selectors
    ["main.UPGRADE_PIN_FIREFOX"],
    //Unexpected selectors:
    ["input#action-checkbox"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");
  await waitForDialogClose(browser);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
 * Make sure we don't get an extraneous checkbox here.
 */
add_task(
  async function test_aboutwelcome_upgrade_mr_pin_not_needed_default_needed() {
    OnboardingMessageProvider._doesAppNeedPin.resolves(false);
    OnboardingMessageProvider._doesAppNeedDefault.resolves(false);

    let browser = await openMRUpgradeWelcome(["UPGRADE_GET_STARTED"]);

    await test_upgrade_screen_content(
      browser,
      //Expected selectors
      ["main.UPGRADE_GET_STARTED"],
      //Unexpected selectors:
      ["input#action-checkbox"]
    );

    await clickVisibleButton(browser, ".action-buttons button.secondary");
    await waitForDialogClose(browser);
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function test_aboutwelcome_privacy_segmentation_pref() {
  async function testPrivacySegmentation(enabled = false) {
    await pushPrefs(["browser.privacySegmentation.preferences.show", enabled]);
    let screenIds = ["UPGRADE_DATA_RECOMMENDATION", "UPGRADE_GRATITUDE"];
    let browser = await openMRUpgradeWelcome(screenIds);
    await test_upgrade_screen_content(
      browser,
      //Expected selectors
      [`main.${screenIds[enabled ? 0 : 1]}`],
      //Unexpected selectors:
      [`main.${screenIds[enabled ? 1 : 0]}`]
    );
    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await popPrefs();
  }

  for (let enabled of [true, false]) {
    await testPrivacySegmentation(enabled);
  }
});

add_task(async function test_aboutwelcome_upgrade_show_firefox_view() {
  let browser = await openMRUpgradeWelcome(["UPGRADE_GRATITUDE"]);

  // execution
  await test_upgrade_screen_content(
    browser,
    //Expected selectors
    ["main.UPGRADE_GRATITUDE"],
    //Unexpected selectors:
    []
  );
  await clickVisibleButton(browser, ".action-buttons button.primary");

  // verification
  await BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  assertFirefoxViewTabSelected(gBrowser.ownerGlobal);

  closeFirefoxViewTab(gBrowser.ownerGlobal);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

/*
 *Checkbox shouldn't be shown if pinPBMDisabled pref is true
 */
add_task(async function test_aboutwelcome_upgrade_mr_private_pin_not_needed() {
  OnboardingMessageProvider._doesAppNeedPin.resolves(true);
  await pushPrefs([PINPBM_DISABLED_PREF, true]);

  const browser = await openMRUpgradeWelcome(["UPGRADE_PIN_FIREFOX"]);

  await test_upgrade_screen_content(
    browser,
    //Expected selectors
    ["main.UPGRADE_PIN_FIREFOX"],
    //Unexpected selectors:
    ["input#action-checkbox"]
  );

  await clickVisibleButton(browser, ".action-buttons button.secondary");
  await waitForDialogClose(browser);
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

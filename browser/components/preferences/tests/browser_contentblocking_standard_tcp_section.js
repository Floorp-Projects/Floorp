/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the TCP info box in the ETP standard section of about:preferences#privacy.
 */

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const CAT_PREF = "browser.contentblocking.category";

const LEARN_MORE_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  Services.prefs.getStringPref(
    "privacy.restrict3rdpartystorage.preferences.learnMoreURLSuffix"
  );

const {
  BEHAVIOR_REJECT_TRACKER,
  BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
} = Ci.nsICookieService;

async function testTCPSection({ dFPIEnabled }) {
  info(
    "Testing TCP preferences section in standard " +
      JSON.stringify({ dFPIEnabled })
  );

  // In order to test the "standard" category we need to set the default value
  // for the cookie behavior pref. A user value would get cleared as soon as we
  // switch to "standard".
  Services.prefs
    .getDefaultBranch("")
    .setIntPref(
      COOKIE_BEHAVIOR_PREF,
      dFPIEnabled
        ? BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
        : BEHAVIOR_REJECT_TRACKER
    );

  // Setting to standard category explicitly, since changing the default cookie
  // behavior still switches us to custom initially.
  await SpecialPowers.pushPrefEnv({
    set: [[CAT_PREF, "standard"]],
  });

  const uiEnabled =
    Services.prefs.getIntPref(COOKIE_BEHAVIOR_PREF) ==
    BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let standardRadioOption = doc.getElementById("standardRadio");
  let strictRadioOption = doc.getElementById("strictRadio");
  let customRadioOption = doc.getElementById("customRadio");

  ok(standardRadioOption.selected, "Standard category is selected");

  let etpStandardTCPBox = doc.getElementById("etpStandardTCPBox");
  is(
    BrowserTestUtils.is_visible(etpStandardTCPBox),
    uiEnabled,
    `TCP section in standard is ${uiEnabled ? " " : "not "}visible.`
  );

  if (uiEnabled) {
    // If visible, test the TCP section elements.
    let learnMoreLink = etpStandardTCPBox.querySelector("#tcp-learn-more-link");
    ok(learnMoreLink, "Should have a learn more link");
    BrowserTestUtils.is_visible(
      learnMoreLink,
      "Learn more link should be visible."
    );
    ok(
      learnMoreLink.href && !learnMoreLink.href.startsWith("about:blank"),
      "Learn more link should be valid."
    );
    is(
      learnMoreLink.href,
      LEARN_MORE_URL,
      "Learn more link should have the correct target."
    );

    let description = etpStandardTCPBox.querySelector(".tail-with-learn-more");
    ok(description, "Should have a description element.");
    BrowserTestUtils.is_visible(description, "Description should be visible.");

    let title = etpStandardTCPBox.querySelector(
      ".content-blocking-warning-title"
    );
    ok(title, "Should have a title element.");
    BrowserTestUtils.is_visible(title, "Title should be visible.");
  }

  info("Switch to ETP strict.");
  let categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "strict");
  strictRadioOption.click();
  await categoryPrefChange;
  ok(
    !BrowserTestUtils.is_visible(etpStandardTCPBox),
    "When strict is selected TCP UI is not visible."
  );

  info("Switch to ETP custom.");
  categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "custom");
  customRadioOption.click();
  await categoryPrefChange;
  ok(
    !BrowserTestUtils.is_visible(etpStandardTCPBox),
    "When custom is selected TCP UI is not visible."
  );

  info("Switch back to standard and ensure we show the TCP UI again.");
  categoryPrefChange = waitForAndAssertPrefState(CAT_PREF, "standard");
  standardRadioOption.click();
  await categoryPrefChange;
  is(
    BrowserTestUtils.is_visible(etpStandardTCPBox),
    uiEnabled,
    `TCP section in standard is ${uiEnabled ? " " : "not "}visible.`
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
  Services.prefs.setStringPref(CAT_PREF, "standard");
}

add_setup(async function() {
  // Register cleanup function to restore default cookie behavior.
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const previousDefaultCB = defaultPrefs.getIntPref(COOKIE_BEHAVIOR_PREF);

  registerCleanupFunction(function() {
    defaultPrefs.setIntPref(COOKIE_BEHAVIOR_PREF, previousDefaultCB);
  });
});

// Clients which don't have dFPI enabled should not see the
// preferences section.
add_task(async function test_dfpi_disabled() {
  await testTCPSection({ dFPIEnabled: false });
});

add_task(async function test_dfpi_enabled() {
  await testTCPSection({ dFPIEnabled: true });
});

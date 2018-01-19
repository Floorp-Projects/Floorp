"use strict";

/* eslint-disable mozilla/no-cpows-in-tests */

Cu.import("resource://gre/modules/Services.jsm", this);

const OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const FHR_PREF = "datareporting.healthreport.uploadEnabled";

function withPrivacyPrefs(testFunc) {
  return async (...args) => (
    BrowserTestUtils.withNewTab("about:preferences#privacy", async browser => (
      testFunc(...args, browser)
    ))
  );
}

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, true]],
  }),
  withPrivacyPrefs,
  async function testCheckedOnLoad(browser) {
    const checkbox = browser.contentDocument.getElementById("optOutStudiesEnabled");
    ok(checkbox.checked, "Opt-out checkbox is checked on load when the pref is true");
  }
);

decorate_task(
  withPrefEnv({
    set: [[OPT_OUT_PREF, false]],
  }),
  withPrivacyPrefs,
  async function testUncheckedOnLoad(browser) {
    const checkbox = browser.contentDocument.getElementById("optOutStudiesEnabled");
    ok(!checkbox.checked, "Opt-out checkbox is unchecked on load when the pref is false");
  }
);

decorate_task(
  withPrefEnv({
    set: [[FHR_PREF, true]],
  }),
  withPrivacyPrefs,
  async function testEnabledOnLoad(browser) {
    const checkbox = browser.contentDocument.getElementById("optOutStudiesEnabled");
    ok(!checkbox.disabled, "Opt-out checkbox is enabled on load when the FHR pref is true");
  }
);

decorate_task(
  withPrefEnv({
    set: [[FHR_PREF, false]],
  }),
  withPrivacyPrefs,
  async function testDisabledOnLoad(browser) {
    const checkbox = browser.contentDocument.getElementById("optOutStudiesEnabled");
    ok(checkbox.disabled, "Opt-out checkbox is disabled on load when the FHR pref is false");
  }
);

decorate_task(
  withPrefEnv({
    set: [
      [FHR_PREF, true],
      [OPT_OUT_PREF, true],
    ],
  }),
  withPrivacyPrefs,
  async function testCheckboxes(browser) {
    const optOutCheckbox = browser.contentDocument.getElementById("optOutStudiesEnabled");
    const fhrCheckbox = browser.contentDocument.getElementById("submitHealthReportBox");

    optOutCheckbox.click();
    ok(
      !Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Unchecking the opt-out checkbox sets the pref to false."
    );
    optOutCheckbox.click();
    ok(
      Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Checking the opt-out checkbox sets the pref to true."
    );

    fhrCheckbox.click();
    ok(
      !Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Unchecking the FHR checkbox sets the opt-out pref to false."
    );
    ok(
      optOutCheckbox.disabled,
      "Unchecking the FHR checkbox disables the opt-out checkbox."
    );
    ok(
      !optOutCheckbox.checked,
      "Unchecking the FHR checkbox unchecks the opt-out checkbox."
    );

    fhrCheckbox.click();
    ok(
      Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Checking the FHR checkbox sets the opt-out pref to true."
    );
    ok(
      !optOutCheckbox.disabled,
      "Checking the FHR checkbox enables the opt-out checkbox."
    );
    ok(
      optOutCheckbox.checked,
      "Checking the FHR checkbox checks the opt-out checkbox."
    );
  }
);

decorate_task(
  withPrefEnv({
    set: [
      [FHR_PREF, true],
      [OPT_OUT_PREF, true],
    ],
  }),
  withPrivacyPrefs,
  async function testPrefWatchers(browser) {
    const optOutCheckbox = browser.contentDocument.getElementById("optOutStudiesEnabled");

    Services.prefs.setBoolPref(OPT_OUT_PREF, false);
    ok(
      !optOutCheckbox.checked,
      "Disabling the opt-out pref unchecks the opt-out checkbox."
    );
    Services.prefs.setBoolPref(OPT_OUT_PREF, true);
    ok(
      optOutCheckbox.checked,
      "Enabling the opt-out pref checks the opt-out checkbox."
    );

    Services.prefs.setBoolPref(FHR_PREF, false);
    ok(
      !Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Disabling the FHR pref sets the opt-out pref to false."
    );
    ok(
      optOutCheckbox.disabled,
      "Disabling the FHR pref disables the opt-out checkbox."
    );
    ok(
      !optOutCheckbox.checked,
      "Disabling the FHR pref unchecks the opt-out checkbox."
    );

    Services.prefs.setBoolPref(FHR_PREF, true);
    ok(
      Services.prefs.getBoolPref(OPT_OUT_PREF),
      "Enabling the FHR pref sets the opt-out pref to true."
    );
    ok(
      !optOutCheckbox.disabled,
      "Enabling the FHR pref enables the opt-out checkbox."
    );
    ok(
      optOutCheckbox.checked,
      "Enabling the FHR pref checks the opt-out checkbox."
    );
  }
);

decorate_task(
  withPrivacyPrefs,
  async function testViewStudiesLink(browser) {
    browser.contentDocument.getElementById("viewShieldStudies").click();
    await BrowserTestUtils.waitForLocationChange(gBrowser);

    is(
      gBrowser.currentURI.spec,
      "about:studies",
      "Clicking the view studies link opens about:studies in a new tab."
    );

    gBrowser.removeCurrentTab();
  }
);

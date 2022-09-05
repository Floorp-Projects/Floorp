/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures, ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);
const { OnboardingMessageProvider } = ChromeUtils.import(
  "resource://activity-stream/lib/OnboardingMessageProvider.jsm"
);

add_task(async function not_major_upgrade() {
  Services.telemetry.clearEvents();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Not major upgrade for upgrade dialog requirements", [
    "trigger",
    "reason",
    "not-major",
  ]);
});

add_task(async function remote_disabled() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithRollout({
    featureId: NimbusFeatures.upgradeDialog.featureId,
    value: {
      enabled: false,
    },
  });

  // Simulate starting from a previous version.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage_override.mstone", "88.0"]],
  });
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Feature disabled for upgrade dialog requirements", [
    "trigger",
    "reason",
    "disabled",
  ]);

  await doCleanup();
});

add_task(async function enterprise_disabled() {
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const pref = "browser.aboutwelcome.enabled";
  const orig = defaultPrefs.getBoolPref(pref, true);
  defaultPrefs.setBoolPref(pref, false);

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Welcome disabled like enterprise policy", [
    "trigger",
    "reason",
    "no-welcome",
  ]);
  defaultPrefs.setBoolPref(pref, orig);
});

add_task(async function show_major_upgrade() {
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const pref = "browser.startup.upgradeDialog.enabled";
  const orig = defaultPrefs.getBoolPref(pref, true);
  defaultPrefs.setBoolPref(pref, true);

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();
  const [win] = await TestUtils.topicObserved("subdialog-loaded");
  const data = await OnboardingMessageProvider.getUpgradeMessage();
  Assert.equal(data.id, "FX_MR_106_UPGRADE", "MR 106 Upgrade Dialog Shown");
  win.close();

  AssertEvents("Upgrade dialog opened from major upgrade", [
    "trigger",
    "reason",
    "satisfied",
  ]);

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  defaultPrefs.setBoolPref(pref, orig);
});

add_task(async function dont_reshow() {
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Shouldn't reshow for upgrade dialog requirements", [
    "trigger",
    "reason",
    "already-shown",
  ]);
});

registerCleanupFunction(() => {
  Cc["@mozilla.org/browser/clh;1"].getService(
    Ci.nsIBrowserHandler
  ).majorUpgrade = false;
  Services.prefs.clearUserPref("browser.startup.upgradeDialog.version");
});

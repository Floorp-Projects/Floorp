/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { OnboardingMessageProvider } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/OnboardingMessageProvider.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetters(this, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

add_setup(() => {
  Services.telemetry.clearEvents();
});

async function forceMajorUpgrade() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage_override.mstone", "88.0"]],
  });

  void BrowserHandler.defaultArgs;

  return async () => {
    await SpecialPowers.popPrefEnv();
    BrowserHandler.majorUpgrade = false;
    Services.prefs.clearUserPref("browser.startup.upgradeDialog.version");
  };
}

add_task(async function not_major_upgrade() {
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Not major upgrade for upgrade dialog requirements", [
    "trigger",
    "reason",
    "not-major",
  ]);
});

add_task(async function remote_disabled() {
  await ExperimentAPI.ready();
  let doCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: NimbusFeatures.upgradeDialog.featureId,
      value: {
        enabled: false,
      },
    },
    { isRollout: true }
  );

  // Simulate starting from a previous version.
  let cleanupUpgrade = await forceMajorUpgrade();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Feature disabled for upgrade dialog requirements", [
    "trigger",
    "reason",
    "disabled",
  ]);

  doCleanup();
  await cleanupUpgrade();
});

add_task(async function enterprise_disabled() {
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const pref = "browser.aboutwelcome.enabled";
  const orig = defaultPrefs.getBoolPref(pref, true);
  defaultPrefs.setBoolPref(pref, false);

  let cleanupUpgrade = await forceMajorUpgrade();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Welcome disabled like enterprise policy", [
    "trigger",
    "reason",
    "no-welcome",
  ]);

  await cleanupUpgrade();
  defaultPrefs.setBoolPref(pref, orig);
});

add_task(async function show_major_upgrade() {
  const defaultPrefs = Services.prefs.getDefaultBranch("");
  const pref = "browser.startup.upgradeDialog.enabled";
  const orig = defaultPrefs.getBoolPref(pref, true);
  defaultPrefs.setBoolPref(pref, true);

  let cleanupUpgrade = await forceMajorUpgrade();

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

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Shouldn't reshow for upgrade dialog requirements", [
    "trigger",
    "reason",
    "already-shown",
  ]);

  defaultPrefs.setBoolPref(pref, orig);
  await cleanupUpgrade();
});

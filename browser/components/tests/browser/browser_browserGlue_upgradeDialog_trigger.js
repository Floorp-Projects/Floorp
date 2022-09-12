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
  let doCleanup = await ExperimentFakes.enrollWithRollout({
    featureId: NimbusFeatures.upgradeDialog.featureId,
    value: {
      enabled: false,
    },
  });

  // Simulate starting from a previous version.
  let cleanupUpgrade = await forceMajorUpgrade();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Feature disabled for upgrade dialog requirements", [
    "trigger",
    "reason",
    "disabled",
  ]);

  await doCleanup();
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

add_task(async function test_mr2022_upgradeDialogEnabled() {
  const FALLBACK_PREF = "browser.startup.upgradeDialog.enabled";

  async function runMajorReleaseTest(
    { onboarding = undefined, enabled = undefined, fallbackPref = undefined },
    expected
  ) {
    info("Testing upgradeDialog with:");
    info(`  majorRelease2022.onboarding=${onboarding}`);
    info(`  upgradeDialog.enabled=${enabled}`);
    info(`  ${FALLBACK_PREF}=${fallbackPref}`);

    let mr2022Cleanup = async () => {};
    let upgradeDialogCleanup = async () => {};

    if (typeof onboarding !== "undefined") {
      mr2022Cleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "majorRelease2022",
        value: { onboarding },
      });
    }

    if (typeof enabled !== "undefined") {
      upgradeDialogCleanup = await ExperimentFakes.enrollWithFeatureConfig({
        featureId: "upgradeDialog",
        value: { enabled },
      });
    }

    if (typeof fallbackPref !== "undefined") {
      await SpecialPowers.pushPrefEnv({
        set: [[FALLBACK_PREF, fallbackPref]],
      });
    }

    const cleanupForcedUpgrade = await forceMajorUpgrade();

    try {
      await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();
      AssertEvents(`Upgrade dialog ${expected ? "shown" : "not shown"}`, [
        "trigger",
        "reason",
        expected ? "satisfied" : "disabled",
      ]);

      if (expected) {
        const [win] = await TestUtils.topicObserved("subdialog-loaded");
        win.close();
        await BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }
    } finally {
      await cleanupForcedUpgrade();
      if (typeof fallbackPref !== "undefined") {
        await SpecialPowers.popPrefEnv();
      }
      await upgradeDialogCleanup();
      await mr2022Cleanup();
    }
  }

  await runMajorReleaseTest({ onboarding: true }, true);
  await runMajorReleaseTest({ onboarding: true, enabled: false }, true);
  await runMajorReleaseTest({ onboarding: true, fallbackPref: false }, true);

  await runMajorReleaseTest({ onboarding: false }, false);
  await runMajorReleaseTest({ onboarding: false, enabled: true }, false);
  await runMajorReleaseTest({ onboarding: false, fallbackPref: true }, false);

  await runMajorReleaseTest({ enabled: true }, true);
  await runMajorReleaseTest({ enabled: true, fallbackPref: false }, true);
  await runMajorReleaseTest({ fallbackPref: true }, true);

  await runMajorReleaseTest({ enabled: false }, false);
  await runMajorReleaseTest({ enabled: false, fallbackPref: true }, false);
  await runMajorReleaseTest({ fallbackPref: false }, false);

  // Test the default configuration.
  await runMajorReleaseTest({}, true);
});

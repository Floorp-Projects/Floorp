/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);
const { NimbusFeatures, ExperimentFeature, ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

function AssertEvents(message, ...events) {
  info(`Checking telemetry events: ${message}`);
  TelemetryTestUtils.assertEvents(
    events.map(event => ["upgrade_dialog", ...event]),
    { category: "upgrade_dialog" }
  );
}

add_task(async function open_close_dialog() {
  mockShell();

  await showAndWaitForDialog();

  Assert.ok(true, "Upgrade dialog opened and closed");
});

add_task(async function theme_change() {
  const theme = await AddonManager.getAddonByID(
    "foto-soft-colorway@mozilla.org"
  );

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "variations");
    win.document.querySelectorAll("[name=theme]")[3].click();
    await TestUtils.waitForCondition(() => theme.isActive, "Theme is active");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.ok(theme.isActive, "Theme change saved");
  theme.disable();
});

add_task(async function keyboard_focus_okay() {
  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    Assert.equal(
      win.document.activeElement,
      win.document.getElementById("primary"),
      "Primary button has focus"
    );

    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    Assert.equal(
      win.document.activeElement.name,
      "theme",
      "A theme radio button has focus"
    );

    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    Assert.equal(
      win.document.activeElement,
      win.document.getElementById("primary"),
      "Primary button has focus"
    );

    win.close();
  });
});

add_task(async function keep_home() {
  Services.prefs.setStringPref("browser.startup.homepage", "about:blank");

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("checkbox").click();
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.ok(
    Services.prefs.prefHasUserValue("browser.startup.homepage"),
    "Homepage kept"
  );
});

add_task(async function revert_home() {
  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    Services.prefs.prefHasUserValue("browser.startup.homepage"),
    false,
    "Homepage reverted"
  );
});

add_task(async function skip_screens() {
  Services.telemetry.clearEvents();

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
  });

  AssertEvents(
    "Skipped over colorway screen",
    ["content", "show", "3-screens"],
    ["content", "show", "upgrade-dialog-start-primary-button"],
    ["content", "button", "upgrade-dialog-start-secondary-button"],
    ["content", "show", "upgrade-dialog-thankyou-primary-button"],
    ["content", "button", "upgrade-dialog-thankyou-primary-button"],
    ["content", "close", "complete"]
  );
});

add_task(async function all_3_screens() {
  await showAndWaitForDialog(async win => {
    // Always "randomly" select the first colorway.
    win.Math.random = () => 0;

    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "variations");
    win.document.querySelectorAll("[name=variation]")[1].click();
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  AssertEvents(
    "Shows all 3 screens with variations",
    ["content", "show", "3-screens"],
    ["content", "show", "upgrade-dialog-start-primary-button"],
    ["content", "button", "upgrade-dialog-start-primary-button"],
    ["content", "show", "random-1"],
    ["content", "show", "upgrade-dialog-colorway-primary-button"],
    ["content", "theme", "balanced"],
    ["content", "button", "upgrade-dialog-colorway-secondary-button"],
    ["content", "show", "upgrade-dialog-thankyou-primary-button"],
    ["content", "close", "external"]
  );
});

add_task(async function quit_app() {
  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    const cancelled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    cancelled.data = true;
    Services.obs.notifyObservers(
      cancelled,
      "quit-application-requested",
      "test"
    );
  });

  AssertEvents(
    "Dialog closed on quit request",
    ["content", "show", "3-screens"],
    ["content", "show", "upgrade-dialog-start-primary-button"],
    ["content", "close", "quit-application-requested"]
  );
});

add_task(async function window_warning() {
  // Dismiss the alert when it opens.
  const warning = BrowserTestUtils.promiseAlertDialog("cancel");

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");

    // Show close warning without blocking to allow this callback to complete.
    setTimeout(() => gBrowser.warnAboutClosingTabs());
  });
  await warning;

  AssertEvents(
    "Dialog closed when close warning wants to open",
    ["content", "show", "3-screens"],
    ["content", "show", "upgrade-dialog-start-primary-button"],
    ["content", "close", "external"]
  );
});

add_task(async function not_major_upgrade() {
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Not major upgrade for upgrade dialog requirements", [
    "trigger",
    "reason",
    "not-major",
  ]);
});

add_task(async function remote_disabled() {
  mockWin7(false);
  await ExperimentAPI.ready();
  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.upgradeDialog,
    configuration: {
      slug: "upgradeDialog_remoteDisabled",
      variables: { enabled: false },
      targeting: "true",
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

  // Re-enable back
  await ExperimentFakes.remoteDefaultsHelper({
    feature: NimbusFeatures.upgradeDialog,
    configuration: {
      slug: "upgradeDialog_remoteEnabled",
      variables: { enabled: true },
      targeting: "true",
    },
  });
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

add_task(async function win7_excluded() {
  mockWin7(true);

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertEvents("Not showing dialog for win7", ["trigger", "reason", "win7"]);
});

add_task(async function show_major_upgrade() {
  mockWin7(false);

  const promise = waitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();
  await promise;

  AssertEvents(
    "Upgrade dialog opened and closed from major upgrade",
    ["trigger", "reason", "satisfied"],
    ["content", "show", "3-screens"],
    ["content", "show", "upgrade-dialog-start-primary-button"],
    ["content", "close", "external"]
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
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

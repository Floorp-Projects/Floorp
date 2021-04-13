/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const BROWSER_GLUE = Cc["@mozilla.org/browser/browserglue;1"].getService()
  .wrappedJSObject;

sinon.stub(window, "getShellService");
function mockShell(overrides = {}) {
  const mock = {
    canPin: false,
    isDefault: false,
    isPinned: false,

    checkPinCurrentAppToTaskbar() {
      if (!this.canPin) {
        throw Error;
      }
    },
    isCurrentAppPinnedToTaskbarAsync() {
      return Promise.resolve(this.isPinned);
    },
    isDefaultBrowser() {
      return this.isDefault;
    },
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface() {
      return this;
    },

    pinCurrentAppToTaskbar: sinon.stub(),
    setAsDefault: sinon.stub(),
    ...overrides,
  };

  getShellService.returns(mock);
  return mock;
}

function waitForDialog(callback = win => win.close()) {
  return BrowserTestUtils.promiseAlertDialog(
    null,
    "chrome://browser/content/upgradeDialog.html",
    { callback, isSubDialog: true }
  );
}

function showAndWaitForDialog(callback) {
  const promise = waitForDialog(callback);
  BROWSER_GLUE._showUpgradeDialog();
  return promise;
}

add_task(async function open_close_dialog() {
  mockShell();

  await showAndWaitForDialog();

  Assert.ok(true, "Upgrade dialog opened and closed");
});

add_task(async function set_as_default() {
  const mock = mockShell();

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
});

add_task(async function need_pin() {
  const mock = mockShell({ canPin: true });

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  Assert.equal(
    mock.pinCurrentAppToTaskbar.callCount,
    1,
    "Primary button also pins"
  );
});

add_task(async function already_pin() {
  const mock = mockShell({ canPin: true, isPinned: true });

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    1,
    "Primary button sets as default"
  );
  Assert.equal(
    mock.pinCurrentAppToTaskbar.callCount,
    0,
    "Primary button avoids re-pinning"
  );
});

add_task(async function already_default() {
  const mock = mockShell({ isDefault: true });

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    0,
    "Primary button moves to second screen"
  );
});

add_task(async function already_default_need_pin() {
  const mock = mockShell({ canPin: true, isDefault: true });

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("primary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.close();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    0,
    "Primary button doesn't need to default"
  );
  Assert.equal(
    mock.pinCurrentAppToTaskbar.callCount,
    1,
    "Primary button pins even when already default"
  );
});

add_task(async function theme_change() {
  const theme = await AddonManager.getAddonByID(
    "firefox-alpenglow@mozilla.org"
  );
  mockShell();

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.querySelectorAll("[name=theme]")[3].click();
    await TestUtils.waitForCondition(() => theme.isActive, "Theme is active");
    win.document.getElementById("primary").click();
  });

  Assert.ok(theme.isActive, "Theme change saved");
  theme.disable();
});

add_task(async function skip_screens() {
  const mock = mockShell();

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    0,
    "Skipped both screens without setting default"
  );
});

add_task(async function exit_early() {
  const mock = mockShell({ isDefault: true });

  await showAndWaitForDialog(async win => {
    await BrowserTestUtils.waitForEvent(win, "ready");
    win.document.getElementById("secondary").click();
  });

  Assert.equal(
    mock.setAsDefault.callCount,
    0,
    "Only 1 screen to skip when default"
  );
});

function AssertTelemetry(telemetry, message) {
  Assert.stringContains(
    Services.telemetry
      .snapshotEvents(Services.telemetry.DATASET_ALL_CHANNELS)
      .parent.map(e => e.join("#"))
      .toString(),
    telemetry.join("#"),
    message
  );
}

add_task(async function not_major_upgrade() {
  Services.telemetry.clearEvents();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertTelemetry(
    ["upgrade_dialog", "trigger", "reason", "not-major"],
    "Not major upgrade for upgrade dialog requirements"
  );
});

add_task(async function remote_disabled() {
  Services.telemetry.clearEvents();

  // TODO(bug 1701948): Set up actual remote defaults.
  NimbusFeatures.upgradeDialog._onRemoteReady();
  Services.prefs.setBoolPref("browser.startup.upgradeDialog.enabled", false);

  // Simulate starting from a previous version.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage_override.mstone", "88.0"]],
  });
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertTelemetry(
    ["upgrade_dialog", "trigger", "reason", "disabled"],
    "Feature disabled for upgrade dialog requirements"
  );
  Services.prefs.clearUserPref("browser.startup.upgradeDialog.enabled");
});

add_task(async function show_major_upgrade() {
  Services.telemetry.clearEvents();

  const promise = waitForDialog();
  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();
  await promise;

  Assert.ok(true, "Upgrade dialog opened and closed from major upgrade");
  AssertTelemetry(
    ["upgrade_dialog", "trigger", "reason", "satisfied"],
    "Satisfied upgrade dialog requirements"
  );
  AssertTelemetry(
    ["upgrade_dialog", "content", "close", "external"],
    "Telemetry tracked dialog close"
  );
});

add_task(async function dont_reshow() {
  Services.telemetry.clearEvents();

  await BROWSER_GLUE._maybeShowDefaultBrowserPrompt();

  AssertTelemetry(
    ["upgrade_dialog", "trigger", "reason", "already-shown"],
    "Shouldn't reshow for upgrade dialog requirements"
  );
});

registerCleanupFunction(() => {
  getShellService.restore();
  Cc["@mozilla.org/browser/clh;1"].getService(
    Ci.nsIBrowserHandler
  ).majorUpgrade = false;
  Services.prefs.clearUserPref("browser.startup.upgradeDialog.version");
});

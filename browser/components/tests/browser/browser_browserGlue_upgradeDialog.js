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

registerCleanupFunction(() => {
  getShellService.restore();
});

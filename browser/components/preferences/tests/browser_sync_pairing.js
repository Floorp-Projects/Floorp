/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UIState } = ChromeUtils.import(
  "resource://services-sync/UIState.jsm",
  {}
);
const { FxAccountsPairingFlow } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsPairing.jsm",
  {}
);

// Use sinon for mocking.
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

let flowCounter = 0;

add_task(async function setup() {
  Services.prefs.setBoolPref("identity.fxaccounts.pairing.enabled", true);
  // Sync start-up might interfere with our tests, don't let UIState send UI updates.
  const origNotifyStateUpdated = UIState._internal.notifyStateUpdated;
  UIState._internal.notifyStateUpdated = () => {};

  const origGet = UIState.get;
  UIState.get = () => {
    return { status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" };
  };

  const origStart = FxAccountsPairingFlow.start;
  FxAccountsPairingFlow.start = ({ emitter: e }) => {
    return `https://foo.bar/${flowCounter++}`;
  };

  registerCleanupFunction(() => {
    UIState._internal.notifyStateUpdated = origNotifyStateUpdated;
    UIState.get = origGet;
    FxAccountsPairingFlow.start = origStart;
  });
});

add_task(async function testShowsQRCode() {
  await runWithPairingDialog(async win => {
    let doc = win.document;
    let qrContainer = doc.getElementById("qrContainer");
    let qrWrapper = doc.getElementById("qrWrapper");

    await TestUtils.waitForCondition(
      () => qrWrapper.getAttribute("pairing-status") == "ready"
    );

    // Verify that a QRcode is being shown.
    Assert.ok(
      qrContainer.style.backgroundImage.startsWith(
        `url("data:image/gif;base64,R0lGODdhOgA6AIAAAAAAAP///ywAAAAAOgA6AAAC/4yPqcvtD6OctNqLs968+w+G4gKU5nkiJYO2JuW6KsDGKEw3a7AbPZ+r4Ry7nzFIQkKKN6Avlzowo78`
      )
    );

    // Close the dialog.
    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    gBrowser.contentDocument.querySelector(".dialogClose").click();

    info("waiting for dialog to unload");
    await promiseUnloaded;
  });
});

add_task(async function testCantShowQrCode() {
  const origStart = FxAccountsPairingFlow.start;
  FxAccountsPairingFlow.start = async () => {
    throw new Error("boom");
  };
  await runWithPairingDialog(async win => {
    let doc = win.document;
    let qrWrapper = doc.getElementById("qrWrapper");

    await TestUtils.waitForCondition(
      () => qrWrapper.getAttribute("pairing-status") == "error"
    );

    // Close the dialog.
    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    gBrowser.contentDocument.querySelector(".dialogClose").click();

    info("waiting for dialog to unload");
    await promiseUnloaded;
  });
  FxAccountsPairingFlow.start = origStart;
});

add_task(async function testSwitchToWebContent() {
  await runWithPairingDialog(async win => {
    let doc = win.document;
    let qrWrapper = doc.getElementById("qrWrapper");

    await TestUtils.waitForCondition(
      () => qrWrapper.getAttribute("pairing-status") == "ready"
    );

    const spySwitchURL = sinon.spy(win.gFxaPairDeviceDialog, "_switchToUrl");
    const emitter = win.gFxaPairDeviceDialog._emitter;
    emitter.emit("view:SwitchToWebContent", "about:robots");

    Assert.equal(spySwitchURL.callCount, 1);
  });
});

add_task(async function testError() {
  await runWithPairingDialog(async win => {
    let doc = win.document;
    let qrWrapper = doc.getElementById("qrWrapper");

    await TestUtils.waitForCondition(
      () => qrWrapper.getAttribute("pairing-status") == "ready"
    );

    const emitter = win.gFxaPairDeviceDialog._emitter;
    emitter.emit("view:Error");

    await TestUtils.waitForCondition(
      () => qrWrapper.getAttribute("pairing-status") == "error"
    );

    // Close the dialog.
    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    gBrowser.contentDocument.querySelector(".dialogClose").click();

    info("waiting for dialog to unload");
    await promiseUnloaded;
  });
});

async function runWithPairingDialog(test) {
  await openPreferencesViaOpenPreferencesAPI("paneSync", { leaveOpen: true });

  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/fxaPairDevice.xhtml"
  );
  gBrowser.contentWindow.gSyncPane.pairAnotherDevice();

  let win = await promiseSubDialogLoaded;

  await test(win);

  sinon.restore();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

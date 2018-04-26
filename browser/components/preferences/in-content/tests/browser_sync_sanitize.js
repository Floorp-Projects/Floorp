/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* global sinon */

"use strict";

const {UIState} = ChromeUtils.import("resource://services-sync/UIState.jsm", {});
const {Log} = ChromeUtils.import("resource://gre/modules/Log.jsm", {});
const {AsyncShutdown} = ChromeUtils.import("resource://gre/modules/AsyncShutdown.jsm", {});

const {SyncDisconnect, SyncDisconnectInternal} = ChromeUtils.import("resource://services-sync/SyncDisconnect.jsm", {});

// Use sinon for mocking.
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");
registerCleanupFunction(() => {
  delete window.sinon; // test fails with this reference left behind.
});


add_task(async function setup() {
  // Sync start-up will interfere with our tests, don't let UIState send UI updates.
  const origNotifyStateUpdated = UIState._internal.notifyStateUpdated;
  UIState._internal.notifyStateUpdated = () => {};

  const origGet = UIState.get;
  UIState.get = () => { return { status: UIState.STATUS_SIGNED_IN, email: "foo@bar.com" }; };

  // browser_sync_sanitize uses the sync log, so arrange for that to end up
  // in the test output.
  const log = Log.repository.getLogger("Sync");
  const appender = new Log.DumpAppender();
  log.addAppender(appender);
  log.level = appender.level = Log.Level.Trace;

  registerCleanupFunction(() => {
    UIState._internal.notifyStateUpdated = origNotifyStateUpdated;
    UIState.get = origGet;
  });
});

add_task(async function testDisconnectUI() {
  await runTestWithSanitizeDialog(async (win, sinon) => {
    let doc = win.document;
    let butDisconnect = doc.getElementById("butDisconnect");
    let butDeleteSync = doc.getElementById("deleteRemoteSyncData");
    let butDeleteOther = doc.getElementById("deleteRemoteOtherData");

    // mock both sanitize functions and the fxa signout.
    let spyBrowser = sinon.spy(SyncDisconnectInternal, "doSanitizeBrowserData");
    let spySync = sinon.spy(SyncDisconnectInternal, "doSanitizeSyncData");
    let spySignout = sinon.spy(SyncDisconnectInternal, "doSyncAndAccountDisconnect");

    Assert.equal(butDisconnect.getAttribute("data-l10n-id"), "sync-disconnect-confirm-disconnect");

    // Both checkboxes default to unchecked.
    Assert.ok(!butDeleteSync.checked);
    Assert.ok(!butDeleteOther.checked);

    // Hitting either of the checkboxes should change the text on the disconnect button/
    butDeleteSync.click();
    Assert.ok(butDeleteSync.checked);
    Assert.equal(butDisconnect.getAttribute("data-l10n-id"), "sync-disconnect-confirm-disconnect-delete");

    butDeleteOther.click();
    Assert.ok(butDeleteOther.checked);
    Assert.equal(butDisconnect.getAttribute("data-l10n-id"), "sync-disconnect-confirm-disconnect-delete");

    butDeleteSync.click();
    Assert.ok(!butDeleteSync.checked);
    Assert.equal(butDisconnect.getAttribute("data-l10n-id"), "sync-disconnect-confirm-disconnect-delete");

    butDeleteOther.click();
    Assert.ok(!butDeleteOther.checked);
    // button text should be back to "just disconnect"
    Assert.equal(butDisconnect.getAttribute("data-l10n-id"), "sync-disconnect-confirm-disconnect");

    // Cancel the dialog - ensure it closes without sanitizing anything and
    // without disconnecting FxA.
    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    doc.getElementById("butCancel").click();

    info("waiting for dialog to unload");
    await promiseUnloaded;

    Assert.equal(spyBrowser.callCount, 0, "should not have sanitized the browser");
    Assert.equal(spySync.callCount, 0, "should not have sanitized Sync");
    Assert.equal(spySignout.callCount, 0, "should not have signed out of FxA");
  });
});

add_task(async function testDisconnectNoSanitize() {
  await runTestWithSanitizeDialog(async (win, sinon) => {
    let doc = win.document;
    let butDisconnect = doc.getElementById("butDisconnect");

    let spySignout = sinon.spy(SyncDisconnectInternal, "doSyncAndAccountDisconnect");
    let spySync = sinon.spy(SyncDisconnectInternal, "doSanitizeSyncData");
    let spyBrowser = sinon.spy(SyncDisconnectInternal, "doSanitizeBrowserData");

    let Weave = {
      Service: {
        enabled: true,
        lock: sinon.stub().returns(true),
        unlock: sinon.spy(),
        startOver: sinon.spy(),
      }
    };
    let weaveStub = sinon.stub(SyncDisconnectInternal, "getWeave");
    weaveStub.returns(Weave);

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    butDisconnect.click();
    info("waiting for dialog to unload");
    await promiseUnloaded;

    Assert.equal(Weave.Service.lock.callCount, 1, "should have taken the lock");
    Assert.equal(Weave.Service.unlock.callCount, 1, "should have unlocked at the end");
    Assert.equal(Weave.Service.enabled, true, "sync should be enabled");
    Assert.equal(spySync.callCount, 0, "should not have sanitized sync data");
    Assert.equal(spySignout.callCount, 1, "should have disconnected");
    Assert.equal(spyBrowser.callCount, 0, "should not sanitized browser data");
    Assert.equal(Weave.Service.startOver.callCount, 1, "should have reset sync");
  });
});

add_task(async function testSanitizeSync() {
  await runTestWithSanitizeDialog(async (win, sinon) => {
    let doc = win.document;
    let butDisconnect = doc.getElementById("butDisconnect");
    let butDeleteSync = doc.getElementById("deleteRemoteSyncData");

    SyncDisconnectInternal.lockRetryInterval = 100;

    let spySignout = sinon.spy(SyncDisconnectInternal, "doSyncAndAccountDisconnect");

    // mock the "browser" sanitize function - it should not be called by
    // this test.
    let spyBrowser = sinon.spy(SyncDisconnectInternal, "doSanitizeBrowserData");
    // mock Sync
    let mockEngine1 = {
      enabled: true,
      name: "Test Engine 1",
      wipeClient: sinon.spy(),
    };
    let mockEngine2 = {
      enabled: false,
      name: "Test Engine 2",
      wipeClient: sinon.spy(),
    };

    let lockStub = sinon.stub();
    lockStub.onCall(0).returns(false); // first call fails to get the lock.
    lockStub.onCall(1).returns(true); // second call gets the lock.
    let Weave = {
      Service: {
        enabled: true,
        startOver: sinon.spy(),
        lock: lockStub,
        unlock: sinon.spy(),

        engineManager: {
          getAll: sinon.stub().returns([mockEngine1, mockEngine2]),
        },
        errorHandler: {
          resetFileLog: sinon.spy(),
        }
      }
    };
    let weaveStub = sinon.stub(SyncDisconnectInternal, "getWeave");
    weaveStub.returns(Weave);

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    butDeleteSync.click();
    butDisconnect.click();
    info("waiting for dialog to unload");
    await promiseUnloaded;

    Assert.equal(Weave.Service.lock.callCount, 2, "should have tried the lock twice");
    Assert.equal(Weave.Service.unlock.callCount, 1, "should have unlocked at the end");
    Assert.ok(Weave.Service.enabled, "Weave should be enabled");
    Assert.equal(Weave.Service.errorHandler.resetFileLog.callCount, 1, "should have reset the log");
    Assert.equal(mockEngine1.wipeClient.callCount, 1, "enabled engine should have been wiped");
    Assert.equal(mockEngine2.wipeClient.callCount, 0, "disabled engine should not have been wiped");
    Assert.equal(spyBrowser.callCount, 0, "should not sanitize the browser");
    Assert.equal(spySignout.callCount, 1, "should have signed out of FxA");
    Assert.equal(Weave.Service.startOver.callCount, 1, "should have reset sync");
  });
});

add_task(async function testSanitizeBrowser() {
  await runTestWithSanitizeDialog(async (win, sinon) => {
    let doc = win.document;

    // The dialog should have the main UI visible.
    Assert.equal(doc.getElementById("deleteOptionsContent").hidden, false);
    Assert.equal(doc.getElementById("deletingContent").hidden, true);

    let butDisconnect = doc.getElementById("butDisconnect");
    let butDeleteOther = doc.getElementById("deleteRemoteOtherData");

    let spySignout = sinon.spy(SyncDisconnectInternal, "doSyncAndAccountDisconnect");

    // mock both sanitize functions.
    let spyBrowser = sinon.spy(SyncDisconnectInternal, "doSanitizeBrowserData");
    let spySync = sinon.spy(SyncDisconnectInternal, "doSanitizeSyncData");

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    butDeleteOther.click();
    butDisconnect.click();
    info("waiting for dialog to unload");
    await promiseUnloaded;

    Assert.equal(spyBrowser.callCount, 1, "should have sanitized the browser");
    Assert.equal(spySync.callCount, 0, "should not have sanitized Sync");
    Assert.equal(spySignout.callCount, 1, "should have signed out of FxA");
  });
});

add_task(async function testDisconnectAlreadyRunning() {
  // Mock the sanitize process to indicate one is already in progress.
  let resolveExisting;
  SyncDisconnectInternal.promiseDisconnectFinished =
    new Promise(resolve => resolveExisting = resolve);

  await runTestWithSanitizeDialog(async (win, sinon) => {
    let doc = win.document;
    // The dialog should have "waiting" visible.
    Assert.equal(doc.getElementById("deleteOptionsContent").hidden, true);
    Assert.equal(doc.getElementById("deletingContent").hidden, false);

    let promiseUnloaded = BrowserTestUtils.waitForEvent(win, "unload");
    resolveExisting();

    info("waiting for dialog to unload");
    await promiseUnloaded;
  });
  SyncDisconnectInternal.promiseDisconnectFinished = null;
});

async function runTestWithSanitizeDialog(test) {
  await openPreferencesViaOpenPreferencesAPI("paneSync", {leaveOpen: true});

  let doc = gBrowser.contentDocument;

  let promiseSubDialogLoaded =
      promiseLoadSubDialog("chrome://browser/content/preferences/in-content/syncDisconnect.xul");
  doc.getElementById("fxaUnlinkButton").doCommand();

  let win = await promiseSubDialogLoaded;

  let ss = sinon.sandbox.create();

  await test(win, ss);

  ss.restore();

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}


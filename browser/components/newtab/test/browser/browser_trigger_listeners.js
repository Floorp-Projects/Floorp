const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);

const mockIdleService = {
  _observers: new Set(),
  _fireObservers(state) {
    for (let observer of this._observers.values()) {
      observer.observe(this, state, null);
    }
  },
  QueryInterface: ChromeUtils.generateQI(["nsIUserIdleService"]),
  idleTime: 1200000,
  addIdleObserver(observer, time) {
    this._observers.add(observer);
  },
  removeIdleObserver(observer, time) {
    this._observers.delete(observer);
  },
};

const sleepMs = ms => new Promise(resolve => setTimeout(resolve, ms)); // eslint-disable-line mozilla/no-arbitrary-setTimeout

add_setup(async function() {
  registerCleanupFunction(() => {
    const trigger = ASRouterTriggerListeners.get("openURL");
    trigger.uninit();
  });
});

add_task(async function test_openURL_visit_counter() {
  const trigger = ASRouterTriggerListeners.get("openURL");
  const stub = sinon.stub();
  trigger.uninit();

  trigger.init(stub, ["example.com"]);

  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("https://example.com/");
  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("http://example.com/");
  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("http://example.com/");

  Assert.equal(stub.callCount, 3, "Stub called 3 times for example.com host");
  Assert.equal(
    stub.firstCall.args[1].context.visitsCount,
    1,
    "First call should have count 1"
  );
  Assert.equal(
    stub.thirdCall.args[1].context.visitsCount,
    2,
    "Third call should have count 2 for http://example.com"
  );
});

add_task(async function test_openURL_visit_counter_withPattern() {
  const trigger = ASRouterTriggerListeners.get("openURL");
  const stub = sinon.stub();
  trigger.uninit();

  // Match any valid URL
  trigger.init(stub, [], ["*://*/*"]);

  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("https://example.com/");
  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("http://example.com/");
  await waitForUrlLoad("about:blank");
  await waitForUrlLoad("http://example.com/");

  Assert.equal(stub.callCount, 3, "Stub called 3 times for example.com host");
  Assert.equal(
    stub.firstCall.args[1].context.visitsCount,
    1,
    "First call should have count 1"
  );
  Assert.equal(
    stub.thirdCall.args[1].context.visitsCount,
    2,
    "Third call should have count 2 for http://example.com"
  );
});

add_task(async function test_captivePortalLogin() {
  const stub = sinon.stub();
  const captivePortalTrigger = ASRouterTriggerListeners.get(
    "captivePortalLogin"
  );

  captivePortalTrigger.init(stub);

  Services.obs.notifyObservers(this, "captive-portal-login-success", {});

  Assert.ok(stub.called, "Called after login event");

  captivePortalTrigger.uninit();

  Services.obs.notifyObservers(this, "captive-portal-login-success", {});

  Assert.equal(stub.callCount, 1, "Not called after uninit");
});

add_task(async function test_preferenceObserver() {
  const stub = sinon.stub();
  const poTrigger = ASRouterTriggerListeners.get("preferenceObserver");

  poTrigger.uninit();

  poTrigger.init(stub, ["foo.bar", "bar.foo"]);

  Services.prefs.setStringPref("foo.bar", "foo.bar");

  Assert.ok(stub.calledOnce, "Called for pref foo.bar");
  Assert.deepEqual(
    stub.firstCall.args[1],
    {
      id: "preferenceObserver",
      param: { type: "foo.bar" },
    },
    "Called with expected arguments"
  );

  Services.prefs.setStringPref("bar.foo", "bar.foo");
  Assert.ok(stub.calledTwice, "Called again for second pref.");
  Services.prefs.clearUserPref("foo.bar");
  Assert.ok(stub.calledThrice, "Called when clearing the pref as well.");

  stub.resetHistory();
  poTrigger.uninit();

  Services.prefs.clearUserPref("bar.foo");
  Assert.ok(stub.notCalled, "Not called after uninit");
});

add_task(async function test_nthTabClosed() {
  const handlerStub = sinon.stub();
  const tabClosedTrigger = ASRouterTriggerListeners.get("nthTabClosed");
  tabClosedTrigger.uninit();
  tabClosedTrigger.init(handlerStub);

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  BrowserTestUtils.removeTab(tab1);
  Assert.ok(handlerStub.calledOnce, "Called once after first tab closed");

  BrowserTestUtils.removeTab(tab2);
  Assert.ok(handlerStub.calledTwice, "Called twice after second tab closed");

  handlerStub.resetHistory();
  tabClosedTrigger.uninit();

  Assert.ok(handlerStub.notCalled, "Not called after uninit");
});

function getIdleTriggerMock() {
  const idleTrigger = ASRouterTriggerListeners.get("activityAfterIdle");
  idleTrigger.uninit();
  const sandbox = sinon.createSandbox();
  const handlerStub = sandbox.stub();
  sandbox.stub(idleTrigger, "_triggerDelay").value(0);
  sandbox.stub(idleTrigger, "_wakeDelay").value(30);
  sandbox.stub(idleTrigger, "_idleService").value(mockIdleService);
  let restored = false;
  const restore = () => {
    if (restored) return;
    restored = true;
    idleTrigger.uninit();
    sandbox.restore();
  };
  registerCleanupFunction(restore);
  idleTrigger.init(handlerStub);
  return { idleTrigger, handlerStub, restore };
}

// Test that the trigger fires under normal conditions.
add_task(async function test_activityAfterIdle() {
  const { handlerStub, restore } = getIdleTriggerMock();
  let firedOnActive = new Promise(resolve =>
    handlerStub.callsFake(() => resolve(true))
  );
  mockIdleService._fireObservers("idle");
  await TestUtils.waitForTick();
  ok(handlerStub.notCalled, "Not called when idle");
  mockIdleService._fireObservers("active");
  ok(await firedOnActive, "Called once when active after idle");
  restore();
});

// Test that the trigger does not fire when the active window is private.
add_task(async function test_activityAfterIdlePrivateWindow() {
  const { handlerStub, restore } = getIdleTriggerMock();
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWin), "Window is private");
  await TestUtils.waitForTick();
  mockIdleService._fireObservers("idle");
  await TestUtils.waitForTick();
  mockIdleService._fireObservers("active");
  await TestUtils.waitForTick();
  ok(handlerStub.notCalled, "Not called when active window is private");
  await BrowserTestUtils.closeWindow(privateWin);
  restore();
});

// Test that the trigger does not fire when the window is minimized, but does
// fire after the window is restored.
add_task(async function test_activityAfterIdleHiddenWindow() {
  const { handlerStub, restore } = getIdleTriggerMock();
  let firedOnRestore = new Promise(resolve =>
    handlerStub.callsFake(() => resolve(true))
  );
  window.minimize();
  await BrowserTestUtils.waitForCondition(
    () => window.windowState === window.STATE_MINIMIZED,
    "Window should be minimized"
  );
  mockIdleService._fireObservers("idle");
  await TestUtils.waitForTick();
  mockIdleService._fireObservers("active");
  await TestUtils.waitForTick();
  ok(handlerStub.notCalled, "Not called when window is minimized");
  window.restore();
  ok(await firedOnRestore, "Called once after restoring minimized window");
  restore();
});

// Test that the trigger does not fire immediately after waking from sleep.
add_task(async function test_activityAfterIdleWake() {
  const { handlerStub, restore } = getIdleTriggerMock();
  let firedAfterWake = new Promise(resolve =>
    handlerStub.callsFake(() => resolve(true))
  );
  mockIdleService._fireObservers("wake_notification");
  mockIdleService._fireObservers("idle");
  await TestUtils.waitForTick();
  mockIdleService._fireObservers("active");
  await sleepMs(31);
  ok(handlerStub.notCalled, "Not called immediately after waking from sleep");

  mockIdleService._fireObservers("idle");
  await TestUtils.waitForTick();
  mockIdleService._fireObservers("active");
  ok(
    await firedAfterWake,
    "Called once after waiting for wake delay before firing idle"
  );
  restore();
});

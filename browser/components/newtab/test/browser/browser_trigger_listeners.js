const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);

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

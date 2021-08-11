const { ASRouterTriggerListeners } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);

add_task(async function setup() {
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

  Services.obs.notifyObservers(
    this,
    "captive-portal-login-success-after-button-pressed",
    {}
  );

  Assert.ok(stub.called, "Called after button press and login");

  captivePortalTrigger.uninit();

  Services.obs.notifyObservers(
    this,
    "captive-portal-login-success-after-button-pressed",
    {}
  );

  Assert.equal(stub.callCount, 1, "Not called after uninit");
});

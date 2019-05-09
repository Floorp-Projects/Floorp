import {ASRouterTargeting, CachedTargetingGetter} from "lib/ASRouterTargeting.jsm";
import {OnboardingMessageProvider} from "lib/OnboardingMessageProvider.jsm";

// Note that tests for the ASRouterTargeting environment can be found in
// test/functional/mochitest/browser_asrouter_targeting.js

describe("#CachedTargetingGetter", () => {
  const sixHours = 6 * 60 * 60 * 1000;
  let sandbox;
  let clock;
  let frecentStub;
  let topsitesCache;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    clock = sinon.useFakeTimers();
    frecentStub = sandbox.stub(global.NewTabUtils.activityStreamProvider, "getTopFrecentSites");
    sandbox.stub(global.Cu, "reportError");
    topsitesCache = new CachedTargetingGetter("getTopFrecentSites");
  });

  afterEach(() => {
    sandbox.restore();
    clock.restore();
  });

  it("should only make a request every 6 hours", async () => {
    frecentStub.resolves();
    clock.tick(sixHours);

    await topsitesCache.get();
    await topsitesCache.get();

    assert.calledOnce(global.NewTabUtils.activityStreamProvider.getTopFrecentSites);

    clock.tick(sixHours);

    await topsitesCache.get();

    assert.calledTwice(global.NewTabUtils.activityStreamProvider.getTopFrecentSites);
  });
  it("should report errors", async () => {
    frecentStub.rejects(new Error("fake error"));
    clock.tick(sixHours);

    // assert.throws expect a function as the first parameter, try/catch is a
    // workaround
    try {
      await topsitesCache.get();
      assert.isTrue(false);
    } catch (e) {
      assert.calledOnce(global.Cu.reportError);
    }
  });
  it("should check targeted message before message without targeting", async () => {
    const messages = (await OnboardingMessageProvider.getUntranslatedMessages());
    const stub = sandbox.stub(ASRouterTargeting, "checkMessageTargeting").resolves();
    const context = {attributionData: {campaign: "non-fx-button", source: "addons.mozilla.org"}};
    await ASRouterTargeting.findMatchingMessage({messages, trigger: {id: "firstRun"}, context});

    assert.equal(stub.callCount, 6);
    const calls = stub.getCalls().map(call => call.args[0]);
    const lastCall = calls[calls.length - 1];
    assert.equal(lastCall.id, "FXA_1");
  });
  it("should return FxA message (is fallback)", async () => {
    const messages = (await OnboardingMessageProvider.getUntranslatedMessages())
      .filter(m => m.id !== "RETURN_TO_AMO_1");
    const context = {attributionData: {campaign: "non-fx-button", source: "addons.mozilla.org"}};
    const result = await ASRouterTargeting.findMatchingMessage({messages, trigger: {id: "firstRun"}, context});

    assert.isDefined(result);
    assert.equal(result.id, "FXA_1");
  });
  describe("combineContexts", () => {
    it("should combine the properties of the two objects", () => {
      const joined = ASRouterTargeting.combineContexts({
        get foo() { return "foo"; },
      }, {
        get bar() { return "bar"; },
      });
      assert.propertyVal(joined, "foo", "foo");
      assert.propertyVal(joined, "bar", "bar");
    });
    it("should warn when properties overlap", () => {
      ASRouterTargeting.combineContexts({
        get foo() { return "foo"; },
      }, {
        get foo() { return "bar"; },
      });
      assert.calledOnce(global.Cu.reportError);
    });
  });
});

import {
  ASRouterTargeting,
  CachedTargetingGetter,
} from "lib/ASRouterTargeting.jsm";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";

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
    frecentStub = sandbox.stub(
      global.NewTabUtils.activityStreamProvider,
      "getTopFrecentSites"
    );
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

    assert.calledOnce(
      global.NewTabUtils.activityStreamProvider.getTopFrecentSites
    );

    clock.tick(sixHours);

    await topsitesCache.get();

    assert.calledTwice(
      global.NewTabUtils.activityStreamProvider.getTopFrecentSites
    );
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
    const messages = await OnboardingMessageProvider.getUntranslatedMessages();
    const stub = sandbox
      .stub(ASRouterTargeting, "checkMessageTargeting")
      .resolves();
    const context = {
      attributionData: {
        campaign: "non-fx-button",
        source: "addons.mozilla.org",
      },
    };
    await ASRouterTargeting.findMatchingMessage({
      messages,
      trigger: { id: "firstRun" },
      context,
    });

    assert.equal(stub.callCount, 6);
    const calls = stub.getCalls().map(call => call.args[0]);
    const lastCall = calls[calls.length - 1];
    assert.equal(lastCall.id, "FXA_1");
  });
  it("should return FxA message (is fallback)", async () => {
    const messages = (await OnboardingMessageProvider.getUntranslatedMessages()).filter(
      m => m.id !== "RETURN_TO_AMO_1"
    );
    const context = {
      attributionData: {
        campaign: "non-fx-button",
        source: "addons.mozilla.org",
      },
    };
    const result = await ASRouterTargeting.findMatchingMessage({
      messages,
      trigger: { id: "firstRun" },
      context,
    });

    assert.isDefined(result);
    assert.equal(result.id, "FXA_1");
  });
  describe("sortMessagesByPriority", () => {
    it("should sort messages in descending priority order", async () => {
      const [
        m1,
        m2,
        m3,
      ] = await OnboardingMessageProvider.getUntranslatedMessages();
      const checkMessageTargetingStub = sandbox
        .stub(ASRouterTargeting, "checkMessageTargeting")
        .resolves(false);
      sandbox.stub(ASRouterTargeting, "isTriggerMatch").resolves(true);

      await ASRouterTargeting.findMatchingMessage({
        messages: [
          { ...m1, priority: 0 },
          { ...m2, priority: 1 },
          { ...m3, priority: 2 },
        ],
        trigger: "testing",
      });

      assert.equal(checkMessageTargetingStub.callCount, 3);

      const [arg_m1] = checkMessageTargetingStub.firstCall.args;
      assert.equal(arg_m1.id, m3.id);

      const [arg_m2] = checkMessageTargetingStub.secondCall.args;
      assert.equal(arg_m2.id, m2.id);

      const [arg_m3] = checkMessageTargetingStub.thirdCall.args;
      assert.equal(arg_m3.id, m1.id);
    });
    it("should sort messages with no priority last", async () => {
      const [
        m1,
        m2,
        m3,
      ] = await OnboardingMessageProvider.getUntranslatedMessages();
      const checkMessageTargetingStub = sandbox
        .stub(ASRouterTargeting, "checkMessageTargeting")
        .resolves(false);
      sandbox.stub(ASRouterTargeting, "isTriggerMatch").resolves(true);

      await ASRouterTargeting.findMatchingMessage({
        messages: [
          { ...m1, priority: 0 },
          { ...m2, priority: undefined },
          { ...m3, priority: 2 },
        ],
        trigger: "testing",
      });

      assert.equal(checkMessageTargetingStub.callCount, 3);

      const [arg_m1] = checkMessageTargetingStub.firstCall.args;
      assert.equal(arg_m1.id, m3.id);

      const [arg_m2] = checkMessageTargetingStub.secondCall.args;
      assert.equal(arg_m2.id, m1.id);

      const [arg_m3] = checkMessageTargetingStub.thirdCall.args;
      assert.equal(arg_m3.id, m2.id);
    });
    it("should keep the order of messages with same priority unchanged", async () => {
      const [
        m1,
        m2,
        m3,
      ] = await OnboardingMessageProvider.getUntranslatedMessages();
      const checkMessageTargetingStub = sandbox
        .stub(ASRouterTargeting, "checkMessageTargeting")
        .resolves(false);
      sandbox.stub(ASRouterTargeting, "isTriggerMatch").resolves(true);

      await ASRouterTargeting.findMatchingMessage({
        messages: [
          { ...m1, priority: 2, targeting: undefined, rank: 1 },
          { ...m2, priority: undefined, targeting: undefined, rank: 1 },
          { ...m3, priority: 2, targeting: undefined, rank: 1 },
        ],
        trigger: "testing",
      });

      assert.equal(checkMessageTargetingStub.callCount, 3);

      const [arg_m1] = checkMessageTargetingStub.firstCall.args;
      assert.equal(arg_m1.id, m1.id);

      const [arg_m2] = checkMessageTargetingStub.secondCall.args;
      assert.equal(arg_m2.id, m3.id);

      const [arg_m3] = checkMessageTargetingStub.thirdCall.args;
      assert.equal(arg_m3.id, m2.id);
    });
  });
  describe("combineContexts", () => {
    it("should combine the properties of the two objects", () => {
      const joined = ASRouterTargeting.combineContexts(
        {
          get foo() {
            return "foo";
          },
        },
        {
          get bar() {
            return "bar";
          },
        }
      );
      assert.propertyVal(joined, "foo", "foo");
      assert.propertyVal(joined, "bar", "bar");
    });
    it("should warn when properties overlap", () => {
      ASRouterTargeting.combineContexts(
        {
          get foo() {
            return "foo";
          },
        },
        {
          get foo() {
            return "bar";
          },
        }
      );
      assert.calledOnce(global.Cu.reportError);
    });
  });
});

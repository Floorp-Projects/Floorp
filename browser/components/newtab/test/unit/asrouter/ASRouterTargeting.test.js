import {
  ASRouterTargeting,
  CachedTargetingGetter,
  getSortedMessages,
  QueryCache,
} from "lib/ASRouterTargeting.jsm";
import { OnboardingMessageProvider } from "lib/OnboardingMessageProvider.jsm";
import { ASRouterPreferences } from "lib/ASRouterPreferences.jsm";
import { GlobalOverrider } from "test/unit/utils";

// Note that tests for the ASRouterTargeting environment can be found in
// test/functional/mochitest/browser_asrouter_targeting.js

describe("#CachedTargetingGetter", () => {
  const sixHours = 6 * 60 * 60 * 1000;
  let sandbox;
  let clock;
  let frecentStub;
  let topsitesCache;
  let globals;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    clock = sinon.useFakeTimers();
    frecentStub = sandbox.stub(
      global.NewTabUtils.activityStreamProvider,
      "getTopFrecentSites"
    );
    sandbox.stub(global.Cu, "reportError");
    topsitesCache = new CachedTargetingGetter("getTopFrecentSites");
    globals = new GlobalOverrider();
    globals.set(
      "TargetingContext",
      class {
        static combineContexts(...args) {
          return sinon.stub();
        }

        evalWithDefault(expr) {
          return sinon.stub();
        }
      }
    );
  });

  afterEach(() => {
    sandbox.restore();
    clock.restore();
    globals.restore();
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
  it("throws when failing getter", async () => {
    frecentStub.rejects(new Error("fake error"));
    clock.tick(sixHours);

    // assert.throws expect a function as the first parameter, try/catch is a
    // workaround
    let rejected = false;
    try {
      await topsitesCache.get();
    } catch (e) {
      rejected = true;
    }

    assert(rejected);
  });
  describe("sortMessagesByPriority", () => {
    it("should sort messages in descending priority order", async () => {
      const [
        m1,
        m2,
        m3 = { id: "m3" },
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
        m3 = { id: "m3" },
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
        m3 = { id: "m3" },
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
});
describe("#isTriggerMatch", () => {
  let trigger;
  let message;
  beforeEach(() => {
    trigger = { id: "openURL" };
    message = { id: "openURL" };
  });
  it("should return false if trigger and candidate ids are different", () => {
    trigger.id = "trigger";
    message.id = "message";

    assert.isFalse(ASRouterTargeting.isTriggerMatch(trigger, message));
    assert.isTrue(
      ASRouterTargeting.isTriggerMatch({ id: "foo" }, { id: "foo" })
    );
  });
  it("should return true if the message we check doesn't have trigger params or patterns", () => {
    // No params or patterns defined
    assert.isTrue(ASRouterTargeting.isTriggerMatch(trigger, message));
  });
  it("should return false if the trigger does not have params defined", () => {
    message.params = {};

    // trigger.param is undefined
    assert.isFalse(ASRouterTargeting.isTriggerMatch(trigger, message));
  });
  it("should return true if message params includes trigger host", () => {
    message.params = ["mozilla.org"];
    trigger.param = { host: "mozilla.org" };

    assert.isTrue(ASRouterTargeting.isTriggerMatch(trigger, message));
  });
  it("should return true if message params includes trigger param.type", () => {
    message.params = ["ContentBlockingMilestone"];
    trigger.param = { type: "ContentBlockingMilestone" };

    assert.isTrue(Boolean(ASRouterTargeting.isTriggerMatch(trigger, message)));
  });
  it("should return true if message params match trigger mask", () => {
    // STATE_BLOCKED_FINGERPRINTING_CONTENT
    message.params = [0x00000040];
    trigger.param = { type: 538091584 };

    assert.isTrue(Boolean(ASRouterTargeting.isTriggerMatch(trigger, message)));
  });
});
describe("#CacheListAttachedOAuthClients", () => {
  const fourHours = 4 * 60 * 60 * 1000;
  let sandbox;
  let clock;
  let fakeFxAccount;
  let authClientsCache;
  let globals;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = sinon.createSandbox();
    clock = sinon.useFakeTimers();
    fakeFxAccount = {
      listAttachedOAuthClients: () => {},
    };
    globals.set("fxAccounts", fakeFxAccount);
    authClientsCache = QueryCache.queries.ListAttachedOAuthClients;
    sandbox
      .stub(fxAccounts, "listAttachedOAuthClients")
      .returns(Promise.resolve({}));
  });

  afterEach(() => {
    authClientsCache.expire();
    sandbox.restore();
    clock.restore();
  });

  it("should only make additional request every 4 hours", async () => {
    clock.tick(fourHours);

    await authClientsCache.get();
    assert.calledOnce(fxAccounts.listAttachedOAuthClients);

    clock.tick(fourHours);
    await authClientsCache.get();
    assert.calledTwice(fxAccounts.listAttachedOAuthClients);
  });

  it("should not make additional request before 4 hours", async () => {
    clock.tick(fourHours);

    await authClientsCache.get();
    assert.calledOnce(fxAccounts.listAttachedOAuthClients);

    await authClientsCache.get();
    assert.calledOnce(fxAccounts.listAttachedOAuthClients);
  });
});
describe("ASRouterTargeting", () => {
  let evalStub;
  let sandbox;
  let clock;
  let globals;
  let fakeTargetingContext;
  beforeEach(() => {
    sandbox = sinon.createSandbox();
    sandbox.replace(ASRouterTargeting, "Environment", {});
    clock = sinon.useFakeTimers();
    fakeTargetingContext = {
      combineContexts: sandbox.stub(),
      evalWithDefault: sandbox.stub().resolves(),
    };
    globals = new GlobalOverrider();
    globals.set(
      "TargetingContext",
      class {
        static combineContexts(...args) {
          return fakeTargetingContext.combineContexts.apply(sandbox, args);
        }

        evalWithDefault(expr) {
          return fakeTargetingContext.evalWithDefault(expr);
        }
      }
    );
    evalStub = fakeTargetingContext.evalWithDefault;
  });
  afterEach(() => {
    clock.restore();
    sandbox.restore();
    globals.restore();
  });
  it("should cache evaluation result", async () => {
    evalStub.resolves(true);
    let targetingContext = new global.TargetingContext();

    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl1" },
      targetingContext,
      sandbox.stub(),
      true
    );
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl2" },
      targetingContext,
      sandbox.stub(),
      true
    );
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl1" },
      targetingContext,
      sandbox.stub(),
      true
    );

    assert.calledTwice(evalStub);
  });
  it("should not cache evaluation result", async () => {
    evalStub.resolves(true);
    let targetingContext = new global.TargetingContext();

    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      false
    );
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      false
    );
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      false
    );

    assert.calledThrice(evalStub);
  });
  it("should expire cache entries", async () => {
    evalStub.resolves(true);
    let targetingContext = new global.TargetingContext();

    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      true
    );
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      true
    );
    clock.tick(5 * 60 * 1000 + 1);
    await ASRouterTargeting.checkMessageTargeting(
      { targeting: "jexl" },
      targetingContext,
      sandbox.stub(),
      true
    );

    assert.calledTwice(evalStub);
  });

  describe("#findMatchingMessage", () => {
    let matchStub;
    let messages = [
      { id: "FOO", targeting: "match" },
      { id: "BAR", targeting: "match" },
      { id: "BAZ" },
    ];
    beforeEach(() => {
      matchStub = sandbox
        .stub(ASRouterTargeting, "_isMessageMatch")
        .callsFake(message => message.targeting === "match");
    });
    it("should return an array of matches if returnAll is true", async () => {
      assert.deepEqual(
        await ASRouterTargeting.findMatchingMessage({
          messages,
          returnAll: true,
        }),
        [
          { id: "FOO", targeting: "match" },
          { id: "BAR", targeting: "match" },
        ]
      );
    });
    it("should return an empty array if no matches were found and returnAll is true", async () => {
      matchStub.returns(false);
      assert.deepEqual(
        await ASRouterTargeting.findMatchingMessage({
          messages,
          returnAll: true,
        }),
        []
      );
    });
    it("should return the first match if returnAll is false", async () => {
      assert.deepEqual(
        await ASRouterTargeting.findMatchingMessage({
          messages,
        }),
        messages[0]
      );
    });
    it("should return null if if no matches were found and returnAll is false", async () => {
      matchStub.returns(false);
      assert.deepEqual(
        await ASRouterTargeting.findMatchingMessage({
          messages,
        }),
        null
      );
    });
  });
});

/**
 * Messages should be sorted in the following order:
 * 1. Rank
 * 2. Priority
 * 3. If the message has targeting
 * 4. Order or randomization, depending on input
 */
describe("getSortedMessages", () => {
  let globals = new GlobalOverrider();
  let sandbox;
  beforeEach(() => {
    globals.set({ ASRouterPreferences });
    sandbox = sinon.createSandbox();
  });
  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  /**
   * assertSortsCorrectly - Tests to see if an array, when sorted with getSortedMessages,
   *                        returns the items in the expected order.
   *
   * @param {Message[]} expectedOrderArray - The array of messages in its expected order
   * @param {{}} options - The options param for getSortedMessages
   * @returns
   */
  function assertSortsCorrectly(expectedOrderArray, options) {
    const input = [...expectedOrderArray].reverse();
    const result = getSortedMessages(input, options);
    const indexes = result.map(message => expectedOrderArray.indexOf(message));
    return assert.equal(
      indexes.join(","),
      [...expectedOrderArray.keys()].join(","),
      "Messsages are out of order"
    );
  }

  it("should sort messages by priority, then by targeting", () => {
    assertSortsCorrectly([
      { priority: 100, targeting: "isFoo" },
      { priority: 100 },
      { priority: 99 },
      { priority: 1, targeting: "isFoo" },
      { priority: 1 },
      {},
    ]);
  });
  it("should sort messages by priority, then targeting, then order if ordered param is true", () => {
    assertSortsCorrectly(
      [
        { priority: 100, order: 4 },
        { priority: 100, order: 5 },
        { priority: 1, order: 3, targeting: "isFoo" },
        { priority: 1, order: 0 },
        { priority: 1, order: 1 },
        { priority: 1, order: 2 },
        { order: 0 },
      ],
      { ordered: true }
    );
  });
});

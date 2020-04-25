import { _ASRouter, MessageLoaderUtils } from "lib/ASRouter.jsm";
import { ASRouterTargeting, QueryCache } from "lib/ASRouterTargeting.jsm";
import {
  CHILD_TO_PARENT_MESSAGE_NAME,
  FAKE_LOCAL_MESSAGES,
  FAKE_LOCAL_PROVIDER,
  FAKE_LOCAL_PROVIDERS,
  FAKE_RECOMMENDATION,
  FAKE_REMOTE_MESSAGES,
  FAKE_REMOTE_PROVIDER,
  FAKE_REMOTE_SETTINGS_PROVIDER,
  FakeRemotePageManager,
  PARENT_TO_CHILD_MESSAGE_NAME,
} from "./constants";
import { actionCreators as ac } from "common/Actions.jsm";
import {
  ASRouterPreferences,
  TARGETING_PREFERENCES,
} from "lib/ASRouterPreferences.jsm";
import { ASRouterTriggerListeners } from "lib/ASRouterTriggerListeners.jsm";
import { CFRPageActions } from "lib/CFRPageActions.jsm";
import { GlobalOverrider } from "test/unit/utils";
import { PanelTestProvider } from "lib/PanelTestProvider.jsm";
import ProviderResponseSchema from "content-src/asrouter/schemas/provider-response.schema.json";
import { SnippetsTestMessageProvider } from "lib/SnippetsTestMessageProvider.jsm";
import MessageGroupSchema from "content-src/asrouter/schemas/message-group.schema.json";

const OUTGOING_MESSAGE_NAME = "ASRouter:parent-to-child";
const MESSAGE_PROVIDER_PREF_NAME =
  "browser.newtabpage.activity-stream.asrouter.providers.snippets";
const FAKE_PROVIDERS = [
  FAKE_LOCAL_PROVIDER,
  FAKE_REMOTE_PROVIDER,
  FAKE_REMOTE_SETTINGS_PROVIDER,
];
const FAKE_BUNDLE = [FAKE_LOCAL_MESSAGES[1], FAKE_LOCAL_MESSAGES[2]];
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
const FAKE_RESPONSE_HEADERS = { get() {} };

const USE_REMOTE_L10N_PREF =
  "browser.newtabpage.activity-stream.asrouter.useRemoteL10n";

// Creates a message object that looks like messages returned by
// RemotePageManager listeners
function fakeAsyncMessage(action) {
  return { data: action, target: new FakeRemotePageManager() };
}
// Create a message that looks like a user action
function fakeExecuteUserAction(action) {
  return fakeAsyncMessage({ data: action, type: "USER_ACTION" });
}

// eslint-disable-next-line max-statements
describe("ASRouter", () => {
  let Router;
  let globals;
  let channel;
  let sandbox;
  let messageBlockList;
  let providerBlockList;
  let messageImpressions;
  let providerImpressions;
  let groupImpressions;
  let previousSessionEnd;
  let fetchStub;
  let clock;
  let getStringPrefStub;
  let dispatchStub;
  let fakeAttributionCode;
  let FakeBookmarkPanelHub;
  let FakeToolbarBadgeHub;
  let FakeToolbarPanelHub;
  let FakeMomentsPageHub;
  let personalizedCfrScores;

  function createFakeStorage() {
    const getStub = sandbox.stub();
    getStub.returns(Promise.resolve());
    getStub
      .withArgs("messageBlockList")
      .returns(Promise.resolve(messageBlockList));
    getStub
      .withArgs("providerBlockList")
      .returns(Promise.resolve(providerBlockList));
    getStub
      .withArgs("messageImpressions")
      .returns(Promise.resolve(messageImpressions));
    getStub.withArgs("groupImpressions").resolves(groupImpressions);
    getStub
      .withArgs("providerImpressions")
      .returns(Promise.resolve(providerImpressions));
    getStub
      .withArgs("previousSessionEnd")
      .returns(Promise.resolve(previousSessionEnd));
    return {
      get: getStub,
      set: sandbox.stub().returns(Promise.resolve()),
    };
  }

  function setMessageProviderPref(value) {
    sandbox.stub(ASRouterPreferences, "providers").get(() => value);
  }

  async function createRouterAndInit(providers = FAKE_PROVIDERS) {
    setMessageProviderPref(providers);
    channel = new FakeRemotePageManager();
    dispatchStub = sandbox.stub();
    // `.freeze` to catch any attempts at modifying the object
    Router = new _ASRouter(Object.freeze(FAKE_LOCAL_PROVIDERS));
    await Router.init(channel, createFakeStorage(), dispatchStub);
  }

  beforeEach(async () => {
    globals = new GlobalOverrider();
    messageBlockList = [];
    providerBlockList = [];
    messageImpressions = {};
    providerImpressions = {};
    groupImpressions = {};
    previousSessionEnd = 100;
    sandbox = sinon.createSandbox();
    personalizedCfrScores = {};

    sandbox.spy(ASRouterPreferences, "init");
    sandbox.spy(ASRouterPreferences, "uninit");
    sandbox.spy(ASRouterPreferences, "addListener");
    sandbox.spy(ASRouterPreferences, "removeListener");
    sandbox.stub(ASRouterPreferences, "trailhead").get(() => {
      return { trailheadTriplet: "test" };
    });
    sandbox.replaceGetter(
      ASRouterPreferences,
      "personalizedCfrScores",
      () => personalizedCfrScores
    );

    clock = sandbox.useFakeTimers();
    fetchStub = sandbox
      .stub(global, "fetch")
      .withArgs("http://fake.com/endpoint")
      .resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve({ messages: FAKE_REMOTE_MESSAGES }),
        headers: FAKE_RESPONSE_HEADERS,
      });
    getStringPrefStub = sandbox.stub(global.Services.prefs, "getStringPref");

    fakeAttributionCode = {
      allowedCodeKeys: ["foo", "bar", "baz"],
      _clearCache: () => sinon.stub(),
      getAttrDataAsync: () => Promise.resolve({ content: "addonID" }),
    };
    FakeBookmarkPanelHub = {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      _forceShowMessage: sandbox.stub(),
    };
    FakeToolbarPanelHub = {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      forceShowMessage: sandbox.stub(),
      enableToolbarButton: sandbox.stub(),
    };
    FakeToolbarBadgeHub = {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      registerBadgeNotificationListener: sandbox.stub(),
    };
    FakeMomentsPageHub = {
      init: sandbox.stub(),
      uninit: sandbox.stub(),
      executeAction: sandbox.stub(),
    };
    globals.set({
      // Testing framework doesn't know how to `defineLazyModuleGetter` so we're
      // importing these modules into the global scope ourselves.
      GroupsConfigurationProvider: { getMessages: () => [] },
      ASRouterPreferences,
      TARGETING_PREFERENCES,
      ASRouterTargeting,
      ASRouterTriggerListeners,
      QueryCache,
      gURLBar: {},
      AttributionCode: fakeAttributionCode,
      SnippetsTestMessageProvider,
      PanelTestProvider,
      BookmarkPanelHub: FakeBookmarkPanelHub,
      ToolbarBadgeHub: FakeToolbarBadgeHub,
      ToolbarPanelHub: FakeToolbarPanelHub,
      MomentsPageHub: FakeMomentsPageHub,
      KintoHttpClient: class {
        bucket() {
          return this;
        }
        collection() {
          return this;
        }
        getRecord() {
          return Promise.resolve({ data: {} });
        }
      },
      Downloader: class {
        download() {
          return Promise.resolve("/path/to/download");
        }
      },
      ExperimentAPI: {
        getExperiment: sandbox.stub().returns({ branch: { value: [] } }),
        ready: sandbox.stub().resolves(),
      },
    });
    await createRouterAndInit();
  });
  afterEach(() => {
    ASRouterPreferences.uninit();
    sandbox.restore();
    globals.restore();
  });

  describe(".state", () => {
    it("should throw if an attempt to set .state was made", () => {
      assert.throws(() => {
        Router.state = {};
      });
    });
  });

  describe("#init", () => {
    it("should add a message listener to the RemotePageManager for incoming messages", () => {
      assert.calledWith(
        channel.addMessageListener,
        CHILD_TO_PARENT_MESSAGE_NAME
      );
      const [, listenerAdded] = channel.addMessageListener.firstCall.args;
      assert.isFunction(listenerAdded);
    });
    it("should set state.messageBlockList to the block list in persistent storage", async () => {
      messageBlockList = ["foo"];
      Router = new _ASRouter();
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.messageBlockList, ["foo"]);
    });
    it("should migrate provider impressions to group impressions", async () => {
      setMessageProviderPref([
        { id: "onboarding", type: "local", messages: [] },
      ]);
      providerImpressions = { onboarding: [1, 2, 3] };
      Router = new _ASRouter();
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.property(Router.state.groupImpressions, "onboarding");
      assert.deepEqual(
        Router.state.groupImpressions.onboarding,
        providerImpressions.onboarding
      );
    });
    it("should initialize all the hub providers", async () => {
      // ASRouter init called in `beforeEach` block above

      assert.calledOnce(FakeToolbarBadgeHub.init);
      assert.calledOnce(FakeToolbarPanelHub.init);
      assert.calledOnce(FakeBookmarkPanelHub.init);
      assert.calledOnce(FakeMomentsPageHub.init);

      assert.calledWithExactly(
        FakeToolbarBadgeHub.init,
        Router.waitForInitialized,
        {
          handleMessageRequest: Router.handleMessageRequest,
          addImpression: Router.addImpression,
          blockMessageById: Router.blockMessageById,
          dispatch: Router.dispatch,
          unblockMessageById: Router.unblockMessageById,
        }
      );

      assert.calledWithExactly(
        FakeToolbarPanelHub.init,
        Router.waitForInitialized,
        {
          getMessages: Router.handleMessageRequest,
          dispatch: Router.dispatch,
          handleUserAction: Router.handleUserAction,
        }
      );

      assert.calledWithExactly(
        FakeMomentsPageHub.init,
        Router.waitForInitialized,
        {
          handleMessageRequest: Router.handleMessageRequest,
          addImpression: Router.addImpression,
          blockMessageById: Router.blockMessageById,
          dispatch: Router.dispatch,
        }
      );

      assert.calledWithExactly(
        FakeBookmarkPanelHub.init,
        Router.handleMessageRequest,
        Router.addImpression,
        Router.dispatch
      );
    });
    it("should set state.messageImpressions to the messageImpressions object in persistent storage", async () => {
      // Note that messageImpressions are only kept if a message exists in router and has a .frequency property,
      // otherwise they will be cleaned up by .cleanupImpressions()
      const testMessage = { id: "foo", frequency: { lifetimeCap: 10 } };
      messageImpressions = { foo: [0, 1, 2] };
      setMessageProviderPref([
        { id: "onboarding", type: "local", messages: [testMessage] },
      ]);
      Router = new _ASRouter();
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.deepEqual(Router.state.messageImpressions, messageImpressions);
    });
    it("should clear impressions for groups that are not active", async () => {
      groupImpressions = { foo: [0, 1, 2] };
      Router = new _ASRouter();
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.notProperty(Router.state.groupImpressions, "foo");
    });
    it("should keep impressions for groups that are active", async () => {
      Router = new _ASRouter();
      await Router.init(channel, createFakeStorage(), dispatchStub);
      await Router.setState(() => {
        return {
          groups: [
            {
              id: "foo",
              enabled: true,
              frequency: {
                custom: [{ period: "daily", cap: 10 }],
                lifetime: Infinity,
              },
            },
          ],
          groupImpressions: { foo: [Date.now()] },
        };
      });
      Router.cleanupImpressions();

      assert.property(Router.state.groupImpressions, "foo");
      assert.lengthOf(Router.state.groupImpressions.foo, 1);
    });
    it("should await .loadMessagesFromAllProviders() and add messages from providers to state.messages", async () => {
      Router = new _ASRouter(Object.freeze(FAKE_LOCAL_PROVIDERS));

      const loadMessagesSpy = sandbox.spy(
        Router,
        "loadMessagesFromAllProviders"
      );
      await Router.init(channel, createFakeStorage(), dispatchStub);

      assert.calledOnce(loadMessagesSpy);
      assert.isArray(Router.state.messages);
      assert.lengthOf(
        Router.state.messages,
        FAKE_LOCAL_MESSAGES.length + FAKE_REMOTE_MESSAGES.length
      );
    });
    it("should load additional whitelisted hosts", async () => {
      getStringPrefStub.returns('["whitelist.com"]');
      await createRouterAndInit();

      assert.propertyVal(Router.WHITELIST_HOSTS, "whitelist.com", "preview");
      // Should still include the defaults
      assert.lengthOf(Object.keys(Router.WHITELIST_HOSTS), 3);
    });
    it("should fallback to defaults if pref parsing fails", async () => {
      getStringPrefStub.returns("err");
      await createRouterAndInit();

      assert.lengthOf(Object.keys(Router.WHITELIST_HOSTS), 2);
      assert.propertyVal(
        Router.WHITELIST_HOSTS,
        "snippets-admin.mozilla.org",
        "preview"
      );
      assert.propertyVal(
        Router.WHITELIST_HOSTS,
        "activity-stream-icons.services.mozilla.com",
        "production"
      );
    });
    it("should set this.dispatchToAS to the third parameter passed to .init()", async () => {
      assert.equal(Router.dispatchToAS, dispatchStub);
    });
    it("should set state.previousSessionEnd from IndexedDB", async () => {
      previousSessionEnd = 200;
      await createRouterAndInit();

      assert.equal(Router.state.previousSessionEnd, previousSessionEnd);
    });
    it("should dispatch a AS_ROUTER_INITIALIZED event to AS with ASRouterPreferences.specialConditions", async () => {
      assert.calledWith(
        Router.dispatchToAS,
        ac.BroadcastToContent({
          type: "AS_ROUTER_INITIALIZED",
          data: ASRouterPreferences.specialConditions,
        })
      );
    });
    it("should add observer for `intl:app-locales-changed`", async () => {
      sandbox.spy(global.Services.obs, "addObserver");
      await createRouterAndInit();

      assert.calledOnce(global.Services.obs.addObserver);
      assert.equal(
        global.Services.obs.addObserver.args[0][1],
        "intl:app-locales-changed"
      );
    });
    it("should add a pref observer", async () => {
      sandbox.spy(global.Services.prefs, "addObserver");
      await createRouterAndInit();

      assert.calledOnce(global.Services.prefs.addObserver);
      assert.calledWithExactly(
        global.Services.prefs.addObserver,
        USE_REMOTE_L10N_PREF,
        Router
      );
    });
    describe("lazily loading local test providers", () => {
      afterEach(() => {
        Router.uninit();
      });
      it("should add the local test providers on init if devtools are enabled", async () => {
        sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => true);

        await createRouterAndInit();

        assert.property(Router._localProviders, "SnippetsTestMessageProvider");
        assert.property(Router._localProviders, "PanelTestProvider");
      });
      it("should not add the local test providers on init if devtools are disabled", async () => {
        sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => false);

        await createRouterAndInit();

        assert.notProperty(
          Router._localProviders,
          "SnippetsTestMessageProvider"
        );
        assert.notProperty(Router._localProviders, "PanelTestProvider");
      });
    });
  });

  describe("preference changes", () => {
    it("should call ASRouterPreferences.init and add a listener on init", () => {
      assert.calledOnce(ASRouterPreferences.init);
      assert.calledWith(ASRouterPreferences.addListener, Router.onPrefChange);
    });
    it("should call ASRouterPreferences.uninit and remove the listener on uninit", () => {
      Router.uninit();
      assert.calledOnce(ASRouterPreferences.uninit);
      assert.calledWith(
        ASRouterPreferences.removeListener,
        Router.onPrefChange
      );
    });
    it("should send a AS_ROUTER_TARGETING_UPDATE message", async () => {
      const messageTargeted = {
        id: "1",
        campaign: "foocampaign",
        targeting: "true",
        groups: ["snippets"],
        provider: "snippets",
      };
      const messageNotTargeted = {
        id: "2",
        campaign: "foocampaign",
        groups: ["snippets"],
        provider: "snippets",
      };
      await Router.setState({
        messages: [messageTargeted, messageNotTargeted],
        providers: [{ id: "snippets" }],
      });
      sandbox.stub(ASRouterTargeting, "isMatch").resolves(false);

      await Router.onPrefChange("services.sync.username");

      assert.calledOnce(channel.sendAsyncMessage);
      const [, { type, data }] = channel.sendAsyncMessage.firstCall.args;
      assert.equal(type, "AS_ROUTER_TARGETING_UPDATE");
      assert.equal(data[0], messageTargeted.id);
      assert.lengthOf(data, 1);
    });
    it("should call loadMessagesFromAllProviders on pref change", () => {
      sandbox.spy(Router, "loadMessagesFromAllProviders");

      ASRouterPreferences.observe(null, null, MESSAGE_PROVIDER_PREF_NAME);

      assert.calledOnce(Router.loadMessagesFromAllProviders);
    });
    it("should update the list of providers on pref change", () => {
      const modifiedRemoteProvider = Object.assign({}, FAKE_REMOTE_PROVIDER, {
        url: "baz.com",
      });
      setMessageProviderPref([
        FAKE_LOCAL_PROVIDER,
        modifiedRemoteProvider,
        FAKE_REMOTE_SETTINGS_PROVIDER,
      ]);

      const { length } = Router.state.providers;

      ASRouterPreferences.observe(null, null, MESSAGE_PROVIDER_PREF_NAME);

      const provider = Router.state.providers.find(p => p.url === "baz.com");
      assert.lengthOf(Router.state.providers, length);
      assert.isDefined(provider);
    });
  });

  describe("setState", () => {
    it("should broadcast a message to update the admin tool on a state change if the asrouter.devtoolsEnabled pref is", async () => {
      sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => true);
      sandbox.stub(Router, "getTargetingParameters").resolves({});
      await Router.setState({ foo: 123 });

      assert.calledOnce(channel.sendAsyncMessage);
      assert.deepEqual(channel.sendAsyncMessage.firstCall.args[1], {
        type: "ADMIN_SET_STATE",
        data: Object.assign({}, Router.state, {
          providerPrefs: ASRouterPreferences.providers,
          userPrefs: ASRouterPreferences.getAllUserPreferences(),
          targetingParameters: {},
          trailhead: ASRouterPreferences.trailhead,
          errors: Router.errors,
        }),
      });
    });
    it("should not send a message on a state change asrouter.devtoolsEnabled pref is on", async () => {
      sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => false);
      await Router.setState({ foo: 123 });

      assert.notCalled(channel.sendAsyncMessage);
    });
  });

  describe("getTargetingParameters", () => {
    it("should return the targeting parameters", async () => {
      const stub = sandbox.stub().resolves("foo");
      const obj = { foo: 1 };
      sandbox.stub(obj, "foo").get(stub);
      const result = await Router.getTargetingParameters(obj, obj);

      assert.calledTwice(stub);
      assert.propertyVal(result, "foo", "foo");
    });
  });

  describe("evaluateExpression", () => {
    let stub;
    beforeEach(async () => {
      stub = sandbox.stub();
      stub.resolves("foo");
      sandbox.stub(ASRouterTargeting, "isMatch").callsFake(stub);
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should call ASRouterTargeting to evaluate", async () => {
      const targetStub = { sendAsyncMessage: sandbox.stub() };

      await Router.evaluateExpression(targetStub, {});

      assert.calledOnce(targetStub.sendAsyncMessage);
      assert.equal(
        targetStub.sendAsyncMessage.firstCall.args[1].data.evaluationStatus
          .result,
        "foo"
      );
      assert.isTrue(
        targetStub.sendAsyncMessage.firstCall.args[1].data.evaluationStatus
          .success
      );
    });
    it("should catch evaluation errors", async () => {
      stub.returns(Promise.reject(new Error("fake error")));
      const targetStub = { sendAsyncMessage: sandbox.stub() };

      await Router.evaluateExpression(targetStub, {});

      assert.isFalse(
        targetStub.sendAsyncMessage.firstCall.args[1].data.evaluationStatus
          .success
      );
    });
  });

  describe("#routeMessageToTarget", () => {
    let target;
    beforeEach(() => {
      sandbox.stub(CFRPageActions, "forceRecommendation");
      sandbox.stub(CFRPageActions, "addRecommendation");
      sandbox.stub(CFRPageActions, "showMilestone");
      target = { sendAsyncMessage: sandbox.stub() };
    });
    it("should route whatsnew_panel_message message to the right hub", () => {
      Router.routeMessageToTarget(
        { template: "whatsnew_panel_message" },
        target,
        "",
        true
      );

      assert.calledOnce(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route moments messages to the right hub", () => {
      Router.routeMessageToTarget(
        { template: "update_action" },
        target,
        "",
        true
      );

      assert.calledOnce(FakeMomentsPageHub.executeAction);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
    });
    it("should route toolbar_badge message to the right hub", () => {
      Router.routeMessageToTarget({ template: "toolbar_badge" }, target);

      assert.calledOnce(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route milestone_message to the right hub", () => {
      Router.routeMessageToTarget({ template: "milestone_message" }, target);

      assert.calledOnce(CFRPageActions.showMilestone);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route fxa_bookmark_panel message to the right hub force = true", () => {
      Router.routeMessageToTarget(
        { template: "fxa_bookmark_panel" },
        target,
        {},
        true
      );

      assert.calledOnce(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_doorhanger message to the right hub force = false", () => {
      Router.routeMessageToTarget(
        { template: "cfr_doorhanger" },
        target,
        { param: {} },
        false
      );

      assert.calledOnce(CFRPageActions.addRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_doorhanger message to the right hub force = true", () => {
      Router.routeMessageToTarget(
        { template: "cfr_doorhanger" },
        target,
        {},
        true
      );

      assert.calledOnce(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_urlbar_chiclet message to the right hub force = false", () => {
      Router.routeMessageToTarget(
        { template: "cfr_urlbar_chiclet" },
        target,
        { param: {} },
        false
      );

      assert.calledOnce(CFRPageActions.addRecommendation);
      const { args } = CFRPageActions.addRecommendation.firstCall;
      // Host should be null
      assert.isNull(args[1]);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_urlbar_chiclet message to the right hub force = true", () => {
      Router.routeMessageToTarget(
        { template: "cfr_urlbar_chiclet" },
        target,
        {},
        true
      );

      assert.calledOnce(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(target.sendAsyncMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route default to sending to content", () => {
      Router.routeMessageToTarget({ template: "snippets" }, target, {}, true);

      assert.calledOnce(target.sendAsyncMessage);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.showMilestone);
      assert.notCalled(FakeBookmarkPanelHub._forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
  });

  describe("#loadMessagesFromAllProviders", () => {
    function assertRouterContainsMessages(messages) {
      const messageIdsInRouter = Router.state.messages.map(m => m.id);
      for (const message of messages) {
        assert.include(messageIdsInRouter, message.id);
      }
    }

    it("should not trigger an update if not enough time has passed for a provider", async () => {
      await createRouterAndInit([
        {
          id: "remotey",
          type: "remote",
          enabled: true,
          url: "http://fake.com/endpoint",
          updateCycleInMs: 300,
        },
      ]);

      const previousState = Router.state;

      // Since we've previously gotten messages during init and we haven't advanced our fake timer,
      // no updates should be triggered.
      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should not trigger an update if we only have local providers", async () => {
      await createRouterAndInit([
        {
          id: "foo",
          type: "local",
          enabled: true,
          messages: FAKE_LOCAL_MESSAGES,
        },
      ]);

      const previousState = Router.state;

      clock.tick(300);

      await Router.loadMessagesFromAllProviders();
      assert.equal(Router.state, previousState);
    });
    it("should apply personalization if defined", async () => {
      personalizedCfrScores = { FOO: 1, BAR: 2 };
      const NEW_MESSAGES = [{ id: "FOO" }, { id: "BAR" }];

      fetchStub.withArgs("http://foo.com").resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve({ messages: NEW_MESSAGES }),
        headers: FAKE_RESPONSE_HEADERS,
      });

      await createRouterAndInit([
        {
          id: "cfr",
          personalized: true,
          personalizedModelVersion: "42",
          type: "remote",
          url: "http://foo.com",
          enabled: true,
          updateCycleInMs: 300,
        },
      ]);

      await Router.loadMessagesFromAllProviders();

      // Make sure messages are there
      assertRouterContainsMessages(NEW_MESSAGES);

      // Make sure they have a score and personalizedModelVersion
      for (const expectedMessage of NEW_MESSAGES) {
        const { id } = expectedMessage;
        const message = Router.state.messages.find(msg => msg.id === id);
        assert.propertyVal(message, "score", personalizedCfrScores[message.id]);
        assert.propertyVal(message, "personalizedModelVersion", "42");
      }
    });
    it("should update messages for a provider if enough time has passed, without removing messages for other providers", async () => {
      const NEW_MESSAGES = [{ id: "new_123" }];
      await createRouterAndInit([
        {
          id: "remotey",
          type: "remote",
          url: "http://fake.com/endpoint",
          enabled: true,
          updateCycleInMs: 300,
        },
        {
          id: "alocalprovider",
          type: "local",
          enabled: true,
          messages: FAKE_LOCAL_MESSAGES,
        },
      ]);
      fetchStub.withArgs("http://fake.com/endpoint").resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve({ messages: NEW_MESSAGES }),
        headers: FAKE_RESPONSE_HEADERS,
      });

      clock.tick(301);
      await Router.loadMessagesFromAllProviders();

      // These are the new messages
      assertRouterContainsMessages(NEW_MESSAGES);
      // These are the local messages that should not have been deleted
      assertRouterContainsMessages(FAKE_LOCAL_MESSAGES);
    });
    it("should parse the triggers in the messages and register the trigger listeners", async () => {
      sandbox.spy(
        ASRouterTriggerListeners.get("openURL"),
        "init"
      ); /* eslint-disable object-property-newline */

      /* eslint-disable object-curly-newline */ await createRouterAndInit([
        {
          id: "foo",
          type: "local",
          enabled: true,
          messages: [
            {
              id: "foo",
              template: "simple_template",
              trigger: { id: "firstRun" },
              content: { title: "Foo", body: "Foo123" },
            },
            {
              id: "bar1",
              template: "simple_template",
              trigger: {
                id: "openURL",
                params: ["www.mozilla.org", "www.mozilla.com"],
              },
              content: { title: "Bar1", body: "Bar123" },
            },
            {
              id: "bar2",
              template: "simple_template",
              trigger: { id: "openURL", params: ["www.example.com"] },
              content: { title: "Bar2", body: "Bar123" },
            },
          ],
        },
      ]); /* eslint-enable object-property-newline */
      /* eslint-enable object-curly-newline */ assert.calledTwice(
        ASRouterTriggerListeners.get("openURL").init
      );
      assert.calledWithExactly(
        ASRouterTriggerListeners.get("openURL").init,
        Router._triggerHandler,
        ["www.mozilla.org", "www.mozilla.com"],
        undefined
      );
      assert.calledWithExactly(
        ASRouterTriggerListeners.get("openURL").init,
        Router._triggerHandler,
        ["www.example.com"],
        undefined
      );
    });
    it("should gracefully handle RemoteSettings blowing up and dispatch undesired event", async () => {
      sandbox
        .stub(MessageLoaderUtils, "_getRemoteSettingsMessages")
        .rejects("fake error");
      await createRouterAndInit();
      assert.calledWith(Router.dispatchToAS, {
        data: {
          action: "asrouter_undesired_event",
          event: "ASR_RS_ERROR",
          event_context: "remotey-settingsy",
          message_id: "n/a",
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: "AS_ROUTER_TELEMETRY_USER_EVENT",
      });
    });
    it("should dispatch undesired event if RemoteSettings returns no messages", async () => {
      sandbox
        .stub(MessageLoaderUtils, "_getRemoteSettingsMessages")
        .resolves([]);
      await createRouterAndInit();
      assert.calledWith(Router.dispatchToAS, {
        data: {
          action: "asrouter_undesired_event",
          event: "ASR_RS_NO_MESSAGES",
          event_context: "remotey-settingsy",
          message_id: "n/a",
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: "AS_ROUTER_TELEMETRY_USER_EVENT",
      });
    });
    it("should download the attachment if RemoteSettings returns some messages", async () => {
      sandbox
        .stub(global.Services.locale, "appLocaleAsBCP47")
        .get(() => "en-US");
      sandbox
        .stub(MessageLoaderUtils, "_getRemoteSettingsMessages")
        .resolves([{ id: "message_1" }]);
      const spy = sandbox.spy();
      global.Downloader.prototype.download = spy;
      const provider = {
        id: "cfr",
        enabled: true,
        type: "remote-settings",
        bucket: "cfr",
      };
      await createRouterAndInit([provider]);

      assert.calledOnce(spy);
    });
    it("should dispatch undesired event if the ms-language-packs returns no messages", async () => {
      sandbox
        .stub(global.Services.locale, "appLocaleAsBCP47")
        .get(() => "en-US");
      sandbox
        .stub(MessageLoaderUtils, "_getRemoteSettingsMessages")
        .resolves([{ id: "message_1" }]);
      sandbox
        .stub(global.KintoHttpClient.prototype, "getRecord")
        .resolves(null);
      const provider = {
        id: "cfr",
        enabled: true,
        type: "remote-settings",
        bucket: "cfr",
      };
      await createRouterAndInit([provider]);

      assert.calledWith(Router.dispatchToAS, {
        data: {
          action: "asrouter_undesired_event",
          event: "ASR_RS_NO_MESSAGES",
          event_context: "ms-language-packs",
          message_id: "n/a",
        },
        meta: { from: "ActivityStream:Content", to: "ActivityStream:Main" },
        type: "AS_ROUTER_TELEMETRY_USER_EVENT",
      });
    });
  });

  describe("#_updateMessageProviders", () => {
    it("should correctly replace %STARTPAGE_VERSION% in remote provider urls", () => {
      // If this test fails, you need to update the constant STARTPAGE_VERSION in
      // ASRouter.jsm to match the `version` property of provider-response-schema.json
      const expectedStartpageVersion = ProviderResponseSchema.version;
      const provider = {
        id: "foo",
        enabled: true,
        type: "remote",
        url: "https://www.mozilla.org/%STARTPAGE_VERSION%/",
      };
      setMessageProviderPref([provider]);
      Router._updateMessageProviders();
      assert.equal(
        Router.state.providers[0].url,
        `https://www.mozilla.org/${parseInt(expectedStartpageVersion, 10)}/`
      );
    });
    it("should replace other params in remote provider urls by calling Services.urlFormater.formatURL", () => {
      const url = "https://www.example.com/";
      const replacedUrl = "https://www.foo.bar/";
      const stub = sandbox
        .stub(global.Services.urlFormatter, "formatURL")
        .withArgs(url)
        .returns(replacedUrl);
      const provider = { id: "foo", enabled: true, type: "remote", url };
      setMessageProviderPref([provider]);
      Router._updateMessageProviders();
      assert.calledOnce(stub);
      assert.calledWithExactly(stub, url);
      assert.equal(Router.state.providers[0].url, replacedUrl);
    });
    it("should only add the providers that are enabled", () => {
      const providers = [
        {
          id: "foo",
          enabled: false,
          type: "remote",
          url: "https://www.foo.com/",
        },
        {
          id: "bar",
          enabled: true,
          type: "remote",
          url: "https://www.bar.com/",
        },
      ];
      setMessageProviderPref(providers);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 1);
      assert.equal(Router.state.providers[0].id, providers[1].id);
    });
    it("should return provider `foo` because both categories are enabled", () => {
      const providers = [
        {
          id: "foo",
          enabled: true,
          categories: ["cfrFeatures", "cfrAddons"],
          type: "remote",
          url: "https://www.foo.com/",
        },
      ];
      sandbox.stub(ASRouterPreferences, "providers").value(providers);
      sandbox
        .stub(ASRouterPreferences, "getUserPreference")
        .withArgs("cfrFeatures")
        .returns(true)
        .withArgs("cfrAddons")
        .returns(true);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 1);
      assert.equal(Router.state.providers[0].id, providers[0].id);
    });
    it("should return provider `foo` because at least 1 category is enabled", () => {
      const providers = [
        {
          id: "foo",
          enabled: true,
          categories: ["cfrFeatures", "cfrAddons"],
          type: "remote",
          url: "https://www.foo.com/",
        },
      ];
      sandbox.stub(ASRouterPreferences, "providers").value(providers);
      sandbox
        .stub(ASRouterPreferences, "getUserPreference")
        .withArgs("cfrFeatures")
        .returns(false)
        .withArgs("cfrAddons")
        .returns(true);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 1);
      assert.equal(Router.state.providers[0].id, providers[0].id);
    });
    it("should not return provider `foo` because no categories are enabled", () => {
      const providers = [
        {
          id: "foo",
          enabled: true,
          categories: ["cfrFeatures", "cfrAddons"],
          type: "remote",
          url: "https://www.foo.com/",
        },
      ];
      sandbox.stub(ASRouterPreferences, "providers").value(providers);
      sandbox
        .stub(ASRouterPreferences, "getUserPreference")
        .withArgs("cfrFeatures")
        .returns(false)
        .withArgs("cfrAddons")
        .returns(false);
      Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 0);
    });
  });

  describe("#handleMessageRequest", () => {
    beforeEach(async () => {
      await Router.setState(() => ({
        providers: [{ id: "snippets" }, { id: "badge" }],
      }));
    });
    it("should not return a blocked message", async () => {
      // Block all messages except the first
      await Router.setState(() => ({
        messages: [
          { id: "foo", provider: "snippets", groups: ["snippets"] },
          { id: "bar", provider: "snippets", groups: ["snippets"] },
        ],
        messageBlockList: ["foo"],
      }));
      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.equal(result.id, "bar");
    });
    it("should not return a message from a disabled group", async () => {
      sandbox
        .stub(ASRouterTargeting, "findMatchingMessage")
        .callsFake(({ messages }) => messages[0]);
      // Block all messages except the first
      await Router.setState(() => ({
        messages: [
          { id: "foo", provider: "snippets", groups: ["snippets"] },
          { id: "bar", provider: "snippets", groups: ["snippets"] },
        ],
        groups: [{ id: "snippets", enabled: false }],
      }));
      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.isNull(result);
    });
    it("should not return a message from a blocked campaign", async () => {
      // Block all messages except the first
      await Router.setState(() => ({
        messages: [
          {
            id: "foo",
            provider: "snippets",
            campaign: "foocampaign",
            groups: ["snippets"],
          },
          { id: "bar", provider: "snippets", groups: ["snippets"] },
        ],
        messageBlockList: ["foocampaign"],
      }));

      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });

      assert.equal(result.id, "bar");
    });
    it("should not return a message from a blocked provider", async () => {
      // There are only two providers; block the FAKE_LOCAL_PROVIDER, leaving
      // only FAKE_REMOTE_PROVIDER unblocked, which provides only one message
      await Router.setState(() => ({
        providerBlockList: ["snippets"],
      }));

      await Router.setState(() => ({
        messages: [{ id: "foo", provider: "snippets" }],
        messageBlockList: ["foocampaign"],
      }));

      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });

      assert.isNull(result);
    });
    it("should not return a message excluded by the provider", async () => {
      // There are only two providers; block the FAKE_LOCAL_PROVIDER, leaving
      // only FAKE_REMOTE_PROVIDER unblocked, which provides only one message
      sandbox.stub(Router, "loadMessagesFromAllProviders");
      await Router.setState(() => ({
        providers: [{ id: "snippets", exclude: ["foo"] }],
      }));

      await Router.setState(() => ({
        messages: [{ id: "foo", provider: "snippets" }],
        messageBlockList: ["foocampaign"],
      }));

      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.isNull(result);
    });
    it("should not return a message if the frequency cap has been hit", async () => {
      sandbox.stub(Router, "isBelowFrequencyCaps").returns(false);
      await Router.setState(() => ({
        messages: [{ id: "foo", provider: "snippets" }],
      }));
      const result = await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.isNull(result);
    });
    it("should get unblocked messages that match the trigger", async () => {
      const message1 = {
        id: "1",
        campaign: "foocampaign",
        trigger: { id: "foo" },
        groups: ["snippets"],
        provider: "snippets",
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        trigger: { id: "bar" },
        groups: ["snippets"],
        provider: "snippets",
      };
      await Router.setState({ messages: [message2, message1] });
      // Just return the first message provided as arg
      sandbox
        .stub(ASRouterTargeting, "findMatchingMessage")
        .callsFake(({ messages }) => messages[0]);

      const result = Router.handleMessageRequest({ triggerId: "foo" });

      assert.deepEqual(result, message1);
    });
    it("should get unblocked messages that match trigger and template", async () => {
      const message1 = {
        id: "1",
        campaign: "foocampaign",
        template: "badge",
        trigger: { id: "foo" },
        groups: ["badge"],
        provider: "badge",
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        template: "snippet",
        trigger: { id: "foo" },
        groups: ["snippets"],
        provider: "snippets",
      };
      await Router.setState({ messages: [message2, message1] });
      // Just return the first message provided as arg
      sandbox
        .stub(ASRouterTargeting, "findMatchingMessage")
        .callsFake(({ messages }) => messages[0]);

      const result = Router.handleMessageRequest({
        triggerId: "foo",
        template: "badge",
      });

      assert.deepEqual(result, message1);
    });
    it("should have messageImpressions in the message context", () => {
      assert.propertyVal(
        Router._getMessagesContext(),
        "messageImpressions",
        Router.state.messageImpressions
      );
    });
    it("should return all unblocked messages that match the template, trigger if returnAll=true", async () => {
      const message1 = {
        provider: "whats_new",
        id: "1",
        template: "whatsnew_panel_message",
        trigger: { id: "whatsNewPanelOpened" },
        groups: ["whats_new"],
      };
      const message2 = {
        provider: "whats_new",
        id: "2",
        template: "whatsnew_panel_message",
        trigger: { id: "whatsNewPanelOpened" },
        groups: ["whats_new"],
      };
      const message3 = {
        provider: "whats_new",
        id: "3",
        template: "badge",
        groups: ["whats_new"],
      };
      sandbox
        .stub(ASRouterTargeting, "findMatchingMessage")
        .callsFake(() => [message2, message1]);
      await Router.setState({
        messages: [message3, message2, message1],
        providers: [{ id: "whats_new" }],
      });
      const result = await Router.handleMessageRequest({
        template: "whatsnew_panel_message",
        triggerId: "whatsNewPanelOpened",
        returnAll: true,
      });

      assert.deepEqual(result, [message2, message1]);
    });
    it("should forward trigger param info", async () => {
      const trigger = {
        triggerId: "foo",
        triggerParam: "bar",
        triggerContext: "context",
      };
      const message1 = {
        id: "1",
        campaign: "foocampaign",
        trigger: { id: "foo" },
        groups: ["snippets"],
        provider: "snippets",
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        trigger: { id: "bar" },
        groups: ["badge"],
        provider: "badge",
      };
      await Router.setState({ messages: [message2, message1] });
      // Just return the first message provided as arg
      const stub = sandbox.stub(ASRouterTargeting, "findMatchingMessage");

      Router.handleMessageRequest(trigger);

      assert.calledOnce(stub);

      const [options] = stub.firstCall.args;
      assert.propertyVal(options.trigger, "id", trigger.triggerId);
      assert.propertyVal(options.trigger, "param", trigger.triggerParam);
      assert.propertyVal(options.trigger, "context", trigger.triggerContext);
    });
    it("should cache snippets messages", async () => {
      const trigger = {
        triggerId: "foo",
        triggerParam: "bar",
        triggerContext: "context",
      };
      const message1 = {
        id: "1",
        provider: "snippets",
        campaign: "foocampaign",
        trigger: { id: "foo" },
        groups: ["snippets"],
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        trigger: { id: "bar" },
        groups: ["snippets"],
      };
      await Router.setState({ messages: [message2, message1] });
      // Just return the first message provided as arg
      const stub = sandbox.stub(ASRouterTargeting, "findMatchingMessage");

      Router.handleMessageRequest(trigger);

      assert.calledOnce(stub);

      const [options] = stub.firstCall.args;
      assert.propertyVal(options, "shouldCache", true);
    });
    it("should not cache badge messages", async () => {
      const trigger = {
        triggerId: "bar",
        triggerParam: "bar",
        triggerContext: "context",
      };
      const message1 = {
        id: "1",
        provider: "snippets",
        campaign: "foocampaign",
        trigger: { id: "foo" },
        groups: ["snippets"],
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        trigger: { id: "bar" },
        groups: ["badge"],
        provider: "badge",
      };
      await Router.setState({ messages: [message2, message1] });
      // Just return the first message provided as arg
      const stub = sandbox.stub(ASRouterTargeting, "findMatchingMessage");

      Router.handleMessageRequest(trigger);

      assert.calledOnce(stub);

      const [options] = stub.firstCall.args;
      assert.propertyVal(options, "shouldCache", false);
    });
    it("should filter out messages without a trigger (or different) when a triggerId is defined", async () => {
      const trigger = { triggerId: "foo" };
      const message1 = {
        id: "1",
        campaign: "foocampaign",
        trigger: { id: "foo" },
        groups: ["snippets"],
        provider: "snippets",
      };
      const message2 = {
        id: "2",
        campaign: "foocampaign",
        trigger: { id: "bar" },
        groups: ["snippets"],
        provider: "snippets",
      };
      const message3 = {
        id: "3",
        campaign: "bazcampaign",
        groups: ["snippets"],
        provider: "snippets",
      };
      await Router.setState({
        messages: [message2, message1, message3],
        groups: [{ id: "snippets", enabled: true }],
      });
      // Just return the first message provided as arg
      sandbox
        .stub(ASRouterTargeting, "findMatchingMessage")
        .callsFake(args => args.messages);

      const result = Router.handleMessageRequest(trigger);

      assert.lengthOf(result, 1);
      assert.deepEqual(result[0], message1);
    });
  });

  describe("#uninit", () => {
    it("should remove the message listener on the RemotePageManager", () => {
      const [, listenerAdded] = channel.addMessageListener.firstCall.args;
      assert.isFunction(listenerAdded);

      Router.uninit();

      assert.calledWith(
        channel.removeMessageListener,
        CHILD_TO_PARENT_MESSAGE_NAME,
        listenerAdded
      );
    });
    it("should unregister the trigger listeners", () => {
      for (const listener of ASRouterTriggerListeners.values()) {
        sandbox.spy(listener, "uninit");
      }

      Router.uninit();

      for (const listener of ASRouterTriggerListeners.values()) {
        assert.calledOnce(listener.uninit);
      }
    });
    it("should set .dispatchToAS to null", () => {
      Router.uninit();
      assert.isNull(Router.dispatchToAS);
    });
    it("should save previousSessionEnd", () => {
      Router.uninit();

      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(
        Router._storage.set,
        "previousSessionEnd",
        sinon.match.number
      );
    });
    it("should remove the observer for `intl:app-locales-changed`", () => {
      sandbox.spy(global.Services.obs, "removeObserver");
      Router.uninit();

      assert.calledOnce(global.Services.obs.removeObserver);
      assert.equal(
        global.Services.obs.removeObserver.args[0][1],
        "intl:app-locales-changed"
      );
    });
    it("should remove the pref observer for `USE_REMOTE_L10N_PREF`", async () => {
      sandbox.spy(global.Services.prefs, "removeObserver");
      Router.uninit();

      // Grab the last call as #uninit() also involves multiple calls of `Services.prefs.removeObserver`.
      const call = global.Services.prefs.removeObserver.lastCall;
      assert.calledWithExactly(call, USE_REMOTE_L10N_PREF, Router);
    });
  });

  describe("onMessage", () => {
    describe("#onMessage: NEWTAB_MESSAGE_REQUEST", () => {
      it("should send a message back to the to the target", async () => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
        // force the only message to be a regular message so getRandomItemFromArray picks it
        await Router.setState({
          messages: [
            {
              id: "foo",
              provider: "snippets",
              groups: ["snippets"],
            },
          ],
          providers: [{ id: "snippets" }],
        });
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        await Router.onMessage(msg);

        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          {
            type: "SET_MESSAGE",
            data: {
              id: "foo",
              groups: ["snippets"],
              provider: "snippets",
            },
          }
        );
      });
      it("should send a message back to the target if there is a bundle, too", async () => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
        // force the only message to be a bundled message so getRandomItemFromArray picks it
        sandbox.stub(Router, "_findProvider").returns(null);
        await Router.setState({
          messages: [
            {
              id: "foo1",
              groups: ["snippets"],
              provider: "snippets",
              template: "simple_template",
              bundled: 1,
              content: { title: "Foo1", body: "Foo123-1" },
            },
          ],
          providers: [{ id: "snippets" }],
        });
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        await Router.onMessage(msg);
        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME
        );
        assert.equal(
          msg.target.sendAsyncMessage.firstCall.args[1].type,
          "SET_BUNDLED_MESSAGES"
        );
      });
      it("should properly order the message's bundle if specified", async () => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
        // force the only messages to be a bundled messages so getRandomItemFromArray picks one of them
        sandbox.stub(Router, "_findProvider").returns(null);
        const firstMessage = {
          id: "foo2",
          groups: ["snippets"],
          provider: "snippets",
          template: "simple_template",
          bundled: 2,
          order: 1,
          content: { title: "Foo2", body: "Foo123-2" },
        };
        const secondMessage = {
          id: "foo1",
          groups: ["snippets"],
          provider: "snippets",
          template: "simple_template",
          bundled: 2,
          order: 2,
          content: { title: "Foo1", body: "Foo123-1" },
        };
        await Router.setState({
          messages: [secondMessage, firstMessage],
          providers: [{ id: "snippets" }],
        });
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        await Router.onMessage(msg);
        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME
        );
        assert.equal(
          msg.target.sendAsyncMessage.firstCall.args[1].type,
          "SET_BUNDLED_MESSAGES"
        );
        assert.equal(
          msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[0].content,
          firstMessage.content
        );
        assert.equal(
          msg.target.sendAsyncMessage.firstCall.args[1].data.bundle[1].content,
          secondMessage.content
        );
      });
      it("should return a null bundle if we do not have enough messages to fill the bundle", async () => {
        // force the only message to be a bundled message that needs 2 messages in the bundle
        await Router.setState({
          messages: [
            {
              id: "foo1",
              template: "simple_template",
              bundled: 2,
              content: { title: "Foo1", body: "Foo123-1" },
              groups: ["bundle"],
              provider: "snippets",
            },
          ],
          providers: [{ id: "snippets" }],
        });
        const bundle = await Router._getBundledMessages(
          Router.state.messages[0]
        );
        assert.equal(bundle, null);
      });
      it("should send down extra attributes in the bundle if they exist", async () => {
        sandbox.stub(Router, "_findProvider").returns({
          getExtraAttributes() {
            return Promise.resolve({ header: "header" });
          },
        });
        await Router.setState({
          messages: [
            {
              id: "foo1",
              template: "simple_template",
              bundled: 1,
              content: { title: "Foo1", body: "Foo123-1" },
              groups: ["snippets"],
            },
          ],
          providers: [{ id: "snippets" }],
        });
        const result = await Router._getBundledMessages(
          Router.state.messages[0]
        );
        assert.equal(result.extraTemplateStrings.header, "header");
      });
      it("should send a CLEAR_ALL message if no bundle available", async () => {
        // force the only message to be a bundled message that needs 2 messages in the bundle
        await Router.setState({
          messages: [
            {
              id: "foo1",
              groups: ["snippets"],
              provider: "snippets",
              template: "simple_template",
              bundled: 2,
              content: { title: "Foo1", body: "Foo123-1" },
            },
          ],
        });
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        await Router.onMessage(msg);
        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_ALL" }
        );
      });
      it("should send a CLEAR_ALL message if no messages are available", async () => {
        await Router.setState({ messages: [] });
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        await Router.onMessage(msg);

        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_ALL" }
        );
      });
    });
    describe("#_addPreviewEndpoint", () => {
      it("should make a request to the provided endpoint on NEWTAB_MESSAGE_REQUEST", async () => {
        const url = "https://snippets-admin.mozilla.org/foo";
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: { endpoint: { url } },
        });
        await Router.onMessage(msg);

        assert.calledWith(global.fetch, url);
        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should make a request to the provided endpoint on ADMIN_CONNECT_STATE and remove the endpoint", async () => {
        const url = "https://snippets-admin.mozilla.org/foo";
        const msg = fakeAsyncMessage({
          type: "ADMIN_CONNECT_STATE",
          data: { endpoint: { url } },
        });
        await Router.onMessage(msg);

        assert.calledWith(global.fetch, url);
        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should dispatch SNIPPETS_PREVIEW_MODE when adding a preview endpoint", async () => {
        const url = "https://snippets-admin.mozilla.org/foo";
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: { endpoint: { url } },
        });
        await Router.onMessage(msg);

        assert.calledWithExactly(
          Router.dispatchToAS,
          ac.OnlyToOneContent(
            { type: "SNIPPETS_PREVIEW_MODE" },
            msg.target.portID
          )
        );
      });
      it("should not add a url that is not from a whitelisted host", async () => {
        const url = "https://mozilla.org";
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: { endpoint: { url } },
        });
        await Router.onMessage(msg);

        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should reject bad urls", async () => {
        const url = "foo";
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: { endpoint: { url } },
        });
        await Router.onMessage(msg);

        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should handle onboarding message provider", async () => {
        const handleMessageRequestStub = sandbox.stub(
          Router,
          "handleMessageRequest"
        );
        handleMessageRequestStub
          .withArgs({
            template: "extended_triplets",
          })
          .resolves({ id: "foo" });
        sandbox.stub(Router, "sendNewTabMessage").resolves();
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: {},
        });
        await Router.onMessage(msg);

        assert.calledOnce(Router.sendNewTabMessage);
      });
      it("should fallback to snippets if onboarding message provider returned none", async () => {
        const handleMessageRequestStub = sandbox.stub(
          Router,
          "handleMessageRequest"
        );
        handleMessageRequestStub
          .withArgs({
            template: "extended_triplets",
          })
          .resolves(null);
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: {},
        });
        await Router.onMessage(msg);

        assert.calledTwice(handleMessageRequestStub);
        assert.calledWithExactly(handleMessageRequestStub, {
          template: "extended_triplets",
        });
        assert.calledWithExactly(handleMessageRequestStub, {
          provider: "snippets",
        });
      });
      it("should record telemetry for message request duration", async () => {
        const startTelemetryStopwatch = sandbox.stub(
          global.TelemetryStopwatch,
          "start"
        );
        const finishTelemetryStopwatch = sandbox.stub(
          global.TelemetryStopwatch,
          "finish"
        );
        sandbox.stub(Router, "handleMessageRequest");
        const msg = fakeAsyncMessage({
          type: "NEWTAB_MESSAGE_REQUEST",
          data: {},
        });
        await Router.onMessage(msg);

        assert.calledOnce(startTelemetryStopwatch);
        assert.calledWithExactly(
          startTelemetryStopwatch,
          "MS_MESSAGE_REQUEST_TIME_MS",
          { port: msg.target.portID }
        );
        assert.calledOnce(finishTelemetryStopwatch);
        assert.calledWithExactly(
          finishTelemetryStopwatch,
          "MS_MESSAGE_REQUEST_TIME_MS",
          { port: msg.target.portID }
        );
      });
    });

    describe("#onMessage: BLOCK_MESSAGE_BY_ID", () => {
      it("should add the id to the messageBlockList and broadcast a CLEAR_MESSAGE message with the id", async () => {
        const msg = fakeAsyncMessage({
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: "foo" },
        });
        await Router.onMessage(msg);

        assert.isTrue(Router.state.messageBlockList.includes("foo"));
        assert.calledWith(
          channel.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_MESSAGE", data: { id: "foo" } }
        );
      });
      it("should only send CLEAR_MESSAGE to preloaded if action.data.preloadedOnly is true", async () => {
        sandbox.stub(Router, "sendAsyncMessageToPreloaded");
        const msg = fakeAsyncMessage({
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: "foo", preloadedOnly: true },
        });
        await Router.onMessage(msg);

        assert.calledWith(Router.sendAsyncMessageToPreloaded, {
          type: "CLEAR_MESSAGE",
          data: { id: "foo" },
        });
      });
      it("should add the campaign to the messageBlockList instead of id if .campaign is specified and not select messages of that campaign again", async () => {
        await Router.setState({
          messages: [
            { id: "1", campaign: "foocampaign" },
            { id: "2", campaign: "foocampaign" },
          ],
        });
        const msg = fakeAsyncMessage({
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: "1" },
        });
        await Router.onMessage(msg);

        assert.isTrue(Router.state.messageBlockList.includes("foocampaign"));
        assert.isEmpty(Router.state.messages.filter(Router.isUnblockedMessage));
      });
      it("should not broadcast CLEAR_MESSAGE if preventDismiss is true", async () => {
        const msg = fakeAsyncMessage({
          type: "BLOCK_MESSAGE_BY_ID",
          data: { id: "foo", preventDismiss: true },
        });
        await Router.onMessage(msg);

        assert.notCalled(channel.sendAsyncMessage);
      });
    });

    describe("#onMessage: DISMISS_MESSAGE_BY_ID", () => {
      it("should reply with CLEAR_MESSAGE with the correct id", async () => {
        const msg = fakeAsyncMessage({
          type: "DISMISS_MESSAGE_BY_ID",
          data: { id: "foo" },
        });

        await Router.onMessage(msg);

        assert.calledWith(
          channel.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_MESSAGE", data: { id: "foo" } }
        );
      });
    });

    describe("#onMessage: BLOCK_PROVIDER_BY_ID", () => {
      it("should add the provider id to the providerBlockList and broadcast a CLEAR_PROVIDER with the provider id", async () => {
        const msg = fakeAsyncMessage({
          type: "BLOCK_PROVIDER_BY_ID",
          data: { id: "bar" },
        });
        await Router.onMessage(msg);

        assert.isTrue(Router.state.providerBlockList.includes("bar"));
        assert.calledWith(
          channel.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_PROVIDER", data: { id: "bar" } }
        );
      });
    });

    describe("#onMessage: UNBLOCK_MESSAGE_BY_ID", () => {
      it("should remove the id from the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({ type: "BLOCK_MESSAGE_BY_ID", data: { id: "foo" } })
        );
        assert.isTrue(Router.state.messageBlockList.includes("foo"));
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_MESSAGE_BY_ID",
            data: { id: "foo" },
          })
        );

        assert.isFalse(Router.state.messageBlockList.includes("foo"));
      });
      it("should remove the campaign from the messageBlockList if it is defined", async () => {
        await Router.setState({ messages: [{ id: "1", campaign: "foo" }] });
        await Router.onMessage(
          fakeAsyncMessage({ type: "BLOCK_MESSAGE_BY_ID", data: { id: "1" } })
        );
        assert.isTrue(
          Router.state.messageBlockList.includes("foo"),
          "blocklist has campaign id"
        );
        await Router.onMessage(
          fakeAsyncMessage({ type: "UNBLOCK_MESSAGE_BY_ID", data: { id: "1" } })
        );

        assert.isFalse(
          Router.state.messageBlockList.includes("foo"),
          "campaign id removed from blocklist"
        );
      });
      it("should save the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_MESSAGE_BY_ID",
            data: { id: "foo" },
          })
        );

        assert.calledWithExactly(Router._storage.set, "messageBlockList", []);
      });
    });

    describe("#onMessage: UNBLOCK_PROVIDER_BY_ID", () => {
      it("should remove the id from the providerBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "BLOCK_PROVIDER_BY_ID",
            data: { id: "foo" },
          })
        );
        assert.isTrue(Router.state.providerBlockList.includes("foo"));
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_PROVIDER_BY_ID",
            data: { id: "foo" },
          })
        );

        assert.isFalse(Router.state.providerBlockList.includes("foo"));
      });
      it("should save the providerBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_PROVIDER_BY_ID",
            data: { id: "foo" },
          })
        );

        assert.calledWithExactly(Router._storage.set, "providerBlockList", []);
      });
    });

    describe("#onMessage: BLOCK_BUNDLE", () => {
      it("should add all the ids in the bundle to the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "BLOCK_BUNDLE",
            data: { bundle: FAKE_BUNDLE },
          })
        );
        assert.isTrue(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id)
        );
        assert.isTrue(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id)
        );
      });
      it("should save the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "BLOCK_BUNDLE",
            data: { bundle: FAKE_BUNDLE },
          })
        );

        assert.calledWithExactly(Router._storage.set, "messageBlockList", [
          FAKE_BUNDLE[0].id,
          FAKE_BUNDLE[1].id,
        ]);
      });
    });

    describe("#onMessage: UNBLOCK_BUNDLE", () => {
      it("should remove all the ids in the bundle from the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "BLOCK_BUNDLE",
            data: { bundle: FAKE_BUNDLE },
          })
        );
        assert.isTrue(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id)
        );
        assert.isTrue(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id)
        );
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_BUNDLE",
            data: { bundle: FAKE_BUNDLE },
          })
        );

        assert.isFalse(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id)
        );
        assert.isFalse(
          Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id)
        );
      });
      it("should save the messageBlockList", async () => {
        await Router.onMessage(
          fakeAsyncMessage({
            type: "UNBLOCK_BUNDLE",
            data: { bundle: FAKE_BUNDLE },
          })
        );

        assert.calledWithExactly(Router._storage.set, "messageBlockList", []);
      });
    });

    describe("#onMessage: ADMIN_CONNECT_STATE", () => {
      it("should send a message containing the whole state", async () => {
        sandbox.stub(Router, "getTargetingParameters").resolves({});
        const msg = fakeAsyncMessage({ type: "ADMIN_CONNECT_STATE" });

        await Router.onMessage(msg);
        assert.calledOnce(msg.target.sendAsyncMessage);
        assert.deepEqual(msg.target.sendAsyncMessage.firstCall.args[1], {
          type: "ADMIN_SET_STATE",
          data: Object.assign({}, Router.state, {
            providerPrefs: ASRouterPreferences.providers,
            userPrefs: ASRouterPreferences.getAllUserPreferences(),
            targetingParameters: {},
            trailhead: ASRouterPreferences.trailhead,
            errors: Router.errors,
          }),
        });
      });
    });

    describe("#onMessage: NEWTAB_MESSAGE_REQUEST", () => {
      beforeEach(() => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
      });
      it("should call sendNewTabMessage on NEWTAB_MESSAGE_REQUEST", async () => {
        sandbox.stub(Router, "sendNewTabMessage").resolves();
        const data = { endpoint: "foo" };
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST", data });

        await Router.onMessage(msg);

        assert.calledOnce(Router.sendNewTabMessage);
        assert.calledWithExactly(
          Router.sendNewTabMessage,
          sinon.match.instanceOf(FakeRemotePageManager),
          data
        );
      });
      it("should return the preview message if that's available and remove it from Router.state", async () => {
        const expectedObj = {
          id: "foo",
          groups: ["preview"],
          provider: "preview",
        };
        Router.setState({
          messages: [expectedObj],
          providers: [{ id: "preview" }],
        });

        await Router.sendNewTabMessage(channel, { endpoint: "foo.com" });

        assert.calledWith(
          channel.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "SET_MESSAGE", data: expectedObj }
        );
        assert.isUndefined(
          Router.state.messages.find(m => m.provider === "preview")
        );
      });
      it("should call _getBundledMessages if we request a message that needs to be bundled", async () => {
        sandbox.stub(Router, "_getBundledMessages").resolves();
        // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
        const [, testMessage] = Router.state.messages;
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage.id },
        });
        await Router.onMessage(msg);

        assert.calledOnce(Router._getBundledMessages);
      });
      it("should properly pick another message of the same template if it is bundled; force = true", async () => {
        // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
        const [, testMessage1, testMessage2] = Router.state.messages;
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage1.id },
        });
        await Router.onMessage(msg);

        // Expected object should have some properties of the original message it picked (testMessage1)
        // plus the bundled content of the others that it picked of the same template (testMessage2)
        const expectedObj = {
          template: testMessage1.template,
          provider: testMessage1.provider,
          bundle: [
            { content: testMessage1.content, id: testMessage1.id, order: 1 },
            { content: testMessage2.content, id: testMessage2.id },
          ],
        };
        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "SET_BUNDLED_MESSAGES", data: expectedObj }
        );
      });
      it("should properly pick another message of the same template if it is bundled; force = false", async () => {
        // forcefully pick a message which needs to be bundled (the second message in FAKE_LOCAL_MESSAGES)
        const [, testMessage1, testMessage2] = Router.state.messages;
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage1.id },
        });
        await Router.setMessageById(testMessage1.id, msg.target, false);

        // Expected object should have some properties of the original message it picked (testMessage1)
        // plus the bundled content of the others that it picked of the same template (testMessage2)
        const expectedObj = {
          template: testMessage1.template,
          provider: testMessage1.provider,
          bundle: [
            { content: testMessage1.content, id: testMessage1.id, order: 1 },
            {
              content: testMessage2.content,
              id: testMessage2.id,
              order: 2,
              blockOnClick: false,
            },
          ],
        };
        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "SET_BUNDLED_MESSAGES", data: expectedObj }
        );
      });
      it("should get the bundle and send the message if the message has a bundle", async () => {
        sandbox.stub(Router, "sendNewTabMessage").resolves();
        const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
        msg.bundled = 2; // force this message to want to be bundled
        await Router.onMessage(msg);
        assert.calledOnce(Router.sendNewTabMessage);
      });
    });

    describe("#onMessage: TRIGGER", () => {
      it("should pass the trigger to ASRouterTargeting on TRIGGER message", async () => {
        await Router.setState({
          messages: [
            {
              id: "foo1",
              provider: "onboarding",
              template: "onboarding",
              trigger: { id: "firstRun" },
              content: { title: "Foo1", body: "Foo123-1" },
              groups: ["onboarding"],
            },
          ],
          providers: [{ id: "onboarding" }],
        });
        sandbox.stub(Router, "loadMessagesFromAllProviders").resolves();
        sandbox.stub(ASRouterTargeting, "findMatchingMessage").resolves();
        const msg = fakeAsyncMessage({
          type: "TRIGGER",
          data: { trigger: { id: "firstRun" } },
        });
        await Router.onMessage(msg);

        assert.calledOnce(ASRouterTargeting.findMatchingMessage);
        assert.deepEqual(
          ASRouterTargeting.findMatchingMessage.firstCall.args[0].trigger,
          {
            id: "firstRun",
            param: undefined,
            context: undefined,
          }
        );
      });
      it("should record telemetry information", async () => {
        const startTelemetryStopwatch = sandbox.stub(
          global.TelemetryStopwatch,
          "start"
        );
        const finishTelemetryStopwatch = sandbox.stub(
          global.TelemetryStopwatch,
          "finish"
        );
        const msg = fakeAsyncMessage({
          type: "TRIGGER",
          data: { trigger: { id: "foo" } },
        });

        await Router.onMessage(msg);

        assert.calledOnce(startTelemetryStopwatch);
        assert.calledWithExactly(
          startTelemetryStopwatch,
          "MS_MESSAGE_REQUEST_TIME_MS",
          { port: msg.target.portID }
        );
        assert.calledOnce(finishTelemetryStopwatch);
        assert.calledWithExactly(
          finishTelemetryStopwatch,
          "MS_MESSAGE_REQUEST_TIME_MS",
          { port: msg.target.portID }
        );
      });
      it("should pick a message with the right targeting and trigger", async () => {
        let messages = [
          {
            id: "foo1",
            groups: ["bundled"],
            template: "simple_template",
            bundled: 2,
            trigger: { id: "foo" },
            content: { title: "Foo1", body: "Foo123-1" },
            provider: "onboarding",
          },
          {
            id: "foo2",
            groups: ["bundled"],
            template: "simple_template",
            bundled: 2,
            trigger: { id: "bar" },
            content: { title: "Foo2", body: "Foo123-2" },
            provider: "onboarding",
          },
          {
            id: "foo3",
            groups: ["bundled"],
            template: "simple_template",
            bundled: 2,
            trigger: { id: "foo" },
            content: { title: "Foo3", body: "Foo123-3" },
            provider: "onboarding",
          },
        ];
        sandbox.stub(Router, "_findProvider").returns(null);
        await Router.setState({ messages, providers: [{ id: "onboarding" }] });
        const { target } = fakeAsyncMessage({
          type: "TRIGGER",
          data: { trigger: { id: "foo" } },
        });
        let { bundle } = await Router._getBundledMessages(messages[0], target, {
          id: "foo",
        });
        assert.equal(bundle.length, 2);
        // it should have picked foo1 and foo3 only
        assert.isTrue(
          bundle.every(elem => elem.id === "foo1" || elem.id === "foo3")
        );
      });
      it("should have previousSessionEnd in the message context", () => {
        assert.propertyVal(
          Router._getMessagesContext(),
          "previousSessionEnd",
          100
        );
      });
    });

    describe(".includeBundle", () => {
      let msg;
      beforeEach(async () => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
        let messages = [
          {
            id: "trailhead",
            groups: ["onboarding"],
            template: "trailhead",
            includeBundle: {
              length: 3,
              template: "foo",
              trigger: { id: "foo" },
            },
            trigger: { id: "firstRun" },
            content: {},
            provider: "onboarding",
          },
          {
            id: "foo2",
            groups: ["onboarding"],
            template: "foo",
            bundled: 3,
            order: 2,
            trigger: { id: "foo" },
            content: { title: "Foo2", body: "Foo123-2" },
            provider: "onboarding",
          },
          {
            id: "foo3",
            groups: ["onboarding"],
            template: "foo",
            bundled: 3,
            order: 3,
            trigger: { id: "foo" },
            content: { title: "Foo3", body: "Foo123-3" },
            provider: "onboarding",
          },
          {
            id: "foo4",
            groups: ["onboarding"],
            template: "foo",
            bundled: 3,
            order: 1,
            trigger: { id: "foo" },
            content: { title: "Foo4", body: "Foo123-4" },
            provider: "onboarding",
          },
          {
            id: "foo5",
            groups: ["onboarding"],
            template: "foo",
            bundled: 3,
            order: 4,
            trigger: { id: "foo" },
            content: { title: "Foo5", body: "Foo123-5" },
            provider: "onboarding",
          },
        ];

        sandbox.stub(Router, "_findProvider").returns(null);
        await Router.setState({ messages, providers: [{ id: "onboarding" }] });

        msg = fakeAsyncMessage({
          type: "TRIGGER",
          data: { trigger: { id: "firstRun" } },
        });
      });

      it("should send a message with .includeBundle property with specified length and template", async () => {
        await Router.onMessage(msg);
        const [, resp] = msg.target.sendAsyncMessage.firstCall.args;
        assert.propertyVal(resp, "type", "SET_MESSAGE");
        assert.isArray(resp.data.bundle, "resp.data.bundle");
        assert.lengthOf(resp.data.bundle, 3, "resp.data.bundle");
      });

      it("should set blockOnClick property by default false on returned ordered bundle messages", async () => {
        const expectedBundle = [
          {
            content: { title: "Foo4", body: "Foo123-4" },
            id: "foo4",
            order: 1,
            blockOnClick: false,
          },
          {
            content: { title: "Foo2", body: "Foo123-2" },
            id: "foo2",
            order: 2,
            blockOnClick: false,
          },
          {
            content: { title: "Foo3", body: "Foo123-3" },
            id: "foo3",
            order: 3,
            blockOnClick: false,
          },
        ];

        await Router.onMessage(msg);
        const [, resp] = msg.target.sendAsyncMessage.firstCall.args;

        for (let i = 0; i < 3; i++) {
          assert.deepEqual(resp.data.bundle[i], expectedBundle[i]);
        }
      });

      it("should set blockOnClick property true for dynamic triplet and matching messages more than 3", async () => {
        sandbox.replaceGetter(ASRouterPreferences, "trailhead", function() {
          return {
            trailheadInterrupt: "join",
            trailheadTriplet: "dynamic",
          };
        });
        await Router.onMessage(msg);
        const [, resp] = msg.target.sendAsyncMessage.firstCall.args;
        const expectedBundle = [
          {
            content: { title: "Foo4", body: "Foo123-4" },
            id: "foo4",
            order: 1,
            blockOnClick: true,
          },
          {
            content: { title: "Foo2", body: "Foo123-2" },
            id: "foo2",
            order: 2,
            blockOnClick: true,
          },
          {
            content: { title: "Foo3", body: "Foo123-3" },
            id: "foo3",
            order: 3,
            blockOnClick: true,
          },
        ];

        for (let i = 0; i < 3; i++) {
          assert.deepEqual(resp.data.bundle[i], expectedBundle[i]);
        }
      });

      it("should set blockOnClick property true for triplet branch name that starts with 'dynamic' and matching messages more than 3", async () => {
        sandbox.replaceGetter(ASRouterPreferences, "trailhead", function() {
          return {
            trailheadInterrupt: "join",
            trailheadTriplet: "dynamic_test",
          };
        });
        await Router.onMessage(msg);
        const [, resp] = msg.target.sendAsyncMessage.firstCall.args;
        const expectedBundle = [
          {
            content: { title: "Foo4", body: "Foo123-4" },
            id: "foo4",
            order: 1,
            blockOnClick: true,
          },
          {
            content: { title: "Foo2", body: "Foo123-2" },
            id: "foo2",
            order: 2,
            blockOnClick: true,
          },
          {
            content: { title: "Foo3", body: "Foo123-3" },
            id: "foo3",
            order: 3,
            blockOnClick: true,
          },
        ];

        for (let i = 0; i < 3; i++) {
          assert.deepEqual(resp.data.bundle[i], expectedBundle[i]);
        }
      });
    });

    describe("#onMessage: OVERRIDE_MESSAGE", () => {
      it("should broadcast a SET_MESSAGE message to all clients with a particular id", async () => {
        const [testMessage] = Router.state.messages;
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage.id },
        });
        await Router.onMessage(msg);

        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "SET_MESSAGE", data: testMessage }
        );
      });

      it("should call CFRPageActions.forceRecommendation if the template is cfr_action and force is true", async () => {
        sandbox.stub(CFRPageActions, "forceRecommendation");
        const testMessage = { id: "foo", template: "cfr_doorhanger" };
        await Router.setState({ messages: [testMessage] });
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage.id },
        });
        await Router.onMessage(msg);

        assert.notCalled(msg.target.sendAsyncMessage);
        assert.calledOnce(CFRPageActions.forceRecommendation);
      });

      it("should call BookmarkPanelHub._forceShowMessage the provider is cfr-fxa", async () => {
        const testMessage = { id: "foo", template: "fxa_bookmark_panel" };
        await Router.setState({ messages: [testMessage] });
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: testMessage.id },
        });
        await Router.onMessage(msg);

        assert.notCalled(msg.target.sendAsyncMessage);
        assert.calledOnce(FakeBookmarkPanelHub._forceShowMessage);
      });

      it("should call CFRPageActions.addRecommendation if the template is cfr_action and force is false", async () => {
        sandbox.stub(CFRPageActions, "addRecommendation");
        const testMessage = { id: "foo", template: "cfr_doorhanger" };
        await Router.setState({ messages: [testMessage] });
        await Router._sendMessageToTarget(
          testMessage,
          {},
          { param: {} },
          false
        );

        assert.calledOnce(CFRPageActions.addRecommendation);
      });

      it("should broadcast CLEAR_ALL if provided id did not resolve to a message", async () => {
        const msg = fakeAsyncMessage({
          type: "OVERRIDE_MESSAGE",
          data: { id: -1 },
        });
        await Router.onMessage(msg);

        assert.calledWith(
          msg.target.sendAsyncMessage,
          PARENT_TO_CHILD_MESSAGE_NAME,
          { type: "CLEAR_ALL" }
        );
      });
    });

    describe("#onMessage: Onboarding actions", () => {
      it("should call OpenBrowserWindow with a private window on OPEN_PRIVATE_BROWSER_WINDOW", async () => {
        let [testMessage] = Router.state.messages;
        const msg = fakeExecuteUserAction({
          type: "OPEN_PRIVATE_BROWSER_WINDOW",
          data: testMessage,
        });
        await Router.onMessage(msg);

        assert.calledWith(msg.target.browser.ownerGlobal.OpenBrowserWindow, {
          private: true,
        });
      });
      it("should call openLinkIn with the correct params on OPEN_URL", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = {
          type: "OPEN_URL",
          data: { args: "some/url.com", where: "tabshifted" },
        };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        await Router.onMessage(msg);

        assert.calledOnce(msg.target.browser.ownerGlobal.openLinkIn);
        assert.calledWith(
          msg.target.browser.ownerGlobal.openLinkIn,
          "some/url.com",
          "tabshifted",
          { private: false, triggeringPrincipal: undefined, csp: null }
        );
      });
      it("should call openLinkIn with the correct params on OPEN_ABOUT_PAGE", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = {
          type: "OPEN_ABOUT_PAGE",
          data: { args: "something" },
        };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        await Router.onMessage(msg);

        assert.calledOnce(msg.target.browser.ownerGlobal.openTrustedLinkIn);
        assert.calledWith(
          msg.target.browser.ownerGlobal.openTrustedLinkIn,
          "about:something",
          "tab"
        );
      });
      it("should call MigrationUtils.showMigrationWizard on SHOW_MIGRATION_WIZARD", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = {
          type: "SHOW_MIGRATION_WIZARD",
        };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        globals.set("MigrationUtils", {
          showMigrationWizard: sandbox
            .stub()
            .withArgs(msg.target.browser.ownerGlobal, ["test"]),
          MIGRATION_ENTRYPOINT_NEWTAB: "test",
        });
        await Router.onMessage(msg);

        assert.calledOnce(MigrationUtils.showMigrationWizard);
        assert.calledWith(
          MigrationUtils.showMigrationWizard,
          msg.target.browser.ownerGlobal,
          [MigrationUtils.MIGRATION_ENTRYPOINT_NEWTAB]
        );
      });
    });

    describe("#onMessage: SHOW_FIREFOX_ACCOUNTS", () => {
      beforeEach(() => {
        globals.set("FxAccounts", {
          config: {
            promiseConnectAccountURI: sandbox.stub().resolves("some/url"),
          },
        });
      });
      it("should call openLinkIn with the correct params on OPEN_URL", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = { type: "SHOW_FIREFOX_ACCOUNTS" };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        await Router.onMessage(msg);

        assert.calledOnce(msg.target.browser.ownerGlobal.openLinkIn);
        assert.calledWith(
          msg.target.browser.ownerGlobal.openLinkIn,
          "some/url",
          "current",
          { private: false, triggeringPrincipal: undefined, csp: null }
        );
      });
    });

    describe("#onMessage: OPEN_PREFERENCES_PAGE", () => {
      it("should call openPreferences with the correct params on OPEN_PREFERENCES_PAGE", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = {
          type: "OPEN_PREFERENCES_PAGE",
          data: { category: "something" },
        };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        await Router.onMessage(msg);

        assert.calledOnce(msg.target.browser.ownerGlobal.openPreferences);
        assert.calledWith(
          msg.target.browser.ownerGlobal.openPreferences,
          "something"
        );
      });
      it("should call openPreferences with the correct entrypoint if defined", async () => {
        let [testMessage] = Router.state.messages;
        testMessage.button_action = {
          type: "OPEN_PREFERENCES_PAGE",
          data: { category: "something", entrypoint: "unittest" },
        };
        const msg = fakeExecuteUserAction(testMessage.button_action);
        await Router.onMessage(msg);

        assert.calledOnce(msg.target.browser.ownerGlobal.openPreferences);
        assert.calledWithExactly(
          msg.target.browser.ownerGlobal.openPreferences,
          "something",
          { urlParams: { entrypoint: "unittest" } }
        );
      });
    });

    describe("#onMessage: INSTALL_ADDON_FROM_URL", () => {
      it("should call installAddonFromURL with correct arguments", async () => {
        sandbox.stub(MessageLoaderUtils, "installAddonFromURL").resolves(null);
        const msg = fakeExecuteUserAction({
          type: "INSTALL_ADDON_FROM_URL",
          data: { url: "foo.com", telemetrySource: "foo" },
        });

        await Router.onMessage(msg);

        assert.calledOnce(MessageLoaderUtils.installAddonFromURL);
        assert.calledWithExactly(
          MessageLoaderUtils.installAddonFromURL,
          msg.target.browser,
          "foo.com",
          "foo"
        );
      });

      it("should add/remove observers for `webextension-install-notify`", async () => {
        sandbox.spy(global.Services.obs, "addObserver");
        sandbox.spy(global.Services.obs, "removeObserver");

        sandbox.stub(MessageLoaderUtils, "installAddonFromURL").resolves(null);
        const msg = fakeExecuteUserAction({
          type: "INSTALL_ADDON_FROM_URL",
          data: { url: "foo.com" },
        });

        await Router.onMessage(msg);

        assert.calledOnce(global.Services.obs.addObserver);

        const [cb] = global.Services.obs.addObserver.firstCall.args;

        cb();

        assert.calledOnce(global.Services.obs.removeObserver);
        assert.calledOnce(channel.sendAsyncMessage);
      });
    });

    describe("#onMessage: PIN_CURRENT_TAB", () => {
      it("should call pin tab with the selectedTab", async () => {
        const msg = fakeExecuteUserAction({ type: "PIN_CURRENT_TAB" });
        const { gBrowser, ConfirmationHint } = msg.target.browser.ownerGlobal;

        await Router.onMessage(msg);

        assert.calledOnce(gBrowser.pinTab);
        assert.calledWithExactly(gBrowser.pinTab, gBrowser.selectedTab);
        assert.calledOnce(ConfirmationHint.show);
        assert.calledWithExactly(
          ConfirmationHint.show,
          gBrowser.selectedTab,
          "pinTab",
          { showDescription: true }
        );
      });
    });

    describe("#onMessage: OPEN_PROTECTION_PANEL", () => {
      it("should open protection panel", async () => {
        const msg = fakeExecuteUserAction({ type: "OPEN_PROTECTION_PANEL" });
        let { gProtectionsHandler } = msg.target.browser.ownerGlobal;

        await Router.onMessage(msg);

        assert.calledOnce(gProtectionsHandler.showProtectionsPopup);
        assert.calledWithExactly(gProtectionsHandler.showProtectionsPopup, {});
      });
    });

    describe("#onMessage: OPEN_PROTECTION_REPORT", () => {
      it("should open protection report", async () => {
        const msg = fakeExecuteUserAction({ type: "OPEN_PROTECTION_REPORT" });
        let { gProtectionsHandler } = msg.target.browser.ownerGlobal;

        await Router.onMessage(msg);

        assert.calledOnce(gProtectionsHandler.openProtections);
      });
    });

    describe("#onMessage: DISABLE_STP_DOORHANGERS", () => {
      it("should block STP related messages", async () => {
        const msg = fakeExecuteUserAction({ type: "DISABLE_STP_DOORHANGERS" });

        assert.deepEqual(Router.state.messageBlockList, []);

        await Router.onMessage(msg);

        assert.deepEqual(Router.state.messageBlockList, [
          "SOCIAL_TRACKING_PROTECTION",
          "FINGERPRINTERS_PROTECTION",
          "CRYPTOMINERS_PROTECTION",
        ]);
      });
    });

    describe("#dispatch(action, target)", () => {
      it("should an action and target to onMessage", async () => {
        // use the IMPRESSION action to make sure actions are actually getting processed
        sandbox.stub(Router, "addImpression");
        sandbox.spy(Router, "onMessage");
        const target = {};
        const action = { type: "IMPRESSION" };

        Router.dispatch(action, target);

        assert.calledWith(Router.onMessage, { data: action, target });
        assert.calledOnce(Router.addImpression);
      });
    });

    describe("#onMessage: DOORHANGER_TELEMETRY", () => {
      it("should dispatch an AS_ROUTER_TELEMETRY_USER_EVENT on DOORHANGER_TELEMETRY message", async () => {
        const msg = fakeAsyncMessage({
          type: "DOORHANGER_TELEMETRY",
          data: { message_id: "foo" },
        });
        dispatchStub.reset();

        await Router.onMessage(msg);

        assert.calledOnce(dispatchStub);
        const [action] = dispatchStub.firstCall.args;
        assert.equal(action.type, "AS_ROUTER_TELEMETRY_USER_EVENT");
        assert.equal(action.data.message_id, "foo");
      });
    });

    describe("#onMessage: EXPIRE_QUERY_CACHE", () => {
      it("should clear all QueryCache getters", async () => {
        const msg = fakeAsyncMessage({ type: "EXPIRE_QUERY_CACHE" });
        sandbox.stub(QueryCache, "expireAll");

        await Router.onMessage(msg);

        assert.calledOnce(QueryCache.expireAll);
      });
    });

    describe("#onMessage: ENABLE_PROVIDER", () => {
      it("should enable the provider via ASRouterPreferences", async () => {
        const msg = fakeAsyncMessage({ type: "ENABLE_PROVIDER", data: "foo" });
        sandbox.stub(ASRouterPreferences, "enableOrDisableProvider");

        await Router.onMessage(msg);

        assert.calledWith(
          ASRouterPreferences.enableOrDisableProvider,
          "foo",
          true
        );
      });
    });

    describe("#onMessage: DISABLE_PROVIDER", () => {
      it("should disable the provider via ASRouterPreferences", async () => {
        const msg = fakeAsyncMessage({ type: "DISABLE_PROVIDER", data: "foo" });
        sandbox.stub(ASRouterPreferences, "enableOrDisableProvider");

        await Router.onMessage(msg);

        assert.calledWith(
          ASRouterPreferences.enableOrDisableProvider,
          "foo",
          false
        );
      });
    });

    describe("#onMessage: RESET_PROVIDER_PREF", () => {
      it("should reset provider pref via ASRouterPreferences", async () => {
        const msg = fakeAsyncMessage({
          type: "RESET_PROVIDER_PREF",
          data: "foo",
        });
        sandbox.stub(ASRouterPreferences, "resetProviderPref");

        await Router.onMessage(msg);

        assert.calledOnce(ASRouterPreferences.resetProviderPref);
      });
    });
    describe("#onMessage: SET_PROVIDER_USER_PREF", () => {
      it("should set provider user pref via ASRouterPreferences", async () => {
        const msg = fakeAsyncMessage({
          type: "SET_PROVIDER_USER_PREF",
          data: { id: "foo", value: true },
        });
        sandbox.stub(ASRouterPreferences, "setUserPreference");

        await Router.onMessage(msg);

        assert.calledWith(ASRouterPreferences.setUserPreference, "foo", true);
      });
    });
    describe("#onMessage: SET_GROUP_STATE", () => {
      beforeEach(() => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
      });
      it("should call setGroupState", async () => {
        const msg = fakeAsyncMessage({
          type: "SET_GROUP_STATE",
          data: { id: "foo", value: true },
        });
        sandbox.stub(Router, "setGroupState");

        await Router.onMessage(msg);

        assert.calledOnce(Router.setGroupState);
        assert.calledWithExactly(Router.setGroupState, msg.data.data);
        assert.calledOnce(Router.loadMessagesFromAllProviders);
      });
    });
    describe("#onMessage: BLOCK_GROUP_BY_ID", () => {
      beforeEach(() => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
      });
      it("should call blockGroupById", async () => {
        const msg = fakeAsyncMessage({
          type: "BLOCK_GROUP_BY_ID",
          data: { id: "foo" },
        });
        sandbox.stub(Router, "blockGroupById");

        await Router.onMessage(msg);

        assert.calledOnce(Router.blockGroupById);
        assert.calledWithExactly(Router.blockGroupById, msg.data.data.id);
        assert.calledOnce(Router.loadMessagesFromAllProviders);
      });
    });
    describe("#onMessage: UNBLOCK_GROUP_BY_ID", () => {
      beforeEach(() => {
        sandbox.stub(Router, "loadMessagesFromAllProviders");
      });
      it("should call unblockGroupById", async () => {
        const msg = fakeAsyncMessage({
          type: "UNBLOCK_GROUP_BY_ID",
          data: { id: "foo" },
        });
        sandbox.stub(Router, "unblockGroupById");

        await Router.onMessage(msg);

        assert.calledOnce(Router.unblockGroupById);
        assert.calledWithExactly(Router.unblockGroupById, msg.data.data.id);
        assert.calledOnce(Router.loadMessagesFromAllProviders);
      });
    });
    describe("#onMessage: EVALUATE_JEXL_EXPRESSION", () => {
      it("should call evaluateExpression", async () => {
        const msg = fakeAsyncMessage({
          type: "EVALUATE_JEXL_EXPRESSION",
          data: { foo: true },
        });
        sandbox.stub(Router, "evaluateExpression");

        await Router.onMessage(msg);

        assert.calledOnce(Router.evaluateExpression);
        assert.calledWithExactly(
          Router.evaluateExpression,
          msg.target,
          msg.data.data
        );
      });
    });
    describe("#onMessage: FORCE_ATTRIBUTION", () => {
      let setReferrerUrl;
      beforeEach(() => {
        setReferrerUrl = sinon.spy();
        global.Cc["@mozilla.org/mac-attribution;1"] = {
          getService: () => ({ setReferrerUrl }),
        };
        global.Cc["@mozilla.org/process/environment;1"] = {
          getService: () => ({ set: sandbox.stub() }),
        };
      });
      afterEach(() => {
        globals.restore();
      });
      it("should call forceAttribution", async () => {
        const msg = fakeAsyncMessage({
          type: "FORCE_ATTRIBUTION",
          data: { foo: true },
        });
        sandbox.stub(Router, "forceAttribution");

        await Router.onMessage(msg);

        assert.calledOnce(Router.forceAttribution);
        assert.calledWithExactly(Router.forceAttribution, msg.data.data);
      });
      it("should force attribution and update providers", async () => {
        sandbox.stub(Router, "_updateMessageProviders");
        sandbox.stub(Router, "loadMessagesFromAllProviders");
        sandbox.stub(fakeAttributionCode, "_clearCache");
        sandbox.stub(fakeAttributionCode, "getAttrDataAsync");
        const msg = fakeAsyncMessage({
          type: "FORCE_ATTRIBUTION",
          data: { foo: true },
        });
        await Router.onMessage(msg);

        assert.calledOnce(fakeAttributionCode._clearCache);
        assert.calledOnce(fakeAttributionCode.getAttrDataAsync);
        assert.calledOnce(Router._updateMessageProviders);
        assert.calledOnce(Router.loadMessagesFromAllProviders);
      });
      it("should double encode on windows", async () => {
        sandbox.stub(Router, "_writeAttributionFile");

        Router.forceAttribution({ foo: "FOO!", eh: "NOPE", bar: "BAR?" });

        assert.notCalled(setReferrerUrl);
        assert.calledWithMatch(
          Router._writeAttributionFile,
          "foo%3DFOO!%26bar%3DBAR%253F"
        );
      });
      it("should set referrer on mac", async () => {
        sandbox.stub(AppConstants, "platform").value("macosx");

        Router.forceAttribution({ foo: "FOO!", eh: "NOPE", bar: "BAR?" });

        assert.calledOnce(setReferrerUrl);
        assert.calledWithMatch(setReferrerUrl, "", "?foo=FOO!&bar=BAR%3F");
      });
    });
    describe("#onMessage: FORCE_WHATSNEW_PANEL", () => {
      it("should call forceWNPanel", async () => {
        let messages = [];
        const msg = fakeAsyncMessage({
          type: "FORCE_WHATSNEW_PANEL",
          data: { messages },
        });
        sandbox.stub(Router, "forceWNPanel");

        await Router.onMessage(msg);

        assert.calledOnce(Router.forceWNPanel);
      });
      describe("forceWNPanel", () => {
        let browser = {
          document: new Document(),
          PanelUI: {
            showSubView: sinon.stub(),
          },
        };
        it("should call enableToolbarButton", async () => {
          await Router.forceWNPanel(browser);

          assert.calledOnce(FakeToolbarPanelHub.enableToolbarButton);
        });
        it("should call PanelUI.showSubView", () => {
          assert.calledOnce(browser.PanelUI.showSubView);
        });
      });
    });
    describe("#onMessage: RENDER_WHATSNEW_MESSAGES", () => {
      it("should call renderWNMessages", async () => {
        let messages = [];
        const msg = fakeAsyncMessage({
          type: "RENDER_WHATSNEW_MESSAGES",
          data: { messages },
        });
        sandbox.stub(Router, "renderWNMessages");

        await Router.onMessage(msg);

        assert.calledOnce(Router.renderWNMessages);
      });
      describe("renderWNMessages", () => {
        let messages = [{}, {}];
        let browser = {
          document: new Document(),
          PanelUI: {
            showSubView: sinon.stub(),
          },
        };
        it("should call forceShowMessage", () => {
          Router.renderWNMessages(browser, messages);

          assert.calledOnce(FakeToolbarPanelHub.forceShowMessage);
        });
      });
    });
    describe("#onMessage: default", () => {
      it("should report unknown messages", () => {
        const msg = fakeAsyncMessage({ type: "FOO" });
        sandbox.stub(Cu, "reportError");

        Router.onMessage(msg);

        assert.calledOnce(Cu.reportError);
      });
    });
  });

  describe("_triggerHandler", () => {
    it("should call #onMessage with the correct trigger", () => {
      const getter = sandbox.stub();
      getter.returns(false);
      sandbox.stub(global.BrowserHandler, "kiosk").get(getter);
      sinon.spy(Router, "onMessage");
      const target = {};
      const trigger = { id: "FAKE_TRIGGER", param: "some fake param" };
      Router._triggerHandler(target, trigger);
      assert.calledOnce(Router.onMessage);
      assert.calledWithExactly(Router.onMessage, {
        target,
        data: { type: "TRIGGER", data: { trigger } },
      });
    });
  });

  describe("_triggerHandler_kiosk", () => {
    it("should not call #onMessage", () => {
      const getter = sandbox.stub();
      getter.returns(true);
      sandbox.stub(global.BrowserHandler, "kiosk").get(getter);
      sinon.spy(Router, "onMessage");
      const target = {};
      const trigger = { id: "FAKE_TRIGGER", param: "some fake param" };
      Router._triggerHandler(target, trigger);
      assert.notCalled(Router.onMessage);
    });
  });

  describe("#UITour", () => {
    let showMenuStub;
    const highlightTarget = { target: "target" };
    beforeEach(() => {
      showMenuStub = sandbox.stub();
      globals.set("UITour", {
        showMenu: showMenuStub,
        getTarget: sandbox
          .stub()
          .withArgs(sinon.match.object, "pageAaction-sendToDevice")
          .resolves(highlightTarget),
        showHighlight: sandbox.stub(),
      });
    });
    it("should call UITour.showMenu with the correct params on OPEN_APPLICATIONS_MENU", async () => {
      const msg = fakeExecuteUserAction({
        type: "OPEN_APPLICATIONS_MENU",
        data: { args: "appMenu" },
      });
      await Router.onMessage(msg);

      assert.calledOnce(showMenuStub);
      assert.calledWith(
        showMenuStub,
        msg.target.browser.ownerGlobal,
        "appMenu"
      );
    });
    it("should call UITour.showHighlight with the correct params on HIGHLIGHT_FEATURE", async () => {
      const msg = fakeExecuteUserAction({
        type: "HIGHLIGHT_FEATURE",
        data: { args: "pageAction-sendToDevice" },
      });
      await Router.onMessage(msg);

      assert.calledOnce(UITour.getTarget);
      assert.calledOnce(UITour.showHighlight);
      assert.calledWith(
        UITour.showHighlight,
        msg.target.browser.ownerGlobal,
        highlightTarget
      );
    });
  });

  describe("valid preview endpoint", () => {
    it("should report an error if url protocol is not https", () => {
      sandbox.stub(Cu, "reportError");

      assert.equal(false, Router._validPreviewEndpoint("http://foo.com"));
      assert.calledTwice(Cu.reportError);
    });
  });

  describe("impressions", () => {
    async function addGroupWithFrequency(id, frequency) {
      await Router.setState(state => {
        const newGroup = { id, frequency };
        const groups = [...state.groups, newGroup];
        return { groups };
      });
    }

    describe("frequency normalisation", () => {
      beforeEach(async () => {
        const messages = [
          { frequency: { custom: [{ period: "daily", cap: 10 }] } },
        ];
        const provider = {
          id: "foo",
          frequency: { custom: [{ period: "daily", cap: 100 }] },
          messages,
          enabled: true,
        };
        await createRouterAndInit([provider]);
      });

      it("period aliases in provider frequency caps should be normalised", () => {
        const [provider] = Router.state.providers;
        assert.equal(provider.frequency.custom[0].period, ONE_DAY_IN_MS);
      });
      it("period aliases in message frequency caps should be normalised", async () => {
        const [message] = Router.state.messages;
        assert.equal(message.frequency.custom[0].period, ONE_DAY_IN_MS);
      });
    });

    describe("#addImpression", () => {
      it("should add a message impression and update _storage with the current time if the message has frequency caps", async () => {
        clock.tick(42);
        const msg = fakeAsyncMessage({
          type: "IMPRESSION",
          data: {
            id: "foo",
            provider: FAKE_LOCAL_PROVIDER.id,
            frequency: { lifetime: 5 },
          },
        });
        await Router.onMessage(msg);
        assert.isArray(Router.state.messageImpressions.foo);
        assert.deepEqual(Router.state.messageImpressions.foo, [42]);
        assert.calledWith(Router._storage.set, "messageImpressions", {
          foo: [42],
        });
      });
      it("should not add a message impression if the message doesn't have frequency caps", async () => {
        // Note that storage.set is called during initialization, so it needs to be reset
        Router._storage.set.reset();
        clock.tick(42);
        const msg = fakeAsyncMessage({
          type: "IMPRESSION",
          data: { id: "foo" },
        });
        await Router.onMessage(msg);
        assert.notProperty(Router.state.messageImpressions, "foo");
        assert.notCalled(Router._storage.set);
      });
      it("should add a provider impression and update _storage with the current time if the message's provider has frequency caps", async () => {
        clock.tick(42);
        await addGroupWithFrequency("foo", { lifetime: 5 });
        const msg = fakeAsyncMessage({
          type: "IMPRESSION",
          data: { id: "bar", provider: "foo", groups: ["foo"] },
        });
        await Router.onMessage(msg);
        assert.isArray(Router.state.groupImpressions.foo);
        assert.deepEqual(Router.state.groupImpressions.foo, [42]);
        assert.calledWith(Router._storage.set, "groupImpressions", {
          foo: [42],
        });
      });
      it("should not add a group impression if the message's group doesn't have frequency caps", async () => {
        // Note that storage.set is called during initialization, so it needs to be reset
        Router._storage.set.reset();
        clock.tick(42);
        // Add "foo" group with no frequency
        await addGroupWithFrequency("foo", null);
        const msg = fakeAsyncMessage({
          type: "IMPRESSION",
          data: { id: "bar", provider: "foo", groups: ["foo"] },
        });
        await Router.onMessage(msg);
        assert.notProperty(Router.state.groupImpressions, "foo");
        assert.notCalled(Router._storage.set);
      });
      it("should only send impressions for one message", async () => {
        const getElementById = sandbox.stub().returns({
          setAttribute: sandbox.stub(),
          style: { setProperty: sandbox.stub() },
          addEventListener: sandbox.stub(),
        });
        const data = {
          param: { host: "mozilla.com", url: "https://mozilla.com" },
        };
        const target = {
          sendAsyncMessage: sandbox.stub(),
          documentURI: { scheme: "https", host: "mozilla.com" },
        };
        const gURLBar = document.createElement("div");
        gURLBar.textbox = document.createElement("div");
        target.ownerGlobal = {
          gURLBar,
          gBrowser: { selectedBrowser: target },
          document: { getElementById },
          promiseDocumentFlushed: sandbox.stub().resolves([{ width: 0 }]),
          setTimeout: sandbox.stub(),
          A11yUtils: { announce: sandbox.stub() },
        };
        const firstMessage = { ...FAKE_RECOMMENDATION, id: "first_message" };
        const secondMessage = { ...FAKE_RECOMMENDATION, id: "second_message" };
        await Router.setState({ messages: [firstMessage, secondMessage] });
        global.DOMLocalization = class DOMLocalization {};
        sandbox.spy(CFRPageActions, "addRecommendation");
        sandbox.stub(Router, "addImpression").resolves();

        await Router.setMessageById("first_message", target, false, { data });
        await Router.setMessageById("second_message", target, false, { data });

        assert.calledTwice(CFRPageActions.addRecommendation);
        const [
          firstReturn,
          secondReturn,
        ] = CFRPageActions.addRecommendation.returnValues;
        assert.isTrue(await firstReturn);
        // Adding the second message should fail.
        assert.isFalse(await secondReturn);
        assert.calledOnce(Router.addImpression);
      });
    });

    describe("#isBelowFrequencyCaps", () => {
      it("should call #_isBelowItemFrequencyCap for the message and for the provider with the correct impressions and arguments", async () => {
        sinon.spy(Router, "_isBelowItemFrequencyCap");

        const MAX_MESSAGE_LIFETIME_CAP = 100; // Defined in ASRouter
        const fooMessageImpressions = [0, 1];
        const barGroupImpressions = [0, 1, 2];

        const message = {
          id: "foo",
          provider: "bar",
          groups: ["bar"],
          frequency: { lifetime: 3 },
        };
        const groups = [{ id: "bar", frequency: { lifetime: 5 } }];

        await Router.setState(state => {
          // Add provider
          const providers = [...state.providers];
          // Add fooMessageImpressions
          // eslint-disable-next-line no-shadow
          const messageImpressions = Object.assign(
            {},
            state.messageImpressions
          );
          const gImpressions = Object.assign({}, state.providerImpressions);
          gImpressions.bar = barGroupImpressions;
          messageImpressions.foo = fooMessageImpressions;
          return {
            providers,
            messageImpressions,
            groups,
            groupImpressions: gImpressions,
          };
        });

        await Router.isBelowFrequencyCaps(message);

        assert.calledTwice(Router._isBelowItemFrequencyCap);
        assert.calledWithExactly(
          Router._isBelowItemFrequencyCap,
          message,
          fooMessageImpressions,
          MAX_MESSAGE_LIFETIME_CAP
        );
        assert.calledWithExactly(
          Router._isBelowItemFrequencyCap,
          groups[0],
          barGroupImpressions
        );
      });
    });

    describe("#_isBelowItemFrequencyCap", () => {
      it("should return false if the # of impressions exceeds the maxLifetimeCap", () => {
        const item = { id: "foo", frequency: { lifetime: 5 } };
        const impressions = [0, 1];
        const maxLifetimeCap = 1;
        const result = Router._isBelowItemFrequencyCap(
          item,
          impressions,
          maxLifetimeCap
        );
        assert.isFalse(result);
      });

      describe("lifetime frequency caps", () => {
        it("should return true if .frequency is not defined on the item", () => {
          const item = { id: "foo" };
          const impressions = [0, 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return true if there are no impressions", () => {
          const item = {
            id: "foo",
            frequency: {
              lifetime: 10,
              custom: [{ period: ONE_DAY_IN_MS, cap: 2 }],
            },
          };
          const impressions = [];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return true if the # of impressions is less than .frequency.lifetime of the item", () => {
          const item = { id: "foo", frequency: { lifetime: 3 } };
          const impressions = [0, 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if the # of impressions is equal to .frequency.lifetime of the item", async () => {
          const item = { id: "foo", frequency: { lifetime: 3 } };
          const impressions = [0, 1, 2];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return false if the # of impressions is greater than .frequency.lifetime of the item", async () => {
          const item = { id: "foo", frequency: { lifetime: 3 } };
          const impressions = [0, 1, 2, 3];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
      });

      describe("custom frequency caps", () => {
        it("should return true if impressions in the time period < the cap and total impressions < the lifetime cap", () => {
          clock.tick(ONE_DAY_IN_MS + 10);
          const item = {
            id: "foo",
            frequency: {
              custom: [{ period: ONE_DAY_IN_MS, cap: 2 }],
              lifetime: 3,
            },
          };
          const impressions = [0, ONE_DAY_IN_MS + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if impressions in the time period > the cap and total impressions < the lifetime cap", () => {
          clock.tick(200);
          const item = {
            id: "msg1",
            frequency: { custom: [{ period: 100, cap: 2 }], lifetime: 3 },
          };
          const impressions = [0, 160, 161];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return false if impressions in one of the time periods > the cap and total impressions < the lifetime cap", () => {
          clock.tick(ONE_DAY_IN_MS + 200);
          const itemTrue = {
            id: "msg2",
            frequency: { custom: [{ period: 100, cap: 2 }] },
          };
          const itemFalse = {
            id: "msg1",
            frequency: {
              custom: [
                { period: 100, cap: 2 },
                { period: ONE_DAY_IN_MS, cap: 3 },
              ],
            },
          };
          const impressions = [
            0,
            ONE_DAY_IN_MS + 160,
            ONE_DAY_IN_MS - 100,
            ONE_DAY_IN_MS - 200,
          ];
          assert.isTrue(Router._isBelowItemFrequencyCap(itemTrue, impressions));
          assert.isFalse(
            Router._isBelowItemFrequencyCap(itemFalse, impressions)
          );
        });
        it("should return false if impressions in the time period < the cap and total impressions > the lifetime cap", () => {
          clock.tick(ONE_DAY_IN_MS + 10);
          const item = {
            id: "msg1",
            frequency: {
              custom: [{ period: ONE_DAY_IN_MS, cap: 2 }],
              lifetime: 3,
            },
          };
          const impressions = [0, 1, 2, 3, ONE_DAY_IN_MS + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
        it("should return true if daily impressions < the daily cap and there is no lifetime cap", () => {
          clock.tick(ONE_DAY_IN_MS + 10);
          const item = {
            id: "msg1",
            frequency: { custom: [{ period: ONE_DAY_IN_MS, cap: 2 }] },
          };
          const impressions = [0, 1, 2, 3, ONE_DAY_IN_MS + 1];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isTrue(result);
        });
        it("should return false if daily impressions > the daily cap and there is no lifetime cap", () => {
          clock.tick(ONE_DAY_IN_MS + 10);
          const item = {
            id: "msg1",
            frequency: { custom: [{ period: ONE_DAY_IN_MS, cap: 2 }] },
          };
          const impressions = [
            0,
            1,
            2,
            3,
            ONE_DAY_IN_MS + 1,
            ONE_DAY_IN_MS + 2,
            ONE_DAY_IN_MS + 3,
          ];
          const result = Router._isBelowItemFrequencyCap(item, impressions);
          assert.isFalse(result);
        });
      });
    });

    describe("#getLongestPeriod", () => {
      it("should return the period if there is only one definition", () => {
        const message = {
          id: "foo",
          frequency: { custom: [{ period: 200, cap: 2 }] },
        };
        assert.equal(Router.getLongestPeriod(message), 200);
      });
      it("should return the longest period if there are more than one definitions", () => {
        const message = {
          id: "foo",
          frequency: {
            custom: [
              { period: 1000, cap: 3 },
              { period: ONE_DAY_IN_MS, cap: 5 },
              { period: 100, cap: 2 },
            ],
          },
        };
        assert.equal(Router.getLongestPeriod(message), ONE_DAY_IN_MS);
      });
      it("should return null if there are is no .frequency", () => {
        const message = { id: "foo" };
        assert.isNull(Router.getLongestPeriod(message));
      });
      it("should return null if there are is no .frequency.custom", () => {
        const message = { id: "foo", frequency: { lifetime: 10 } };
        assert.isNull(Router.getLongestPeriod(message));
      });
    });

    describe("cleanup on init", () => {
      it("should clear messageImpressions for messages which do not exist in state.messages", async () => {
        const messages = [{ id: "foo", frequency: { lifetime: 10 } }];
        messageImpressions = { foo: [0], bar: [0, 1] };
        // Impressions for "bar" should be removed since that id does not exist in messages
        const result = { foo: [0] };

        await createRouterAndInit([
          { id: "onboarding", type: "local", messages, enabled: true },
        ]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions older than the period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY_IN_MS * 2;
        clock.tick(CURRENT_TIME);
        const messages = [
          {
            id: "foo",
            frequency: { custom: [{ period: ONE_DAY_IN_MS, cap: 5 }] },
          },
        ];
        messageImpressions = { foo: [0, 1, CURRENT_TIME - 10] };
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = { foo: [CURRENT_TIME - 10] };

        await createRouterAndInit([
          { id: "onboarding", type: "local", messages, enabled: true },
        ]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions older than the longest period if no lifetime impression cap is included", async () => {
        const CURRENT_TIME = ONE_DAY_IN_MS * 2;
        clock.tick(CURRENT_TIME);
        const messages = [
          {
            id: "foo",
            frequency: {
              custom: [
                { period: ONE_DAY_IN_MS, cap: 5 },
                { period: 100, cap: 2 },
              ],
            },
          },
        ];
        messageImpressions = { foo: [0, 1, CURRENT_TIME - 10] };
        // Only 0 and 1 are more than 24 hours before CURRENT_TIME
        const result = { foo: [CURRENT_TIME - 10] };

        await createRouterAndInit([
          { id: "onboarding", type: "local", messages, enabled: true },
        ]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should clear messageImpressions if they are not properly formatted", async () => {
        const messages = [{ id: "foo", frequency: { lifetime: 10 } }];
        // this is impromperly formatted since messageImpressions are supposed to be an array
        messageImpressions = { foo: 0 };
        const result = {};

        await createRouterAndInit([
          { id: "onboarding", type: "local", messages, enabled: true },
        ]);
        assert.calledWith(Router._storage.set, "messageImpressions", result);
        assert.deepEqual(Router.state.messageImpressions, result);
      });
      it("should not clear messageImpressions for messages which do exist in state.messages", async () => {
        const messages = [
          { id: "foo", frequency: { lifetime: 10 } },
          { id: "bar", frequency: { lifetime: 10 } },
        ];
        messageImpressions = { foo: [0], bar: [] };

        await createRouterAndInit([
          { id: "onboarding", type: "local", messages, enabled: true },
        ]);
        assert.notCalled(Router._storage.set);
        assert.deepEqual(Router.state.messageImpressions, messageImpressions);
      });
    });
  });

  describe("handle targeting errors", () => {
    beforeEach(() => {
      sandbox.stub(Router, "loadMessagesFromAllProviders");
    });
    it("should dispatch an event when a targeting expression throws an error", async () => {
      sandbox
        .stub(global.FilterExpressions, "eval")
        .returns(Promise.reject(new Error("fake error")));
      await Router.setState({
        messages: [
          {
            id: "foo",
            provider: "snippets",
            targeting: "foo2.[[(",
            groups: ["snippets"],
          },
        ],
        providers: [{ id: "snippets" }],
      });
      const msg = fakeAsyncMessage({ type: "NEWTAB_MESSAGE_REQUEST" });
      dispatchStub.reset();

      await Router.onMessage(msg);

      assert.calledOnce(dispatchStub);
      const [action] = dispatchStub.firstCall.args;
      assert.equal(action.type, "AS_ROUTER_TELEMETRY_USER_EVENT");
      assert.equal(action.data.message_id, "foo");
    });
  });

  describe("#_onLocaleChanged", () => {
    it("should call _maybeUpdateL10nAttachment in the handler", async () => {
      sandbox.spy(Router, "_maybeUpdateL10nAttachment");
      await Router._onLocaleChanged();

      assert.calledOnce(Router._maybeUpdateL10nAttachment);
    });
  });

  describe("#_maybeUpdateL10nAttachment", () => {
    it("should update the l10n attachment if the locale was changed", async () => {
      const getter = sandbox.stub();
      getter.onFirstCall().returns("en-US");
      getter.onSecondCall().returns("fr");
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").get(getter);
      const provider = {
        id: "cfr",
        enabled: true,
        type: "remote-settings",
        bucket: "cfr",
      };
      await createRouterAndInit([provider]);
      sandbox.spy(Router, "setState");
      sandbox.spy(Router, "loadMessagesFromAllProviders");

      await Router._maybeUpdateL10nAttachment();

      assert.calledWith(Router.setState, {
        localeInUse: "fr",
        providers: [
          {
            id: "cfr",
            enabled: true,
            type: "remote-settings",
            bucket: "cfr",
            lastUpdated: undefined,
            errors: [],
          },
        ],
      });
      assert.calledOnce(Router.loadMessagesFromAllProviders);
    });
    it("should not update the l10n attachment if the provider doesn't need l10n attachment", async () => {
      const getter = sandbox.stub();
      getter.onFirstCall().returns("en-US");
      getter.onSecondCall().returns("fr");
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").get(getter);
      const provider = {
        id: "localProvider",
        enabled: true,
        type: "local",
      };
      await createRouterAndInit([provider]);
      sandbox.spy(Router, "setState");
      sandbox.spy(Router, "loadMessagesFromAllProviders");

      await Router._maybeUpdateL10nAttachment();

      assert.notCalled(Router.setState);
      assert.notCalled(Router.loadMessagesFromAllProviders);
    });
  });
  describe("#observe", () => {
    it("should reload l10n for CFRPageActions when the `USE_REMOTE_L10N_PREF` pref is changed", () => {
      sandbox.spy(CFRPageActions, "reloadL10n");

      Router.observe("", "", USE_REMOTE_L10N_PREF);

      assert.calledOnce(CFRPageActions.reloadL10n);
    });
    it("should not react to other pref changes", () => {
      sandbox.spy(CFRPageActions, "reloadL10n");

      Router.observe("", "", "foo");

      assert.notCalled(CFRPageActions.reloadL10n);
    });
  });
  describe("#sendAsyncMessageToPreloaded", () => {
    it("should send the message to the preloaded browser if there's data and a preloaded browser exists", () => {
      const port = {
        browser: {
          getAttribute() {
            return "preloaded";
          },
        },
        sendAsyncMessage: sinon.spy(),
      };
      Router.messageChannel.messagePorts.push(port);

      const action = { type: "FOO" };
      Router.sendAsyncMessageToPreloaded(action);
      assert.calledWith(port.sendAsyncMessage, OUTGOING_MESSAGE_NAME, action);
    });
    it("should send the message to all the preloaded browsers if there's data and they exist", () => {
      const port = {
        browser: {
          getAttribute() {
            return "preloaded";
          },
        },
        sendAsyncMessage: sinon.spy(),
      };
      Router.messageChannel.messagePorts.push(port);
      Router.messageChannel.messagePorts.push(port);
      Router.sendAsyncMessageToPreloaded({ type: "FOO" });
      assert.calledTwice(port.sendAsyncMessage);
    });
    it("should not send the message to the preloaded browser if there's no data and a preloaded browser does not exists", () => {
      const port = {
        browser: {
          getAttribute() {
            return "consumed";
          },
        },
        sendAsyncMessage: sinon.spy(),
      };
      Router.messageChannel.messagePorts.push(port);
      Router.sendAsyncMessageToPreloaded({ type: "FOO" });
      assert.notCalled(port.sendAsyncMessage);
    });
  });
  describe("#loadAllMessageGroups", () => {
    it("should group local providers and message providers, group enabled", async () => {
      const groupsProvider = {
        id: "message-groups",
        enabled: true,
        userPreferences: [],
        type: "remote-settings",
      };
      const messageGroups = [
        {
          id: "group-1",
          enabled: true,
          userPreferences: [],
          type: "remote-settings",
        },
      ];
      const stub = sandbox
        .stub(MessageLoaderUtils, "_loadDataForProvider")
        .withArgs(groupsProvider, sinon.match.object)
        .returns({ messages: messageGroups });

      await Router.setState({
        providers: [
          {
            id: "provider-group",
            enabled: true,
            type: "local",
            frequency: { lifetime: 3 },
          },
          groupsProvider,
        ],
      });

      await Router.loadAllMessageGroups();

      assert.calledOnce(stub);
      assert.lengthOf(Router.state.groups, 3);
      assert.propertyVal(
        Router.state.groups.find(group => group.id === "provider-group")
          .frequency,
        "lifetime",
        3
      );
      Router.state.groups.forEach(group => {
        assert.jsonSchema(group, MessageGroupSchema);
      });
    });
    it("should group local providers and message providers, group disabled", async () => {
      const groupsProvider = {
        id: "message-groups",
        enabled: false,
        userPreferences: [],
        type: "remote-settings",
      };
      const messageGroups = [
        {
          id: "group-1",
          enabled: true,
          userPreferences: ["foo"],
          type: "local",
        },
      ];
      sandbox
        .stub(MessageLoaderUtils, "_loadDataForProvider")
        .withArgs(groupsProvider, sinon.match.object)
        .returns({ messages: messageGroups });
      const stub = sandbox
        .stub(ASRouterPreferences, "getUserPreference")
        .withArgs("foo")
        .returns(false);

      await Router.setState({
        providers: [
          { id: "provider-group", enabled: false, type: "remote-settings" },
          groupsProvider,
        ],
      });

      await Router.loadAllMessageGroups();

      assert.calledOnce(stub);
      assert.lengthOf(Router.state.groups, 3);
      assert.isTrue(Router.state.groups.every(group => !group.enabled));
      Router.state.groups.forEach(group => {
        assert.jsonSchema(group, MessageGroupSchema);
      });
    });
    it("should disable groups that are in the groupBlockList", async () => {
      const groupsProvider = {
        id: "message-groups",
        enabled: true,
        userPreferences: [],
        type: "remote-settings",
      };
      const messageGroups = [
        {
          id: "group-1",
          enabled: true,
          userPreferences: [],
          type: "remote-settings",
        },
      ];
      const stub = sandbox
        .stub(MessageLoaderUtils, "_loadDataForProvider")
        .withArgs(groupsProvider, sinon.match.object)
        .returns({ messages: messageGroups });

      await Router.setState({
        groupBlockList: ["message-groups", "provider-group", "group-1"],
        providers: [
          {
            id: "provider-group",
            enabled: true,
            type: "local",
            frequency: { lifetime: 3 },
          },
          groupsProvider,
        ],
      });

      await Router.loadAllMessageGroups();

      assert.calledOnce(stub);
      assert.lengthOf(Router.state.groups, 3);
      assert.isTrue(Router.state.groups.every(group => !group.enabled));
      Router.state.groups.forEach(group => {
        assert.jsonSchema(group, MessageGroupSchema);
      });
    });
    it("should override default groups with local ones", async () => {
      global.GroupsConfigurationProvider.getMessages = () => [
        {
          id: "provider-group",
          type: "local",
          enabled: false,
        },
      ];
      await Router.setState({
        providers: [
          {
            id: "message-groups",
            enabled: true,
            type: "remote",
          },
          {
            id: "provider-group",
            enabled: true,
            type: "local",
            frequency: { lifetime: 3 },
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "type", "local");
      assert.propertyVal(group, "enabled", false);
    });
    it("should override default and local groups with remote ones", async () => {
      global.GroupsConfigurationProvider.getMessages = () => [
        {
          id: "provider-group",
          type: "local",
          enabled: false,
        },
      ];
      sandbox.stub(ASRouterPreferences, "getUserPreference").returns(true);
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").resolves([
        {
          id: "provider-group",
          enabled: true,
          type: "remote",
          userPreferences: ["cfrAddons"],
        },
      ]);
      await Router.setState({
        providers: [
          {
            id: "message-groups",
            enabled: true,
            bucket: "bucket",
            type: "remote-settings",
          },
          {
            id: "provider-group",
            enabled: true,
            type: "local",
            frequency: { lifetime: 3 },
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "type", "remote");
      assert.propertyVal(group, "enabled", true);
      assert.lengthOf(Router.state.groups, 2);
    });
    it("should disable the group if the pref is false", async () => {
      sandbox.stub(ASRouterPreferences, "getUserPreference").returns(false);
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").resolves([
        {
          id: "provider-group",
          enabled: true,
          type: "remote",
          userPreferences: ["cfrAddons"],
        },
      ]);
      await Router.setState({
        providers: [
          {
            id: "message-groups",
            enabled: true,
            bucket: "bucket",
            type: "remote-settings",
          },
          {
            id: "provider-group",
            enabled: true,
            type: "local",
            frequency: { lifetime: 3 },
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "enabled", false);
    });
    it("should override local groups with remote ones", async () => {
      global.GroupsConfigurationProvider.getMessages = () => [
        {
          id: "provider-group",
          type: "local",
          enabled: false,
        },
      ];
      sandbox.stub(ASRouterPreferences, "getUserPreference").returns(true);
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").resolves([
        {
          id: "provider-group",
          enabled: true,
          type: "remote",
          userPreferences: ["cfrAddons"],
        },
      ]);
      await Router.setState({
        providers: [
          {
            id: "message-groups",
            enabled: true,
            bucket: "bucket",
            type: "remote-settings",
          },
          {
            id: "some-other-group",
            enabled: true,
            frequency: { lifetime: 3 },
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");
      const group2 = Router.state.groups.find(g => g.id === "some-other-group");

      assert.ok(group);
      assert.propertyVal(group, "type", "remote");
      assert.propertyVal(group, "enabled", true);
      assert.ok(group2);
      assert.propertyVal(group2, "type", "default");
      assert.property(group2, "frequency");
      assert.propertyVal(group2.frequency, "lifetime", 3);
      assert.lengthOf(Router.state.groups, 3);
    });
  });
  describe("#setGroupState", () => {
    it("should clear group impressions", async () => {
      await Router.setState({
        groups: [
          { id: "foo", enabled: true },
          { id: "bar", enabled: true },
        ],
        groupImpressions: { foo: [1], bar: [2] },
      });

      await Router.setGroupState({ id: "foo", value: false });

      assert.equal(
        Router.state.groups.find(({ enabled }) => enabled).id,
        "bar"
      );
      assert.equal(
        Router.state.groups.find(({ enabled }) => !enabled).id,
        "foo"
      );
      assert.notProperty(Router.state.groupImpressions, "foo");
      assert.property(Router.state.groupImpressions, "bar");
    });
  });
  describe("#blockGroupById", () => {
    beforeEach(() => {
      sandbox.stub(Router, "setGroupState");
    });
    it("should do anything if called incorrectly", async () => {
      assert.isFalse(await Router.blockGroupById());
    });
    it("should save to storage the updated blocked list", async () => {
      await Router.setState({ groupBlockList: [1, 2] });

      await Router.blockGroupById(3);

      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(Router._storage.set, "groupBlockList", [
        1,
        2,
        3,
      ]);
    });
    it("should call set group state", async () => {
      await Router.blockGroupById("block");

      assert.calledOnce(Router.setGroupState);
      assert.calledWithExactly(Router.setGroupState, {
        id: "block",
        value: false,
      });
    });
  });
  describe("#unblockGroupById", () => {
    beforeEach(() => {
      sandbox.stub(Router, "setGroupState");
    });
    it("should do anything if called incorrectly", async () => {
      assert.isFalse(await Router.unblockGroupById());
    });
    it("should save to storage the updated blocked list", async () => {
      await Router.setState({ groupBlockList: ["block_1", "block_2"] });

      await Router.unblockGroupById("block_2");

      assert.calledOnce(Router._storage.set);
      assert.calledWithExactly(Router._storage.set, "groupBlockList", [
        "block_1",
      ]);
    });
    it("should call set group state", async () => {
      await Router.unblockGroupById("unblock");

      assert.calledOnce(Router.setGroupState);
      assert.calledWithExactly(Router.setGroupState, {
        id: "unblock",
        value: true,
      });
    });
  });
  describe("#loadMessagesForProvider", () => {
    it("should fetch messages from the ExperimentAPI", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["asrouter"],
      };

      await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.calledOnce(global.ExperimentAPI.getExperiment);
      assert.calledWithExactly(global.ExperimentAPI.getExperiment, {
        group: "asrouter",
      });
    });
    it("should handle the case of no experiments in the ExperimentAPI", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["asrouter"],
      };

      global.ExperimentAPI.getExperiment.throws();
      const stub = sandbox.stub(MessageLoaderUtils, "reportError");

      await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.calledOnce(stub);
    });
    it("should handle the case of no experiments in the ExperimentAPI", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["asrouter"],
      };

      global.ExperimentAPI.getExperiment.returns(null);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.lengthOf(result.messages, 0);
    });
    it("should normally load ExperimentAPI messages", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["asrouter"],
      };

      global.ExperimentAPI.getExperiment.returns({
        branch: { value: ["foo", "bar"] },
      });

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.lengthOf(result.messages, 2);
    });
    it("should fetch messages from the ExperimentAPI", async () => {
      global.ExperimentAPI.ready.throws();
      const args = {
        type: "remote-experiments",
        messageGroups: ["asrouter"],
      };
      const stub = sandbox.stub(MessageLoaderUtils, "reportError");

      await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.notCalled(global.ExperimentAPI.getExperiment);
      assert.calledOnce(stub);
    });
    it("should fetch json from url", async () => {
      let result = await MessageLoaderUtils.loadMessagesForProvider({
        location: "http://fake.com/endpoint",
        type: "json",
      });

      assert.property(result, "messages");
      assert.lengthOf(result.messages, FAKE_REMOTE_MESSAGES.length);
    });
    it("should catch errors", async () => {
      fetchStub.throws();
      let result = await MessageLoaderUtils.loadMessagesForProvider({
        location: "http://fake.com/endpoint",
        type: "json",
      });

      assert.property(result, "messages");
      assert.lengthOf(result.messages, 0);
    });
  });
});

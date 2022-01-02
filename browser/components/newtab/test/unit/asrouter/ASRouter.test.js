import { _ASRouter, MessageLoaderUtils } from "lib/ASRouter.jsm";
import { QueryCache } from "lib/ASRouterTargeting.jsm";
import {
  FAKE_LOCAL_MESSAGES,
  FAKE_LOCAL_PROVIDER,
  FAKE_LOCAL_PROVIDERS,
  FAKE_REMOTE_MESSAGES,
  FAKE_REMOTE_PROVIDER,
  FAKE_REMOTE_SETTINGS_PROVIDER,
} from "./constants";
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

const MESSAGE_PROVIDER_PREF_NAME =
  "browser.newtabpage.activity-stream.asrouter.providers.snippets";
const FAKE_PROVIDERS = [
  FAKE_LOCAL_PROVIDER,
  FAKE_REMOTE_PROVIDER,
  FAKE_REMOTE_SETTINGS_PROVIDER,
];
const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
const FAKE_RESPONSE_HEADERS = { get() {} };
const FAKE_BUNDLE = [FAKE_LOCAL_MESSAGES[1], FAKE_LOCAL_MESSAGES[2]];

const USE_REMOTE_L10N_PREF =
  "browser.newtabpage.activity-stream.asrouter.useRemoteL10n";

// eslint-disable-next-line max-statements
describe("ASRouter", () => {
  let Router;
  let globals;
  let sandbox;
  let initParams;
  let messageBlockList;
  let providerBlockList;
  let messageImpressions;
  let groupImpressions;
  let previousSessionEnd;
  let fetchStub;
  let clock;
  let getStringPrefStub;
  let fakeAttributionCode;
  let fakeTargetingContext;
  let FakeToolbarBadgeHub;
  let FakeToolbarPanelHub;
  let FakeMomentsPageHub;
  let ASRouterTargeting;

  function setMessageProviderPref(value) {
    sandbox.stub(ASRouterPreferences, "providers").get(() => value);
  }

  function initASRouter(router) {
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
      .withArgs("previousSessionEnd")
      .returns(Promise.resolve(previousSessionEnd));
    initParams = {
      storage: {
        get: getStub,
        set: sandbox.stub().returns(Promise.resolve()),
      },
      sendTelemetry: sandbox.stub().resolves(),
      clearChildMessages: sandbox.stub().resolves(),
      clearChildProviders: sandbox.stub().resolves(),
      updateAdminState: sandbox.stub().resolves(),
      dispatchCFRAction: sandbox.stub().resolves(),
    };
    sandbox.stub(router, "loadMessagesFromAllProviders").callThrough();
    return router.init(initParams);
  }

  async function createRouterAndInit(providers = FAKE_PROVIDERS) {
    setMessageProviderPref(providers);
    // `.freeze` to catch any attempts at modifying the object
    Router = new _ASRouter(Object.freeze(FAKE_LOCAL_PROVIDERS));
    await initASRouter(Router);
  }

  beforeEach(async () => {
    globals = new GlobalOverrider();
    messageBlockList = [];
    providerBlockList = [];
    messageImpressions = {};
    groupImpressions = {};
    previousSessionEnd = 100;
    sandbox = sinon.createSandbox();
    ASRouterTargeting = {
      isMatch: sandbox.stub(),
      findMatchingMessage: sandbox.stub(),
      Environment: {
        locale: "en-US",
        localeLanguageCode: "en",
        browserSettings: {
          update: {
            channel: "default",
            enabled: true,
            autoDownload: true,
          },
        },
        attributionData: {},
        currentDate: "2000-01-01T10:00:0.001Z",
        profileAgeCreated: {},
        profileAgeReset: {},
        usesFirefoxSync: false,
        isFxAEnabled: true,
        sync: {
          desktopDevices: 0,
          mobileDevices: 0,
          totalDevices: 0,
        },
        xpinstallEnabled: true,
        addonsInfo: {},
        searchEngines: {},
        isDefaultBrowser: false,
        devToolsOpenedCount: 5,
        topFrecentSites: {},
        recentBookmarks: {},
        pinnedSites: [
          {
            url: "https://amazon.com",
            host: "amazon.com",
            searchTopSite: true,
          },
        ],
        providerCohorts: {
          onboarding: "",
          cfr: "",
          "message-groups": "",
          "messaging-experiments": "",
          snippets: "",
          "whats-new-panel": "",
        },
        totalBookmarksCount: {},
        firefoxVersion: 80,
        region: "US",
        needsUpdate: {},
        hasPinnedTabs: false,
        hasAccessedFxAPanel: false,
        isWhatsNewPanelEnabled: true,
        userPrefs: {
          cfrFeatures: true,
          cfrAddons: true,
          snippets: true,
        },
        totalBlockedCount: {},
        blockedCountByType: {},
        attachedFxAOAuthClients: [],
        platformName: "macosx",
        scores: {},
        scoreThreshold: 5000,
        isChinaRepack: false,
        userId: "adsf",
      },
    };

    ASRouterPreferences.specialConditions = {
      someCondition: true,
    };
    sandbox.spy(ASRouterPreferences, "init");
    sandbox.spy(ASRouterPreferences, "uninit");
    sandbox.spy(ASRouterPreferences, "addListener");
    sandbox.spy(ASRouterPreferences, "removeListener");

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
      deleteFileAsync: () => Promise.resolve(),
      writeAttributionFile: () => Promise.resolve(),
      getCachedAttributionData: sinon.stub(),
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
    fakeTargetingContext = {
      combineContexts: sandbox.stub(),
      evalWithDefault: sandbox.stub().resolves(),
    };
    let fakeNimbusFeatures = [
      "cfr",
      "moments-page",
      "infobar",
      "spotlight",
    ].reduce((features, featureId) => {
      features[featureId] = {
        getAllVariables: sandbox.stub().returns(null),
        recordExposureEvent: sandbox.stub(),
      };
      return features;
    }, {});
    globals.set({
      // Testing framework doesn't know how to `defineLazyModuleGetter` so we're
      // importing these modules into the global scope ourselves.
      GroupsConfigurationProvider: { getMessages: () => Promise.resolve([]) },
      ASRouterPreferences,
      TARGETING_PREFERENCES,
      ASRouterTargeting,
      ASRouterTriggerListeners,
      QueryCache,
      gURLBar: {},
      isSeparateAboutWelcome: true,
      AttributionCode: fakeAttributionCode,
      SnippetsTestMessageProvider,
      PanelTestProvider,
      MacAttribution: { applicationPath: "" },
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
      NimbusFeatures: fakeNimbusFeatures,
      ExperimentAPI: {
        getExperimentMetaData: sandbox.stub().returns({
          slug: "experiment-slug",
          active: true,
          branch: { slug: "experiment-branch-slug" },
        }),
        getExperiment: sandbox.stub().returns({
          branch: {
            slug: "unit-slug",
            feature: {
              featureId: "foo",
              value: { id: "test-message" },
            },
          },
        }),
        getAllBranches: sandbox.stub().resolves([]),
        ready: sandbox.stub().resolves(),
      },
      SpecialMessageActions: {
        handleAction: sandbox.stub(),
      },
      TargetingContext: class {
        static combineContexts(...args) {
          return fakeTargetingContext.combineContexts.apply(sandbox, args);
        }

        evalWithDefault(expr) {
          return fakeTargetingContext.evalWithDefault(expr);
        }
      },
      RemoteL10n: {
        isLocaleSupported: () => true,
      },
    });
    await createRouterAndInit();
  });
  afterEach(() => {
    Router.uninit();
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
    it("should only be called once", async () => {
      Router = new _ASRouter();
      let state = await initASRouter(Router);

      assert.equal(state, Router.state);

      assert.isNull(await Router.init({}));
    });
    it("should only be called once", async () => {
      Router = new _ASRouter();
      initASRouter(Router);
      let secondCall = await Router.init({});

      assert.isNull(
        secondCall,
        "Should not init twice, it should exit early with null"
      );
    });
    it("should set state.messageBlockList to the block list in persistent storage", async () => {
      messageBlockList = ["foo"];
      Router = new _ASRouter();
      await initASRouter(Router);

      assert.deepEqual(Router.state.messageBlockList, ["foo"]);
    });
    it("should initialize all the hub providers", async () => {
      // ASRouter init called in `beforeEach` block above

      assert.calledOnce(FakeToolbarBadgeHub.init);
      assert.calledOnce(FakeToolbarPanelHub.init);
      assert.calledOnce(FakeMomentsPageHub.init);

      assert.calledWithExactly(
        FakeToolbarBadgeHub.init,
        Router.waitForInitialized,
        {
          handleMessageRequest: Router.handleMessageRequest,
          addImpression: Router.addImpression,
          blockMessageById: Router.blockMessageById,
          sendTelemetry: Router.sendTelemetry,
          unblockMessageById: Router.unblockMessageById,
        }
      );

      assert.calledWithExactly(
        FakeToolbarPanelHub.init,
        Router.waitForInitialized,
        {
          getMessages: Router.handleMessageRequest,
          sendTelemetry: Router.sendTelemetry,
        }
      );

      assert.calledWithExactly(
        FakeMomentsPageHub.init,
        Router.waitForInitialized,
        {
          handleMessageRequest: Router.handleMessageRequest,
          addImpression: Router.addImpression,
          blockMessageById: Router.blockMessageById,
          sendTelemetry: Router.sendTelemetry,
        }
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
      await initASRouter(Router);

      assert.deepEqual(Router.state.messageImpressions, messageImpressions);
    });
    it("should clear impressions for groups that are not active", async () => {
      groupImpressions = { foo: [0, 1, 2] };
      Router = new _ASRouter();
      await initASRouter(Router);

      assert.notProperty(Router.state.groupImpressions, "foo");
    });
    it("should keep impressions for groups that are active", async () => {
      Router = new _ASRouter();
      await initASRouter(Router);
      await Router.setState(() => {
        return {
          groups: [
            {
              id: "foo",
              enabled: true,
              frequency: {
                custom: [{ period: ONE_DAY_IN_MS, cap: 10 }],
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
    it("should remove old impressions for a group", async () => {
      Router = new _ASRouter();
      await initASRouter(Router);
      await Router.setState(() => {
        return {
          groups: [
            {
              id: "foo",
              enabled: true,
              frequency: {
                custom: [{ period: ONE_DAY_IN_MS, cap: 10 }],
              },
            },
          ],
          groupImpressions: {
            foo: [Date.now() - ONE_DAY_IN_MS - 1, Date.now()],
          },
        };
      });
      Router.cleanupImpressions();

      assert.property(Router.state.groupImpressions, "foo");
      assert.lengthOf(Router.state.groupImpressions.foo, 1);
    });
    it("should await .loadMessagesFromAllProviders() and add messages from providers to state.messages", async () => {
      Router = new _ASRouter(Object.freeze(FAKE_LOCAL_PROVIDERS));

      await initASRouter(Router);

      assert.calledOnce(Router.loadMessagesFromAllProviders);
      assert.isArray(Router.state.messages);
      assert.lengthOf(
        Router.state.messages,
        FAKE_LOCAL_MESSAGES.length + FAKE_REMOTE_MESSAGES.length
      );
    });
    it("should load additional allowed hosts", async () => {
      getStringPrefStub.returns('["allow.com"]');
      await createRouterAndInit();

      assert.propertyVal(Router.ALLOWLIST_HOSTS, "allow.com", "preview");
      // Should still include the defaults
      assert.lengthOf(Object.keys(Router.ALLOWLIST_HOSTS), 3);
    });
    it("should fallback to defaults if pref parsing fails", async () => {
      getStringPrefStub.returns("err");
      await createRouterAndInit();

      assert.lengthOf(Object.keys(Router.ALLOWLIST_HOSTS), 2);
      assert.propertyVal(
        Router.ALLOWLIST_HOSTS,
        "snippets-admin.mozilla.org",
        "preview"
      );
      assert.propertyVal(
        Router.ALLOWLIST_HOSTS,
        "activity-stream-icons.services.mozilla.com",
        "production"
      );
    });
    it("should set state.previousSessionEnd from IndexedDB", async () => {
      previousSessionEnd = 200;
      await createRouterAndInit();

      assert.equal(Router.state.previousSessionEnd, previousSessionEnd);
    });
    it("should assign ASRouterPreferences.specialConditions to state", async () => {
      assert.isTrue(ASRouterPreferences.specialConditions.someCondition);
      assert.isTrue(Router.state.someCondition);
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
      fakeTargetingContext.evalWithDefault.resolves(false);

      await Router.onPrefChange("services.sync.username");

      assert.calledOnce(initParams.clearChildMessages);
      assert.calledWith(initParams.clearChildMessages, [messageTargeted.id]);
    });
    it("should call loadMessagesFromAllProviders on pref change", () => {
      ASRouterPreferences.observe(null, null, MESSAGE_PROVIDER_PREF_NAME);
      assert.calledOnce(Router.loadMessagesFromAllProviders);
    });
    it("should update groups state if a user pref changes", async () => {
      await Router.setState({
        groups: [{ id: "foo", userPreferences: ["bar"], enabled: true }],
      });
      sandbox.stub(ASRouterPreferences, "getUserPreference");

      await Router.onPrefChange(MESSAGE_PROVIDER_PREF_NAME);

      assert.calledWithExactly(ASRouterPreferences.getUserPreference, "bar");
    });
    it("should update the list of providers on pref change", async () => {
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
      await Router._updateMessageProviders();

      const provider = Router.state.providers.find(p => p.url === "baz.com");
      assert.lengthOf(Router.state.providers, length);
      assert.isDefined(provider);
    });
    it("should clear disabled providers on pref change", async () => {
      const TEST_PROVIDER_ID = "some_provider_id";
      await Router.setState({
        providers: [{ id: TEST_PROVIDER_ID }],
      });
      const modifiedRemoteProvider = Object.assign({}, FAKE_REMOTE_PROVIDER, {
        id: TEST_PROVIDER_ID,
        enabled: false,
      });
      setMessageProviderPref([
        FAKE_LOCAL_PROVIDER,
        modifiedRemoteProvider,
        FAKE_REMOTE_SETTINGS_PROVIDER,
      ]);
      await Router.onPrefChange(MESSAGE_PROVIDER_PREF_NAME);

      assert.calledOnce(initParams.clearChildProviders);
      assert.calledWith(initParams.clearChildProviders, [TEST_PROVIDER_ID]);
    });
  });

  describe("setState", () => {
    it("should broadcast a message to update the admin tool on a state change if the asrouter.devtoolsEnabled pref is", async () => {
      sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => true);
      sandbox.stub(Router, "getTargetingParameters").resolves({});
      const state = await Router.setState({ foo: 123 });

      assert.calledOnce(initParams.updateAdminState);
      assert.deepEqual(state.providerPrefs, ASRouterPreferences.providers);
      assert.deepEqual(
        state.userPrefs,
        ASRouterPreferences.getAllUserPreferences()
      );
      assert.deepEqual(state.targetingParameters, {});
      assert.deepEqual(state.errors, Router.errors);
    });
    it("should not send a message on a state change asrouter.devtoolsEnabled pref is on", async () => {
      sandbox.stub(ASRouterPreferences, "devtoolsEnabled").get(() => false);
      await Router.setState({ foo: 123 });

      assert.notCalled(initParams.updateAdminState);
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
    it("should call ASRouterTargeting to evaluate", async () => {
      fakeTargetingContext.evalWithDefault.resolves("foo");
      const response = await Router.evaluateExpression({});
      assert.equal(response.evaluationStatus.result, "foo");
      assert.isTrue(response.evaluationStatus.success);
    });
    it("should catch evaluation errors", async () => {
      fakeTargetingContext.evalWithDefault.returns(
        Promise.reject(new Error("fake error"))
      );
      const response = await Router.evaluateExpression({});
      assert.isFalse(response.evaluationStatus.success);
    });
  });

  describe("#routeCFRMessage", () => {
    let browser;
    beforeEach(() => {
      sandbox.stub(CFRPageActions, "forceRecommendation");
      sandbox.stub(CFRPageActions, "addRecommendation");
      browser = {};
    });
    it("should route whatsnew_panel_message message to the right hub", () => {
      Router.routeCFRMessage(
        { template: "whatsnew_panel_message" },
        browser,
        "",
        true
      );

      assert.calledOnce(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route moments messages to the right hub", () => {
      Router.routeCFRMessage({ template: "update_action" }, browser, "", true);

      assert.calledOnce(FakeMomentsPageHub.executeAction);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
    });
    it("should route toolbar_badge message to the right hub", () => {
      Router.routeCFRMessage({ template: "toolbar_badge" }, browser);

      assert.calledOnce(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route milestone_message to the right hub", () => {
      Router.routeCFRMessage(
        { template: "milestone_message" },
        browser,
        "",
        false
      );

      assert.calledOnce(CFRPageActions.addRecommendation);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_doorhanger message to the right hub force = false", () => {
      Router.routeCFRMessage(
        { template: "cfr_doorhanger" },
        browser,
        { param: {} },
        false
      );

      assert.calledOnce(CFRPageActions.addRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_doorhanger message to the right hub force = true", () => {
      Router.routeCFRMessage({ template: "cfr_doorhanger" }, browser, {}, true);

      assert.calledOnce(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_urlbar_chiclet message to the right hub force = false", () => {
      Router.routeCFRMessage(
        { template: "cfr_urlbar_chiclet" },
        browser,
        { param: {} },
        false
      );

      assert.calledOnce(CFRPageActions.addRecommendation);
      const { args } = CFRPageActions.addRecommendation.firstCall;
      // Host should be null
      assert.isNull(args[1]);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route cfr_urlbar_chiclet message to the right hub force = true", () => {
      Router.routeCFRMessage(
        { template: "cfr_urlbar_chiclet" },
        browser,
        {},
        true
      );

      assert.calledOnce(CFRPageActions.forceRecommendation);
      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.addRecommendation);
      assert.notCalled(FakeToolbarBadgeHub.registerBadgeNotificationListener);
      assert.notCalled(FakeMomentsPageHub.executeAction);
    });
    it("should route default to sending to content", () => {
      Router.routeCFRMessage({ template: "snippets" }, browser, {}, true);

      assert.notCalled(FakeToolbarPanelHub.forceShowMessage);
      assert.notCalled(CFRPageActions.forceRecommendation);
      assert.notCalled(CFRPageActions.addRecommendation);
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
      assert.deepEqual(Router.state, previousState);
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
      const stub = sandbox.stub(MessageLoaderUtils, "loadMessagesForProvider");

      clock.tick(300);

      await Router.loadMessagesFromAllProviders();

      assert.deepEqual(Router.state, previousState);
      assert.notCalled(stub);
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
      assert.calledWith(initParams.dispatchCFRAction, {
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
      assert.calledWith(initParams.dispatchCFRAction, {
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

      assert.calledWith(initParams.dispatchCFRAction, {
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
    it("should correctly replace %STARTPAGE_VERSION% in remote provider urls", async () => {
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
      await Router._updateMessageProviders();
      assert.equal(
        Router.state.providers[0].url,
        `https://www.mozilla.org/${parseInt(expectedStartpageVersion, 10)}/`
      );
    });
    it("should replace other params in remote provider urls by calling Services.urlFormater.formatURL", async () => {
      const url = "https://www.example.com/";
      const replacedUrl = "https://www.foo.bar/";
      const stub = sandbox
        .stub(global.Services.urlFormatter, "formatURL")
        .withArgs(url)
        .returns(replacedUrl);
      const provider = { id: "foo", enabled: true, type: "remote", url };
      setMessageProviderPref([provider]);
      await Router._updateMessageProviders();
      assert.calledOnce(stub);
      assert.calledWithExactly(stub, url);
      assert.equal(Router.state.providers[0].url, replacedUrl);
    });
    it("should only add the providers that are enabled", async () => {
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
      await Router._updateMessageProviders();
      assert.equal(Router.state.providers.length, 1);
      assert.equal(Router.state.providers[0].id, providers[1].id);
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
      await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.calledWithMatch(ASRouterTargeting.findMatchingMessage, {
        messages: [{ id: "bar", provider: "snippets", groups: ["snippets"] }],
      });
    });
    it("should not return a message from a disabled group", async () => {
      ASRouterTargeting.findMatchingMessage.callsFake(
        ({ messages }) => messages[0]
      );
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

      await Router.handleMessageRequest({
        provider: "snippets",
      });
      assert.calledWithMatch(ASRouterTargeting.findMatchingMessage, {
        messages: [{ id: "bar", provider: "snippets", groups: ["snippets"] }],
      });
    });
    it("should not return a message excluded by the provider", async () => {
      // There are only two providers; block the FAKE_LOCAL_PROVIDER, leaving
      // only FAKE_REMOTE_PROVIDER unblocked, which provides only one message
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
      ASRouterTargeting.findMatchingMessage.callsFake(
        ({ messages }) => messages[0]
      );

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
      ASRouterTargeting.findMatchingMessage.callsFake(
        ({ messages }) => messages[0]
      );

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
      ASRouterTargeting.findMatchingMessage.callsFake(() => [
        message2,
        message1,
      ]);
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

      Router.handleMessageRequest(trigger);

      assert.calledOnce(ASRouterTargeting.findMatchingMessage);

      const [options] = ASRouterTargeting.findMatchingMessage.firstCall.args;
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

      Router.handleMessageRequest(trigger);

      assert.calledOnce(ASRouterTargeting.findMatchingMessage);

      const [options] = ASRouterTargeting.findMatchingMessage.firstCall.args;
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

      Router.handleMessageRequest(trigger);

      assert.calledOnce(ASRouterTargeting.findMatchingMessage);

      const [options] = ASRouterTargeting.findMatchingMessage.firstCall.args;
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
      ASRouterTargeting.findMatchingMessage.callsFake(args => args.messages);

      const result = Router.handleMessageRequest(trigger);

      assert.lengthOf(result, 1);
      assert.deepEqual(result[0], message1);
    });
  });

  describe("#uninit", () => {
    it("should unregister the trigger listeners", () => {
      for (const listener of ASRouterTriggerListeners.values()) {
        sandbox.spy(listener, "uninit");
      }

      Router.uninit();

      for (const listener of ASRouterTriggerListeners.values()) {
        assert.calledOnce(listener.uninit);
      }
    });
    it("should set .dispatchCFRAction to null", () => {
      Router.uninit();
      assert.isNull(Router.dispatchCFRAction);
      assert.isNull(Router.clearChildMessages);
      assert.isNull(Router.sendTelemetry);
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

  describe("sendNewTabMessage", () => {
    it("should construct an appropriate response message", async () => {
      Router.loadMessagesFromAllProviders.resetHistory();
      Router.loadMessagesFromAllProviders.onFirstCall().resolves();

      let message = {
        id: "foo",
        provider: "snippets",
        groups: ["snippets"],
      };

      await Router.setState({
        messages: [message],
        providers: [{ id: "snippets" }],
      });

      ASRouterTargeting.findMatchingMessage.callsFake(
        ({ messages }) => messages[0]
      );

      let response = await Router.sendNewTabMessage({
        tabId: 0,
        browser: {},
      });

      assert.deepEqual(response.message, message);
    });
    it("should send an empty object message if no messages are available", async () => {
      await Router.setState({ messages: [] });
      let response = await Router.sendNewTabMessage({
        tabId: 0,
        browser: {},
      });

      assert.deepEqual(response.message, {});
    });

    describe("#addPreviewEndpoint", () => {
      it("should make a request to the provided endpoint", async () => {
        const url = "https://snippets-admin.mozilla.org/foo";
        const browser = {};
        browser.sendMessageToActor = sandbox.stub();

        await Router.sendNewTabMessage({
          endpoint: { url },
          tabId: 0,
          browser,
        });

        assert.calledWith(global.fetch, url);
        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should send EnterSnippetPreviewMode when adding a preview endpoint", async () => {
        const url = "https://snippets-admin.mozilla.org/foo";
        const browser = {};
        browser.sendMessageToActor = sandbox.stub();

        await Router.addPreviewEndpoint(url, browser);

        assert.calledWithExactly(
          browser.sendMessageToActor,
          "EnterSnippetsPreviewMode",
          {},
          "ASRouter"
        );
      });
      it("should not add a url that is not from an allowed host", async () => {
        const url = "https://mozilla.org";
        const browser = {};
        browser.sendMessageToActor = sandbox.stub();

        await Router.addPreviewEndpoint(url, browser);

        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
      });
      it("should reject bad urls", async () => {
        const url = "foo";
        const browser = {};
        browser.sendMessageToActor = sandbox.stub();

        await Router.addPreviewEndpoint(url, browser);

        assert.lengthOf(
          Router.state.providers.filter(p => p.url === url),
          0
        );
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
      const tabId = 123;
      await Router.sendNewTabMessage({
        tabId,
        browser: {},
      });

      assert.calledOnce(startTelemetryStopwatch);
      assert.calledWithExactly(
        startTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { tabId }
      );
      assert.calledOnce(finishTelemetryStopwatch);
      assert.calledWithExactly(
        finishTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { tabId }
      );
    });
    it("should return the preview message if that's available and remove it from Router.state", async () => {
      const expectedObj = {
        id: "foo",
        groups: ["preview"],
        provider: "preview",
      };
      await Router.setState({
        messages: [expectedObj],
        providers: [{ id: "preview" }],
      });

      ASRouterTargeting.findMatchingMessage.callsFake(
        ({ messages }) => expectedObj
      );

      Router.loadMessagesFromAllProviders.resetHistory();
      Router.loadMessagesFromAllProviders.onFirstCall().resolves();

      let response = await Router.sendNewTabMessage({
        endpoint: { url: "foo.com" },
        tabId: 0,
        browser: {},
      });

      assert.deepEqual(response.message, expectedObj);

      assert.isUndefined(
        Router.state.messages.find(m => m.provider === "preview")
      );
    });
  });

  describe("#setMessageById", async () => {
    it("should send an empty message if provided id did not resolve to a message", async () => {
      let response = await Router.setMessageById({ id: -1 }, true, {});
      assert.deepEqual(response.message, {});
    });
  });

  describe("#isUnblockedMessage", () => {
    it("should block a message if the group is blocked", async () => {
      const msg = { id: "msg1", groups: ["foo"], provider: "unit-test" };
      await Router.setState({
        groups: [{ id: "foo", enabled: false }],
        messages: [msg],
        providers: [{ id: "unit-test" }],
      });
      assert.isFalse(Router.isUnblockedMessage(msg));

      await Router.setState({ groups: [{ id: "foo", enabled: true }] });

      assert.isTrue(Router.isUnblockedMessage(msg));
    });
    it("should block a message if at least one group is blocked", async () => {
      const msg = {
        id: "msg1",
        groups: ["foo", "bar"],
        provider: "unit-test",
      };
      await Router.setState({
        groups: [
          { id: "foo", enabled: false },
          { id: "bar", enabled: false },
        ],
        messages: [msg],
        providers: [{ id: "unit-test" }],
      });
      assert.isFalse(Router.isUnblockedMessage(msg));

      await Router.setState({
        groups: [
          { id: "foo", enabled: true },
          { id: "bar", enabled: false },
        ],
      });

      assert.isFalse(Router.isUnblockedMessage(msg));
    });
  });

  describe("#blockMessageById", () => {
    it("should add the id to the messageBlockList", async () => {
      await Router.blockMessageById("foo");
      assert.isTrue(Router.state.messageBlockList.includes("foo"));
    });
    it("should add the campaign to the messageBlockList instead of id if .campaign is specified and not select messages of that campaign again", async () => {
      await Router.setState({
        messages: [
          { id: "1", campaign: "foocampaign" },
          { id: "2", campaign: "foocampaign" },
        ],
      });
      await Router.blockMessageById("1");

      assert.isTrue(Router.state.messageBlockList.includes("foocampaign"));
      assert.isEmpty(Router.state.messages.filter(Router.isUnblockedMessage));
    });
    it("should be able to add multiple items to the messageBlockList", async () => {
      await await Router.blockMessageById(FAKE_BUNDLE.map(b => b.id));
      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[0].id));
      assert.isTrue(Router.state.messageBlockList.includes(FAKE_BUNDLE[1].id));
    });
    it("should save the messageBlockList", async () => {
      await await Router.blockMessageById(FAKE_BUNDLE.map(b => b.id));
      assert.calledWithExactly(Router._storage.set, "messageBlockList", [
        FAKE_BUNDLE[0].id,
        FAKE_BUNDLE[1].id,
      ]);
    });
  });

  describe("#unblockMessageById", () => {
    it("should remove the id from the messageBlockList", async () => {
      await Router.blockMessageById("foo");
      assert.isTrue(Router.state.messageBlockList.includes("foo"));
      await Router.unblockMessageById("foo");
      assert.isFalse(Router.state.messageBlockList.includes("foo"));
    });
    it("should remove the campaign from the messageBlockList if it is defined", async () => {
      await Router.setState({ messages: [{ id: "1", campaign: "foo" }] });
      await Router.blockMessageById("1");
      assert.isTrue(
        Router.state.messageBlockList.includes("foo"),
        "blocklist has campaign id"
      );
      await Router.unblockMessageById("1");
      assert.isFalse(
        Router.state.messageBlockList.includes("foo"),
        "campaign id removed from blocklist"
      );
    });
    it("should save the messageBlockList", async () => {
      await Router.unblockMessageById("foo");
      assert.calledWithExactly(Router._storage.set, "messageBlockList", []);
    });
  });

  describe("#routeCFRMessage", () => {
    it("should allow for echoing back message modifications", () => {
      const message = { somekey: "some value" };
      const data = { content: message };
      const browser = {};
      let msg = Router.routeCFRMessage(data.content, browser, data, false);
      assert.deepEqual(msg.message, message);
    });
    it("should call CFRPageActions.forceRecommendation if the template is cfr_action and force is true", async () => {
      sandbox.stub(CFRPageActions, "forceRecommendation");
      const testMessage = { id: "foo", template: "cfr_doorhanger" };
      await Router.setState({ messages: [testMessage] });
      Router.routeCFRMessage(testMessage, {}, null, true);

      assert.calledOnce(CFRPageActions.forceRecommendation);
    });
    it("should call CFRPageActions.addRecommendation if the template is cfr_action and force is false", async () => {
      sandbox.stub(CFRPageActions, "addRecommendation");
      const testMessage = { id: "foo", template: "cfr_doorhanger" };
      await Router.setState({ messages: [testMessage] });
      Router.routeCFRMessage(testMessage, {}, {}, false);
      assert.calledOnce(CFRPageActions.addRecommendation);
    });
  });

  describe("#updateTargetingParameters", () => {
    it("should return an object containing the whole state", async () => {
      sandbox.stub(Router, "getTargetingParameters").resolves({});
      let msg = await Router.updateTargetingParameters();
      let expected = Object.assign({}, Router.state, {
        providerPrefs: ASRouterPreferences.providers,
        userPrefs: ASRouterPreferences.getAllUserPreferences(),
        targetingParameters: {},
        errors: Router.errors,
      });

      assert.deepEqual(msg, expected);
    });
  });

  describe("#reachEvent", () => {
    let experimentAPIStub;
    let messageGroups = ["cfr", "moments-page", "infobar", "spotlight"];
    beforeEach(() => {
      let getExperimentMetaDataStub = sandbox.stub();
      let getAllBranchesStub = sandbox.stub();
      messageGroups.forEach(feature => {
        global.NimbusFeatures[feature].getAllVariables.returns({
          id: `message-${feature}`,
        });
        getExperimentMetaDataStub.withArgs({ featureId: feature }).returns({
          slug: `slug-${feature}`,
          branch: {
            slug: `branch-${feature}`,
          },
        });
        getAllBranchesStub.withArgs(`slug-${feature}`).resolves([
          {
            slug: `other-branch-${feature}`,
            [feature]: { value: { trigger: "unit-test" } },
          },
        ]);
      });
      experimentAPIStub = {
        getExperimentMetaData: getExperimentMetaDataStub,
        getAllBranches: getAllBranchesStub,
      };
      globals.set("ExperimentAPI", experimentAPIStub);
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should tag `forReachEvent` for all the expected message types", async () => {
      // This should match the `providers.messaging-experiments`
      let response = await MessageLoaderUtils.loadMessagesForProvider({
        type: "remote-experiments",
        messageGroups,
      });

      // 1 message for reach 1 for expose
      assert.property(response, "messages");
      assert.lengthOf(response.messages, messageGroups.length * 2);
      assert.lengthOf(
        response.messages.filter(m => m.forReachEvent),
        messageGroups.length
      );
    });
  });

  describe("#sendTriggerMessage", () => {
    it("should pass the trigger to ASRouterTargeting when sending trigger message", async () => {
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

      Router.loadMessagesFromAllProviders.resetHistory();
      Router.loadMessagesFromAllProviders.onFirstCall().resolves();

      await Router.sendTriggerMessage({
        tabId: 0,
        browser: {},
        id: "firstRun",
      });

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

      const tabId = 123;

      await Router.sendTriggerMessage({
        tabId,
        browser: {},
        id: "firstRun",
      });

      assert.calledOnce(startTelemetryStopwatch);
      assert.calledWithExactly(
        startTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { tabId }
      );
      assert.calledOnce(finishTelemetryStopwatch);
      assert.calledWithExactly(
        finishTelemetryStopwatch,
        "MS_MESSAGE_REQUEST_TIME_MS",
        { tabId }
      );
    });
    it("should have previousSessionEnd in the message context", () => {
      assert.propertyVal(
        Router._getMessagesContext(),
        "previousSessionEnd",
        100
      );
    });
    it("should record the Reach event if found any", async () => {
      let messages = [
        {
          id: "foo1",
          forReachEvent: { sent: false, group: "cfr" },
          experimentSlug: "exp01",
          branchSlug: "branch01",
          template: "simple_template",
          trigger: { id: "foo" },
          content: { title: "Foo1", body: "Foo123-1" },
        },
        {
          id: "foo2",
          template: "simple_template",
          trigger: { id: "bar" },
          content: { title: "Foo2", body: "Foo123-2" },
          provider: "onboarding",
        },
        {
          id: "foo3",
          forReachEvent: { sent: false, group: "cfr" },
          experimentSlug: "exp02",
          branchSlug: "branch02",
          template: "simple_template",
          trigger: { id: "foo" },
          content: { title: "Foo1", body: "Foo123-1" },
        },
      ];
      sandbox.stub(Router, "handleMessageRequest").resolves(messages);
      sandbox.spy(Services.telemetry, "recordEvent");

      await Router.sendTriggerMessage({
        tabId: 0,
        browser: {},
        id: "foo",
      });

      assert.calledTwice(Services.telemetry.recordEvent);
    });
    it("should not record the Reach event if it's already sent", async () => {
      let messages = [
        {
          id: "foo1",
          forReachEvent: { sent: true, group: "cfr" },
          experimentSlug: "exp01",
          branchSlug: "branch01",
          template: "simple_template",
          trigger: { id: "foo" },
          content: { title: "Foo1", body: "Foo123-1" },
        },
      ];
      sandbox.stub(Router, "handleMessageRequest").resolves(messages);
      sandbox.spy(Services.telemetry, "recordEvent");

      await Router.sendTriggerMessage({
        tabId: 0,
        browser: {},
        id: "foo",
      });
      assert.notCalled(Services.telemetry.recordEvent);
    });
    it("should record the Exposure event for each valid feature", async () => {
      ["cfr_doorhanger", "update_action", "infobar", "spotlight"].forEach(
        async template => {
          let featureMap = {
            cfr_doorhanger: "cfr",
            spotlight: "spotlight",
            infobar: "infobar",
            update_action: "moments-page",
          };
          assert.notCalled(
            global.NimbusFeatures[featureMap[template]].recordExposureEvent
          );

          let messages = [
            {
              id: "foo1",
              template,
              trigger: { id: "foo" },
              content: { title: "Foo1", body: "Foo123-1" },
            },
          ];
          sandbox.stub(Router, "handleMessageRequest").resolves(messages);

          await Router.sendTriggerMessage({
            tabId: 0,
            browser: {},
            id: "foo",
          });

          assert.calledOnce(
            global.NimbusFeatures[featureMap[template]].recordExposureEvent
          );
        }
      );
    });
  });

  describe("forceAttribution", () => {
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
    it("should double encode on windows", async () => {
      sandbox.stub(fakeAttributionCode, "writeAttributionFile");

      Router.forceAttribution({ foo: "FOO!", eh: "NOPE", bar: "BAR?" });

      assert.notCalled(setReferrerUrl);
      assert.calledWithMatch(
        fakeAttributionCode.writeAttributionFile,
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

  describe("#forceWNPanel", () => {
    let browser = {
      ownerGlobal: {
        document: new Document(),
        PanelUI: {
          showSubView: sinon.stub(),
          panel: {
            setAttribute: sinon.stub(),
          },
        },
      },
    };
    let fakePanel = {
      setAttribute: sinon.stub(),
    };
    sinon
      .stub(browser.ownerGlobal.document, "getElementById")
      .returns(fakePanel);

    it("should call enableToolbarButton", async () => {
      await Router.forceWNPanel(browser);
      assert.calledOnce(FakeToolbarPanelHub.enableToolbarButton);
      assert.calledOnce(browser.ownerGlobal.PanelUI.showSubView);
      assert.calledWith(fakePanel.setAttribute, "noautohide", true);
    });
  });

  describe("_triggerHandler", () => {
    it("should call #sendTriggerMessage with the correct trigger", () => {
      const getter = sandbox.stub();
      getter.returns(false);
      sandbox.stub(global.BrowserHandler, "kiosk").get(getter);
      sinon.spy(Router, "sendTriggerMessage");
      const browser = {};
      const trigger = { id: "FAKE_TRIGGER", param: "some fake param" };
      Router._triggerHandler(browser, trigger);
      assert.calledOnce(Router.sendTriggerMessage);
      assert.calledWith(
        Router.sendTriggerMessage,
        sandbox.match({
          id: "FAKE_TRIGGER",
          param: "some fake param",
        })
      );
    });
  });

  describe("_triggerHandler_kiosk", () => {
    it("should not call #sendTriggerMessage", () => {
      const getter = sandbox.stub();
      getter.returns(true);
      sandbox.stub(global.BrowserHandler, "kiosk").get(getter);
      sinon.spy(Router, "sendTriggerMessage");
      const browser = {};
      const trigger = { id: "FAKE_TRIGGER", param: "some fake param" };
      Router._triggerHandler(browser, trigger);
      assert.notCalled(Router.sendTriggerMessage);
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
        assert.equal(provider.frequency.custom[0].period, "daily");
      });
      it("period aliases in message frequency caps should be normalised", async () => {
        const [message] = Router.state.messages;
        assert.equal(message.frequency.custom[0].period, "daily");
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
          let gImpressions = {};
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
      Router.loadMessagesFromAllProviders.resetHistory();

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
      Router.loadMessagesFromAllProviders.resetHistory();
      sandbox.spy(Router, "setState");

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
  describe("#loadAllMessageGroups", () => {
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
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "enabled", false);
    });
    it("should enable the group if at least one pref is true", async () => {
      sandbox
        .stub(ASRouterPreferences, "getUserPreference")
        .withArgs("cfrAddons")
        .returns(false)
        .withArgs("cfrFeatures")
        .returns(true);
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").resolves([
        {
          id: "provider-group",
          enabled: true,
          type: "remote",
          userPreferences: ["cfrAddons", "cfrFeatures"],
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
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "enabled", true);
    });
    it("should be keep the group disabled if disabled is true", async () => {
      sandbox.stub(ASRouterPreferences, "getUserPreference").returns(true);
      sandbox.stub(MessageLoaderUtils, "_getRemoteSettingsMessages").resolves([
        {
          id: "provider-group",
          enabled: false,
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
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "provider-group");

      assert.ok(group);
      assert.propertyVal(group, "enabled", false);
    });
    it("should keep local groups unchanged if provider doesn't require an update", async () => {
      sandbox.stub(MessageLoaderUtils, "shouldProviderUpdate").returns(false);
      sandbox.stub(MessageLoaderUtils, "_loadDataForProvider");
      await Router.setState({
        groups: [
          {
            id: "cfr",
            enabled: true,
            bucket: "bucket",
            type: "remote-settings",
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "cfr");

      assert.ok(group);
      assert.propertyVal(group, "enabled", true);
      // Because it should not have updated
      assert.notCalled(MessageLoaderUtils._loadDataForProvider);
    });
    it("should update local groups on pref change (no RS update)", async () => {
      sandbox.stub(MessageLoaderUtils, "shouldProviderUpdate").returns(false);
      sandbox.stub(ASRouterPreferences, "getUserPreference").returns(false);
      await Router.setState({
        groups: [
          {
            id: "cfr",
            enabled: true,
            bucket: "bucket",
            type: "remote-settings",
            userPreferences: ["cfrAddons"],
          },
        ],
      });

      await Router.loadAllMessageGroups();

      const group = Router.state.groups.find(g => g.id === "cfr");

      assert.ok(group);
      // Pref changed, updated the group state
      assert.propertyVal(group, "enabled", false);
    });
  });
  describe("unblockAll", () => {
    it("Clears the message block list and returns the state value", async () => {
      await Router.setState({ messageBlockList: ["one", "two", "three"] });
      assert.equal(Router.state.messageBlockList.length, 3);
      const state = await Router.unblockAll();
      assert.equal(Router.state.messageBlockList.length, 0);
      assert.equal(state.messageBlockList.length, 0);
    });
  });
  describe("#loadMessagesForProvider", () => {
    it("should fetch messages from the ExperimentAPI", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["spotlight"],
      };

      await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.calledOnce(global.NimbusFeatures.spotlight.getAllVariables);
      assert.calledOnce(global.ExperimentAPI.getExperimentMetaData);
      assert.calledWithExactly(global.ExperimentAPI.getExperimentMetaData, {
        featureId: "spotlight",
      });
    });
    it("should handle the case of no experiments in the ExperimentAPI", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["infobar"],
      };

      global.ExperimentAPI.getExperiment.returns(null);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.lengthOf(result.messages, 0);
    });
    it("should normally load ExperimentAPI messages", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["infobar"],
      };
      const enrollment = {
        branch: {
          slug: "branch01",
          infobar: {
            featureId: "infobar",
            value: { id: "id01", trigger: { id: "openURL" } },
          },
        },
      };

      global.NimbusFeatures.infobar.getAllVariables.returns(
        enrollment.branch.infobar.value
      );
      global.ExperimentAPI.getExperimentMetaData.returns({
        branch: { slug: enrollment.branch.slug },
      });
      global.ExperimentAPI.getAllBranches.returns([
        enrollment.branch,
        {
          slug: "control",
          infobar: {
            featureId: "infobar",
            value: null,
          },
        },
      ]);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.lengthOf(result.messages, 1);
    });
    it("should skip disabled features and not load the messages", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["cfr"],
      };

      global.NimbusFeatures.cfr.getAllVariables.returns(null);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.lengthOf(result.messages, 0);
    });
    it("should fetch branches with trigger", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["cfr"],
      };
      const enrollment = {
        slug: "exp01",
        branch: {
          slug: "branch01",
          cfr: {
            featureId: "cfr",
            value: { id: "id01", trigger: { id: "openURL" } },
          },
        },
      };

      global.NimbusFeatures.cfr.getAllVariables.returns(
        enrollment.branch.cfr.value
      );
      global.ExperimentAPI.getExperimentMetaData.returns({
        slug: enrollment.slug,
        active: true,
        branch: { slug: enrollment.branch.slug },
      });
      global.ExperimentAPI.getAllBranches.resolves([
        enrollment.branch,
        {
          slug: "branch02",
          cfr: {
            featureId: "cfr",
            value: { id: "id02", trigger: { id: "openURL" } },
          },
        },
        {
          // This branch should not be loaded as it doesn't have the trigger
          slug: "branch03",
          cfr: {
            featureId: "cfr",
            value: { id: "id03" },
          },
        },
      ]);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.equal(result.messages.length, 2);
      assert.equal(result.messages[0].id, "id01");
      assert.equal(result.messages[1].id, "id02");
      assert.equal(result.messages[1].experimentSlug, "exp01");
      assert.equal(result.messages[1].branchSlug, "branch02");
      assert.deepEqual(result.messages[1].forReachEvent, {
        sent: false,
        group: "cfr",
      });
    });
    it("should fetch branches with trigger even if enrolled branch is disabled", async () => {
      const args = {
        type: "remote-experiments",
        messageGroups: ["cfr"],
      };
      const enrollment = {
        slug: "exp01",
        branch: {
          slug: "branch01",
          cfr: {
            featureId: "cfr",
            value: {},
          },
        },
      };

      // Nedds to match the `messageGroups` value to return an enrollment
      // for that feature
      global.NimbusFeatures.cfr.getAllVariables.returns(
        enrollment.branch.cfr.value
      );
      global.ExperimentAPI.getExperimentMetaData.returns({
        slug: enrollment.slug,
        active: true,
        branch: { slug: enrollment.branch.slug },
      });
      global.ExperimentAPI.getAllBranches.resolves([
        enrollment.branch,
        {
          slug: "branch02",
          cfr: {
            featureId: "cfr",
            value: { id: "id02", trigger: { id: "openURL" } },
          },
        },
        {
          // This branch should not be loaded as it doesn't have the trigger
          slug: "branch03",
          cfr: {
            featureId: "cfr",
            value: { id: "id03" },
          },
        },
      ]);

      const result = await MessageLoaderUtils.loadMessagesForProvider(args);

      assert.equal(result.messages.length, 1);
      assert.equal(result.messages[0].id, "id02");
      assert.equal(result.messages[0].experimentSlug, "exp01");
      assert.equal(result.messages[0].branchSlug, "branch02");
      assert.deepEqual(result.messages[0].forReachEvent, {
        sent: false,
        group: "cfr",
      });
    });
  });
  describe("#_remoteSettingsLoader", () => {
    let provider;
    let spy;
    beforeEach(() => {
      provider = {
        id: "cfr",
        bucket: "cfr",
      };
      sandbox
        .stub(MessageLoaderUtils, "_getRemoteSettingsMessages")
        .resolves([{ id: "message_1" }]);
      spy = sandbox.spy();
      global.Downloader.prototype.download = spy;
    });
    it("should be called with the expected dir path", async () => {
      const dlSpy = sandbox.spy(global, "Downloader");

      sandbox
        .stub(global.Services.locale, "appLocaleAsBCP47")
        .get(() => "en-US");
      sandbox.stub(global.RemoteL10n, "isLocaleSupported").returns(true);

      await MessageLoaderUtils._remoteSettingsLoader(provider, {});

      assert.calledWith(
        dlSpy,
        "main",
        "ms-language-packs",
        "browser",
        "newtab"
      );
    });
    it("should allow fetch for known locales", async () => {
      sandbox
        .stub(global.Services.locale, "appLocaleAsBCP47")
        .get(() => "en-US");
      sandbox.stub(global.RemoteL10n, "isLocaleSupported").returns(true);

      await MessageLoaderUtils._remoteSettingsLoader(provider, {});

      assert.calledOnce(spy);
    });
    it("should fallback to 'en-US' for locale 'und' ", async () => {
      sandbox.stub(global.Services.locale, "appLocaleAsBCP47").get(() => "und");
      sandbox.stub(global.RemoteL10n, "isLocaleSupported").returns(false);
      const getRecordSpy = sandbox.spy(
        global.KintoHttpClient.prototype,
        "getRecord"
      );

      await MessageLoaderUtils._remoteSettingsLoader(provider, {});

      assert.ok(getRecordSpy.args[0][0].includes("en-US"));
      assert.calledOnce(spy);
    });
    it("should not allow fetch for unsupported locales", async () => {
      sandbox
        .stub(global.Services.locale, "appLocaleAsBCP47")
        .get(() => "unkown");
      sandbox.stub(global.RemoteL10n, "isLocaleSupported").returns(false);

      await MessageLoaderUtils._remoteSettingsLoader(provider, {});

      assert.notCalled(spy);
    });
  });
});

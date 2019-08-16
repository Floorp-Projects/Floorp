import { FakePrefs, GlobalOverrider } from "test/unit/utils";
import { actionTypes as at } from "common/Actions.jsm";
import injector from "inject!lib/TopStoriesFeed.jsm";

describe("Top Stories Feed", () => {
  let TopStoriesFeed;
  let STORIES_UPDATE_TIME;
  let TOPICS_UPDATE_TIME;
  let SECTION_ID;
  let SPOC_IMPRESSION_TRACKING_PREF;
  let REC_IMPRESSION_TRACKING_PREF;
  let MIN_DOMAIN_AFFINITIES_UPDATE_TIME;
  let DEFAULT_RECS_EXPIRE_TIME;
  let instance;
  let clock;
  let globals;
  let sectionsManagerStub;
  let shortURLStub;

  const FAKE_OPTIONS = {
    stories_endpoint: "https://somedomain.org/stories?key=$apiKey",
    stories_referrer: "https://somedomain.org/referrer",
    topics_endpoint: "https://somedomain.org/topics?key=$apiKey",
    survey_link: "https://www.surveymonkey.com/r/newtabffx",
    api_key_pref: "apiKeyPref",
    provider_name: "test-provider",
    provider_icon: "provider-icon",
    provider_description: "provider_desc",
  };

  beforeEach(() => {
    FakePrefs.prototype.prefs.apiKeyPref = "test-api-key";
    FakePrefs.prototype.prefs.pocketCta = JSON.stringify({
      cta_button: "",
      cta_text: "",
      cta_url: "",
      use_cta: false,
    });

    globals = new GlobalOverrider();
    globals.set("PlacesUtils", { history: {} });
    globals.set("pktApi", { isUserLoggedIn() {} });
    clock = sinon.useFakeTimers();
    shortURLStub = sinon.stub().callsFake(site => site.url);
    sectionsManagerStub = {
      onceInitialized: sinon.stub().callsFake(callback => callback()),
      enableSection: sinon.spy(),
      disableSection: sinon.spy(),
      updateSection: sinon.spy(),
      sections: new Map([["topstories", { options: FAKE_OPTIONS }]]),
    };

    class FakeUserDomainAffinityProvider {
      constructor(
        timeSegments,
        parameterSets,
        maxHistoryQueryResults,
        version,
        scores
      ) {
        this.timeSegments = timeSegments;
        this.parameterSets = parameterSets;
        this.maxHistoryQueryResults = maxHistoryQueryResults;
        this.version = version;
        this.scores = scores;
      }

      getAffinities() {
        return {};
      }
    }
    class FakePersonalityProvider extends FakeUserDomainAffinityProvider {}

    ({
      TopStoriesFeed,
      STORIES_UPDATE_TIME,
      TOPICS_UPDATE_TIME,
      SECTION_ID,
      SPOC_IMPRESSION_TRACKING_PREF,
      REC_IMPRESSION_TRACKING_PREF,
      MIN_DOMAIN_AFFINITIES_UPDATE_TIME,
      DEFAULT_RECS_EXPIRE_TIME,
    } = injector({
      "lib/ActivityStreamPrefs.jsm": { Prefs: FakePrefs },
      "lib/ShortURL.jsm": { shortURL: shortURLStub },
      "lib/PersonalityProvider.jsm": {
        PersonalityProvider: FakePersonalityProvider,
      },
      "lib/UserDomainAffinityProvider.jsm": {
        UserDomainAffinityProvider: FakeUserDomainAffinityProvider,
      },
      "lib/SectionsManager.jsm": { SectionsManager: sectionsManagerStub },
    }));

    instance = new TopStoriesFeed();
    instance.store = {
      getState() {
        return { Prefs: { values: { showSponsored: true } } };
      },
      dispatch: sinon.spy(),
    };
    instance.storiesLastUpdated = 0;
    instance.topicsLastUpdated = 0;
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });

  describe("#lazyloading TopStories", () => {
    beforeEach(() => {
      instance.discoveryStreamEnabled = true;
    });
    it("should bind parseOptions to SectionsManager.onceInitialized when discovery stream is true", () => {
      instance.discoveryStreamEnabled = false;
      instance.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.config": JSON.stringify({ enabled: true }),
          },
        },
      });
      instance.onAction({ type: at.INIT, data: {} });

      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("should bind parseOptions to SectionsManager.onceInitialized when discovery stream is false", () => {
      instance.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.config": JSON.stringify({ enabled: false }),
          },
        },
      });
      instance.onAction({ type: at.INIT, data: {} });
      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("Should initialize properties once while lazy loading if not initialized earlier", () => {
      instance.discoveryStreamEnabled = false;
      instance.propertiesInitialized = false;
      sinon.stub(instance, "initializeProperties");
      instance.lazyLoadTopStories();
      assert.calledOnce(instance.initializeProperties);
    });
    it("should not re-initialize properties", () => {
      // For discovery stream experience disabled TopStoriesFeed properties
      // are initialized in constructor and should not be called again while lazy loading topstories
      sinon.stub(instance, "initializeProperties");
      instance.discoveryStreamEnabled = false;
      instance.propertiesInitialized = true;
      instance.lazyLoadTopStories();
      assert.notCalled(instance.initializeProperties);
    });
    it("should have early exit onInit when discovery is true", async () => {
      sinon.stub(instance, "doContentUpdate");
      await instance.onInit();
      assert.notCalled(instance.doContentUpdate);
      assert.isUndefined(instance.storiesLoaded);
    });
    it("should complete onInit when discovery is false", async () => {
      instance.discoveryStreamEnabled = false;
      sinon.stub(instance, "doContentUpdate");
      await instance.onInit();
      assert.calledOnce(instance.doContentUpdate);
      assert.isTrue(instance.storiesLoaded);
    });
    it("should handle limited actions when discoverystream is enabled", async () => {
      sinon.spy(instance, "handleDisabled");
      sinon.stub(instance, "getPocketState");
      instance.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.config": JSON.stringify({ enabled: true }),
            "discoverystream.enabled": true,
          },
        },
      });

      instance.onAction({ type: at.INIT, data: {} });

      assert.calledOnce(instance.handleDisabled);
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.notCalled(instance.getPocketState);
    });
    it("should handle NEW_TAB_REHYDRATED when discoverystream is disabled", async () => {
      instance.discoveryStreamEnabled = false;
      sinon.spy(instance, "handleDisabled");
      sinon.stub(instance, "getPocketState");
      instance.store.getState = () => ({
        Prefs: {
          values: {
            "discoverystream.config": JSON.stringify({ enabled: false }),
          },
        },
      });
      instance.onAction({ type: at.INIT, data: {} });
      assert.notCalled(instance.handleDisabled);

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.getPocketState);
    });
    it("should handle UNINIT when discoverystream is enabled", async () => {
      sinon.stub(instance, "uninit");
      instance.onAction({ type: at.UNINIT });
      assert.calledOnce(instance.uninit);
    });
    it("should fire init on PREF_CHANGED", () => {
      sinon.stub(instance, "onInit");
      instance.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.config", value: {} },
      });
      assert.calledOnce(instance.onInit);
    });
    it("should fire init on DISCOVERY_STREAM_PREF_ENABLED", () => {
      sinon.stub(instance, "onInit");
      instance.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.enabled", value: true },
      });
      assert.calledOnce(instance.onInit);
    });
    it("should not fire init on PREF_CHANGED if stories are loaded", () => {
      sinon.stub(instance, "onInit");
      sinon.spy(instance, "lazyLoadTopStories");
      instance.storiesLoaded = true;
      instance.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.config", value: {} },
      });
      assert.calledOnce(instance.lazyLoadTopStories);
      assert.notCalled(instance.onInit);
    });
    it("should fire init on PREF_CHANGED when discoverystream is disabled", () => {
      instance.discoveryStreamEnabled = false;
      sinon.stub(instance, "onInit");
      instance.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.config", value: {} },
      });
      assert.calledOnce(instance.onInit);
    });
    it("should not fire init on PREF_CHANGED when discoverystream is disabled and stories are loaded", () => {
      instance.discoveryStreamEnabled = false;
      sinon.stub(instance, "onInit");
      sinon.spy(instance, "lazyLoadTopStories");
      instance.storiesLoaded = true;
      instance.onAction({
        type: at.PREF_CHANGED,
        data: { name: "discoverystream.config", value: {} },
      });
      assert.calledOnce(instance.lazyLoadTopStories);
      assert.notCalled(instance.onInit);
    });
  });

  describe("#init", () => {
    it("should create a TopStoriesFeed", () => {
      assert.instanceOf(instance, TopStoriesFeed);
    });
    it("should bind parseOptions to SectionsManager.onceInitialized", () => {
      instance.onAction({ type: at.INIT, data: {} });
      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("should initialize endpoints based on options", async () => {
      await instance.onInit();
      assert.equal(
        "https://somedomain.org/stories?key=test-api-key",
        instance.stories_endpoint
      );
      assert.equal(
        "https://somedomain.org/referrer",
        instance.stories_referrer
      );
      assert.equal(
        "https://somedomain.org/topics?key=test-api-key",
        instance.topics_endpoint
      );
    });
    it("should enable its section", () => {
      instance.onAction({ type: at.INIT, data: {} });
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.calledWith(sectionsManagerStub.enableSection, SECTION_ID);
    });
    it("init should fire onInit", () => {
      instance.onInit = sinon.spy();
      instance.onAction({ type: at.INIT, data: {} });
      assert.calledOnce(instance.onInit);
    });
    it("should fetch stories on init", async () => {
      instance.fetchStories = sinon.spy();
      await instance.onInit();
      assert.calledOnce(instance.fetchStories);
    });
    it("should fetch topics on init", async () => {
      instance.fetchTopics = sinon.spy();
      await instance.onInit();
      assert.calledOnce(instance.fetchTopics);
    });
    it("should not fetch if endpoint not configured", () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      sectionsManagerStub.sections.set("topstories", { options: {} });
      instance.init();
      assert.notCalled(fetchStub);
    });
    it("should report error for invalid configuration", () => {
      globals.sandbox.spy(global.Cu, "reportError");
      sectionsManagerStub.sections.set("topstories", {
        options: {
          api_key_pref: "invalid",
          stories_endpoint: "https://invalid.com/?apiKey=$apiKey",
        },
      });
      instance.init();

      assert.calledWith(
        Cu.reportError,
        "Problem initializing top stories feed: An API key was specified but none configured: https://invalid.com/?apiKey=$apiKey"
      );
    });
    it("should report error for missing api key", () => {
      globals.sandbox.spy(global.Cu, "reportError");
      sectionsManagerStub.sections.set("topstories", {
        options: {
          stories_endpoint: "https://somedomain.org/stories?key=$apiKey",
          topics_endpoint: "https://somedomain.org/topics?key=$apiKey",
        },
      });
      instance.init();

      assert.called(Cu.reportError);
    });
    it("should load data from cache on init", async () => {
      instance.loadCachedData = sinon.spy();
      await instance.onInit();
      assert.calledOnce(instance.loadCachedData);
    });
  });
  describe("#uninit", () => {
    it("should disable its section", () => {
      instance.onAction({ type: at.UNINIT });
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledWith(sectionsManagerStub.disableSection, SECTION_ID);
    });
    it("should unload stories on uninit", async () => {
      sinon.stub(instance.cache, "set").returns(Promise.resolve());
      await instance.clearCache();
      assert.calledWith(instance.cache.set.firstCall, "stories", {});
      assert.calledWith(instance.cache.set.secondCall, "topics", {});
      assert.calledWith(instance.cache.set.thirdCall, "spocs", {});
    });
  });
  describe("#cache", () => {
    it("should clear all cache items when calling clearCache", () => {
      sinon.stub(instance.cache, "set").returns(Promise.resolve());
      instance.storiesLoaded = true;
      instance.uninit();
      assert.equal(instance.storiesLoaded, false);
    });
    it("should set spocs cache on fetch", async () => {
      const response = {
        recommendations: [{ id: "1" }, { id: "2" }],
        settings: { timeSegments: {}, domainAffinityParameterSets: {} },
        spocs: [{ id: "spoc1" }],
      };

      instance.show_spocs = true;
      instance.personalized = true;
      instance.stories_endpoint = "stories-endpoint";

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", { blockedLinks: { isBlocked: () => {} } });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      sinon.spy(instance.cache, "set");

      await instance.fetchStories();

      assert.calledOnce(instance.cache.set);
      const { args } = instance.cache.set.firstCall;
      assert.equal(args[0], "stories");
      assert.equal(args[1].spocs[0].id, "spoc1");
    });
    it("should get spocs on cache load", async () => {
      instance.cache.get = () => ({
        stories: {
          recommendations: [{ id: "1" }, { id: "2" }],
          spocs: [{ id: "spoc1" }],
        },
      });
      instance.storiesLastUpdated = 0;
      globals.set("NewTabUtils", { blockedLinks: { isBlocked: () => {} } });

      await instance.loadCachedData();
      assert.equal(instance.spocs[0].guid, "spoc1");
    });
  });
  describe("#fetch", () => {
    it("should fetch stories, send event and cache results", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: {
          stories_endpoint: "stories-endpoint",
          stories_referrer: "referrer",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      const response = {
        recommendations: [
          {
            id: "1",
            title: "title",
            excerpt: "description",
            image_src: "image-url",
            url: "rec-url",
            published_timestamp: "123",
            context: "trending",
            icon: "icon",
          },
        ],
      };
      const stories = [
        {
          guid: "1",
          type: "now",
          title: "title",
          context: "trending",
          icon: "icon",
          description: "description",
          image: "image-url",
          referrer: "referrer",
          url: "rec-url",
          hostname: "rec-url",
          min_score: 0,
          score: 1,
          spoc_meta: {},
        },
      ];

      instance.cache.set = sinon.spy();
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      assert.calledOnce(fetchStub);
      assert.calledOnce(shortURLStub);
      assert.calledWithExactly(fetchStub, instance.stories_endpoint, {
        credentials: "omit",
      });
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {
        rows: stories,
      });
      assert.calledOnce(instance.cache.set);
      assert.calledWith(
        instance.cache.set,
        "stories",
        Object.assign({}, response, { _timestamp: 0 })
      );
    });
    it("should use domain as hostname, if present", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: {
          stories_endpoint: "stories-endpoint",
          stories_referrer: "referrer",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      const response = {
        recommendations: [
          {
            id: "1",
            title: "title",
            excerpt: "description",
            image_src: "image-url",
            url: "rec-url",
            domain: "domain",
            published_timestamp: "123",
            context: "trending",
            icon: "icon",
          },
        ],
      };
      const stories = [
        {
          guid: "1",
          type: "now",
          title: "title",
          context: "trending",
          icon: "icon",
          description: "description",
          image: "image-url",
          referrer: "referrer",
          url: "rec-url",
          hostname: "domain",
          min_score: 0,
          score: 1,
          spoc_meta: {},
        },
      ];

      instance.cache.set = sinon.spy();
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      assert.calledOnce(fetchStub);
      assert.notCalled(shortURLStub);
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {
        rows: stories,
      });
    });
    it("should call SectionsManager.updateSection", () => {
      instance.dispatchUpdateEvent(123, {});
      assert.calledOnce(sectionsManagerStub.updateSection);
    });
    it("should report error for unexpected stories response", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: { stories_endpoint: "stories-endpoint" },
      });
      globals.set("fetch", fetchStub);
      globals.sandbox.spy(global.Cu, "reportError");

      fetchStub.resolves({ ok: false, status: 400 });
      await instance.onInit();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.stories_endpoint, {
        credentials: "omit",
      });
      assert.equal(instance.storiesLastUpdated, 0);
      assert.called(Cu.reportError);
    });
    it("should exclude blocked (dismissed) URLs", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: { stories_endpoint: "stories-endpoint" },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: site => site.url === "blocked" },
      });

      const response = {
        recommendations: [{ url: "blocked" }, { url: "not_blocked" }],
      };
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      // Issue!
      // Should actually be fixed when cache is fixed.
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows.length,
        1
      );
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows[0].url,
        "not_blocked"
      );
    });
    it("should mark stories as new", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: { stories_endpoint: "stories-endpoint" },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      clock.restore();
      const response = {
        recommendations: [
          { published_timestamp: Date.now() / 1000 },
          { published_timestamp: "0" },
          {
            published_timestamp: (Date.now() - 2 * 24 * 60 * 60 * 1000) / 1000,
          },
        ],
      };

      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });

      await instance.onInit();
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows.length,
        3
      );
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows[0].type,
        "now"
      );
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows[1].type,
        "trending"
      );
      assert.equal(
        sectionsManagerStub.updateSection.firstCall.args[1].rows[2].type,
        "trending"
      );
    });
    it("should fetch topics, send event and cache results", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: { topics_endpoint: "topics-endpoint" },
      });
      globals.set("fetch", fetchStub);

      const response = {
        topics: [
          { name: "topic1", url: "url-topic1" },
          { name: "topic2", url: "url-topic2" },
        ],
      };
      const topics = [
        {
          name: "topic1",
          url: "url-topic1",
        },
        {
          name: "topic2",
          url: "url-topic2",
        },
      ];

      instance.cache.set = sinon.spy();
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.topics_endpoint, {
        credentials: "omit",
      });
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithMatch(sectionsManagerStub.updateSection, SECTION_ID, {
        topics,
      });
      assert.calledOnce(instance.cache.set);
      assert.calledWith(
        instance.cache.set,
        "topics",
        Object.assign({}, response, { _timestamp: 0 })
      );
    });
    it("should report error for unexpected topics response", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.sandbox.spy(global.Cu, "reportError");

      instance.topics_endpoint = "topics-endpoint";
      fetchStub.resolves({ ok: false, status: 400 });
      await instance.fetchTopics();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.topics_endpoint, {
        credentials: "omit",
      });
      assert.notCalled(instance.store.dispatch);
      assert.called(Cu.reportError);
    });
  });
  describe("#personalization", () => {
    it("should sort stories if personalization is preffed on", async () => {
      const response = {
        recommendations: [{ id: "1" }, { id: "2" }],
        settings: { timeSegments: {}, domainAffinityParameterSets: {} },
      };

      instance.personalized = true;
      instance.compareScore = sinon.spy();
      instance.stories_endpoint = "stories-endpoint";

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });

      await instance.fetchStories();
      assert.calledOnce(instance.compareScore);
    });
    it("should not sort stories if personalization is preffed off", async () => {
      const response = `{
        "recommendations":  [{"id" : "1"}, {"id" : "2"}],
        "settings": {"timeSegments": {}, "domainAffinityParameterSets": {}}
      }`;

      instance.personalized = false;
      instance.compareScore = sinon.spy();
      instance.stories_endpoint = "stories-endpoint";

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });

      await instance.fetchStories();
      assert.notCalled(instance.compareScore);
    });
    it("should sort items based on relevance score", () => {
      let items = [{ score: 0.1 }, { score: 0.2 }];
      items = items.sort(instance.compareScore);
      assert.deepEqual(items, [{ score: 0.2 }, { score: 0.1 }]);
    });
    it("should rotate items if personalization is preffed on", () => {
      let items = [
        { guid: "g1" },
        { guid: "g2" },
        { guid: "g3" },
        { guid: "g4" },
        { guid: "g5" },
        { guid: "g6" },
      ];
      instance.personalized = true;

      // No impressions should leave items unchanged
      let rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);

      // Recent impression should leave items unchanged
      instance._prefs.get = pref =>
        pref === REC_IMPRESSION_TRACKING_PREF &&
        JSON.stringify({ g1: 1, g2: 1, g3: 1 });
      rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);

      // Impression older than expiration time should rotate items
      clock.tick(DEFAULT_RECS_EXPIRE_TIME + 1);
      rotated = instance.rotate(items);
      assert.deepEqual(
        [
          { guid: "g4" },
          { guid: "g5" },
          { guid: "g6" },
          { guid: "g1" },
          { guid: "g2" },
          { guid: "g3" },
        ],
        rotated
      );

      instance._prefs.get = pref =>
        pref === REC_IMPRESSION_TRACKING_PREF &&
        JSON.stringify({
          g1: 1,
          g2: 1,
          g3: 1,
          g4: DEFAULT_RECS_EXPIRE_TIME + 1,
        });
      clock.tick(DEFAULT_RECS_EXPIRE_TIME);
      rotated = instance.rotate(items);
      assert.deepEqual(
        [
          { guid: "g5" },
          { guid: "g6" },
          { guid: "g1" },
          { guid: "g2" },
          { guid: "g3" },
          { guid: "g4" },
        ],
        rotated
      );
    });
    it("should not rotate items if personalization is preffed off", () => {
      let items = [
        { guid: "g1" },
        { guid: "g2" },
        { guid: "g3" },
        { guid: "g4" },
      ];

      instance.personalized = false;

      instance._prefs.get = pref =>
        pref === REC_IMPRESSION_TRACKING_PREF &&
        JSON.stringify({ g1: 1, g2: 1, g3: 1 });
      clock.tick(DEFAULT_RECS_EXPIRE_TIME + 1);
      let rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);
    });
    it("should not dispatch ITEM_RELEVANCE_SCORE_DURATION metrics for not personalized", () => {
      instance.personalized = false;
      instance.dispatchRelevanceScore(50);
      assert.notCalled(instance.store.dispatch);
    });
    it("should not dispatch v2 ITEM_RELEVANCE_SCORE_DURATION until initialized", () => {
      instance.personalized = true;
      instance.affinityProviderV2 = { use_v2: true };
      instance.affinityProvider = {};
      instance.dispatchRelevanceScore(50);
      assert.notCalled(instance.store.dispatch);
      instance.affinityProvider = { initialized: true };
      instance.dispatchRelevanceScore(50);
      assert.calledOnce(instance.store.dispatch);
      const [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, "TELEMETRY_PERFORMANCE_EVENT");
      assert.equal(
        action.data.event,
        "PERSONALIZATION_V2_ITEM_RELEVANCE_SCORE_DURATION"
      );
    });
    it("should dispatch v1 ITEM_RELEVANCE_SCORE_DURATION", () => {
      instance.personalized = true;
      instance.dispatchRelevanceScore(50);
      assert.calledOnce(instance.store.dispatch);
      const [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, "TELEMETRY_PERFORMANCE_EVENT");
      assert.equal(
        action.data.event,
        "PERSONALIZATION_V1_ITEM_RELEVANCE_SCORE_DURATION"
      );
    });
    it("should record top story impressions", async () => {
      instance._prefs = { get: pref => undefined, set: sinon.spy() };
      instance.personalized = true;

      clock.tick(1);
      let expectedPrefValue = JSON.stringify({ 1: 1, 2: 1, 3: 1 });
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 1 }, { id: 2 }, { id: 3 }],
        },
      });
      assert.calledWith(
        instance._prefs.set.firstCall,
        REC_IMPRESSION_TRACKING_PREF,
        expectedPrefValue
      );

      // Only need to record first impression, so impression pref shouldn't change
      instance._prefs.get = pref => expectedPrefValue;
      clock.tick(1);
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 1 }, { id: 2 }, { id: 3 }],
        },
      });
      assert.calledOnce(instance._prefs.set);

      // New first impressions should be added
      clock.tick(1);
      let expectedPrefValueTwo = JSON.stringify({
        1: 1,
        2: 1,
        3: 1,
        4: 3,
        5: 3,
        6: 3,
      });
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 4 }, { id: 5 }, { id: 6 }],
        },
      });
      assert.calledWith(
        instance._prefs.set.secondCall,
        REC_IMPRESSION_TRACKING_PREF,
        expectedPrefValueTwo
      );
    });
    it("should not record top story impressions for non-view impressions", async () => {
      instance._prefs = { get: pref => undefined, set: sinon.spy() };
      instance.personalized = true;

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", click: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", block: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", pocket: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);
    });
    it("should clean up top story impressions", async () => {
      instance._prefs = {
        get: pref => JSON.stringify({ 1: 1, 2: 1, 3: 1 }),
        set: sinon.spy(),
      };

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      instance.stories_endpoint = "stories-endpoint";
      const response = { recommendations: [{ id: 3 }, { id: 4 }, { id: 5 }] };
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.fetchStories();

      // Should remove impressions for rec 1 and 2 as no longer in the feed
      assert.calledWith(
        instance._prefs.set.firstCall,
        REC_IMPRESSION_TRACKING_PREF,
        JSON.stringify({ 3: 1 })
      );
    });
    it("should re init on affinityProviderV2 pref change", async () => {
      sinon.stub(instance, "uninit");
      sinon.stub(instance, "init");
      sinon.stub(instance, "clearCache").returns(Promise.resolve());
      await instance.onAction({
        type: at.PREF_CHANGED,
        data: {
          name: "feeds.section.topstories.options",
          value: JSON.stringify({ version: 2 }),
        },
      });
      assert.calledOnce(instance.uninit);
      assert.calledOnce(instance.init);
      assert.calledOnce(instance.clearCache);
    });
    it("should use UserDomainAffinityProvider from affinityProividerSwitcher not using v2", async () => {
      instance.affinityProviderV2 = { use_v2: false };
      sinon.stub(instance, "UserDomainAffinityProvider");
      sinon.stub(instance, "PersonalityProvider");

      await instance.affinityProividerSwitcher();
      assert.notCalled(instance.PersonalityProvider);
      assert.calledOnce(instance.UserDomainAffinityProvider);
    });
    it("should not change provider with badly formed JSON", async () => {
      sinon.stub(instance, "uninit");
      sinon.stub(instance, "init");
      sinon.stub(instance, "clearCache").returns(Promise.resolve());
      await instance.onAction({
        type: at.PREF_CHANGED,
        data: {
          name: "feeds.section.topstories.options",
          value: "{version: 2}",
        },
      });
      assert.notCalled(instance.uninit);
      assert.notCalled(instance.init);
      assert.notCalled(instance.clearCache);
    });
    it("should use PersonalityProvider from affinityProividerSwitcher using v2", async () => {
      instance.affinityProviderV2 = { use_v2: true };
      sinon.stub(instance, "UserDomainAffinityProvider");
      sinon.stub(instance, "PersonalityProvider");
      instance.PersonalityProvider = () => ({ init: sinon.stub() });

      const provider = instance.affinityProividerSwitcher();
      assert.calledOnce(provider.init);
      assert.notCalled(instance.UserDomainAffinityProvider);
    });
    it("should use init and callback from affinityProividerSwitcher using v2", async () => {
      const stories = { recommendations: {} };
      sinon.stub(instance, "doContentUpdate");
      sinon.stub(instance, "rotate").returns(stories);
      sinon.stub(instance, "transform");
      instance.cache.get = () => ({ stories });
      instance.cache.set = sinon.spy();
      instance.affinityProvider = { getAffinities: () => ({}) };
      await instance.onPersonalityProviderInit();

      assert.calledOnce(instance.doContentUpdate);
      assert.calledWith(
        instance.doContentUpdate,
        { stories: { recommendations: {} } },
        false
      );
      assert.calledOnce(instance.rotate);
      assert.calledOnce(instance.transform);
      const { args } = instance.cache.set.firstCall;
      assert.equal(args[0], "domainAffinities");
      assert.equal(args[1]._timestamp, 0);
    });
    it("should call dispatchUpdateEvent from affinityProividerSwitcher using v2", async () => {
      const stories = { recommendations: {} };
      sinon.stub(instance, "rotate").returns(stories);
      sinon.stub(instance, "transform");
      sinon.spy(instance, "dispatchUpdateEvent");
      instance.cache.get = () => ({ stories });
      instance.cache.set = sinon.spy();
      instance.affinityProvider = { getAffinities: () => ({}) };

      await instance.onPersonalityProviderInit();

      assert.calledOnce(instance.dispatchUpdateEvent);
    });
    it("should return an object for UserDomainAffinityProvider", () => {
      assert.equal(typeof instance.UserDomainAffinityProvider(), "object");
    });
    it("should return an object for PersonalityProvider", () => {
      assert.equal(typeof instance.PersonalityProvider(), "object");
    });
    it("should call affinityProividerSwitcher on loadCachedData", async () => {
      instance.affinityProviderV2 = true;
      instance.personalized = true;
      sinon
        .stub(instance, "affinityProividerSwitcher")
        .returns(Promise.resolve());
      const domainAffinities = {
        parameterSets: {
          default: {
            recencyFactor: 0.4,
            frequencyFactor: 0.5,
            combinedDomainFactor: 0.5,
            perfectFrequencyVisits: 10,
            perfectCombinedDomainScore: 2,
            multiDomainBoost: 0.1,
            itemScoreFactor: 0,
          },
        },
        scores: { "a.com": 1, "b.com": 0.9 },
        maxHistoryQueryResults: 1000,
        timeSegments: {},
        version: "v1",
      };

      instance.cache.get = () => ({ domainAffinities });
      await instance.loadCachedData();
      assert.calledOnce(instance.affinityProividerSwitcher);
    });
    it("should change domainAffinitiesLastUpdated on loadCachedData", async () => {
      instance.affinityProviderV2 = true;
      instance.personalized = true;
      const domainAffinities = {
        parameterSets: {
          default: {
            recencyFactor: 0.4,
            frequencyFactor: 0.5,
            combinedDomainFactor: 0.5,
            perfectFrequencyVisits: 10,
            perfectCombinedDomainScore: 2,
            multiDomainBoost: 0.1,
            itemScoreFactor: 0,
          },
        },
        scores: { "a.com": 1, "b.com": 0.9 },
        maxHistoryQueryResults: 1000,
        timeSegments: {},
        version: "v1",
      };

      instance.cache.get = () => ({ domainAffinities });
      await instance.loadCachedData();
      assert.notEqual(instance.domainAffinitiesLastUpdated, 0);
    });
    it("should return false and do nothing if v2 already set", () => {
      instance.affinityProviderV2 = { use_v2: true, model_keys: ["item1orig"] };
      const result = instance.processAffinityProividerVersion({
        version: 2,
        model_keys: ["item1"],
      });
      assert.isTrue(instance.affinityProviderV2.use_v2);
      assert.isFalse(result);
      assert.equal(instance.affinityProviderV2.model_keys[0], "item1orig");
    });
    it("should return false and do nothing if v1 already set", () => {
      instance.affinityProviderV2 = null;
      const result = instance.processAffinityProividerVersion({ version: 1 });
      assert.isFalse(result);
      assert.isNull(instance.affinityProviderV2);
    });
    it("should return true and set v2", () => {
      const result = instance.processAffinityProividerVersion({
        version: 2,
        model_keys: ["item1"],
      });
      assert.isTrue(instance.affinityProviderV2.use_v2);
      assert.isTrue(result);
      assert.equal(instance.affinityProviderV2.model_keys[0], "item1");
    });
    it("should return true and set v1", () => {
      instance.affinityProviderV2 = {};
      const result = instance.processAffinityProividerVersion({ version: 1 });
      assert.isTrue(result);
      assert.isNull(instance.affinityProviderV2);
    });
  });
  describe("#spocs", async () => {
    it("should not display expired or untimestamped spocs", async () => {
      clock.tick(441792000000); // 01/01/1984

      instance.spocsPerNewTabs = 1;
      instance.show_spocs = true;
      instance.isBelowFrequencyCap = () => true;

      // NOTE: `expiration_timestamp` is seconds since UNIX epoch
      instance.spocs = [
        // No timestamp stays visible
        {
          id: "spoc1",
        },
        // Expired spoc gets filtered out
        {
          id: "spoc2",
          expiration_timestamp: 1,
        },
        // Far future expiration spoc stays visible
        {
          id: "spoc3",
          expiration_timestamp: 32503708800, // 01/01/3000
        },
      ];

      sinon.spy(instance, "filterSpocs");

      instance.filterSpocs();

      assert.equal(instance.filterSpocs.firstCall.returnValue.length, 2);
      assert.equal(instance.filterSpocs.firstCall.returnValue[0].id, "spoc1");
      assert.equal(instance.filterSpocs.firstCall.returnValue[1].id, "spoc3");
    });
    it("should insert spoc with provided probability", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      instance.dispatchRelevanceScore = () => {};

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        recommendations: [{ guid: "rec1" }, { guid: "rec2" }, { guid: "rec3" }],
        // Include spocs with a expiration in the very distant future
        spocs: [
          { id: "spoc1", expiration_timestamp: 9999999999999 },
          { id: "spoc2", expiration_timestamp: 9999999999999 },
        ],
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      instance.storiesLoaded = true;
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.fetchStories();

      instance.store.getState = () => ({
        Sections: [{ id: "topstories", rows: response.recommendations }],
        Prefs: { values: { showSponsored: true } },
      });

      globals.set("Math", {
        random: () => 0.4,
        min: Math.min,
      });
      instance.dispatchSpocDone = () => {};
      instance.getPocketState = () => {};

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.store.dispatch);
      let [action] = instance.store.dispatch.firstCall.args;

      assert.equal(at.SECTION_UPDATE, action.type);
      assert.equal(true, action.meta.skipMain);
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      // Make sure spoc is marked as pinned so it doesn't get removed when preloaded tabs refresh
      assert.equal(action.data.rows[2].pinned, true);

      // Second new tab shouldn't trigger a section update event (spocsPerNewTab === 0.5)
      globals.set("Math", {
        random: () => 0.6,
        min: Math.min,
      });
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.store.dispatch);

      globals.set("Math", {
        random: () => 0.3,
        min: Math.min,
      });
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledTwice(instance.store.dispatch);
      [action] = instance.store.dispatch.secondCall.args;
      assert.equal(at.SECTION_UPDATE, action.type);
      assert.equal(true, action.meta.skipMain);
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      // Make sure spoc is marked as pinned so it doesn't get removed when preloaded tabs refresh
      assert.equal(action.data.rows[2].pinned, true);
    });
    it("should delay inserting spoc if stories haven't been fetched", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchRelevanceScore = () => {};
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      globals.set("Math", {
        random: () => 0.4,
        min: Math.min,
        floor: Math.floor,
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        recommendations: [{ id: "rec1" }, { id: "rec2" }, { id: "rec3" }],
        // Include one spoc with a expiration in the very distant future
        spocs: [
          { id: "spoc1", expiration_timestamp: 9999999999999 },
          { id: "spoc2" },
        ],
      };

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.notCalled(instance.store.dispatch);
      assert.equal(instance.contentUpdateQueue.length, 1);

      instance.spocsPerNewTabs = 0.5;
      instance.store.getState = () => ({
        Sections: [{ id: "topstories", rows: response.recommendations }],
        Prefs: { values: { showSponsored: true } },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });

      await instance.onInit();
      assert.equal(instance.contentUpdateQueue.length, 0);
      assert.calledOnce(instance.store.dispatch);
      let [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, at.SECTION_UPDATE);
    });
    it("should not insert spoc if preffed off", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: false,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        spocs: [{ id: "spoc1" }, { id: "spoc2" }],
      };
      sinon.spy(instance, "maybeAddSpoc");
      sinon.spy(instance, "shouldShowSpocs");

      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.maybeAddSpoc);
      assert.calledOnce(instance.shouldShowSpocs);
      assert.notCalled(instance.store.dispatch);
    });
    it("should call dispatchSpocDone when calling maybeAddSpoc", async () => {
      instance.dispatchSpocDone = sinon.spy();
      instance.storiesLoaded = true;
      await instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.dispatchSpocDone);
      assert.calledWith(instance.dispatchSpocDone, {});
    });
    it("should fire POCKET_WAITING_FOR_SPOC action with false", () => {
      instance.dispatchSpocDone({});
      assert.calledOnce(instance.store.dispatch);
      const [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, "POCKET_WAITING_FOR_SPOC");
      assert.equal(action.data, false);
    });
    it("should not insert spoc if user opted out", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchRelevanceScore = () => {};
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        spocs: [{ id: "spoc1" }, { id: "spoc2" }],
      };

      instance.store.getState = () => ({
        Sections: [{ id: "topstories", rows: response.recommendations }],
        Prefs: { values: { showSponsored: false } },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.notCalled(instance.store.dispatch);
    });
    it("should not fail if there is no spoc", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      globals.set("Math", {
        random: () => 0.4,
        min: Math.min,
      });

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        recommendations: [{ id: "rec1" }, { id: "rec2" }, { id: "rec3" }],
      };

      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.notCalled(instance.store.dispatch);
    });
    it("should record spoc/campaign impressions for frequency capping", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      globals.set("Math", {
        random: () => 0.4,
        min: Math.min,
        floor: Math.floor,
      });

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        spocs: [{ id: 1, campaign_id: 5 }, { id: 4, campaign_id: 6 }],
      };

      instance._prefs = { get: pref => undefined, set: sinon.spy() };
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.fetchStories();

      let expectedPrefValue = JSON.stringify({ 5: [0] });
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 3 }, { id: 2 }, { id: 1 }],
        },
      });
      assert.calledWith(
        instance._prefs.set.firstCall,
        SPOC_IMPRESSION_TRACKING_PREF,
        expectedPrefValue
      );

      clock.tick(1);
      instance._prefs.get = pref => expectedPrefValue;
      let expectedPrefValueCallTwo = JSON.stringify({ 5: [0, 1] });
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 3 }, { id: 2 }, { id: 1 }],
        },
      });
      assert.calledWith(
        instance._prefs.set.secondCall,
        SPOC_IMPRESSION_TRACKING_PREF,
        expectedPrefValueCallTwo
      );

      clock.tick(1);
      instance._prefs.get = pref => expectedPrefValueCallTwo;
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 3 }, { id: 2 }, { id: 4 }],
        },
      });
      assert.calledWith(
        instance._prefs.set.thirdCall,
        SPOC_IMPRESSION_TRACKING_PREF,
        JSON.stringify({ 5: [0, 1], 6: [2] })
      );
    });
    it("should not record spoc/campaign impressions for non-view impressions", async () => {
      let fetchStub = globals.sandbox.stub();
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        spocs: [{ id: 1, campaign_id: 5 }, { id: 4, campaign_id: 6 }],
      };

      instance._prefs = { get: pref => undefined, set: sinon.spy() };
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", click: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", block: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);

      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: { source: "TOP_STORIES", pocket: 0, tiles: [{ id: 1 }] },
      });
      assert.notCalled(instance._prefs.set);
    });
    it("should clean up spoc/campaign impressions", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      instance._prefs = { get: pref => undefined, set: sinon.spy() };
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";

      const response = {
        settings: { spocsPerNewTabs: 0.5 },
        spocs: [{ id: 1, campaign_id: 5 }, { id: 4, campaign_id: 6 }],
      };
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.fetchStories();

      // simulate impressions for campaign 5 and 6
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 3 }, { id: 2 }, { id: 1 }],
        },
      });
      instance._prefs.get = pref =>
        pref === SPOC_IMPRESSION_TRACKING_PREF && JSON.stringify({ 5: [0] });
      instance.onAction({
        type: at.TELEMETRY_IMPRESSION_STATS,
        data: {
          source: "TOP_STORIES",
          tiles: [{ id: 3 }, { id: 2 }, { id: 4 }],
        },
      });

      let expectedPrefValue = JSON.stringify({ 5: [0], 6: [0] });
      assert.calledWith(
        instance._prefs.set.secondCall,
        SPOC_IMPRESSION_TRACKING_PREF,
        expectedPrefValue
      );
      instance._prefs.get = pref =>
        pref === SPOC_IMPRESSION_TRACKING_PREF && expectedPrefValue;

      // remove campaign 5 from response
      const updatedResponse = {
        settings: { spocsPerNewTabs: 1 },
        spocs: [{ id: 4, campaign_id: 6 }],
      };
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(updatedResponse),
      });
      await instance.fetchStories();

      // should remove campaign 5 from pref as no longer active
      assert.calledWith(
        instance._prefs.set.thirdCall,
        SPOC_IMPRESSION_TRACKING_PREF,
        JSON.stringify({ 6: [0] })
      );
    });
    it("should maintain frequency caps when inserting spocs", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchRelevanceScore = () => {};
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      const response = {
        settings: { spocsPerNewTabs: 1 },
        recommendations: [{ guid: "rec1" }, { guid: "rec2" }, { guid: "rec3" }],
        spocs: [
          // Set spoc `expiration_timestamp`s in the very distant future to ensure they show up
          {
            id: "spoc1",
            campaign_id: 1,
            caps: { lifetime: 3, campaign: { count: 2, period: 3600 } },
            expiration_timestamp: 999999999999,
          },
          {
            id: "spoc2",
            campaign_id: 2,
            caps: { lifetime: 1 },
            expiration_timestamp: 999999999999,
          },
        ],
      };

      instance.store.getState = () => ({
        Sections: [{ id: "topstories", rows: response.recommendations }],
        Prefs: { values: { showSponsored: true } },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();
      instance.spocsPerNewTabs = 1;

      clock.tick();
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      let [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref => JSON.stringify({ 1: [1] });

      clock.tick();
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      [action] = instance.store.dispatch.secondCall.args;
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref => JSON.stringify({ 1: [1, 2] });

      // campaign 1 period frequency cap now reached (spoc 2 should be shown)
      clock.tick();
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      [action] = instance.store.dispatch.thirdCall.args;
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc2");
      instance._prefs.get = pref => JSON.stringify({ 1: [1, 2], 2: [3] });

      // new campaign 1 period starting (spoc 1 sohuld be shown again)
      clock.tick(2 * 60 * 60 * 1000);
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      [action] = instance.store.dispatch.lastCall.args;
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref =>
        JSON.stringify({ 1: [1, 2, 7200003], 2: [3] });

      // campaign 1 lifetime cap now reached (no spoc should be sent)
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.callCount(instance.store.dispatch, 4);
    });
    it("should maintain client-side MAX_LIFETIME_CAP", async () => {
      let fetchStub = globals.sandbox.stub();
      instance.dispatchRelevanceScore = () => {};
      instance.dispatchSpocDone = () => {};
      sectionsManagerStub.sections.set("topstories", {
        options: {
          show_spocs: true,
          personalized: true,
          stories_endpoint: "stories-endpoint",
        },
      });
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};

      const response = {
        settings: { spocsPerNewTabs: 1 },
        recommendations: [{ guid: "rec1" }, { guid: "rec2" }, { guid: "rec3" }],
        spocs: [{ id: "spoc1", campaign_id: 1, caps: { lifetime: 501 } }],
      };

      instance.store.getState = () => ({
        Sections: [{ id: "topstories", rows: response.recommendations }],
        Prefs: { values: { showSponsored: true } },
      });
      fetchStub.resolves({
        ok: true,
        status: 200,
        json: () => Promise.resolve(response),
      });
      await instance.onInit();

      instance._prefs.get = pref =>
        JSON.stringify({ 1: [...Array(500).keys()] });
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.notCalled(instance.store.dispatch);
    });
  });
  describe("#update", () => {
    it("should fetch stories after update interval", async () => {
      await instance.onInit();
      sinon.spy(instance, "fetchStories");
      await instance.onAction({ type: at.SYSTEM_TICK });
      assert.notCalled(instance.fetchStories);

      clock.tick(STORIES_UPDATE_TIME);
      await instance.onAction({ type: at.SYSTEM_TICK });
      assert.calledOnce(instance.fetchStories);
    });
    it("should fetch topics after update interval", async () => {
      await instance.onInit();
      sinon.spy(instance, "fetchTopics");
      await instance.onAction({ type: at.SYSTEM_TICK });
      assert.notCalled(instance.fetchTopics);

      clock.tick(TOPICS_UPDATE_TIME);
      await instance.onAction({ type: at.SYSTEM_TICK });
      assert.calledOnce(instance.fetchTopics);
    });
    it("should return updated stories and topics on system tick", async () => {
      await instance.onInit();
      sinon.spy(instance, "dispatchUpdateEvent");
      const stories = [{ guid: "rec1" }, { guid: "rec2" }, { guid: "rec3" }];
      const topics = [
        { name: "topic1", url: "url-topic1" },
        { name: "topic2", url: "url-topic2" },
      ];
      clock.tick(TOPICS_UPDATE_TIME);
      globals.sandbox.stub(instance, "fetchStories").resolves(stories);
      globals.sandbox.stub(instance, "fetchTopics").resolves(topics);

      await instance.onAction({ type: at.SYSTEM_TICK });

      assert.calledOnce(instance.dispatchUpdateEvent);
      assert.calledWith(instance.dispatchUpdateEvent, false, {
        rows: [{ guid: "rec1" }, { guid: "rec2" }, { guid: "rec3" }],
        topics: [
          { name: "topic1", url: "url-topic1" },
          { name: "topic2", url: "url-topic2" },
        ],
        read_more_endpoint: undefined,
      });
    });
    it("should update domain affinities on idle-daily, if personalization preffed on", async () => {
      instance.init();
      instance.affinityProvider = undefined;
      instance.cache.set = sinon.spy();

      instance.observe("", "idle-daily");
      assert.isUndefined(instance.affinityProvider);

      instance.personalized = true;
      instance.updateSettings({
        timeSegments: {},
        domainAffinityParameterSets: {},
      });
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      await instance.observe("", "idle-daily");
      assert.isDefined(instance.affinityProvider);
      assert.calledOnce(instance.cache.set);
      assert.calledWith(
        instance.cache.set,
        "domainAffinities",
        Object.assign({}, instance.affinityProvider.getAffinities(), {
          _timestamp: MIN_DOMAIN_AFFINITIES_UPDATE_TIME,
        })
      );
    });
    it("should not update domain affinities too often", () => {
      instance.init();
      instance.affinityProvider = undefined;
      instance.cache.set = sinon.spy();

      instance.personalized = true;
      instance.updateSettings({
        timeSegments: {},
        domainAffinityParameterSets: {},
      });
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.domainAffinitiesLastUpdated = Date.now();
      instance.observe("", "idle-daily");
      assert.isUndefined(instance.affinityProvider);
    });
    it("should send performance telemetry when updating domain affinities", async () => {
      instance.getPocketState = () => {};
      instance.dispatchPocketCta = () => {};
      instance.init();
      instance.personalized = true;
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.updateSettings({
        timeSegments: {},
        domainAffinityParameterSets: {},
      });
      await instance.observe("", "idle-daily");

      assert.calledOnce(instance.store.dispatch);
      let [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(
        action.data.event,
        "topstories.domain.affinity.calculation.ms"
      );
    });
    it("should add idle-daily observer right away, before waiting on init data", async () => {
      const addObserver = globals.sandbox.stub();
      globals.set("Services", { obs: { addObserver } });
      const initPromise = instance.onInit();
      assert.calledOnce(addObserver);
      await initPromise;
    });
    it("should not call init and uninit if data doesn't match on options change ", () => {
      sinon.spy(instance, "init");
      sinon.spy(instance, "uninit");
      instance.onAction({ type: at.SECTION_OPTIONS_CHANGED, data: "foo" });
      assert.notCalled(sectionsManagerStub.disableSection);
      assert.notCalled(sectionsManagerStub.enableSection);
      assert.notCalled(instance.init);
      assert.notCalled(instance.uninit);
    });
    it("should call init and uninit on options change", async () => {
      sinon.stub(instance, "clearCache").returns(Promise.resolve());
      sinon.spy(instance, "init");
      sinon.spy(instance, "uninit");
      await instance.onAction({
        type: at.SECTION_OPTIONS_CHANGED,
        data: "topstories",
      });
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.calledOnce(instance.clearCache);
      assert.calledOnce(instance.init);
      assert.calledOnce(instance.uninit);
    });
    it("should set LastUpdated to 0 on init", async () => {
      instance.storiesLastUpdated = 1;
      instance.topicsLastUpdated = 1;

      await instance.onInit();
      assert.equal(instance.storiesLastUpdated, 0);
      assert.equal(instance.topicsLastUpdated, 0);
    });
    it("should filter spocs when link is blocked", async () => {
      instance.spocs = [{ url: "not_blocked" }, { url: "blocked" }];
      await instance.onAction({
        type: at.PLACES_LINK_BLOCKED,
        data: { url: "blocked" },
      });

      assert.deepEqual(instance.spocs, [{ url: "not_blocked" }]);
    });
    it("should reset domain affinity scores if version changed", async () => {
      instance.init();
      instance.personalized = true;
      instance.resetDomainAffinityScores = sinon.spy();
      instance.updateSettings({
        timeSegments: {},
        domainAffinityParameterSets: {},
        version: "1",
      });
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      await instance.observe("", "idle-daily");
      assert.notCalled(instance.resetDomainAffinityScores);

      instance.updateSettings({
        timeSegments: {},
        domainAffinityParameterSets: {},
        version: "2",
      });
      assert.calledOnce(instance.resetDomainAffinityScores);
    });
  });
  describe("#loadCachedData", () => {
    it("should update section with cached stories and topics if available", async () => {
      sectionsManagerStub.sections.set("topstories", {
        options: { stories_referrer: "referrer" },
      });
      const stories = {
        _timestamp: 123,
        recommendations: [
          {
            id: "1",
            title: "title",
            excerpt: "description",
            image_src: "image-url",
            url: "rec-url",
            published_timestamp: "123",
            context: "trending",
            icon: "icon",
            item_score: 0.98,
          },
        ],
      };
      const transformedStories = [
        {
          guid: "1",
          type: "now",
          title: "title",
          context: "trending",
          icon: "icon",
          description: "description",
          image: "image-url",
          referrer: "referrer",
          url: "rec-url",
          hostname: "rec-url",
          min_score: 0,
          score: 0.98,
          spoc_meta: {},
        },
      ];
      const topics = {
        _timestamp: 123,
        topics: [
          { name: "topic1", url: "url-topic1" },
          { name: "topic2", url: "url-topic2" },
        ],
      };
      instance.cache.get = () => ({ stories, topics });
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });

      await instance.onInit();
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {
        rows: transformedStories,
        topics: topics.topics,
        read_more_endpoint: undefined,
      });
    });
    it("should NOT update section if there is no cached data", async () => {
      instance.cache.get = () => ({});
      globals.set("NewTabUtils", {
        blockedLinks: { isBlocked: globals.sandbox.spy() },
      });
      await instance.loadCachedData();
      assert.notCalled(sectionsManagerStub.updateSection);
    });
    it("should use store rows if no stories sent to doContentUpdate", async () => {
      instance.store = {
        getState() {
          return {
            Sections: [{ id: "topstories", rows: [1, 2, 3] }],
          };
        },
      };
      sinon.spy(instance, "dispatchUpdateEvent");

      instance.doContentUpdate({}, false);

      assert.calledOnce(instance.dispatchUpdateEvent);
      assert.calledWith(instance.dispatchUpdateEvent, false, {
        rows: [1, 2, 3],
      });
    });
    it("should broadcast in doContentUpdate when updating from cache", async () => {
      sectionsManagerStub.sections.set("topstories", {
        options: { stories_referrer: "referrer" },
      });
      globals.set("NewTabUtils", { blockedLinks: { isBlocked: () => {} } });
      const stories = { recommendations: [{}] };
      const topics = { topics: [{}] };
      sinon.spy(instance, "doContentUpdate");
      instance.cache.get = () => ({ stories, topics });
      await instance.onInit();
      assert.calledOnce(instance.doContentUpdate);
      assert.calledWith(
        instance.doContentUpdate,
        {
          stories: [
            {
              context: undefined,
              description: undefined,
              guid: undefined,
              hostname: undefined,
              icon: undefined,
              image: undefined,
              min_score: 0,
              referrer: "referrer",
              score: 1,
              spoc_meta: {},
              title: undefined,
              type: "trending",
              url: undefined,
            },
          ],
          topics: [{}],
        },
        true
      );
    });
    it("should initialize user domain affinity provider from cache if personalization is preffed on", async () => {
      const domainAffinities = {
        parameterSets: {
          default: {
            recencyFactor: 0.4,
            frequencyFactor: 0.5,
            combinedDomainFactor: 0.5,
            perfectFrequencyVisits: 10,
            perfectCombinedDomainScore: 2,
            multiDomainBoost: 0.1,
            itemScoreFactor: 0,
          },
        },
        scores: { "a.com": 1, "b.com": 0.9 },
        maxHistoryQueryResults: 1000,
        timeSegments: {},
        version: "v1",
      };

      instance.affinityProvider = undefined;
      instance.cache.get = () => ({ domainAffinities });

      await instance.loadCachedData();
      assert.isUndefined(instance.affinityProvider);
      instance.personalized = true;
      await instance.loadCachedData();
      assert.isDefined(instance.affinityProvider);
      assert.deepEqual(
        instance.affinityProvider.timeSegments,
        domainAffinities.timeSegments
      );
      assert.equal(
        instance.affinityProvider.maxHistoryQueryResults,
        domainAffinities.maxHistoryQueryResults
      );
      assert.deepEqual(
        instance.affinityProvider.parameterSets,
        domainAffinities.parameterSets
      );
      assert.deepEqual(
        instance.affinityProvider.scores,
        domainAffinities.scores
      );
      assert.deepEqual(
        instance.affinityProvider.version,
        domainAffinities.version
      );
    });
    it("should clear domain affinity cache when history is cleared", () => {
      instance.cache.set = sinon.spy();
      instance.affinityProvider = {};
      instance.personalized = true;

      instance.onAction({ type: at.PLACES_HISTORY_CLEARED });
      assert.calledWith(instance.cache.set, "domainAffinities", {});
      assert.isUndefined(instance.affinityProvider);
    });
  });
  describe("#pocket", () => {
    it("should call getPocketState when hitting NEW_TAB_REHYDRATED", () => {
      instance.getPocketState = sinon.spy();
      instance.onAction({
        type: at.NEW_TAB_REHYDRATED,
        meta: { fromTarget: {} },
      });
      assert.calledOnce(instance.getPocketState);
      assert.calledWith(instance.getPocketState, {});
    });
    it("should call dispatch in getPocketState", () => {
      const isUserLoggedIn = sinon.spy();
      globals.set("pktApi", { isUserLoggedIn });
      instance.getPocketState({});
      assert.calledOnce(instance.store.dispatch);
      const [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, "POCKET_LOGGED_IN");
      assert.calledOnce(isUserLoggedIn);
    });
    it("should call dispatchPocketCta when hitting onInit", async () => {
      instance.dispatchPocketCta = sinon.spy();
      await instance.onInit();
      assert.calledOnce(instance.dispatchPocketCta);
      assert.calledWith(
        instance.dispatchPocketCta,
        JSON.stringify({
          cta_button: "",
          cta_text: "",
          cta_url: "",
          use_cta: false,
        }),
        false
      );
    });
    it("should call dispatch in dispatchPocketCta", () => {
      instance.dispatchPocketCta(JSON.stringify({ use_cta: true }), false);
      assert.calledOnce(instance.store.dispatch);
      const [action] = instance.store.dispatch.firstCall.args;
      assert.equal(action.type, "POCKET_CTA");
      assert.equal(action.data.use_cta, true);
    });
    it("should call dispatchPocketCta with a pocketCta pref change", () => {
      instance.dispatchPocketCta = sinon.spy();
      instance.onAction({
        type: at.PREF_CHANGED,
        data: {
          name: "pocketCta",
          value: JSON.stringify({
            cta_button: "",
            cta_text: "",
            cta_url: "",
            use_cta: false,
          }),
        },
      });
      assert.calledOnce(instance.dispatchPocketCta);
      assert.calledWith(
        instance.dispatchPocketCta,
        JSON.stringify({
          cta_button: "",
          cta_text: "",
          cta_url: "",
          use_cta: false,
        }),
        true
      );
    });
  });
  it("should call uninit and init on disabling of showSponsored pref", async () => {
    sinon.stub(instance, "clearCache").returns(Promise.resolve());
    sinon.stub(instance, "uninit");
    sinon.stub(instance, "init");
    await instance.onAction({
      type: at.PREF_CHANGED,
      data: { name: "showSponsored", value: false },
    });
    assert.calledOnce(instance.clearCache);
    assert.calledOnce(instance.uninit);
    assert.calledOnce(instance.init);
  });
});

import {FakePrefs, GlobalOverrider} from "test/unit/utils";
import {actionTypes as at} from "common/Actions.jsm";
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
    "stories_endpoint": "https://somedomain.org/stories?key=$apiKey",
    "stories_referrer": "https://somedomain.org/referrer",
    "topics_endpoint": "https://somedomain.org/topics?key=$apiKey",
    "survey_link": "https://www.surveymonkey.com/r/newtabffx",
    "api_key_pref": "apiKeyPref",
    "provider_name": "test-provider",
    "provider_icon": "provider-icon",
    "provider_description": "provider_desc"
  };

  beforeEach(() => {
    FakePrefs.prototype.prefs.apiKeyPref = "test-api-key";

    globals = new GlobalOverrider();
    globals.set("PlacesUtils", {history: {}});
    clock = sinon.useFakeTimers();
    shortURLStub = sinon.stub().callsFake(site => site.url);
    sectionsManagerStub = {
      onceInitialized: sinon.stub().callsFake(callback => callback()),
      enableSection: sinon.spy(),
      disableSection: sinon.spy(),
      updateSection: sinon.spy(),
      sections: new Map([["topstories", {order: 0, options: FAKE_OPTIONS}]])
    };

    class FakeUserDomainAffinityProvider {
      constructor(timeSegments, parameterSets, maxHistoryQueryResults, version, scores) {
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

    ({
      TopStoriesFeed,
      STORIES_UPDATE_TIME,
      TOPICS_UPDATE_TIME,
      SECTION_ID,
      SPOC_IMPRESSION_TRACKING_PREF,
      REC_IMPRESSION_TRACKING_PREF,
      MIN_DOMAIN_AFFINITIES_UPDATE_TIME,
      DEFAULT_RECS_EXPIRE_TIME
    } = injector({
      "lib/ActivityStreamPrefs.jsm": {Prefs: FakePrefs},
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/UserDomainAffinityProvider.jsm": {UserDomainAffinityProvider: FakeUserDomainAffinityProvider},
      "lib/SectionsManager.jsm": {SectionsManager: sectionsManagerStub}
    }));

    instance = new TopStoriesFeed();
    instance.store = {getState() { return {Prefs: {values: {showSponsored: true}}}; }, dispatch: sinon.spy()};
    instance.storiesLastUpdated = 0;
    instance.topicsLastUpdated = 0;
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });
  describe("#init", () => {
    it("should create a TopStoriesFeed", () => {
      assert.instanceOf(instance, TopStoriesFeed);
    });
    it("should bind parseOptions to SectionsManager.onceInitialized", () => {
      instance.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("should initialize endpoints based on options", () => {
      instance.onAction({type: at.INIT});
      assert.equal("https://somedomain.org/stories?key=test-api-key", instance.stories_endpoint);
      assert.equal("https://somedomain.org/referrer", instance.stories_referrer);
      assert.equal("https://somedomain.org/topics?key=test-api-key", instance.topics_endpoint);
    });
    it("should enable its section", () => {
      instance.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.calledWith(sectionsManagerStub.enableSection, SECTION_ID);
    });
    it("should fetch stories on init", () => {
      instance.fetchStories = sinon.spy();
      instance.fetchTopics = sinon.spy();
      instance.onAction({type: at.INIT});
      assert.calledOnce(instance.fetchStories);
    });
    it("should fetch topics on init", () => {
      instance.fetchStories = sinon.spy();
      instance.fetchTopics = sinon.spy();
      instance.onAction({type: at.INIT});
      assert.calledOnce(instance.fetchTopics);
    });
    it("should not fetch if endpoint not configured", () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      sectionsManagerStub.sections.set("topstories", {options: {}});
      instance.init();
      assert.notCalled(fetchStub);
    });
    it("should report error for invalid configuration", () => {
      globals.sandbox.spy(global.Components.utils, "reportError");
      sectionsManagerStub.sections.set("topstories", {options: {api_key_pref: "invalid"}});
      instance.init();

      assert.called(Components.utils.reportError);
    });
    it("should report error for missing api key", () => {
      globals.sandbox.spy(global.Components.utils, "reportError");
      sectionsManagerStub.sections.set("topstories", {
        options: {
          "stories_endpoint": "https://somedomain.org/stories?key=$apiKey",
          "topics_endpoint": "https://somedomain.org/topics?key=$apiKey"
        }
      });
      instance.init();

      assert.called(Components.utils.reportError);
    });
    it("should load data from cache on init", () => {
      instance.loadCachedData = sinon.spy();
      instance.onAction({type: at.INIT});
      assert.calledOnce(instance.loadCachedData);
    });
  });
  describe("#uninit", () => {
    it("should disable its section", () => {
      instance.onAction({type: at.UNINIT});
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledWith(sectionsManagerStub.disableSection, SECTION_ID);
    });
  });
  describe("#fetch", () => {
    it("should fetch stories, send event and cache results", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "recommendations": [{
          "id": "1",
          "title": "title",
          "excerpt": "description",
          "image_src": "image-url",
          "url": "rec-url",
          "published_timestamp": "123",
          "context": "trending",
          "icon": "icon"
        }]
      };
      const stories = [{
        "guid": "1",
        "type": "now",
        "title": "title",
        "context": "trending",
        "icon": "icon",
        "description": "description",
        "image": "image-url",
        "referrer": "referrer",
        "url": "rec-url",
        "hostname": "rec-url",
        "min_score": 0,
        "score": 1,
        "spoc_meta": {}
      }];

      instance.stories_endpoint = "stories-endpoint";
      instance.stories_referrer = "referrer";
      instance.cache.set = sinon.spy();
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      assert.calledOnce(fetchStub);
      assert.calledOnce(shortURLStub);
      assert.calledWithExactly(fetchStub, instance.stories_endpoint);
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {rows: stories});
      assert.calledOnce(instance.cache.set);
      assert.calledWith(instance.cache.set, "stories", Object.assign({}, response, {_timestamp: 0}));
    });
    it("should call SectionsManager.updateSection", () => {
      instance.dispatchUpdateEvent(123, {});
      assert.calledOnce(sectionsManagerStub.updateSection);
    });
    it("should report error for unexpected stories response", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.sandbox.spy(global.Components.utils, "reportError");

      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: false, status: 400});
      await instance.fetchStories();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.stories_endpoint);
      assert.notCalled(sectionsManagerStub.updateSection);
      assert.called(Components.utils.reportError);
    });
    it("should exclude blocked (dismissed) URLs", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: site => site.url === "blocked"}});

      const response = {"recommendations": [{"url": "blocked"}, {"url": "not_blocked"}]};
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows.length, 1);
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows[0].url, "not_blocked");
    });
    it("should mark stories as new", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});
      clock.restore();
      const response = {
        "recommendations": [
          {"published_timestamp": Date.now() / 1000},
          {"published_timestamp": "0"},
          {"published_timestamp": (Date.now() - 2 * 24 * 60 * 60 * 1000) / 1000}
        ]
      };

      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});

      await instance.fetchStories();
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows.length, 3);
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows[0].type, "now");
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows[1].type, "trending");
      assert.equal(sectionsManagerStub.updateSection.firstCall.args[1].rows[2].type, "trending");
    });
    it("should fetch topics, send event and cache results", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);

      const response = {"topics": [{"name": "topic1", "url": "url-topic1"}, {"name": "topic2", "url": "url-topic2"}]};
      const topics = [{
        "name": "topic1",
        "url": "url-topic1"
      }, {
        "name": "topic2",
        "url": "url-topic2"
      }];

      instance.topics_endpoint = "topics-endpoint";
      instance.cache.set = sinon.spy();
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchTopics();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.topics_endpoint);
      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithMatch(sectionsManagerStub.updateSection, SECTION_ID, {topics});
      assert.calledOnce(instance.cache.set);
      assert.calledWith(instance.cache.set, "topics", Object.assign({}, response, {_timestamp: 0}));
    });
    it("should report error for unexpected topics response", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.sandbox.spy(global.Components.utils, "reportError");

      instance.topics_endpoint = "topics-endpoint";
      fetchStub.resolves({ok: false, status: 400});
      await instance.fetchTopics();

      assert.calledOnce(fetchStub);
      assert.calledWithExactly(fetchStub, instance.topics_endpoint);
      assert.notCalled(instance.store.dispatch);
      assert.called(Components.utils.reportError);
    });
  });
  describe("#personalization", () => {
    it("should sort stories if personalization is preffed on", async () => {
      const response = {
        "recommendations":  [{"id": "1"}, {"id": "2"}],
        "settings": {"timeSegments": {}, "domainAffinityParameterSets": {}}
      };

      instance.personalized = true;
      instance.compareScore = sinon.spy();
      instance.stories_endpoint = "stories-endpoint";

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});

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
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});

      await instance.fetchStories();
      assert.notCalled(instance.compareScore);
    });
    it("should sort items based on relevance score", () => {
      let items = [{"score": 0.1}, {"score": 0.2}];
      items = items.sort(instance.compareScore);
      assert.deepEqual(items, [{"score": 0.2}, {"score": 0.1}]);
    });
    it("should rotate items if personalization is preffed on", () => {
      let items = [{"guid": "g1"}, {"guid": "g2"}, {"guid": "g3"}, {"guid": "g4"}, {"guid": "g5"}, {"guid": "g6"}];
      instance.personalized = true;

      // No impressions should leave items unchanged
      let rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);

      // Recent impression should leave items unchanged
      instance._prefs.get = pref => (pref === REC_IMPRESSION_TRACKING_PREF) && JSON.stringify({"g1": 1, "g2": 1, "g3": 1});
      rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);

      // Impression older than expiration time should rotate items
      clock.tick(DEFAULT_RECS_EXPIRE_TIME + 1);
      rotated = instance.rotate(items);
      assert.deepEqual([{"guid": "g4"}, {"guid": "g5"}, {"guid": "g6"}, {"guid": "g1"}, {"guid": "g2"}, {"guid": "g3"}], rotated);

      instance._prefs.get = pref => (pref === REC_IMPRESSION_TRACKING_PREF) &&
        JSON.stringify({"g1": 1, "g2": 1, "g3": 1, "g4": DEFAULT_RECS_EXPIRE_TIME + 1});
      clock.tick(DEFAULT_RECS_EXPIRE_TIME);
      rotated = instance.rotate(items);
      assert.deepEqual([{"guid": "g5"}, {"guid": "g6"}, {"guid": "g1"}, {"guid": "g2"}, {"guid": "g3"}, {"guid": "g4"}], rotated);
    });
    it("should not rotate items if personalization is preffed off", () => {
      let items = [{"guid": "g1"}, {"guid": "g2"}, {"guid": "g3"}, {"guid": "g4"}];

      instance.personalized = false;

      instance._prefs.get = pref => (pref === REC_IMPRESSION_TRACKING_PREF) && JSON.stringify({"g1": 1, "g2": 1, "g3": 1});
      clock.tick(DEFAULT_RECS_EXPIRE_TIME + 1);
      let rotated = instance.rotate(items);
      assert.deepEqual(items, rotated);
    });
    it("should record top story impressions", async () => {
      instance._prefs = {get: pref => undefined, set: sinon.spy()};
      instance.personalized = true;

      clock.tick(1);
      let expectedPrefValue = JSON.stringify({1: 1, 2: 1, 3: 1});
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 1}, {id: 2}, {id: 3}]}});
      assert.calledWith(instance._prefs.set.firstCall, REC_IMPRESSION_TRACKING_PREF, expectedPrefValue);

      // Only need to record first impression, so impression pref shouldn't change
      instance._prefs.get = pref => expectedPrefValue;
      clock.tick(1);
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 1}, {id: 2}, {id: 3}]}});
      assert.calledOnce(instance._prefs.set);

      // New first impressions should be added
      clock.tick(1);
      let expectedPrefValueTwo = JSON.stringify({1: 1, 2: 1, 3: 1, 4: 3, 5: 3, 6: 3});
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 4}, {id: 5}, {id: 6}]}});
      assert.calledWith(instance._prefs.set.secondCall, REC_IMPRESSION_TRACKING_PREF, expectedPrefValueTwo);
    });
    it("should not record top story impressions for non-view impressions", async () => {
      instance._prefs = {get: pref => undefined, set: sinon.spy()};
      instance.personalized = true;

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {click: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {block: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {pocket: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);
    });
    it("should clean up top story impressions", async () => {
      instance._prefs = {get: pref => JSON.stringify({1: 1, 2: 1, 3: 1}), set: sinon.spy()};

      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      instance.stories_endpoint = "stories-endpoint";
      const response = {"recommendations": [{"id": 3}, {"id": 4}, {"id": 5}]};
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      // Should remove impressions for rec 1 and 2 as no longer in the feed
      assert.calledWith(instance._prefs.set.firstCall, REC_IMPRESSION_TRACKING_PREF, JSON.stringify({3: 1}));
    });
  });
  describe("#spocs", () => {
    it("should insert spoc at provided interval", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "recommendations": [{"guid": "rec1"}, {"guid": "rec2"}, {"guid": "rec3"}],
        "spocs": [{"id": "spoc1"}, {"id": "spoc2"}]
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance.store.getState = () => ({Sections: [{rows: response.recommendations}], Prefs: {values: {showSponsored: true}}});

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.calledOnce(instance.store.dispatch);
      let action = instance.store.dispatch.firstCall.args[0];

      assert.equal(at.SECTION_UPDATE, action.type);
      assert.equal(true, action.meta.skipMain);
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");

      // Second new tab shouldn't trigger a section update event (spocsPerNewTab === 2)
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.calledOnce(instance.store.dispatch);

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.calledTwice(instance.store.dispatch);
      action = instance.store.dispatch.secondCall.args[0];
      assert.equal(at.SECTION_UPDATE, action.type);
      assert.equal(true, action.meta.skipMain);
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
    });
    it("should delay inserting spoc if stories haven't been fetched", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "recommendations": [{"id": "rec1"}, {"id": "rec2"}, {"id": "rec3"}],
        "spocs": [{"id": "spoc1"}, {"id": "spoc2"}]
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      instance.store.getState = () => ({Sections: [{rows: response.recommendations}], Prefs: {values: {showSponsored: true}}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.notCalled(instance.store.dispatch);
      assert.equal(instance.contentUpdateQueue.length, 1);

      instance.store.getState = () => ({Sections: [{rows: response.recommendations}]});

      await instance.fetchStories();
      assert.equal(instance.contentUpdateQueue.length, 0);
      assert.calledOnce(instance.store.dispatch);
      let action = instance.store.dispatch.firstCall.args[0];
      assert.equal(action.type, at.SECTION_UPDATE);
    });
    it("should not insert spoc if preffed off", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": "spoc1"}, {"id": "spoc2"}]
      };

      instance.personalized = true;
      instance.show_spocs = false;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.notCalled(instance.store.dispatch);
    });
    it("should not insert spoc if user opted out", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": "spoc1"}, {"id": "spoc2"}]
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      instance.store.getState = () => ({Sections: [{rows: response.recommendations}], Prefs: {values: {showSponsored: false}}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.notCalled(instance.store.dispatch);
    });
    it("should not fail if there is no spoc", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "recommendations": [{"id": "rec1"}, {"id": "rec2"}, {"id": "rec3"}]
      };

      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.notCalled(instance.store.dispatch);
    });
    it("should record spoc/campaign impressions for frequency capping", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": 1, "campaign_id": 5}, {"id": 4, "campaign_id": 6}]
      };

      instance._prefs = {get: pref => undefined, set: sinon.spy()};
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      let expectedPrefValue = JSON.stringify({5: [0]});
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 3}, {id: 2}, {id: 1}]}});
      assert.calledWith(instance._prefs.set.firstCall, SPOC_IMPRESSION_TRACKING_PREF, expectedPrefValue);

      clock.tick(1);
      instance._prefs.get = pref => expectedPrefValue;
      let expectedPrefValueCallTwo = JSON.stringify({5: [0, 1]});
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 3}, {id: 2}, {id: 1}]}});
      assert.calledWith(instance._prefs.set.secondCall, SPOC_IMPRESSION_TRACKING_PREF, expectedPrefValueCallTwo);

      clock.tick(1);
      instance._prefs.get = pref => expectedPrefValueCallTwo;
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 3}, {id: 2}, {id: 4}]}});
      assert.calledWith(instance._prefs.set.thirdCall, SPOC_IMPRESSION_TRACKING_PREF, JSON.stringify({5: [0, 1], 6: [2]}));
    });
    it("should not record spoc/campaign impressions for non-view impressions", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": 1, "campaign_id": 5}, {"id": 4, "campaign_id": 6}]
      };

      instance._prefs = {get: pref => undefined, set: sinon.spy()};
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {click: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {block: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);

      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {pocket: 0, tiles: [{id: 1}]}});
      assert.notCalled(instance._prefs.set);
    });
    it("should clean up spoc/campaign impressions", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      instance._prefs = {get: pref => undefined, set: sinon.spy()};
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";

      const response = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": 1, "campaign_id": 5}, {"id": 4, "campaign_id": 6}]
      };
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      // simulate impressions for campaign 5 and 6
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 3}, {id: 2}, {id: 1}]}});
      instance._prefs.get = pref => (pref === SPOC_IMPRESSION_TRACKING_PREF) && JSON.stringify({5: [0]});
      instance.onAction({type: at.TELEMETRY_IMPRESSION_STATS, data: {tiles: [{id: 3}, {id: 2}, {id: 4}]}});

      let expectedPrefValue = JSON.stringify({5: [0], 6: [0]});
      assert.calledWith(instance._prefs.set.secondCall, SPOC_IMPRESSION_TRACKING_PREF, expectedPrefValue);
      instance._prefs.get = pref => (pref === SPOC_IMPRESSION_TRACKING_PREF) && expectedPrefValue;

      // remove campaign 5 from response
      const updatedResponse = {
        "settings": {"spocsPerNewTabs": 2},
        "spocs": [{"id": 4, "campaign_id": 6}]
      };
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(updatedResponse)});
      await instance.fetchStories();

      // should remove campaign 5 from pref as no longer active
      assert.calledWith(instance._prefs.set.thirdCall, SPOC_IMPRESSION_TRACKING_PREF, JSON.stringify({6: [0]}));
    });
    it("should maintain frequency caps when inserting spocs", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 1},
        "recommendations": [{"guid": "rec1"}, {"guid": "rec2"}, {"guid": "rec3"}],
        "spocs": [
          {"id": "spoc1", "campaign_id": 1, "caps": {"lifetime": 3, "campaign": {"count": 2, "period": 3600}}},
          {"id": "spoc2", "campaign_id": 2, "caps": {"lifetime": 1}}
        ]
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      instance.store.getState = () => ({Sections: [{rows: response.recommendations}], Prefs: {values: {showSponsored: true}}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      clock.tick();
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      let action = instance.store.dispatch.firstCall.args[0];
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref => JSON.stringify({1: [1]});

      clock.tick();
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      action = instance.store.dispatch.secondCall.args[0];
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref => JSON.stringify({1: [1, 2]});

      // campaign 1 period frequency cap now reached (spoc 2 should be shown)
      clock.tick();
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      action = instance.store.dispatch.thirdCall.args[0];
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc2");
      instance._prefs.get = pref => JSON.stringify({1: [1, 2], 2: [3]});

      // new campaign 1 period starting (spoc 1 sohuld be shown again)
      clock.tick(2 * 60 * 60 * 1000);
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      action = instance.store.dispatch.lastCall.args[0];
      assert.equal(action.data.rows[0].guid, "rec1");
      assert.equal(action.data.rows[1].guid, "rec2");
      assert.equal(action.data.rows[2].guid, "spoc1");
      instance._prefs.get = pref => JSON.stringify({1: [1, 2, 7200003], 2: [3]});

      // campaign 1 lifetime cap now reached (no spoc should be sent)
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.callCount(instance.store.dispatch, 4);
    });
    it("should maintain client-side MAX_LIFETIME_CAP", async () => {
      let fetchStub = globals.sandbox.stub();
      globals.set("fetch", fetchStub);
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      const response = {
        "settings": {"spocsPerNewTabs": 1},
        "recommendations": [{"guid": "rec1"}, {"guid": "rec2"}, {"guid": "rec3"}],
        "spocs": [
          {"id": "spoc1", "campaign_id": 1, "caps": {"lifetime": 101}}
        ]
      };

      instance.personalized = true;
      instance.show_spocs = true;
      instance.stories_endpoint = "stories-endpoint";
      instance.store.getState = () => ({Sections: [{rows: response.recommendations}], Prefs: {values: {showSponsored: true}}});
      fetchStub.resolves({ok: true, status: 200, json: () => Promise.resolve(response)});
      await instance.fetchStories();

      instance._prefs.get = pref => JSON.stringify({1: [...Array(100).keys()]});
      instance.onAction({type: at.NEW_TAB_REHYDRATED, meta: {fromTarget: {}}});
      assert.notCalled(instance.store.dispatch);
    });
  });
  describe("#update", () => {
    it("should fetch stories after update interval", () => {
      instance.init();
      instance.fetchStories = sinon.spy();
      instance.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(instance.fetchStories);

      clock.tick(STORIES_UPDATE_TIME);
      instance.onAction({type: at.SYSTEM_TICK});
      assert.calledOnce(instance.fetchStories);
    });
    it("should fetch topics after update interval", () => {
      instance.init();
      instance.fetchTopics = sinon.spy();
      instance.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(instance.fetchTopics);

      clock.tick(TOPICS_UPDATE_TIME);
      instance.onAction({type: at.SYSTEM_TICK});
      assert.calledOnce(instance.fetchTopics);
    });
    it("should update domain affinities on idle-daily, if personalization preffed on", () => {
      instance.init();
      instance.affinityProvider = undefined;
      instance.cache.set = sinon.spy();

      instance.observe("", "idle-daily");
      assert.isUndefined(instance.affinityProvider);

      instance.personalized = true;
      instance.updateSettings({timeSegments: {}, domainAffinityParameterSets: {}});
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.observe("", "idle-daily");
      assert.isDefined(instance.affinityProvider);
      assert.calledOnce(instance.cache.set);
      assert.calledWith(instance.cache.set, "domainAffinities",
        Object.assign({}, instance.affinityProvider.getAffinities(), {"_timestamp": MIN_DOMAIN_AFFINITIES_UPDATE_TIME}));
    });
    it("should not update domain affinities too often", () => {
      instance.init();
      instance.affinityProvider = undefined;
      instance.cache.set = sinon.spy();

      instance.personalized = true;
      instance.updateSettings({timeSegments: {}, domainAffinityParameterSets: {}});
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.domainAffinitiesLastUpdated = Date.now();
      instance.observe("", "idle-daily");
      assert.isUndefined(instance.affinityProvider);
    });
    it("should send performance telemetry when updating domain affinities", () => {
      instance.init();
      instance.personalized = true;
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.updateSettings({timeSegments: {}, domainAffinityParameterSets: {}});
      instance.observe("", "idle-daily");

      assert.calledOnce(instance.store.dispatch);
      let action = instance.store.dispatch.firstCall.args[0];
      assert.equal(action.type, at.TELEMETRY_PERFORMANCE_EVENT);
      assert.equal(action.data.event, "topstories.domain.affinity.calculation.ms");
    });
    it("should re-init on options change", () => {
      instance.storiesLastUpdated = 1;
      instance.topicsLastUpdated = 1;

      instance.onAction({type: at.SECTION_OPTIONS_CHANGED, data: "foo"});
      assert.notCalled(sectionsManagerStub.disableSection);
      assert.notCalled(sectionsManagerStub.enableSection);
      assert.equal(instance.storiesLastUpdated, 1);
      assert.equal(instance.topicsLastUpdated, 1);

      instance.onAction({type: at.SECTION_OPTIONS_CHANGED, data: "topstories"});
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.equal(instance.storiesLastUpdated, 0);
      assert.equal(instance.topicsLastUpdated, 0);
    });
    it("should filter spocs when link is blocked", () => {
      instance.spocs = [{"url": "not_blocked"}, {"url": "blocked"}];
      instance.onAction({type: at.PLACES_LINK_BLOCKED, data: {url: "blocked"}});

      assert.deepEqual(instance.spocs, [{"url": "not_blocked"}]);
    });
    it("should reset domain affinity scores if version changed", () => {
      instance.init();
      instance.personalized = true;
      instance.resetDomainAffinityScores = sinon.spy();
      instance.updateSettings({timeSegments: {}, domainAffinityParameterSets: {}, version: "1"});
      clock.tick(MIN_DOMAIN_AFFINITIES_UPDATE_TIME);
      instance.observe("", "idle-daily");
      assert.notCalled(instance.resetDomainAffinityScores);

      instance.updateSettings({timeSegments: {}, domainAffinityParameterSets: {}, version: "2"});
      assert.calledOnce(instance.resetDomainAffinityScores);
    });
  });
  describe("#loadCachedData", () => {
    it("should update section with cached stories and topics if available", async () => {
      const stories = {
        "_timestamp": 123,
        "recommendations": [{
          "id": "1",
          "title": "title",
          "excerpt": "description",
          "image_src": "image-url",
          "url": "rec-url",
          "published_timestamp": "123",
          "context": "trending",
          "icon": "icon",
          "item_score": 0.98
        }]
      };
      const transformedStories = [{
        "guid": "1",
        "type": "now",
        "title": "title",
        "context": "trending",
        "icon": "icon",
        "description": "description",
        "image": "image-url",
        "referrer": "referrer",
        "url": "rec-url",
        "hostname": "rec-url",
        "min_score": 0,
        "score": 0.98,
        "spoc_meta": {}
      }];
      const topics = {
        "_timestamp": 123,
        "topics": [{"name": "topic1", "url": "url-topic1"}, {"name": "topic2", "url": "url-topic2"}]
      };
      instance.cache.get = () => ({stories, topics});
      instance.stories_referrer = "referrer";
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});

      await instance.loadCachedData();
      assert.calledTwice(sectionsManagerStub.updateSection);
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {topics: topics.topics, read_more_endpoint: undefined});
      assert.calledWith(sectionsManagerStub.updateSection, SECTION_ID, {rows: transformedStories});
    });
    it("should NOT update section if there is no cached data", async () => {
      instance.cache.get = () => ({});
      globals.set("NewTabUtils", {blockedLinks: {isBlocked: globals.sandbox.spy()}});
      await instance.loadCachedData();
      assert.notCalled(sectionsManagerStub.updateSection);
    });
    it("should initialize user domain affinity provider from cache if personalization is preffed on", async () => {
      const domainAffinities = {
        "parameterSets": {
          "default": {
            "recencyFactor": 0.4,
            "frequencyFactor": 0.5,
            "combinedDomainFactor": 0.5,
            "perfectFrequencyVisits": 10,
            "perfectCombinedDomainScore": 2,
            "multiDomainBoost": 0.1,
            "itemScoreFactor": 0
          }
        },
        "scores": {"a.com": 1, "b.com": 0.9},
        "maxHistoryQueryResults": 1000,
        "timeSegments": {},
        "version": "v1"
      };

      instance.affinityProvider = undefined;
      instance.cache.get = () => ({domainAffinities});

      await instance.loadCachedData();
      assert.isUndefined(this.affinityProvider);

      instance.personalized = true;
      await instance.loadCachedData();
      assert.isDefined(instance.affinityProvider);
      assert.deepEqual(instance.affinityProvider.timeSegments, domainAffinities.timeSegments);
      assert.equal(instance.affinityProvider.maxHistoryQueryResults, domainAffinities.maxHistoryQueryResults);
      assert.deepEqual(instance.affinityProvider.parameterSets, domainAffinities.parameterSets);
      assert.deepEqual(instance.affinityProvider.scores, domainAffinities.scores);
      assert.deepEqual(instance.affinityProvider.version, domainAffinities.version);
    });
    it("should clear domain affinity cache when history is cleared", () => {
      instance.cache.set = sinon.spy();
      instance.affinityProvider = {};
      instance.personalized = true;

      instance.onAction({type: at.PLACES_HISTORY_CLEARED});
      assert.calledWith(instance.cache.set, "domainAffinities", {});
      assert.isUndefined(instance.affinityProvider);
    });
  });
});

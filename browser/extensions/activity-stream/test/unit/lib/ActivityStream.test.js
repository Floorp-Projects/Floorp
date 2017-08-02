const injector = require("inject!lib/ActivityStream.jsm");
const {CONTENT_MESSAGE_TYPE} = require("common/Actions.jsm");

const REASON_ADDON_UNINSTALL = 6;

describe("ActivityStream", () => {
  let sandbox;
  let as;
  let ActivityStream;
  let PREFS_CONFIG;
  function Fake() {}

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    ({ActivityStream, PREFS_CONFIG} = injector({
      "lib/LocalizationFeed.jsm": {LocalizationFeed: Fake},
      "lib/ManualMigration.jsm": {ManualMigration: Fake},
      "lib/NewTabInit.jsm": {NewTabInit: Fake},
      "lib/PlacesFeed.jsm": {PlacesFeed: Fake},
      "lib/PrefsFeed.jsm": {PrefsFeed: Fake},
      "lib/SnippetsFeed.jsm": {SnippetsFeed: Fake},
      "lib/SystemTickFeed.jsm": {SystemTickFeed: Fake},
      "lib/TelemetryFeed.jsm": {TelemetryFeed: Fake},
      "lib/TopSitesFeed.jsm": {TopSitesFeed: Fake},
      "lib/TopStoriesFeed.jsm": {TopStoriesFeed: Fake}
    }));
    as = new ActivityStream();
    sandbox.stub(as.store, "init");
    sandbox.stub(as.store, "uninit");
    sandbox.stub(as._defaultPrefs, "init");
    sandbox.stub(as._defaultPrefs, "reset");
  });

  afterEach(() => sandbox.restore());

  it("should exist", () => {
    assert.ok(ActivityStream);
  });
  it("should initialize with .initialized=false", () => {
    assert.isFalse(as.initialized, ".initialized");
  });
  describe("#init", () => {
    beforeEach(() => {
      as.init();
    });
    it("should initialize default prefs", () => {
      assert.calledOnce(as._defaultPrefs.init);
    });
    it("should set .initialized to true", () => {
      assert.isTrue(as.initialized, ".initialized");
    });
    it("should call .store.init", () => {
      assert.calledOnce(as.store.init);
    });
    it("should emit an INIT event with the right version", () => {
      as = new ActivityStream({version: "1.2.3"});
      sandbox.stub(as.store, "init");
      sandbox.stub(as.store, "dispatch");
      sandbox.stub(as._defaultPrefs, "init");

      as.init();

      assert.calledOnce(as.store.dispatch);
      const action = as.store.dispatch.firstCall.args[0];
      assert.propertyVal(action.data, "version", "1.2.3");
    });
    it("should emit an INIT event to content", () => {
      sandbox.stub(as.store, "dispatch");

      as.init();

      const action = as.store.dispatch.firstCall.args[0];
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
  });
  describe("#uninit", () => {
    beforeEach(() => {
      as.init();
      as.uninit();
    });
    it("should set .initialized to false", () => {
      assert.isFalse(as.initialized, ".initialized");
    });
    it("should call .store.uninit", () => {
      assert.calledOnce(as.store.uninit);
    });
  });
  describe("#uninstall", () => {
    it("should reset default prefs if the reason is REASON_ADDON_UNINSTALL", () => {
      as.uninstall(REASON_ADDON_UNINSTALL);
      assert.calledOnce(as._defaultPrefs.reset);
    });
    it("should not reset default prefs if the reason is something else", () => {
      as.uninstall("foo");
      assert.notCalled(as._defaultPrefs.reset);
    });
  });
  describe("feeds", () => {
    it("should create a Localization feed", () => {
      const feed = as.feeds.get("feeds.localization")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a NewTabInit feed", () => {
      const feed = as.feeds.get("feeds.newtabinit")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Places feed", () => {
      const feed = as.feeds.get("feeds.places")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a TopSites feed", () => {
      const feed = as.feeds.get("feeds.topsites")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Telemetry feed", () => {
      const feed = as.feeds.get("feeds.telemetry")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Prefs feed", () => {
      const feed = as.feeds.get("feeds.prefs")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a section feed for each section in PREFS_CONFIG", () => {
      // If new sections are added, their feeds will have to be added to the
      // list of injected feeds above for this test to pass
      let feedCount = 0;
      for (const pref of PREFS_CONFIG.keys()) {
        if (pref.search(/^feeds\.section\.[^.]+$/) === 0) {
          const feed = as.feeds.get(pref)();
          assert.instanceOf(feed, Fake);
          feedCount++;
        }
      }
      assert.isAbove(feedCount, 0);
    });
    it("should create a ManualMigration feed", () => {
      const feed = as.feeds.get("feeds.migration")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Snippets feed", () => {
      const feed = as.feeds.get("feeds.snippets")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a SystemTick feed", () => {
      const feed = as.feeds.get("feeds.systemtick")();
      assert.instanceOf(feed, Fake);
    });
  });
  describe("_updateDynamicPrefs topstories default value", () => {
    it("should be false with no geo/locale", () => {
      as._updateDynamicPrefs();

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should be false with unexpected geo", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getStringPref").returns("NOGEO");

      as._updateDynamicPrefs();

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should be false with expected geo and unexpected locale", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getStringPref").returns("US");
      sandbox.stub(global.Services.locale, "getRequestedLocale").returns("no-LOCALE");

      as._updateDynamicPrefs();

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should be true with expected geo and locale", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getStringPref").returns("US");
      sandbox.stub(global.Services.locale, "getRequestedLocale").returns("en-US");

      as._updateDynamicPrefs();

      assert.isTrue(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should be false after expected geo and locale then unexpected", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getStringPref")
        .onFirstCall()
        .returns("US")
        .onSecondCall()
        .returns("NOGEO");
      sandbox.stub(global.Services.locale, "getRequestedLocale").returns("en-US");

      as._updateDynamicPrefs();
      as._updateDynamicPrefs();

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
  });
  describe("_updateDynamicPrefs topstories delayed default value", () => {
    let clock;
    beforeEach(() => {
      clock = sinon.useFakeTimers();

      // Have addObserver cause prefHasUserValue to now return true then observe
      sandbox.stub(global.Services.prefs, "addObserver").callsFake((pref, obs) => {
        sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
        setTimeout(() => obs.observe(null, "nsPref:changed", pref)); // eslint-disable-line max-nested-callbacks
      });
    });
    afterEach(() => clock.restore());

    it("should set false with unexpected geo", () => {
      sandbox.stub(global.Services.prefs, "getStringPref").returns("NOGEO");

      as._updateDynamicPrefs();
      clock.tick(1);

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should set true with expected geo and locale", () => {
      sandbox.stub(global.Services.prefs, "getStringPref").returns("US");
      sandbox.stub(global.Services.locale, "getRequestedLocale").returns("en-US");

      as._updateDynamicPrefs();
      clock.tick(1);

      assert.isTrue(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
  });
});

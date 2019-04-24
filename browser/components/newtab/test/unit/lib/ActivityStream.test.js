import {CONTENT_MESSAGE_TYPE} from "common/Actions.jsm";
import injector from "inject!lib/ActivityStream.jsm";

describe("ActivityStream", () => {
  let sandbox;
  let as;
  let ActivityStream;
  let PREFS_CONFIG;
  function Fake() {}
  function FakeStore() {
    return {init: () => {}, uninit: () => {}, feeds: {get: () => {}}};
  }

  beforeEach(() => {
    sandbox = sinon.createSandbox();
    ({ActivityStream, PREFS_CONFIG} = injector({
      "lib/Store.jsm": {Store: FakeStore},
      "lib/AboutPreferences.jsm": {AboutPreferences: Fake},
      "lib/NewTabInit.jsm": {NewTabInit: Fake},
      "lib/PlacesFeed.jsm": {PlacesFeed: Fake},
      "lib/PrefsFeed.jsm": {PrefsFeed: Fake},
      "lib/SectionsManager.jsm": {SectionsFeed: Fake},
      "lib/SystemTickFeed.jsm": {SystemTickFeed: Fake},
      "lib/TelemetryFeed.jsm": {TelemetryFeed: Fake},
      "lib/FaviconFeed.jsm": {FaviconFeed: Fake},
      "lib/TopSitesFeed.jsm": {TopSitesFeed: Fake},
      "lib/TopStoriesFeed.jsm": {TopStoriesFeed: Fake},
      "lib/HighlightsFeed.jsm": {HighlightsFeed: Fake},
      "lib/ASRouterFeed.jsm": {ASRouterFeed: Fake},
      "lib/DiscoveryStreamFeed.jsm": {DiscoveryStreamFeed: Fake},
    }));
    as = new ActivityStream();
    sandbox.stub(as.store, "init");
    sandbox.stub(as.store, "uninit");
    sandbox.stub(as._defaultPrefs, "init");
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
    it("should pass to Store an INIT event for content", () => {
      as.init();

      const [, action] = as.store.init.firstCall.args;
      assert.equal(action.meta.to, CONTENT_MESSAGE_TYPE);
    });
    it("should pass to Store an UNINIT event", () => {
      as.init();

      const [, , action] = as.store.init.firstCall.args;
      assert.equal(action.type, "UNINIT");
    });
    it("should call _migratePrefs on init", () => {
      sandbox.stub(as, "_migratePrefs");
      as.init();
      assert.calledOnce(as._migratePrefs);
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
  describe("feeds", () => {
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
    it("should create a AboutPreferences feed", () => {
      const feed = as.feeds.get("feeds.aboutpreferences")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a SectionsFeed", () => {
      const feed = as.feeds.get("feeds.sections")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a SystemTick feed", () => {
      const feed = as.feeds.get("feeds.systemtick")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Favicon feed", () => {
      const feed = as.feeds.get("feeds.favicon")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a ASRouter feed", () => {
      const feed = as.feeds.get("feeds.asrouterfeed")();
      assert.instanceOf(feed, Fake);
    });
    it("should create a DiscoveryStreamFeed feed", () => {
      const feed = as.feeds.get("feeds.discoverystreamfeed")();
      assert.instanceOf(feed, Fake);
    });
  });
  describe("_migratePrefs and _migratePref", () => {
    it("should migrate the correct prefs", () => {
      sandbox.stub(as, "_migratePref");
      as._migratePrefs();
      // pref names we want to migrate
      assert.calledWith(as._migratePref.firstCall, "browser.newtabpage.rows");
      assert.calledWith(as._migratePref.secondCall, "browser.newtabpage.activity-stream.showTopSites");
      assert.calledWith(as._migratePref.thirdCall, "browser.newtabpage.activity-stream.topSitesCount");
    });
    it("should properly set the new value for browser.newtabpage.rows migration", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getPrefType").returns("integer");
      sandbox.stub(global.Services.prefs, "getIntPref").returns(10);
      sandbox.stub(global.Services.prefs, "setIntPref");
      as._migratePrefs();
      // Set new pref with value of old pref 'browser.newtabpage.rows'
      assert.calledWith(global.Services.prefs.setIntPref, "browser.newtabpage.activity-stream.topSitesRows", 10);

      global.Services.prefs.getIntPref.returns(0);
      sandbox.stub(global.Services.prefs, "setBoolPref");
      as._migratePrefs();
      // Turn off top sites feed if 'browser.newtabpage.rows' was <= 0
      assert.calledWith(global.Services.prefs.setBoolPref, "browser.newtabpage.activity-stream.feeds.topsites", false);
    });
    it("should properly set the new value for browser.newtabpage.activity-stream.showTopSites migration", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getPrefType").returns("boolean");
      sandbox.stub(global.Services.prefs, "getBoolPref").returns(true);
      sandbox.stub(global.Services.prefs, "setBoolPref");
      as._migratePrefs();
      // If top sites should be shown, do nothing during migration
      assert.notCalled(global.Services.prefs.setBoolPref);

      global.Services.prefs.getBoolPref.returns(false);
      as._migratePrefs();
      // If top sites has been turned off, turn the top sites feed off too
      assert.calledWith(global.Services.prefs.setBoolPref, "browser.newtabpage.activity-stream.feeds.topsites", false);
    });
    it("should properly set the new value for browser.newtabpage.activity-stream.topSitesCount migration", () => {
      const count = 40;
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getPrefType").returns("integer");
      sandbox.stub(global.Services.prefs, "getIntPref").returns(count);
      sandbox.stub(global.Services.prefs, "setIntPref");
      const expectedCount = Math.ceil(count / 6);
      as._migratePrefs();
      // If topSitesCount has been modified, calculate how many rows of top sites to show
      assert.calledWith(global.Services.prefs.setIntPref, "browser.newtabpage.activity-stream.topSitesRows", expectedCount);
    });
    it("should migrate a pref if the user has set a custom value", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getPrefType").returns("integer");
      sandbox.stub(global.Services.prefs, "getIntPref").returns(10);
      as._migratePref("oldPrefName", result => assert.equal(10, result));
    });
    it("should not migrate a pref if the user has not set a custom value", () => {
      // we bailed out early so we don't check the pref type later
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(false);
      sandbox.stub(global.Services.prefs, "getPrefType");
      as._migratePref("oldPrefName");
      assert.notCalled(global.Services.prefs.getPrefType);
    });
    it("should use the proper pref getter for each type", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);

      // Integer
      sandbox.stub(global.Services.prefs, "getIntPref");
      sandbox.stub(global.Services.prefs, "getPrefType").returns("integer");
      as._migratePref("oldPrefName", () => {});
      assert.calledWith(global.Services.prefs.getIntPref, "oldPrefName");

      // Boolean
      sandbox.stub(global.Services.prefs, "getBoolPref");
      global.Services.prefs.getPrefType.returns("boolean");
      as._migratePref("oldPrefName", () => {});
      assert.calledWith(global.Services.prefs.getBoolPref, "oldPrefName");

      // String
      sandbox.stub(global.Services.prefs, "getStringPref");
      global.Services.prefs.getPrefType.returns("string");
      as._migratePref("oldPrefName", () => {});
      assert.calledWith(global.Services.prefs.getStringPref, "oldPrefName");
    });
    it("should clear the old pref after setting the new one", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "clearUserPref");
      sandbox.stub(global.Services.prefs, "getPrefType").returns("integer");
      as._migratePref("oldPrefName", () => {});
      assert.calledWith(global.Services.prefs.clearUserPref, "oldPrefName");
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
      sandbox.stub(global.Services.locale, "appLocaleAsLangTag").get(() => "no-LOCALE");

      as._updateDynamicPrefs();

      assert.isFalse(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
    it("should be true with expected geo and locale", () => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      sandbox.stub(global.Services.prefs, "getStringPref").returns("US");
      sandbox.stub(global.Services.locale, "appLocaleAsLangTag").get(() => "en-US");

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
      sandbox.stub(global.Services.locale, "appLocaleAsLangTag").get(() => "en-US");

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
      sandbox.stub(global.Services.locale, "appLocaleAsLangTag").get(() => "en-US");

      as._updateDynamicPrefs();
      clock.tick(1);

      assert.isTrue(PREFS_CONFIG.get("feeds.section.topstories").value);
    });
  });
  describe("telemetry reporting on init failure", () => {
    it("should send a ping on init error", () => {
      as = new ActivityStream();
      const telemetry = {handleUndesiredEvent: sandbox.spy()};
      sandbox.stub(as.store, "init").throws();
      sandbox.stub(as.store.feeds, "get").returns(telemetry);
      try {
        as.init();
      } catch (e) {
      }
      assert.calledOnce(telemetry.handleUndesiredEvent);
    });
  });

  describe("searchs shortcuts shouldPin pref", () => {
    const SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF = "improvesearch.topSiteSearchShortcuts.searchEngines";
    let stub;

    beforeEach(() => {
      sandbox.stub(global.Services.prefs, "prefHasUserValue").returns(true);
      stub = sandbox.stub(global.Services.prefs, "getStringPref");
    });

    it("should be an empty string when no geo is available", () => {
      as._updateDynamicPrefs();
      assert.equal(PREFS_CONFIG.get(SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF).value, "");
    });

    it("should be 'baidu' in China", () => {
      stub.returns("CN");
      as._updateDynamicPrefs();
      assert.equal(PREFS_CONFIG.get(SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF).value, "baidu");
    });

    it("should be 'yandex' in Russia, Belarus, Kazakhstan, and Turkey", () => {
      const geos = ["BY", "KZ", "RU", "TR"];
      for (const geo of geos) {
        stub.returns(geo);
        as._updateDynamicPrefs();
        assert.equal(PREFS_CONFIG.get(SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF).value, "yandex");
      }
    });

    it("should be 'google,amazon' in Germany, France, the UK, Japan, Italy, and the US", () => {
      const geos = ["DE", "FR", "GB", "IT", "JP", "US"];
      for (const geo of geos) {
        stub.returns(geo);
        as._updateDynamicPrefs();
        assert.equal(PREFS_CONFIG.get(SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF).value, "google,amazon");
      }
    });

    it("should be 'google' elsewhere", () => {
      // A selection of other geos
      const geos = ["BR", "CA", "ES", "ID", "IN"];
      for (const geo of geos) {
        stub.returns(geo);
        as._updateDynamicPrefs();
        assert.equal(PREFS_CONFIG.get(SEARCH_SHORTCUTS_SEARCH_ENGINES_PREF).value, "google");
      }
    });
  });
});

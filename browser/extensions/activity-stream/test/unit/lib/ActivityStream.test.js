const injector = require("inject!lib/ActivityStream.jsm");

const REASON_ADDON_UNINSTALL = 6;

describe("ActivityStream", () => {
  let sandbox;
  let as;
  let ActivityStream;
  function Fake() {}

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    ({ActivityStream} = injector({
      "lib/LocalizationFeed.jsm": {LocalizationFeed: Fake},
      "lib/NewTabInit.jsm": {NewTabInit: Fake},
      "lib/PlacesFeed.jsm": {PlacesFeed: Fake},
      "lib/TelemetryFeed.jsm": {TelemetryFeed: Fake},
      "lib/TopSitesFeed.jsm": {TopSitesFeed: Fake},
      "lib/PrefsFeed.jsm": {PrefsFeed: Fake}
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
  });
});

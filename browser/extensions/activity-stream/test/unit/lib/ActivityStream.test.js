const injector = require("inject!lib/ActivityStream.jsm");

describe("ActivityStream", () => {
  let sandbox;
  let as;
  let ActivityStream;
  function Fake() {}
  before(() => {
    sandbox = sinon.sandbox.create();
    ({ActivityStream} = injector({
      "lib/LocalizationFeed.jsm": {LocalizationFeed: Fake},
      "lib/NewTabInit.jsm": {NewTabInit: Fake},
      "lib/PlacesFeed.jsm": {PlacesFeed: Fake},
      "lib/SearchFeed.jsm": {SearchFeed: Fake},
      "lib/TelemetryFeed.jsm": {TelemetryFeed: Fake},
      "lib/TopSitesFeed.jsm": {TopSitesFeed: Fake}
    }));
  });

  afterEach(() => sandbox.restore());

  beforeEach(() => {
    as = new ActivityStream();
    sandbox.stub(as.store, "init");
    sandbox.stub(as.store, "uninit");
  });

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
  describe("feeds", () => {
    it("should create a Localization feed", () => {
      const feed = as.feeds["feeds.localization"]();
      assert.instanceOf(feed, Fake);
    });
    it("should create a NewTabInit feed", () => {
      const feed = as.feeds["feeds.newtabinit"]();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Places feed", () => {
      const feed = as.feeds["feeds.places"]();
      assert.instanceOf(feed, Fake);
    });
    it("should create a TopSites feed", () => {
      const feed = as.feeds["feeds.topsites"]();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Telemetry feed", () => {
      const feed = as.feeds["feeds.telemetry"]();
      assert.instanceOf(feed, Fake);
    });
    it("should create a Search feed", () => {
      const feed = as.feeds["feeds.search"]();
      assert.instanceOf(feed, Fake);
    });
  });
});

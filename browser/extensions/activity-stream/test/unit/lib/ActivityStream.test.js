const injector = require("inject!lib/ActivityStream.jsm");

describe("ActivityStream", () => {
  let sandbox;
  let as;
  let ActivityStream;
  function NewTabInit() {}
  function TopSitesFeed() {}
  function SearchFeed() {}
  before(() => {
    sandbox = sinon.sandbox.create();
    ({ActivityStream} = injector({
      "lib/NewTabInit.jsm": {NewTabInit},
      "lib/TopSitesFeed.jsm": {TopSitesFeed},
      "lib/SearchFeed.jsm": {SearchFeed}
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
      const feed = as.feeds["feeds.newtabinit"]();
      assert.instanceOf(feed, NewTabInit);
    });
    it("should create a TopSites feed", () => {
      const feed = as.feeds["feeds.topsites"]();
      assert.instanceOf(feed, TopSitesFeed);
    });
    it("should create a Search feed", () => {
      const feed = as.feeds["feeds.search"]();
      assert.instanceOf(feed, SearchFeed);
    });
  });
});

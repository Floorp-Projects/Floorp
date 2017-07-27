"use strict";
const injector = require("inject!lib/TopSitesFeed.jsm");
const {UPDATE_TIME, TOP_SITES_SHOWMORE_LENGTH} = require("lib/TopSitesFeed.jsm");
const {FakePrefs, GlobalOverrider} = require("test/unit/utils");
const action = {meta: {fromTarget: {}}};
const {actionCreators: ac, actionTypes: at} = require("common/Actions.jsm");
const {insertPinned} = require("common/Reducers.jsm");
const FAKE_LINKS = new Array(TOP_SITES_SHOWMORE_LENGTH).fill(null).map((v, i) => ({url: `site${i}.com`}));
const FAKE_SCREENSHOT = "data123";

describe("Top Sites Feed", () => {
  let TopSitesFeed;
  let DEFAULT_TOP_SITES;
  let feed;
  let globals;
  let sandbox;
  let links;
  let clock;
  let fakeNewTabUtils;
  let fakeScreenshot;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fakeNewTabUtils = {
      activityStreamLinks: {getTopSites: sandbox.spy(() => Promise.resolve(links))},
      pinnedLinks: {
        links: [],
        isPinned: () => false,
        pin: sandbox.spy(),
        unpin: sandbox.spy()
      }
    };
    fakeScreenshot = {getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_SCREENSHOT))};
    globals.set("NewTabUtils", fakeNewTabUtils);
    FakePrefs.prototype.prefs["default.sites"] = "https://foo.com/";
    ({TopSitesFeed, DEFAULT_TOP_SITES} = injector({
      "lib/ActivityStreamPrefs.jsm": {Prefs: FakePrefs},
      "common/Reducers.jsm": {insertPinned},
      "lib/Screenshots.jsm": {Screenshots: fakeScreenshot}
    }));
    feed = new TopSitesFeed();
    feed.store = {dispatch: sinon.spy(), getState() { return {TopSites: {rows: Array(12).fill("site")}}; }};
    links = FAKE_LINKS;
    clock = sinon.useFakeTimers();
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });

  describe("#init", () => {
    it("should add defaults on INIT", () => {
      feed.onAction({type: at.INIT});
      assert.ok(DEFAULT_TOP_SITES.length);
    });
    it("should have default sites with .isDefault = true", () => {
      feed.init();
      DEFAULT_TOP_SITES.forEach(link => assert.propertyVal(link, "isDefault", true));
    });
    it("should add no defaults on empty pref", () => {
      FakePrefs.prototype.prefs["default.sites"] = "";
      feed.init();
      assert.equal(DEFAULT_TOP_SITES.length, 0);
    });
  });
  describe("#getLinksWithDefaults", () => {
    beforeEach(() => {
      feed.init();
    });

    it("should get the links from NewTabUtils", async () => {
      const result = await feed.getLinksWithDefaults();
      assert.deepEqual(result, links);
      assert.calledOnce(global.NewTabUtils.activityStreamLinks.getTopSites);
    });
    it("should add defaults if there are are not enough links", async () => {
      links = [{url: "foo.com"}];
      const result = await feed.getLinksWithDefaults();
      assert.deepEqual(result, [{url: "foo.com"}, ...DEFAULT_TOP_SITES]);
    });
    it("should only add defaults up to TOP_SITES_SHOWMORE_LENGTH", async () => {
      links = new Array(TOP_SITES_SHOWMORE_LENGTH - 1).fill({url: "foo.com"});
      const result = await feed.getLinksWithDefaults();
      assert.lengthOf(result, TOP_SITES_SHOWMORE_LENGTH);
      assert.deepEqual(result, [...links, DEFAULT_TOP_SITES[0]]);
    });
    it("should not throw if NewTabUtils returns null", () => {
      links = null;
      assert.doesNotThrow(() => {
        feed.getLinksWithDefaults(action);
      });
    });
  });
  describe("#refresh", () => {
    it("should dispatch an action with the links returned", async () => {
      sandbox.stub(feed, "getScreenshot");
      await feed.refresh(action);
      assert.calledOnce(feed.store.dispatch);
      assert.propertyVal(feed.store.dispatch.firstCall.args[0], "type", at.TOP_SITES_UPDATED);
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data, links);
    });
    it("should reuse screenshots for existing links, and call feed.getScreenshot for others", async () => {
      sandbox.stub(feed, "getScreenshot");
      const rows = [{url: FAKE_LINKS[0].url, screenshot: "foo.jpg"}];
      feed.store.getState = () => ({TopSites: {rows}});
      await feed.refresh(action);

      const results = feed.store.dispatch.firstCall.args[0].data;

      results.forEach(link => {
        if (link.url === FAKE_LINKS[0].url) {
          assert.equal(link.screenshot, "foo.jpg");
        } else {
          assert.calledWith(feed.getScreenshot, link.url);
        }
      });
    });
    it("should handle empty slots in the resulting top sites array", async () => {
      links = [FAKE_LINKS[0]];
      fakeNewTabUtils.pinnedLinks.links = [null, null, FAKE_LINKS[1], null, null, null, null, null, FAKE_LINKS[2]];
      sandbox.stub(feed, "getScreenshot");
      await feed.refresh(action);
      assert.calledOnce(feed.store.dispatch);
    });
  });
  describe("getScreenshot", () => {
    it("should call Screenshots.getScreenshotForURL with the right url", async () => {
      const url = "foo.com";
      await feed.getScreenshot(url);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, url);
    });
  });
  describe("#onAction", () => {
    const newTabAction = {type: at.NEW_TAB_LOAD, meta: {fromTarget: "target"}};
    it("should call refresh if there are not enough sites on NEW_TAB_LOAD", () => {
      feed.store.getState = function() { return {TopSites: {rows: []}}; };
      sinon.stub(feed, "refresh");
      feed.onAction(newTabAction);
      assert.calledWith(feed.refresh, newTabAction.meta.fromTarget);
    });
    it("should call refresh if there are not sites on NEW_TAB_LOAD, not counting defaults", () => {
      feed.store.getState = function() { return {TopSites: {rows: [{url: "foo.com"}, ...DEFAULT_TOP_SITES]}}; };
      sinon.stub(feed, "refresh");
      feed.onAction(newTabAction);
      assert.calledWith(feed.refresh, newTabAction.meta.fromTarget);
    });
    it("should not call refresh if there are enough sites on NEW_TAB_LOAD", () => {
      feed.lastUpdated = Date.now();
      sinon.stub(feed, "refresh");
      feed.onAction(newTabAction);
      assert.notCalled(feed.refresh);
    });
    it("should call refresh if .lastUpdated is too old on NEW_TAB_LOAD", () => {
      feed.lastUpdated = 0;
      clock.tick(UPDATE_TIME);
      sinon.stub(feed, "refresh");
      feed.onAction(newTabAction);
      assert.calledWith(feed.refresh, newTabAction.meta.fromTarget);
    });
    it("should not call refresh if .lastUpdated is less than update time on NEW_TAB_LOAD", () => {
      feed.lastUpdated = 0;
      clock.tick(UPDATE_TIME - 1);
      sinon.stub(feed, "refresh");
      feed.onAction(newTabAction);
      assert.notCalled(feed.refresh);
    });
    it("should call with correct parameters on TOP_SITES_PIN", () => {
      const pinAction = {
        type: at.TOP_SITES_PIN,
        data: {site: {url: "foo.com"}, index: 7}
      };
      feed.onAction(pinAction);
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, pinAction.data.site, pinAction.data.index);
    });
    it("should get correct isDefault and index property", () => {
      // When calling _getPinnedWithData the state in `pinnedLinks` and in the store should merge.
      const site = {url: "foo.com"};
      const siteWithIndex = {url: "foo.com", isDefault: true, index: 7};
      fakeNewTabUtils.pinnedLinks.links = [siteWithIndex];
      feed.store = {dispatch: sinon.spy(), getState() { return {TopSites: {rows: [site]}}; }};
      const pinAction = {
        type: at.TOP_SITES_PIN,
        data: {}
      };
      feed.onAction(pinAction);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({
        type: at.PINNED_SITES_UPDATED,
        data: [siteWithIndex]
      }));
    });
    it("should get correct pinned links and data", () => {
      // When calling _getPinnedWithData the state in `pinnedLinks` and in the store should merge.
      // In this case `site1` is a pinned site and should be returned.
      const site1 = {url: "foo.com", isDefault: true};
      const site2 = {url: "bar.com", isDefault: false};
      fakeNewTabUtils.pinnedLinks.links = [site1];
      feed.store = {dispatch: sinon.spy(), getState() { return {TopSites: {rows: [site2]}}; }};
      const pinAction = {
        type: at.TOP_SITES_PIN,
        data: {}
      };
      feed.onAction(pinAction);
      assert.calledOnce(feed.store.dispatch);
      assert.calledWith(feed.store.dispatch, ac.BroadcastToContent({
        type: at.PINNED_SITES_UPDATED,
        data: [site1]
      }));
    });
    it("should call unpin with correct parameters on TOP_SITES_UNPIN", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, null, {url: "foo.com"}, null, null, null, null, null, FAKE_LINKS[0]];
      const unpinAction = {
        type: at.TOP_SITES_UNPIN,
        data: {site: {url: "foo.com"}}
      };
      feed.onAction(unpinAction);
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.unpin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.unpin, unpinAction.data.site);
    });
    it("should call refresh without a target if we clear history with PLACES_HISTORY_CLEARED", () => {
      sandbox.stub(feed, "refresh");
      feed.onAction({type: at.PLACES_HISTORY_CLEARED});
      assert.calledOnce(feed.refresh);
      assert.equal(feed.refresh.firstCall.args[0], null);
    });
    it("should still dispatch an action even if there's no target provided", async () => {
      sandbox.stub(feed, "getScreenshot");
      await feed.refresh();
      assert.calledOnce(feed.store.dispatch);
      assert.propertyVal(feed.store.dispatch.firstCall.args[0], "type", at.TOP_SITES_UPDATED);
    });
  });
});

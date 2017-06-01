"use strict";
const {TopSitesFeed, UPDATE_TIME, TOP_SITES_SHOWMORE_LENGTH, DEFAULT_TOP_SITES} = require("lib/TopSitesFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const action = {meta: {fromTarget: {}}};
const {actionTypes: at} = require("common/Actions.jsm");
const FAKE_LINKS = new Array(TOP_SITES_SHOWMORE_LENGTH).fill(null).map((v, i) => ({url: `site${i}.com`}));
const FAKE_SCREENSHOT = "data123";

describe("Top Sites Feed", () => {
  let feed;
  let globals;
  let sandbox;
  let links;
  let clock;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("NewTabUtils", {activityStreamLinks: {getTopSites: sandbox.spy(() => Promise.resolve(links))}});
    globals.set("PreviewProvider", {getThumbnail: sandbox.spy(() => Promise.resolve(FAKE_SCREENSHOT))});
    feed = new TopSitesFeed();
    feed.store = {dispatch: sinon.spy(), getState() { return {TopSites: {rows: Array(12).fill("site")}}; }};
    links = FAKE_LINKS;
    clock = sinon.useFakeTimers();
  });
  afterEach(() => {
    globals.restore();
    clock.restore();
  });

  it("should have default sites with .isDefault = true", () => {
    DEFAULT_TOP_SITES.forEach(link => assert.propertyVal(link, "isDefault", true));
  });

  describe("#getLinksWithDefaults", () => {
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
    it("should call .getScreenshot for each link", async () => {
      sandbox.stub(feed, "getScreenshot");
      await feed.refresh(action);

      links.forEach(link => assert.calledWith(feed.getScreenshot, link.url));
    });
  });
  describe("getScreenshot", () => {
    it("should call PreviewProvider.getThumbnail with the right url", async () => {
      const url = "foo.com";
      await feed.getScreenshot(url);
      assert.calledWith(global.PreviewProvider.getThumbnail, url);
    });
  });
  describe("#onAction", () => {
    it("should call refresh if there are not enough sites on NEW_TAB_LOAD", () => {
      feed.store.getState = function() { return {TopSites: {rows: []}}; };
      sinon.stub(feed, "refresh");
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.refresh);
    });
    it("should call refresh if there are not sites on NEW_TAB_LOAD, not counting defaults", () => {
      feed.store.getState = function() { return {TopSites: {rows: [{url: "foo.com"}, ...DEFAULT_TOP_SITES]}}; };
      sinon.stub(feed, "refresh");
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.refresh);
    });
    it("should not call refresh if there are enough sites on NEW_TAB_LOAD", () => {
      feed.lastUpdated = Date.now();
      sinon.stub(feed, "refresh");
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.notCalled(feed.refresh);
    });
    it("should call refresh if .lastUpdated is too old on NEW_TAB_LOAD", () => {
      feed.lastUpdated = 0;
      clock.tick(UPDATE_TIME);
      sinon.stub(feed, "refresh");
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.calledOnce(feed.refresh);
    });
    it("should not call refresh if .lastUpdated is less than update time on NEW_TAB_LOAD", () => {
      feed.lastUpdated = 0;
      clock.tick(UPDATE_TIME - 1);
      sinon.stub(feed, "refresh");
      feed.onAction({type: at.NEW_TAB_LOAD});
      assert.notCalled(feed.refresh);
    });
    it("should call openNewWindow with the correct url on OPEN_NEW_WINDOW", () => {
      sinon.stub(feed, "openNewWindow");
      const openWindowAction = {type: at.OPEN_NEW_WINDOW, data: {url: "foo.com"}};
      feed.onAction(openWindowAction);
      assert.calledWith(feed.openNewWindow, openWindowAction);
    });
    it("should call openNewWindow with the correct url and privacy args on OPEN_PRIVATE_WINDOW", () => {
      sinon.stub(feed, "openNewWindow");
      const openWindowAction = {type: at.OPEN_PRIVATE_WINDOW, data: {url: "foo.com"}};
      feed.onAction(openWindowAction);
      assert.calledWith(feed.openNewWindow, openWindowAction, true);
    });
    it("should call openNewWindow with the correct url on OPEN_NEW_WINDOW", () => {
      const openWindowAction = {
        type: at.OPEN_NEW_WINDOW,
        data: {url: "foo.com"},
        _target: {browser: {ownerGlobal: {openLinkIn: () => {}}}}
      };
      sinon.stub(openWindowAction._target.browser.ownerGlobal, "openLinkIn");
      feed.onAction(openWindowAction);
      assert.calledOnce(openWindowAction._target.browser.ownerGlobal.openLinkIn);
    });
  });
});

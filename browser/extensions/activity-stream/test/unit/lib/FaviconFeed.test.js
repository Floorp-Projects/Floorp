"use strict";
const {FaviconFeed} = require("lib/FaviconFeed.jsm");
const {GlobalOverrider} = require("test/unit/utils");
const {actionTypes: at} = require("common/Actions.jsm");

const FAKE_ENDPOINT = "https://foo.com/";

describe("FaviconFeed", () => {
  let feed;
  let globals;
  let sandbox;
  let clock;
  let siteIconsPref;

  beforeEach(() => {
    clock = sinon.useFakeTimers();
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    globals.set("PlacesUtils", {
      favicons: {
        setAndFetchFaviconForPage: sandbox.spy(),
        FAVICON_LOAD_NON_PRIVATE: 1
      }
    });
    siteIconsPref = true;
    sandbox.stub(global.Services.prefs, "getBoolPref")
      .withArgs("browser.chrome.site_icons").callsFake(() => siteIconsPref);
    let fetchStub = globals.sandbox.stub();
    globals.set("fetch", fetchStub);
    fetchStub.resolves({
      ok: true,
      status: 200,
      json: () => Promise.resolve([{
        "domains": ["facebook.com"],
        "image_url": "https://www.facebook.com/icon.png"
      }, {
        "domains": ["gmail.com", "mail.google.com"],
        "image_url": "https://iconserver.com/gmail.png"
      }])
    });

    feed = new FaviconFeed();
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      state: {Prefs: {values: {"tippyTop.service.endpoint": FAKE_ENDPOINT}}}
    };
  });
  afterEach(() => {
    clock.restore();
    globals.restore();
  });

  it("should create a TopStoriesFeed", () => {
    assert.instanceOf(feed, FaviconFeed);
  });

  describe("#getSitesByDomain", () => {
    it("should loadCachedData and maybeRefresh if _sitesByDomain isn't set", async () => {
      feed.loadCachedData = sinon.spy(() => ([]));
      feed.maybeRefresh = sinon.spy(() => {
        feed._sitesByDomain = {"mozilla.org": {"image_url": "https://mozilla.org/icon.png"}};
        return [];
      });
      await feed.getSitesByDomain();
      assert.calledOnce(feed.loadCachedData);
      assert.calledOnce(feed.maybeRefresh);
    });
    it("should NOT loadCachedData and maybeRefresh if _sitesByDomain is already set", async () => {
      feed._sitesByDomain = {"mozilla.org": {"image_url": "https://mozilla.org/icon.png"}};
      feed.loadCachedData = sinon.spy(() => ([]));
      feed.maybeRefresh = sinon.spy(() => ([]));
      await feed.getSitesByDomain();
      assert.notCalled(feed.loadCachedData);
      assert.notCalled(feed.maybeRefresh);
    });
    it("should resolve to empty object if there is no cache and fetch fails", async () => {
      feed.loadCachedData = sinon.spy(() => ([]));
      feed.maybeRefresh = sinon.spy(() => ([]));
      await feed.getSitesByDomain();
      assert.deepEqual(feed._sitesByDomain, {});
    });
  });

  describe("#loadCachedData", () => {
    it("should set _sitesByDomain if there is cached data", async () => {
      const cachedData = {
        "mozilla.org": {"image_url": "https://mozilla.org/icon.png"},
        "_timestamp": Date.now(),
        "_etag": "foobaretag1234567890"
      };
      feed.cache.get = () => cachedData;
      await feed.loadCachedData();
      assert.deepEqual(feed._sitesByDomain, cachedData);
      assert.equal(feed.tippyTopNextUpdate, cachedData._timestamp + 24 * 60 * 60 * 1000);
      assert.equal(feed._sitesByDomain._etag, cachedData._etag);
    });
    it("should NOT set _sitesByDomain if there is no cached data", async () => {
      feed.cache.get = () => ({});
      await feed.loadCachedData();
      assert.isNull(feed._sitesByDomain);
    });
  });

  describe("#maybeRefresh", () => {
    it("should refresh if next update is due", () => {
      feed.refresh = sinon.spy();
      feed.tippyTopNextUpdate = Date.now();
      feed.maybeRefresh();
      assert.calledOnce(feed.refresh);
    });
    it("should NOT refresh if next update is in future", () => {
      feed.refresh = sinon.spy();
      feed.tippyTopNextUpdate = Date.now() + 6000;
      feed.maybeRefresh();
      assert.notCalled(feed.refresh);
    });
  });

  describe("#refresh", () => {
    it("should loadFromURL with the right URL from prefs", async () => {
      feed.loadFromURL = sinon.spy(() => ({data: []}));
      await feed.refresh();
      assert.calledOnce(feed.loadFromURL);
      assert.calledWith(feed.loadFromURL, FAKE_ENDPOINT);
    });
    it("should set _sitesByDomain if new sites are returned from loadFromURL", async () => {
      const data = {
        data: [
          {"domains": ["mozilla.org"], "image_url": "https://mozilla.org/icon.png"},
          {"domains": ["facebook.com"], "image_url": "https://facebook.com/icon.png"}
        ],
        etag: "etag1234567890",
        status: 200
      };
      const expectedData = {
        "facebook.com": {"image_url": "https://facebook.com/icon.png"},
        "mozilla.org": {"image_url": "https://mozilla.org/icon.png"},
        "_etag": "etag1234567890",
        "_timestamp": Date.now()
      };
      feed.loadFromURL = sinon.spy(url => data);
      feed.cache.set = sinon.spy();
      await feed.refresh();
      assert.equal(feed._sitesByDomain._etag, data.etag);
      assert.deepEqual(feed._sitesByDomain, expectedData);
      assert.calledOnce(feed.cache.set);
      assert.calledWith(feed.cache.set, "sites", expectedData);
    });
    it("should pass If-None-Match if we have a last known etag", async () => {
      feed.loadFromURL = sinon.spy(url => ({data: [], status: 304}));
      feed._sitesByDomain = {};
      feed._sitesByDomain._etag = "etag1234567890";
      await feed.refresh();
      const headers = feed.loadFromURL.getCall(0).args[1];
      assert.equal(headers.get("If-None-Match"), feed._sitesByDomain._etag);
    });
    it("should not set _sitesByDomain if the remote manifest is not modified since last fetch", async () => {
      const data = {"mozilla.org": {"image_url": "https://mozilla.org/icon.png"}};
      feed._sitesByDomain = data;
      feed._sitesByDomain._timestamp = Date.now() - 1000;
      feed.loadFromURL = sinon.spy(url => ({data: [], status: 304}));
      feed.cache.set = sinon.spy();
      await feed.refresh();
      assert.deepEqual(feed._sitesByDomain, data);
      assert.calledOnce(feed.cache.set);
      assert.calledWith(feed.cache.set, "sites", Object.assign({_timestamp: Date.now()}, data));
    });
    it("should handle server errors by retrying with exponential backoff", async () => {
      const expectedDelay = 5 * 60 * 1000;
      feed.loadFromURL = sinon.spy(url => ({data: [], status: 500}));
      await feed.refresh();
      assert.equal(1, feed.numRetries);
      assert.equal(expectedDelay, feed.tippyTopNextUpdate);
      await feed.refresh();
      assert.equal(2, feed.numRetries);
      assert.equal(2 * expectedDelay, feed.tippyTopNextUpdate);
      await feed.refresh();
      assert.equal(3, feed.numRetries);
      assert.equal(2 * 2 * expectedDelay, feed.tippyTopNextUpdate);
      await feed.refresh();
      assert.equal(4, feed.numRetries);
      assert.equal(2 * 2 * 2 * expectedDelay, feed.tippyTopNextUpdate);
      // Verify the delay maxes out at 24 hours.
      feed.numRetries = 100;
      await feed.refresh();
      assert.equal(24 * 60 * 60 * 1000, feed.tippyTopNextUpdate);
      // Verify the numRetries gets reset on a successful fetch.
      feed.loadFromURL = sinon.spy(url => ({data: [], status: 200}));
      await feed.refresh();
      assert.equal(0, feed.numRetries);
      assert.equal(24 * 60 * 60 * 1000, feed.tippyTopNextUpdate);
    });
  });

  describe("#fetchIcon", () => {
    let domain;
    let url;
    beforeEach(() => {
      domain = "mozilla.org";
      url = `https://${domain}/`;
      feed._sitesByDomain = {[domain]: {url, image_url: `${url}/icon.png`}};
    });

    it("should setAndFetchFaviconForPage if the url is in the TippyTop data", async () => {
      await feed.fetchIcon(url);

      assert.calledOnce(global.PlacesUtils.favicons.setAndFetchFaviconForPage);
      assert.calledWith(global.PlacesUtils.favicons.setAndFetchFaviconForPage,
        {spec: url},
        {ref: "tippytop", spec: `${url}/icon.png`},
        false,
        global.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        undefined);
    });
    it("should NOT setAndFetchFaviconForPage if site_icons pref is false", async () => {
      siteIconsPref = false;

      await feed.fetchIcon(url);

      assert.notCalled(global.PlacesUtils.favicons.setAndFetchFaviconForPage);
    });
    it("should NOT setAndFetchFaviconForPage if the endpoint is empty", async () => {
      feed.store.state.Prefs.values["tippyTop.service.endpoint"] = "";

      await feed.fetchIcon(url);

      assert.notCalled(global.PlacesUtils.favicons.setAndFetchFaviconForPage);
    });
    it("should NOT setAndFetchFaviconForPage if the url is NOT in the TippyTop data", async () => {
      await feed.fetchIcon("https://example.com");

      assert.notCalled(global.PlacesUtils.favicons.setAndFetchFaviconForPage);
    });
    it("should cause sites to initialize with fetched sites if no sites", async () => {
      delete feed._sitesByDomain;

      await feed.fetchIcon(url);

      assert.containsAllKeys(feed._sitesByDomain, ["facebook.com", "gmail.com", "mail.google.com"]);
    });
  });

  describe("#onAction", () => {
    it("should maybeRefresh on SYSTEM_TICK if initialized", async () => {
      feed._sitesByDomain = {"mozilla.org": {}};
      feed.maybeRefresh = sinon.spy();
      feed.onAction({type: at.SYSTEM_TICK});
      assert.calledOnce(feed.maybeRefresh);
    });
    it("should NOT maybeRefresh on SYSTEM_TICK if NOT initialized", async () => {
      feed._sitesByDomain = null;
      feed.maybeRefresh = sinon.spy();
      feed.onAction({type: at.SYSTEM_TICK});
      assert.notCalled(feed.maybeRefresh);
    });
    it("should fetchIcon on RICH_ICON_MISSING", async () => {
      feed.fetchIcon = sinon.spy();
      const url = "https://mozilla.org";
      feed.onAction({type: at.RICH_ICON_MISSING, data: {url}});
      assert.calledOnce(feed.fetchIcon);
      assert.calledWith(feed.fetchIcon, url);
    });
  });
});

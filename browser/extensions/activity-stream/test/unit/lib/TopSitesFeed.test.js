"use strict";

import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {FakePrefs, GlobalOverrider} from "test/unit/utils";
import {insertPinned, TOP_SITES_DEFAULT_ROWS, TOP_SITES_MAX_SITES_PER_ROW} from "common/Reducers.jsm";
import {getDefaultOptions} from "lib/ActivityStreamStorage.jsm";
import injector from "inject!lib/TopSitesFeed.jsm";
import {Screenshots} from "lib/Screenshots.jsm";

const FAKE_FAVICON = "data987";
const FAKE_FAVICON_SIZE = 128;
const FAKE_FRECENCY = 200;
const FAKE_LINKS = new Array(2 * TOP_SITES_MAX_SITES_PER_ROW).fill(null).map((v, i) => ({
  frecency: FAKE_FRECENCY,
  url: `http://www.site${i}.com`
}));
const FAKE_SCREENSHOT = "data123";

function FakeTippyTopProvider() {}
FakeTippyTopProvider.prototype = {
  async init() { this.initialized = true; },
  processSite(site) { return site; }
};

describe("Top Sites Feed", () => {
  let TopSitesFeed;
  let DEFAULT_TOP_SITES;
  let feed;
  let globals;
  let sandbox;
  let links;
  let fakeNewTabUtils;
  let fakeScreenshot;
  let filterAdultStub;
  let shortURLStub;
  let fakePageThumbs;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fakeNewTabUtils = {
      blockedLinks: {
        links: [],
        isBlocked: () => false
      },
      activityStreamLinks: {getTopSites: sandbox.spy(() => Promise.resolve(links))},
      activityStreamProvider: {
        _addFavicons: sandbox.spy(l => Promise.resolve(l.map(link => {
          link.favicon = FAKE_FAVICON;
          link.faviconSize = FAKE_FAVICON_SIZE;
          return link;
        }))),
        _faviconBytesToDataURI: sandbox.spy()
      },
      pinnedLinks: {
        links: [],
        isPinned: () => false,
        pin: sandbox.spy(),
        unpin: sandbox.spy()
      }
    };
    fakeScreenshot = {
      getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_SCREENSHOT)),
      maybeCacheScreenshot: sandbox.spy(Screenshots.maybeCacheScreenshot),
      _shouldGetScreenshots: sinon.stub().returns(true)
    };
    filterAdultStub = sinon.stub().returns([]);
    shortURLStub = sinon.stub().callsFake(site => site.url);
    const fakeDedupe = function() {};
    fakePageThumbs = {
      addExpirationFilter: sinon.stub(),
      removeExpirationFilter: sinon.stub()
    };
    globals.set("PageThumbs", fakePageThumbs);
    globals.set("NewTabUtils", fakeNewTabUtils);
    FakePrefs.prototype.prefs["default.sites"] = "https://foo.com/";
    ({TopSitesFeed, DEFAULT_TOP_SITES} = injector({
      "lib/ActivityStreamPrefs.jsm": {Prefs: FakePrefs},
      "common/Dedupe.jsm": {Dedupe: fakeDedupe},
      "common/Reducers.jsm": {insertPinned, TOP_SITES_DEFAULT_ROWS, TOP_SITES_MAX_SITES_PER_ROW},
      "lib/FilterAdult.jsm": {filterAdult: filterAdultStub},
      "lib/Screenshots.jsm": {Screenshots: fakeScreenshot},
      "lib/TippyTopProvider.jsm": {TippyTopProvider: FakeTippyTopProvider},
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/ActivityStreamStorage.jsm": {ActivityStreamStorage: function Fake() {}, getDefaultOptions}
    }));
    feed = new TopSitesFeed();
    const storage = {
      init: sandbox.stub().resolves(),
      get: sandbox.stub().resolves(),
      set: sandbox.stub().resolves()
    };
    // Setup for tests that don't call `init` but require feed.storage
    feed._storage = storage;
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      state: {
        Prefs: {values: {filterAdult: false, topSitesRows: 2}},
        TopSites: {rows: Array(12).fill("site")}
      },
      dbStorage: {getDbTable: sandbox.stub().returns(storage)}
    };
    feed.dedupe.group = (...sites) => sites;
    links = FAKE_LINKS;
  });
  afterEach(() => {
    globals.restore();
  });

  function stubFaviconsToUseScreenshots() {
    fakeNewTabUtils.activityStreamProvider._addFavicons = sandbox.stub();
  }

  describe("#refreshDefaults", () => {
    it("should add defaults on PREFS_INITIAL_VALUES", () => {
      feed.onAction({type: at.PREFS_INITIAL_VALUES, data: {"default.sites": "https://foo.com"}});

      assert.isAbove(DEFAULT_TOP_SITES.length, 0);
    });
    it("should add defaults on PREF_CHANGED", () => {
      feed.onAction({type: at.PREF_CHANGED, data: {name: "default.sites", value: "https://foo.com"}});

      assert.isAbove(DEFAULT_TOP_SITES.length, 0);
    });
    it("should have default sites with .isDefault = true", () => {
      feed.refreshDefaults("https://foo.com");

      DEFAULT_TOP_SITES.forEach(link => assert.propertyVal(link, "isDefault", true));
    });
    it("should have default sites with appropriate hostname", () => {
      feed.refreshDefaults("https://foo.com");

      DEFAULT_TOP_SITES.forEach(link => assert.propertyVal(link, "hostname",
        shortURLStub(link)));
    });
    it("should add no defaults on empty pref", () => {
      feed.refreshDefaults("");

      assert.equal(DEFAULT_TOP_SITES.length, 0);
    });
    it("should clear defaults", () => {
      feed.refreshDefaults("https://foo.com");
      feed.refreshDefaults("");

      assert.equal(DEFAULT_TOP_SITES.length, 0);
    });
  });
  describe("#filterForThumbnailExpiration", () => {
    it("should pass rows.urls to the callback provided", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com", "customScreenshotURL": "custom"}];
      feed.store.state.TopSites = {rows};
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, ["foo.com", "bar.com", "custom"]);
    });
  });
  describe("#getLinksWithDefaults", () => {
    beforeEach(() => {
      feed.refreshDefaults("https://foo.com");
    });

    describe("general", () => {
      it("should get the links from NewTabUtils", async () => {
        const result = await feed.getLinksWithDefaults();
        const reference = links.map(site => Object.assign({}, site, {
          hostname: shortURLStub(site),
          typedBonus: true
        }));

        assert.deepEqual(result, reference);
        assert.calledOnce(global.NewTabUtils.activityStreamLinks.getTopSites);
      });
      it("should indicate the links get typed bonus", async () => {
        const result = await feed.getLinksWithDefaults();

        assert.propertyVal(result[0], "typedBonus", true);
      });
      it("should not filter out adult sites when pref is false", async () => {
        await feed.getLinksWithDefaults();

        assert.notCalled(filterAdultStub);
      });
      it("should filter out non-pinned adult sites when pref is true", async () => {
        feed.store.state.Prefs.values.filterAdult = true;
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/"}];

        const result = await feed.getLinksWithDefaults();

        // The stub filters out everything
        assert.calledOnce(filterAdultStub);
        assert.equal(result.length, 1);
        assert.equal(result[0].url, fakeNewTabUtils.pinnedLinks.links[0].url);
      });
      it("should filter out the defaults that have been blocked", async () => {
        // make sure we only have one top site, and we block the only default site we have to show
        const url = "www.myonlytopsite.com";
        const topsite = {
          frecency: FAKE_FRECENCY,
          hostname: shortURLStub({url}),
          typedBonus: true,
          url
        };
        const blockedDefaultSite = {url: "https://foo.com"};
        fakeNewTabUtils.activityStreamLinks.getTopSites = () => [topsite];
        fakeNewTabUtils.blockedLinks.isBlocked = site => (site.url === blockedDefaultSite.url);
        const result = await feed.getLinksWithDefaults();

        // what we should be left with is just the top site we added, and not the default site we blocked
        assert.lengthOf(result, 1);
        assert.deepEqual(result[0], topsite);
        assert.notInclude(result, blockedDefaultSite);
      });
      it("should call dedupe on the links", async () => {
        const stub = sinon.stub(feed.dedupe, "group").callsFake((...id) => id);

        await feed.getLinksWithDefaults();

        assert.calledOnce(stub);
      });
      it("should dedupe the links by hostname", async () => {
        const site = {url: "foo", hostname: "bar"};
        const result = feed._dedupeKey(site);

        assert.equal(result, site.hostname);
      });
      it("should add defaults if there are are not enough links", async () => {
        links = [{frecency: FAKE_FRECENCY, url: "foo.com"}];

        const result = await feed.getLinksWithDefaults();
        const reference = [...links, ...DEFAULT_TOP_SITES].map(s =>
          Object.assign({}, s, {
            hostname: shortURLStub(s),
            typedBonus: true
          }));

        assert.deepEqual(result, reference);
      });
      it("should only add defaults up to the number of visible slots", async () => {
        links = [];
        const numVisible = TOP_SITES_DEFAULT_ROWS * TOP_SITES_MAX_SITES_PER_ROW;
        for (let i = 0; i < numVisible - 1; i++) {
          links.push({frecency: FAKE_FRECENCY, url: `foo${i}.com`});
        }
        const result = await feed.getLinksWithDefaults();
        const reference = [...links, DEFAULT_TOP_SITES[0]].map(s =>
          Object.assign({}, s, {
            hostname: shortURLStub(s),
            typedBonus: true
          }));

        assert.lengthOf(result, numVisible);
        assert.deepEqual(result, reference);
      });
      it("should not throw if NewTabUtils returns null", () => {
        links = null;
        assert.doesNotThrow(() => {
          feed.getLinksWithDefaults();
        });
      });
      it("should get more if the user has asked for more", async () => {
        links = new Array(4 * TOP_SITES_MAX_SITES_PER_ROW).fill(null).map((v, i) => ({
          frecency: FAKE_FRECENCY,
          url: `http://www.site${i}.com`
        }));
        feed.store.state.Prefs.values.topSitesRows = 3;

        const result = await feed.getLinksWithDefaults();

        assert.propertyVal(result, "length", feed.store.state.Prefs.values.topSitesRows * TOP_SITES_MAX_SITES_PER_ROW);
      });
    });
    describe("caching", () => {
      it("should reuse the cache on subsequent calls", async () => {
        await feed.getLinksWithDefaults();
        await feed.getLinksWithDefaults();

        assert.calledOnce(global.NewTabUtils.activityStreamLinks.getTopSites);
      });
      it("should ignore the cache when requesting more", async () => {
        await feed.getLinksWithDefaults();
        feed.store.state.Prefs.values.topSitesRows *= 3;

        await feed.getLinksWithDefaults();

        assert.calledTwice(global.NewTabUtils.activityStreamLinks.getTopSites);
      });
      it("should migrate frecent screenshot data without getting screenshots again", async () => {
        stubFaviconsToUseScreenshots();
        await feed.getLinksWithDefaults();
        const {callCount} = fakeScreenshot.getScreenshotForURL;
        feed.frecentCache.expire();

        const result = await feed.getLinksWithDefaults();

        assert.calledTwice(global.NewTabUtils.activityStreamLinks.getTopSites);
        assert.callCount(fakeScreenshot.getScreenshotForURL, callCount);
        assert.propertyVal(result[0], "screenshot", FAKE_SCREENSHOT);
      });
      it("should migrate pinned favicon data without getting favicons again", async () => {
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/"}];
        await feed.getLinksWithDefaults();
        const {callCount} = fakeNewTabUtils.activityStreamProvider._addFavicons;
        feed.pinnedCache.expire();

        const result = await feed.getLinksWithDefaults();

        assert.callCount(fakeNewTabUtils.activityStreamProvider._addFavicons, callCount);
        assert.propertyVal(result[0], "favicon", FAKE_FAVICON);
        assert.propertyVal(result[0], "faviconSize", FAKE_FAVICON_SIZE);
      });
      it("should not expose internal link properties", async () => {
        const result = await feed.getLinksWithDefaults();

        const internal = Object.keys(result[0]).filter(key => key.startsWith("__"));
        assert.equal(internal.join(""), "");
      });
      it("should copy the screenshot of the frecent site if pinned site doesn't have customScreenshotURL", async () => {
        links = [{url: "https://foo.com/", screenshot: "screenshot"}];
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/"}];

        const result = await feed.getLinksWithDefaults();

        assert.equal(result[0].screenshot, links[0].screenshot);
      });
      it("should not copy the frecent screenshot if customScreenshotURL is set", async () => {
        links = [{url: "https://foo.com/", screenshot: "screenshot"}];
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/", customScreenshotURL: "custom"}];

        const result = await feed.getLinksWithDefaults();

        assert.isUndefined(result[0].screenshot);
      });
      it("should keep the same screenshot if no frecent site is found", async () => {
        links = [];
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/", screenshot: "custom"}];

        const result = await feed.getLinksWithDefaults();

        assert.equal(result[0].screenshot, "custom");
      });
      it("should not overwrite pinned site screenshot", async () => {
        links = [{url: "https://foo.com/", screenshot: "foo"}];
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/", screenshot: "bar"}];

        const result = await feed.getLinksWithDefaults();

        assert.equal(result[0].screenshot, "bar");
      });
      describe("concurrency", () => {
        let resolvers;
        beforeEach(() => {
          stubFaviconsToUseScreenshots();
          resolvers = [];
          fakeScreenshot.getScreenshotForURL = sandbox.spy(() => new Promise(
            resolve => resolvers.push(resolve)));
        });

        const getTwice = () => Promise.all([feed.getLinksWithDefaults(), feed.getLinksWithDefaults()]);
        const resolveAll = () => resolvers.forEach(resolve => resolve(FAKE_SCREENSHOT));

        it("should call the backing data once", async () => {
          await getTwice();

          assert.calledOnce(global.NewTabUtils.activityStreamLinks.getTopSites);
        });
        it("should get screenshots once per link", async () => {
          await getTwice();

          assert.callCount(fakeScreenshot.getScreenshotForURL, FAKE_LINKS.length);
        });
        it("should dispatch once per link screenshot fetched", async () => {
          feed._requestRichIcon = sinon.stub();
          await getTwice();

          await resolveAll();

          assert.callCount(feed.store.dispatch, FAKE_LINKS.length);
        });
      });
    });
    describe("deduping", () => {
      beforeEach(() => {
        ({TopSitesFeed, DEFAULT_TOP_SITES} = injector({
          "lib/ActivityStreamPrefs.jsm": {Prefs: FakePrefs},
          "common/Reducers.jsm": {insertPinned, TOP_SITES_DEFAULT_ROWS, TOP_SITES_MAX_SITES_PER_ROW},
          "lib/Screenshots.jsm": {Screenshots: fakeScreenshot}
        }));
        sandbox.stub(global.Services.eTLD, "getPublicSuffix").returns("com");
        feed = Object.assign(new TopSitesFeed(), {store: feed.store});
      });
      it("should not dedupe pinned sites", async () => {
        fakeNewTabUtils.pinnedLinks.links = [
          {url: "https://developer.mozilla.org/en-US/docs/Web"},
          {url: "https://developer.mozilla.org/en-US/docs/Learn"}
        ];

        const sites = await feed.getLinksWithDefaults();

        assert.lengthOf(sites, 2 * TOP_SITES_MAX_SITES_PER_ROW);
        assert.equal(sites[0].url, fakeNewTabUtils.pinnedLinks.links[0].url);
        assert.equal(sites[1].url, fakeNewTabUtils.pinnedLinks.links[1].url);
        assert.equal(sites[0].hostname, sites[1].hostname);
      });
      it("should prefer pinned sites over links", async () => {
        fakeNewTabUtils.pinnedLinks.links = [
          {url: "https://developer.mozilla.org/en-US/docs/Web"},
          {url: "https://developer.mozilla.org/en-US/docs/Learn"}
        ];
        // These will be the frecent results.
        links = [
          {frecency: FAKE_FRECENCY, url: "https://developer.mozilla.org/"},
          {frecency: FAKE_FRECENCY, url: "https://www.mozilla.org/"}
        ];

        const sites = await feed.getLinksWithDefaults();

        // Expecting 3 links where there's 2 pinned and 1 www.mozilla.org, so
        // the frecent with matching hostname as pinned is removed.
        assert.lengthOf(sites, 3);
        assert.equal(sites[0].url, fakeNewTabUtils.pinnedLinks.links[0].url);
        assert.equal(sites[1].url, fakeNewTabUtils.pinnedLinks.links[1].url);
        assert.equal(sites[2].url, links[1].url);
      });
      it("should return sites that have a title", async () => {
        // Simulate a pinned link with no title.
        fakeNewTabUtils.pinnedLinks.links = [{url: "https://github.com/mozilla/activity-stream"}];

        const sites = await feed.getLinksWithDefaults();

        for (const site of sites) {
          assert.isDefined(site.hostname);
        }
      });
      it("should check against null entries", async () => {
        fakeNewTabUtils.pinnedLinks.links = [null];

        await feed.getLinksWithDefaults();
      });
    });
    it("should call _fetchIcon for each link", async () => {
      sinon.spy(feed, "_fetchIcon");

      const results = await feed.getLinksWithDefaults();

      assert.callCount(feed._fetchIcon, results.length);
      results.forEach(link => {
        assert.calledWith(feed._fetchIcon, link);
      });
    });
    it("should call _fetchScreenshot when customScreenshotURL is set", async () => {
      links = [];
      fakeNewTabUtils.pinnedLinks.links = [{url: "foo", customScreenshotURL: "custom"}];
      sinon.stub(feed, "_fetchScreenshot");

      await feed.getLinksWithDefaults();

      assert.calledWith(feed._fetchScreenshot, sinon.match.object, "custom");
    });
  });
  describe("#init", () => {
    it("should call refresh (broadcast:true)", async () => {
      sandbox.stub(feed, "refresh");

      await feed.init();

      assert.calledOnce(feed.refresh);
      assert.calledWithExactly(feed.refresh, {broadcast: true});
    });
    it("should initialise the storage", async () => {
      await feed.init();

      assert.calledOnce(feed.store.dbStorage.getDbTable);
      assert.calledWithExactly(feed.store.dbStorage.getDbTable, "sectionPrefs");
    });
  });
  describe("#refresh", () => {
    beforeEach(() => {
      sandbox.stub(feed, "_fetchIcon");
    });
    it("should wait for tippytop to initialize", async () => {
      feed._tippyTopProvider.initialized = false;
      sinon.stub(feed._tippyTopProvider, "init").resolves();

      await feed.refresh();

      assert.calledOnce(feed._tippyTopProvider.init);
    });
    it("should not init the tippyTopProvider if already initialized", async () => {
      feed._tippyTopProvider.initialized = true;
      sinon.stub(feed._tippyTopProvider, "init").resolves();

      await feed.refresh();

      assert.notCalled(feed._tippyTopProvider.init);
    });
    it("should broadcast TOP_SITES_UPDATED", async () => {
      sinon.stub(feed, "getLinksWithDefaults").returns(Promise.resolve([]));

      await feed.refresh({broadcast: true});

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.BroadcastToContent({
        type: at.TOP_SITES_UPDATED,
        data: {links: [], pref: {collapsed: false}}
      }));
    });
    it("should dispatch an action with the links returned", async () => {
      await feed.refresh({broadcast: true});
      const reference = links.map(site => Object.assign({}, site, {
        hostname: shortURLStub(site),
        typedBonus: true
      }));

      assert.calledOnce(feed.store.dispatch);
      assert.propertyVal(feed.store.dispatch.firstCall.args[0], "type", at.TOP_SITES_UPDATED);
      assert.deepEqual(feed.store.dispatch.firstCall.args[0].data.links, reference);
    });
    it("should handle empty slots in the resulting top sites array", async () => {
      links = [FAKE_LINKS[0]];
      fakeNewTabUtils.pinnedLinks.links = [null, null, FAKE_LINKS[1], null, null, null, null, null, FAKE_LINKS[2]];
      await feed.refresh({broadcast: true});
      assert.calledOnce(feed.store.dispatch);
    });
    it("should dispatch AlsoToPreloaded when broadcast is false", async () => {
      sandbox.stub(feed, "getLinksWithDefaults").returns([]);
      await feed.refresh({broadcast: false});

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.AlsoToPreloaded({
        type: at.TOP_SITES_UPDATED,
        data: {links: [], pref: {collapsed: false}}
      }));
    });
    it("should not init storage if it is already initialized", async () => {
      feed._storage.initialized = true;

      await feed.refresh({broadcast: false});

      assert.notCalled(feed._storage.init);
    });
    it("should catch indexedDB errors", async () => {
      feed._storage.get.throws(new Error());
      globals.sandbox.spy(global.Cu, "reportError");

      try {
        await feed.refresh({broadcast: false});
      } catch (e) {
        assert.fails();
      }

      assert.calledOnce(Cu.reportError);
    });
  });
  describe("#updateSectionPrefs", () => {
    it("should call updateSectionPrefs on UPDATE_SECTION_PREFS", () => {
      sandbox.stub(feed, "updateSectionPrefs");

      feed.onAction({type: at.UPDATE_SECTION_PREFS, data: {id: "topsites"}});

      assert.calledOnce(feed.updateSectionPrefs);
    });
    it("should dispatch TOP_SITES_PREFS_UPDATED", async () => {
      await feed.updateSectionPrefs({collapsed: true});

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.BroadcastToContent({
        type: at.TOP_SITES_PREFS_UPDATED,
        data: {pref: {collapsed: true}}
      }));
    });
  });
  describe("#getScreenshotPreview", () => {
    it("should dispatch preview if request is succesful", async () => {
      await feed.getScreenshotPreview("custom", 1234);

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.OnlyToOneContent({
        data: {preview: FAKE_SCREENSHOT, url: "custom"},
        type: at.PREVIEW_RESPONSE
      }, 1234));
    });
    it("should return empty string if request fails", async () => {
      fakeScreenshot.getScreenshotForURL = sandbox.stub().returns(Promise.resolve(null));
      await feed.getScreenshotPreview("custom", 1234);

      assert.calledOnce(feed.store.dispatch);
      assert.calledWithExactly(feed.store.dispatch, ac.OnlyToOneContent({
        data: {preview: "", url: "custom"},
        type: at.PREVIEW_RESPONSE
      }, 1234));
    });
  });
  describe("#_fetchIcon", () => {
    it("should reuse screenshot on the link", () => {
      const link = {screenshot: "reuse.png"};

      feed._fetchIcon(link);

      assert.notCalled(fakeScreenshot.getScreenshotForURL);
      assert.propertyVal(link, "screenshot", "reuse.png");
    });
    it("should reuse existing fetching screenshot on the link", async () => {
      const link = {__sharedCache: {fetchingScreenshot: Promise.resolve("fetching.png")}};

      await feed._fetchIcon(link);

      assert.notCalled(fakeScreenshot.getScreenshotForURL);
    });
    it("should get a screenshot if the link is missing it", () => {
      feed._fetchIcon(Object.assign({__sharedCache: {}}, FAKE_LINKS[0]));

      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_LINKS[0].url);
    });
    it("should update the link's cache with a screenshot", async () => {
      const updateLink = sandbox.stub();
      const link = {__sharedCache: {updateLink}};

      await feed._fetchIcon(link);

      assert.calledOnce(updateLink);
      assert.calledWith(updateLink, "screenshot", FAKE_SCREENSHOT);
    });
    it("should skip getting a screenshot if there is a tippy top icon", () => {
      feed._tippyTopProvider.processSite = site => {
        site.tippyTopIcon = "icon.png";
        site.backgroundColor = "#fff";
        return site;
      };
      const link = {url: "example.com"};
      feed._fetchIcon(link);
      assert.propertyVal(link, "tippyTopIcon", "icon.png");
      assert.notProperty(link, "screenshot");
      assert.notCalled(fakeScreenshot.getScreenshotForURL);
    });
    it("should skip getting a screenshot if there is an icon of size greater than 96x96 and no tippy top", () => {
      const link = {
        url: "foo.com",
        favicon: "data:foo",
        faviconSize: 196
      };
      feed._fetchIcon(link);
      assert.notProperty(link, "tippyTopIcon");
      assert.notProperty(link, "screenshot");
      assert.notCalled(fakeScreenshot.getScreenshotForURL);
    });
    it("should use the link's rich icon even if there's a tippy top", () => {
      feed._tippyTopProvider.processSite = site => {
        site.tippyTopIcon = "icon.png";
        site.backgroundColor = "#fff";
        return site;
      };
      const link = {
        url: "foo.com",
        favicon: "data:foo",
        faviconSize: 196
      };
      feed._fetchIcon(link);
      assert.notProperty(link, "tippyTopIcon");
    });
  });
  describe("#_fetchScreenshot", () => {
    it("should call maybeCacheScreenshot", async () => {
      const updateLink = sinon.stub();
      const link = {customScreenshotURL: "custom", __sharedCache: {updateLink}};
      await feed._fetchScreenshot(link, "custom");

      assert.calledOnce(fakeScreenshot.maybeCacheScreenshot);
      assert.calledWithExactly(fakeScreenshot.maybeCacheScreenshot, link, link.customScreenshotURL,
        "screenshot", sinon.match.func);
    });
    it("should not call maybeCacheScreenshot if screenshot is set", async () => {
      const updateLink = sinon.stub();
      const link = {customScreenshotURL: "custom", __sharedCache: {updateLink}, screenshot: true};
      await feed._fetchScreenshot(link, "custom");

      assert.notCalled(fakeScreenshot.maybeCacheScreenshot);
    });
  });
  describe("#onAction", () => {
    it("should call getScreenshotPreview on PREVIEW_REQUEST", () => {
      sandbox.stub(feed, "getScreenshotPreview");

      feed.onAction({type: at.PREVIEW_REQUEST, data: {url: "foo"}, meta: {fromTarget: 1234}});

      assert.calledOnce(feed.getScreenshotPreview);
      assert.calledWithExactly(feed.getScreenshotPreview, "foo", 1234);
    });
    it("should refresh on SYSTEM_TICK", async () => {
      sandbox.stub(feed, "refresh");

      feed.onAction({type: at.SYSTEM_TICK});

      assert.calledOnce(feed.refresh);
      assert.calledWithExactly(feed.refresh, {broadcast: false});
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
    it("should call pin on TOP_SITES_PIN", () => {
      sinon.stub(feed, "pin");
      const pinExistingAction = {type: at.TOP_SITES_PIN, data: {site: FAKE_LINKS[4], index: 4}};

      feed.onAction(pinExistingAction);

      assert.calledOnce(feed.pin);
    });
    it("should trigger refresh on TOP_SITES_PIN", async () => {
      sinon.stub(feed, "refresh");
      const pinExistingAction = {type: at.TOP_SITES_PIN, data: {site: FAKE_LINKS[4], index: 4}};

      await feed.pin(pinExistingAction);

      assert.calledOnce(feed.refresh);
    });
    it("should call insert on TOP_SITES_INSERT", async () => {
      sinon.stub(feed, "insert");
      const addAction = {type: at.TOP_SITES_INSERT, data: {site: {url: "foo.com"}}};

      feed.onAction(addAction);

      assert.calledOnce(feed.insert);
    });
    it("should trigger refresh on TOP_SITES_INSERT", async () => {
      sinon.stub(feed, "refresh");
      const addAction = {type: at.TOP_SITES_INSERT, data: {site: {url: "foo.com"}}};

      await feed.insert(addAction);

      assert.calledOnce(feed.refresh);
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
      assert.calledWithExactly(feed.refresh, {broadcast: true});
    });
    it("should still dispatch an action even if there's no target provided", async () => {
      sandbox.stub(feed, "_fetchIcon");
      await feed.refresh({broadcast: true});
      assert.calledOnce(feed.store.dispatch);
      assert.propertyVal(feed.store.dispatch.firstCall.args[0], "type", at.TOP_SITES_UPDATED);
    });
    it("should call init on INIT action", async () => {
      sinon.stub(feed, "init");
      feed.onAction({type: at.INIT});
      assert.calledOnce(feed.init);
    });
    it("should call refresh on MIGRATION_COMPLETED action", async () => {
      sinon.stub(feed, "refresh");

      await feed.onAction({type: at.MIGRATION_COMPLETED});

      assert.calledOnce(feed.refresh);
      assert.calledWithExactly(feed.refresh, {broadcast: true});
    });
    it("should call refresh on PLACES_LINK_BLOCKED action", async () => {
      sinon.stub(feed, "refresh");
      await feed.onAction({type: at.PLACES_LINK_BLOCKED});
      assert.calledOnce(feed.refresh);
      assert.calledWithExactly(feed.refresh, {broadcast: true});
    });
    it("should call refresh on PLACES_LINKS_CHANGED action", async () => {
      sinon.stub(feed, "refresh");
      await feed.onAction({type: at.PLACES_LINKS_CHANGED});
      assert.calledOnce(feed.refresh);
      assert.calledWithExactly(feed.refresh, {broadcast: false});
    });
    it("should call pin with correct args on TOP_SITES_INSERT without an index specified", () => {
      const addAction = {
        type: at.TOP_SITES_INSERT,
        data: {site: {url: "foo.bar", label: "foo"}}
      };
      feed.onAction(addAction);
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, addAction.data.site, 0);
    });
    it("should call pin with correct args on TOP_SITES_INSERT", () => {
      const dropAction = {
        type: at.TOP_SITES_INSERT,
        data: {site: {url: "foo.bar", label: "foo"}, index: 3}
      };
      feed.onAction(dropAction);
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, dropAction.data.site, 3);
    });
    it("should remove the expiration filter on UNINIT", () => {
      feed.onAction({type: "UNINIT"});

      assert.calledOnce(fakePageThumbs.removeExpirationFilter);
    });
  });
  describe("#add", () => {
    it("should pin site in first slot of empty pinned list", () => {
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {site}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
    });
    it("should pin site in first slot of pinned list with empty first slot", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, {url: "example.com"}];
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {site}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
    });
    it("should move a pinned site in first slot to the next slot: part 1", () => {
      const site1 = {url: "example.com"};
      fakeNewTabUtils.pinnedLinks.links = [site1];
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {site}});
      assert.calledTwice(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site1, 1);
    });
    it("should move a pinned site in first slot to the next slot: part 2", () => {
      const site1 = {url: "example.com"};
      const site2 = {url: "example.org"};
      fakeNewTabUtils.pinnedLinks.links = [site1, null, site2];
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {site}});
      assert.calledTwice(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site1, 1);
    });
    it("should unpin the last site if all slots are already pinned", () => {
      const site1 = {url: "example.com"};
      const site2 = {url: "example.org"};
      const site3 = {url: "example.net"};
      const site4 = {url: "example.biz"};
      const site5 = {url: "example.info"};
      const site6 = {url: "example.news"};
      const site7 = {url: "example.lol"};
      const site8 = {url: "example.golf"};
      fakeNewTabUtils.pinnedLinks.links = [site1, site2, site3, site4, site5, site6, site7, site8];
      feed.store.state.Prefs.values.topSitesRows = 1;
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {site}});
      assert.equal(fakeNewTabUtils.pinnedLinks.pin.callCount, 8);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site1, 1);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site2, 2);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site3, 3);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site4, 4);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site5, 5);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site6, 6);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site7, 7);
    });
  });
  describe("#pin", () => {
    it("should pin site in specified slot empty pinned list", async () => {
      const site = {url: "foo.bar", label: "foo", customScreenshotURL: "screenshot"};
      await feed.pin({data: {index: 2, site}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 2);
    });
    it("should lookup the link object to update the custom screenshot", async () => {
      const site = {url: "foo.bar", label: "foo", customScreenshotURL: "screenshot"};
      sandbox.spy(feed.pinnedCache, "request");

      await feed.pin({data: {index: 2, site}});

      assert.calledOnce(feed.pinnedCache.request);
    });
    it("should lookup the link object to update the custom screenshot", async () => {
      const site = {url: "foo.bar", label: "foo", customScreenshotURL: null};
      sandbox.spy(feed.pinnedCache, "request");

      await feed.pin({data: {index: 2, site}});

      assert.calledOnce(feed.pinnedCache.request);
    });
    it("should not do a link object lookup if custom screenshot field is not set", async () => {
      const site = {url: "foo.bar", label: "foo"};
      sandbox.spy(feed.pinnedCache, "request");

      await feed.pin({data: {index: 2, site}});

      assert.notCalled(feed.pinnedCache.request);
    });
    it("should pin site in specified slot of pinned list that is free", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, {url: "example.com"}];
      const site = {url: "foo.bar", label: "foo"};
      feed.pin({data: {index: 2, site}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 2);
    });
    it("should NOT move a pinned site in specified slot to the next slot", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, null, {url: "example.com"}];
      const site = {url: "foo.bar", label: "foo"};
      feed.pin({data: {index: 2, site}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 2);
    });
    it("should properly update LinksCache object properties between migrations", async () => {
      fakeNewTabUtils.pinnedLinks.links = [{url: "https://foo.com/"}];

      let pinnedLinks = await feed.pinnedCache.request();
      assert.equal(pinnedLinks.length, 1);
      feed.pinnedCache.expire();
      pinnedLinks[0].__sharedCache.updateLink("screenshot", "foo");

      pinnedLinks = await feed.pinnedCache.request();
      assert.propertyVal(pinnedLinks[0], "screenshot", "foo");

      // Force cache expiration in order to trigger a migration of objects
      feed.pinnedCache.expire();
      pinnedLinks[0].__sharedCache.updateLink("screenshot", "bar");

      pinnedLinks = await feed.pinnedCache.request();
      assert.propertyVal(pinnedLinks[0], "screenshot", "bar");
    });
    it("should call insert if index < 0", () => {
      const site = {url: "foo.bar", label: "foo"};
      const action = {data: {index: -1, site}};

      sandbox.spy(feed, "insert");
      feed.pin(action);

      assert.calledOnce(feed.insert);
      assert.calledWithExactly(feed.insert, action);
    });
    it("should not call insert if index == 0", () => {
      const site = {url: "foo.bar", label: "foo"};
      const action = {data: {index: 0, site}};

      sandbox.spy(feed, "insert");
      feed.pin(action);

      assert.notCalled(feed.insert);
    });
  });
  describe("clearLinkCustomScreenshot", () => {
    it("should remove cached screenshot if custom url changes", async () => {
      const stub = sandbox.stub();
      sandbox.stub(feed.pinnedCache, "request").returns(Promise.resolve([{
        url: "foo",
        customScreenshotURL: "old_screenshot",
        __sharedCache: {updateLink: stub}
      }]));

      await feed._clearLinkCustomScreenshot({url: "foo", customScreenshotURL: "new_screenshot"});

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, "screenshot", undefined);
    });
    it("should remove cached screenshot if custom url is removed", async () => {
      const stub = sandbox.stub();
      sandbox.stub(feed.pinnedCache, "request").returns(Promise.resolve([{
        url: "foo",
        customScreenshotURL: "old_screenshot",
        __sharedCache: {updateLink: stub}
      }]));

      await feed._clearLinkCustomScreenshot({url: "foo", customScreenshotURL: "new_screenshot"});

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, "screenshot", undefined);
    });
  });
  describe("#drop", () => {
    it("should correctly handle different index values", () => {
      let index = -1;
      const site = {url: "foo.bar", label: "foo"};
      const action = {data: {index, site}};

      feed.insert(action);

      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);

      index = undefined;
      feed.insert(action);

      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 0);
    });
    it("should pin site in specified slot that is free", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, {url: "example.com"}];
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {index: 2, site, draggedFromIndex: 0}});
      assert.calledOnce(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 2);
    });
    it("should move a pinned site in specified slot to the next slot", () => {
      fakeNewTabUtils.pinnedLinks.links = [null, null, {url: "example.com"}];
      const site = {url: "foo.bar", label: "foo"};
      feed.insert({data: {index: 2, site, draggedFromIndex: 3}});
      assert.calledTwice(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site, 2);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, {url: "example.com"}, 3);
    });
    it("should move pinned sites in the direction of the dragged site", () => {
      const site1 = {url: "foo.bar", label: "foo"};
      const site2 = {url: "example.com", label: "example"};
      fakeNewTabUtils.pinnedLinks.links = [null, null, site2];
      feed.insert({data: {index: 2, site: site1, draggedFromIndex: 0}});
      assert.calledTwice(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site1, 2);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site2, 1);
      fakeNewTabUtils.pinnedLinks.pin.reset();
      feed.insert({data: {index: 2, site: site1, draggedFromIndex: 5}});
      assert.calledTwice(fakeNewTabUtils.pinnedLinks.pin);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site1, 2);
      assert.calledWith(fakeNewTabUtils.pinnedLinks.pin, site2, 3);
    });
    it("should not insert past the visible top sites", () => {
      const site1 = {url: "foo.bar", label: "foo"};
      feed.insert({data: {index: 42, site: site1, draggedFromIndex: 0}});
      assert.notCalled(fakeNewTabUtils.pinnedLinks.pin);
    });
  });
  describe("integration", () => {
    let resolvers = [];
    beforeEach(() => {
      feed.store.dispatch = sandbox.stub().callsFake(() => {
        resolvers.shift()();
      });
      fakeScreenshot.getScreenshotForURL = sandbox.spy();
    });

    const forDispatch = action => new Promise(resolve => {
      resolvers.push(resolve);
      feed.onAction(action);
    });

    it("should add a pinned site and remove it", async () => {
      feed._requestRichIcon = sinon.stub();
      const url = "pin.me";
      fakeNewTabUtils.pinnedLinks.pin = sandbox.stub().callsFake(link => {
        fakeNewTabUtils.pinnedLinks.links.push(link);
      });

      await forDispatch({type: at.TOP_SITES_INSERT, data: {site: {url}}});
      fakeNewTabUtils.pinnedLinks.links.pop();
      await forDispatch({type: at.PLACES_LINK_BLOCKED});

      assert.calledTwice(feed.store.dispatch);
      assert.equal(feed.store.dispatch.firstCall.args[0].data.links[0].url, url);
      assert.equal(feed.store.dispatch.secondCall.args[0].data.links[0].url, FAKE_LINKS[0].url);
    });
  });
});

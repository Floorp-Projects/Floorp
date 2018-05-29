"use strict";

import {actionTypes as at} from "common/Actions.jsm";
import {Dedupe} from "common/Dedupe.jsm";
import {GlobalOverrider} from "test/unit/utils";
import injector from "inject!lib/HighlightsFeed.jsm";
import {Screenshots} from "lib/Screenshots.jsm";

const FAKE_LINKS = new Array(20).fill(null).map((v, i) => ({url: `http://www.site${i}.com`}));
const FAKE_IMAGE = "data123";

describe("Highlights Feed", () => {
  let HighlightsFeed;
  let SECTION_ID;
  let SYNC_BOOKMARKS_FINISHED_EVENT;
  let BOOKMARKS_RESTORE_SUCCESS_EVENT;
  let BOOKMARKS_RESTORE_FAILED_EVENT;
  let feed;
  let globals;
  let sandbox;
  let links;
  let fakeScreenshot;
  let fakeNewTabUtils;
  let filterAdultStub;
  let sectionsManagerStub;
  let downloadsManagerStub;
  let shortURLStub;
  let fakePageThumbs;

  beforeEach(() => {
    globals = new GlobalOverrider();
    sandbox = globals.sandbox;
    fakeNewTabUtils = {
      activityStreamLinks: {
        getHighlights: sandbox.spy(() => Promise.resolve(links)),
        deletePocketEntry: sandbox.spy(() => Promise.resolve({})),
        archivePocketEntry: sandbox.spy(() => Promise.resolve({}))
      },
      activityStreamProvider: {_processHighlights: sandbox.spy(l => l.slice(0, 1))}
    };
    sectionsManagerStub = {
      onceInitialized: sinon.stub().callsFake(callback => callback()),
      enableSection: sinon.spy(),
      disableSection: sinon.spy(),
      updateSection: sinon.spy(),
      updateSectionCard: sinon.spy(),
      sections: new Map([["highlights", {id: "highlights"}]])
    };
    downloadsManagerStub = sinon.stub().returns({
      getDownloads: () => [{"url": "https://site.com/download"}],
      onAction: sinon.spy(),
      init: sinon.spy()
    });
    fakeScreenshot = {
      getScreenshotForURL: sandbox.spy(() => Promise.resolve(FAKE_IMAGE)),
      maybeCacheScreenshot: Screenshots.maybeCacheScreenshot,
      _shouldGetScreenshots: sinon.stub().returns(true)
    };
    filterAdultStub = sinon.stub().returns([]);
    shortURLStub = sinon.stub().callsFake(site => site.url.match(/\/([^/]+)/)[1]);
    fakePageThumbs = {
      addExpirationFilter: sinon.stub(),
      removeExpirationFilter: sinon.stub()
    };

    globals.set("NewTabUtils", fakeNewTabUtils);
    globals.set("PageThumbs", fakePageThumbs);
    ({HighlightsFeed, SECTION_ID, SYNC_BOOKMARKS_FINISHED_EVENT, BOOKMARKS_RESTORE_SUCCESS_EVENT, BOOKMARKS_RESTORE_FAILED_EVENT} = injector({
      "lib/FilterAdult.jsm": {filterAdult: filterAdultStub},
      "lib/ShortURL.jsm": {shortURL: shortURLStub},
      "lib/SectionsManager.jsm": {SectionsManager: sectionsManagerStub},
      "lib/Screenshots.jsm": {Screenshots: fakeScreenshot},
      "common/Dedupe.jsm": {Dedupe},
      "lib/DownloadsManager.jsm": {DownloadsManager: downloadsManagerStub}
    }));
    sandbox.spy(global.Services.obs, "addObserver");
    sandbox.spy(global.Services.obs, "removeObserver");
    feed = new HighlightsFeed();
    feed.store = {
      dispatch: sinon.spy(),
      getState() { return this.state; },
      state: {
        Prefs: {values: {"filterAdult": false, "section.highlights.includePocket": false, "section.highlights.includeDownloads": false}},
        TopSites: {
          initialized: true,
          rows: Array(12).fill(null).map((v, i) => ({url: `http://www.topsite${i}.com`}))
        },
        Sections: [{id: "highlights", initialized: false}]
      },
      subscribe: sinon.stub().callsFake(cb => { cb(); return () => {}; })
    };
    links = FAKE_LINKS;
  });
  afterEach(() => {
    globals.restore();
  });

  describe("#init", () => {
    it("should create a HighlightsFeed", () => {
      assert.instanceOf(feed, HighlightsFeed);
    });
    it("should register a expiration filter", () => {
      assert.calledOnce(fakePageThumbs.addExpirationFilter);
    });
    it("should add the sync observer", () => {
      feed.onAction({type: at.INIT});
      assert.calledWith(global.Services.obs.addObserver, feed, SYNC_BOOKMARKS_FINISHED_EVENT);
      assert.calledWith(global.Services.obs.addObserver, feed, BOOKMARKS_RESTORE_SUCCESS_EVENT);
      assert.calledWith(global.Services.obs.addObserver, feed, BOOKMARKS_RESTORE_FAILED_EVENT);
    });
    it("should call SectionsManager.onceInitialized on INIT", () => {
      feed.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.onceInitialized);
    });
    it("should enable its section", () => {
      feed.onAction({type: at.INIT});
      assert.calledOnce(sectionsManagerStub.enableSection);
      assert.calledWith(sectionsManagerStub.enableSection, SECTION_ID);
    });
    it("should fetch highlights on postInit", () => {
      feed.fetchHighlights = sinon.spy();
      feed.postInit();
      assert.calledOnce(feed.fetchHighlights);
    });
    it("should hook up the store for the DownloadsManager", () => {
      feed.onAction({type: at.INIT});
      assert.calledOnce(feed.downloadsManager.init);
    });
  });
  describe("#observe", () => {
    beforeEach(() => {
      feed.fetchHighlights = sinon.spy();
    });
    it("should fetch higlights when we are done a sync for bookmarks", () => {
      feed.observe(null, SYNC_BOOKMARKS_FINISHED_EVENT, "bookmarks");
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights after a successful import", () => {
      feed.observe(null, BOOKMARKS_RESTORE_SUCCESS_EVENT, "html");
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights after a failed import", () => {
      feed.observe(null, BOOKMARKS_RESTORE_FAILED_EVENT, "json");
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should not fetch higlights when we are doing a sync for something that is not bookmarks", () => {
      feed.observe(null, SYNC_BOOKMARKS_FINISHED_EVENT, "tabs");
      assert.notCalled(feed.fetchHighlights);
    });
    it("should not fetch higlights for other events", () => {
      feed.observe(null, "someotherevent", "bookmarks");
      assert.notCalled(feed.fetchHighlights);
    });
  });
  describe("#filterForThumbnailExpiration", () => {
    it("should pass rows.urls to the callback provided", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com"}];
      feed.store.state.Sections = [{id: "highlights", rows, initialized: true}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, rows.map(r => r.url));
    });
    it("should include preview_image_url (if present) in the callback results", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com", "preview_image_url": "bar.jpg"}];
      feed.store.state.Sections = [{id: "highlights", rows, initialized: true}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, ["foo.com", "bar.com", "bar.jpg"]);
    });
    it("should pass an empty array if not initialized", () => {
      const rows = [{url: "foo.com"}, {"url": "bar.com"}];
      feed.store.state.Sections = [{rows, initialized: false}];
      const stub = sinon.stub();

      feed.filterForThumbnailExpiration(stub);

      assert.calledOnce(stub);
      assert.calledWithExactly(stub, []);
    });
  });
  describe("#fetchHighlights", () => {
    const fetchHighlights = async options => {
      await feed.fetchHighlights(options);
      return sectionsManagerStub.updateSection.firstCall.args[1].rows;
    };
    it("should return early if TopSites are not initialised", async () => {
      sandbox.spy(feed.linksCache, "request");
      feed.store.state.TopSites.initialized = false;
      feed.store.state.Prefs.values["feeds.topsites"] = true;

      // Initially TopSites is uninitialised and fetchHighlights should return.
      await feed.fetchHighlights();

      assert.notCalled(fakeNewTabUtils.activityStreamLinks.getHighlights);
      assert.notCalled(feed.linksCache.request);
    });
    it("should return early if Sections are not initialised", async () => {
      sandbox.spy(feed.linksCache, "request");
      feed.store.state.TopSites.initialized = true;
      feed.store.state.Prefs.values["feeds.topsites"] = true;
      feed.store.state.Sections = [];

      await feed.fetchHighlights();

      assert.notCalled(fakeNewTabUtils.activityStreamLinks.getHighlights);
      assert.notCalled(feed.linksCache.request);
    });
    it("should fetch Highlights if TopSites are initialised", async () => {
      sandbox.spy(feed.linksCache, "request");
      // fetchHighlights should continue
      feed.store.state.TopSites.initialized = true;

      await feed.fetchHighlights();

      assert.calledOnce(feed.linksCache.request);
      assert.calledOnce(fakeNewTabUtils.activityStreamLinks.getHighlights);
    });
    it("should chronologically order highlight data types", async () => {
      links = [
        {url: "https://site0.com", type: "bookmark", bookmarkGuid: "1234", date_added: Date.now() - 80}, // 4th newest
        {url: "https://site1.com", type: "history", bookmarkGuid: "1234", date_added: Date.now() - 60}, // 3rd newest
        {url: "https://site2.com", type: "history", date_added: Date.now() - 160}, // append at the end
        {url: "https://site3.com", type: "history", date_added: Date.now() - 60}, // append at the end
        {url: "https://site4.com", type: "pocket", date_added: Date.now()}, // newest highlight
        {url: "https://site5.com", type: "pocket", date_added: Date.now() - 100}, // 5th newest
        {url: "https://site6.com", type: "bookmark", bookmarkGuid: "1234", date_added: Date.now() - 40} // 2nd newest
      ];

      let highlights = await fetchHighlights();
      assert.equal(highlights[0].url, links[4].url);
      assert.equal(highlights[1].url, links[6].url);
      assert.equal(highlights[2].url, links[1].url);
      assert.equal(highlights[3].url, links[0].url);
      assert.equal(highlights[4].url, links[5].url);
      assert.equal(highlights[5].url, links[2].url);
      assert.equal(highlights[6].url, links[3].url);
    });
    it("should fetch Highlights if TopSites are not enabled", async () => {
      sandbox.spy(feed.linksCache, "request");
      feed.store.state.Prefs.values["feeds.topsites"] = false;

      await feed.fetchHighlights();

      assert.calledOnce(feed.linksCache.request);
      assert.calledOnce(fakeNewTabUtils.activityStreamLinks.getHighlights);
    });
    it("should add hostname and hasImage to each link", async () => {
      links = [{url: "https://mozilla.org"}];

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].hostname, "mozilla.org");
      assert.equal(highlights[0].hasImage, true);
    });
    it("should add an existing image if it exists to the link without calling fetchImage", async () => {
      links = [{url: "https://mozilla.org", image: FAKE_IMAGE}];
      sinon.spy(feed, "fetchImage");

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].image, FAKE_IMAGE);
      assert.notCalled(feed.fetchImage);
    });
    it("should call fetchImage with the correct arguments for new links", async () => {
      links = [{url: "https://mozilla.org", preview_image_url: "https://mozilla.org/preview.jog"}];
      sinon.spy(feed, "fetchImage");

      await feed.fetchHighlights();

      assert.calledOnce(feed.fetchImage);
      const [arg] = feed.fetchImage.firstCall.args;
      assert.propertyVal(arg, "url", links[0].url);
      assert.propertyVal(arg, "preview_image_url", links[0].preview_image_url);
    });
    it("should not include any links already in Top Sites", async () => {
      links = [
        {url: "https://mozilla.org"},
        {url: "http://www.topsite0.com"},
        {url: "http://www.topsite1.com"},
        {url: "http://www.topsite2.com"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 1);
      assert.equal(highlights[0].url, links[0].url);
    });
    it("should include bookmark but not history already in Top Sites", async () => {
      links = [
        {url: "http://www.topsite0.com", type: "bookmark"},
        {url: "http://www.topsite1.com", type: "history"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 1);
      assert.equal(highlights[0].url, links[0].url);
    });
    it("should not include history of same hostname as a bookmark", async () => {
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/history", type: "history"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 1);
      assert.equal(highlights[0].url, links[0].url);
    });
    it("should take the first history of a hostname", async () => {
      links = [
        {url: "https://site.com/first", type: "history"},
        {url: "https://site.com/second", type: "history"},
        {url: "https://other", type: "history"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 2);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, links[2].url);
    });
    it("should take a bookmark, a pocket, and downloaded item of the same hostname", async () => {
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/pocket", type: "pocket"},
        {url: "https://site.com/download", type: "download"}
      ];

      const highlights = await fetchHighlights();

      assert.equal(highlights.length, 3);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, links[1].url);
      assert.equal(highlights[2].url, links[2].url);
    });
    it("should includePocket pocket items when pref is true", async () => {
      feed.store.state.Prefs.values["section.highlights.includePocket"] = true;
      sandbox.spy(feed.linksCache, "request");
      await feed.fetchHighlights();

      assert.propertyVal(feed.linksCache.request.firstCall.args[0], "excludePocket", false);
    });
    it("should not includePocket pocket items when pref is false", async () => {
      sandbox.spy(feed.linksCache, "request");
      await feed.fetchHighlights();

      assert.propertyVal(feed.linksCache.request.firstCall.args[0], "excludePocket", true);
    });
    it("should not include downloads when includeDownloads pref is false", async () => {
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/pocket", type: "pocket"}
      ];

      // Check that we don't have the downloaded item in highlights
      const highlights = await fetchHighlights();
      assert.equal(highlights.length, 2);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, links[1].url);

      assert.notCalled(global.NewTabUtils.activityStreamProvider._processHighlights);
    });
    it("should include downloads when includeDownloads pref is true", async () => {
      feed.store.state.Prefs.values["section.highlights.includeDownloads"] = true;
      links = [
        {url: "https://site.com/bookmark", type: "bookmark"},
        {url: "https://site.com/pocket", type: "pocket"}
      ];

      // Check that we did get the downloaded item in highlights
      const highlights = await fetchHighlights();
      assert.equal(highlights.length, 3);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, links[1].url);
      assert.equal(highlights[2].url, "https://site.com/download");

      assert.calledOnce(global.NewTabUtils.activityStreamProvider._processHighlights);
    });
    it("should only take 1 download", async () => {
      feed.store.state.Prefs.values["section.highlights.includeDownloads"] = true;
      feed.downloadsManager.getDownloads = () => [
        {"url": "https://site1.com/download"},
        {"url": "https://site2.com/download"}
      ];
      links = [{url: "https://site.com/bookmark", type: "bookmark"}];

      // Check that we did get the most single recent downloaded item in highlights
      const highlights = await fetchHighlights();
      assert.equal(highlights.length, 2);
      assert.equal(highlights[0].url, links[0].url);
      assert.equal(highlights[1].url, "https://site1.com/download");

      assert.calledOnce(global.NewTabUtils.activityStreamProvider._processHighlights);
    });
    it("should sort bookmarks, pocket, and downloads chronologically", async () => {
      feed.store.state.Prefs.values["section.highlights.includeDownloads"] = true;
      feed.downloadsManager.getDownloads = () => [
        {url: "https://site1.com/download", type: "download", date_added: Date.now()}
      ];
      links = [
        {url: "https://site.com/bookmark", type: "bookmark", date_added: Date.now() - 10000},
        {url: "https://site2.com/pocket", type: "pocket", date_added: Date.now() - 5000},
        {url: "https://site3.com/visited", type: "history", date_added: Date.now()}
      ];

      // Check that the higlights are ordered chronologically by their 'date_added'
      const highlights = await fetchHighlights();
      assert.equal(highlights.length, 4);
      assert.equal(highlights[0].url, "https://site1.com/download");
      assert.equal(highlights[1].url, links[1].url);
      assert.equal(highlights[2].url, links[0].url);
      assert.equal(highlights[3].url, links[2].url); // history item goes last
    });
    it("should set type to bookmark if there is a bookmarkGuid", async () => {
      feed.store.state.Prefs.values["section.highlights.includeBookmarks"] = true;
      links = [{url: "https://mozilla.org", type: "history", bookmarkGuid: "1234567890"}];

      const highlights = await fetchHighlights();

      assert.equal(highlights[0].type, "bookmark");
    });
    it("should keep history type if there is a bookmarkGuid but don't include bookmarks", async () => {
      feed.store.state.Prefs.values["section.highlights.includeBookmarks"] = false;
      links = [{url: "https://mozilla.org", type: "history", bookmarkGuid: "1234567890"}];

      const highlights = await fetchHighlights();

      assert.propertyVal(highlights[0], "type", "history");
    });
    it("should not filter out adult pages when pref is false", async () => {
      await feed.fetchHighlights();

      assert.notCalled(filterAdultStub);
    });
    it("should filter out adult pages when pref is true", async () => {
      feed.store.state.Prefs.values.filterAdult = true;

      const highlights = await fetchHighlights();

      // The stub filters out everything
      assert.calledOnce(filterAdultStub);
      assert.equal(highlights.length, 0);
    });
    it("should not expose internal link properties", async () => {
      const highlights = await fetchHighlights();

      const internal = Object.keys(highlights[0]).filter(key => key.startsWith("__"));
      assert.equal(internal.join(""), "");
    });
    it("should broadcast if feed is not initialized", async () => {
      links = [];
      await fetchHighlights();

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, true);
    });
    it("should broadcast if options.broadcast is true", async () => {
      links = [];
      feed.store.state.Sections[0].initialized = true;
      await fetchHighlights({broadcast: true});

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, true);
    });
    it("should not broadcast if options.broadcast is false and initialized is true", async () => {
      links = [];
      feed.store.state.Sections[0].initialized = true;
      await fetchHighlights({broadcast: false});

      assert.calledOnce(sectionsManagerStub.updateSection);
      assert.calledWithExactly(sectionsManagerStub.updateSection, SECTION_ID, {rows: []}, false);
    });
  });
  describe("#fetchImage", () => {
    const FAKE_URL = "https://mozilla.org";
    const FAKE_IMAGE_URL = "https://mozilla.org/preview.jpg";
    function fetchImage(page) {
      return feed.fetchImage(Object.assign({__sharedCache: {updateLink() {}}},
        page));
    }
    it("should capture the image, if available", async () => {
      await fetchImage({
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      });

      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_IMAGE_URL);
    });
    it("should fall back to capturing a screenshot", async () => {
      await fetchImage({url: FAKE_URL});

      assert.calledOnce(fakeScreenshot.getScreenshotForURL);
      assert.calledWith(fakeScreenshot.getScreenshotForURL, FAKE_URL);
    });
    it("should call SectionsManager.updateSectionCard with the right arguments", async () => {
      await fetchImage({
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      });

      assert.calledOnce(sectionsManagerStub.updateSectionCard);
      assert.calledWith(sectionsManagerStub.updateSectionCard, "highlights", FAKE_URL, {image: FAKE_IMAGE}, true);
    });
    it("should not update the card with the image", async () => {
      const card = {
        preview_image_url: FAKE_IMAGE_URL,
        url: FAKE_URL
      };

      await feed.fetchImage(card);

      assert.notProperty(card, "image");
    });
  });
  describe("#uninit", () => {
    it("should disable its section", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(sectionsManagerStub.disableSection);
      assert.calledWith(sectionsManagerStub.disableSection, SECTION_ID);
    });
    it("should remove the expiration filter", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledOnce(fakePageThumbs.removeExpirationFilter);
    });
    it("should remove the sync and Places observers", () => {
      feed.onAction({type: at.UNINIT});
      assert.calledWith(global.Services.obs.removeObserver, feed, SYNC_BOOKMARKS_FINISHED_EVENT);
      assert.calledWith(global.Services.obs.removeObserver, feed, BOOKMARKS_RESTORE_SUCCESS_EVENT);
      assert.calledWith(global.Services.obs.removeObserver, feed, BOOKMARKS_RESTORE_FAILED_EVENT);
    });
  });
  describe("#onAction", () => {
    it("should relay all actions to DownloadsManager.onAction", () => {
      let action = {type: at.COPY_DOWNLOAD_LINK, data: {url: "foo.png"}, _target: {}};
      feed.onAction(action);
      assert.calledWith(feed.downloadsManager.onAction, action);
    });
    it("should fetch highlights on SYSTEM_TICK", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.SYSTEM_TICK});

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: false});
    });
    it("should fetch highlights on PREF_CHANGED for include prefs", async () => {
      feed.fetchHighlights = sinon.spy();

      feed.onAction({type: at.PREF_CHANGED, data: {name: "section.highlights.includeBookmarks"}});

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should not fetch highlights on PREF_CHANGED for other prefs", async () => {
      feed.fetchHighlights = sinon.spy();

      feed.onAction({type: at.PREF_CHANGED, data: {name: "section.topstories.showDisclaimer"}});

      assert.notCalled(feed.fetchHighlights);
    });
    it("should fetch highlights on MIGRATION_COMPLETED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.MIGRATION_COMPLETED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_HISTORY_CLEARED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_HISTORY_CLEARED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on DOWNLOAD_CHANGED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.DOWNLOAD_CHANGED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights on PLACES_LINKS_CHANGED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      sandbox.stub(feed.linksCache, "expire");

      feed.onAction({type: at.PLACES_LINKS_CHANGED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: false});
      assert.calledOnce(feed.linksCache.expire);
    });
    it("should fetch highlights on PLACES_LINK_BLOCKED", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      feed.onAction({type: at.PLACES_LINK_BLOCKED});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: true});
    });
    it("should fetch highlights and expire the cache on PLACES_SAVED_TO_POCKET", async () => {
      await feed.fetchHighlights();
      feed.fetchHighlights = sinon.spy();
      sandbox.stub(feed.linksCache, "expire");

      feed.onAction({type: at.PLACES_SAVED_TO_POCKET});
      assert.calledOnce(feed.fetchHighlights);
      assert.calledWith(feed.fetchHighlights, {broadcast: false});
      assert.calledOnce(feed.linksCache.expire);
    });
    it("should call fetchHighlights with broadcast false on TOP_SITES_UPDATED", () => {
      sandbox.stub(feed, "fetchHighlights");
      feed.onAction({type: at.TOP_SITES_UPDATED});

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: false});
    });
    it("should call deleteFromPocket on DELETE_FROM_POCKET", () => {
      sandbox.stub(feed, "deleteFromPocket");
      feed.onAction({type: at.DELETE_FROM_POCKET, data: {pocket_id: 12345}});

      assert.calledOnce(feed.deleteFromPocket);
      assert.calledWithExactly(feed.deleteFromPocket, 12345);
    });
    it("should call fetchHighlights when deleting from Pocket", async () => {
      feed.fetchHighlights = sinon.spy();
      await feed.deleteFromPocket(12345);

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: true});
    });
    it("should catch if deletePocketEntry throws", async () => {
      sandbox.spy(global.Cu, "reportError");
      fakeNewTabUtils.activityStreamLinks.deletePocketEntry = sandbox.stub().rejects("not ok");
      await feed.deleteFromPocket(12345);

      assert.calledOnce(global.Cu.reportError);
    });
    it("should call NewTabUtils.deletePocketEntry when deleting from Pocket", async () => {
      await feed.deleteFromPocket(12345);

      assert.calledOnce(global.NewTabUtils.activityStreamLinks.deletePocketEntry);
      assert.calledWith(global.NewTabUtils.activityStreamLinks.deletePocketEntry, 12345);
    });
    it("should call archiveFromPocket on ARCHIVE_FROM_POCKET", () => {
      sandbox.stub(feed, "archiveFromPocket");
      feed.onAction({type: at.ARCHIVE_FROM_POCKET, data: {pocket_id: 12345}});

      assert.calledOnce(feed.archiveFromPocket);
      assert.calledWithExactly(feed.archiveFromPocket, 12345);
    });
    it("should call fetchHighlights when archiving from Pocket", async () => {
      feed.fetchHighlights = sinon.spy();
      await feed.archiveFromPocket(12345);

      assert.calledOnce(feed.fetchHighlights);
      assert.calledWithExactly(feed.fetchHighlights, {broadcast: true});
    });
    it("should catch if archiveFromPocket throws", async () => {
      sandbox.spy(global.Cu, "reportError");
      fakeNewTabUtils.activityStreamLinks.archivePocketEntry = sandbox.stub().rejects("not ok");
      await feed.archiveFromPocket(12345);

      assert.calledOnce(global.Cu.reportError);
    });
    it("should call NewTabUtils.archivePocketEntry when deleting from Pocket", async () => {
      await feed.archiveFromPocket(12345);

      assert.calledOnce(global.NewTabUtils.activityStreamLinks.archivePocketEntry);
      assert.calledWith(global.NewTabUtils.activityStreamLinks.archivePocketEntry, 12345);
    });
  });
});
